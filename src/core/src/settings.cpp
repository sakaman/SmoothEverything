#include "smootheverything/settings.h"

#include "smootheverything/json.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <utility>

namespace smootheverything {
namespace {

[[nodiscard]] bool ReadBoolean(const JsonValue& object, const std::string_view key, const bool fallback) {
    const JsonValue* value = object.Find(key);
    return value != nullptr && value->IsBoolean() ? value->AsBoolean() : fallback;
}

[[nodiscard]] double ReadNumber(const JsonValue& object, const std::string_view key, const double fallback) {
    const JsonValue* value = object.Find(key);
    return value != nullptr && value->IsNumber() && std::isfinite(value->AsNumber())
        ? value->AsNumber()
        : fallback;
}

[[nodiscard]] std::string ReadString(
    const JsonValue& object,
    const std::string_view key,
    const std::string_view fallback = {}) {
    const JsonValue* value = object.Find(key);
    return value != nullptr && value->IsString() ? value->AsString() : std::string(fallback);
}

[[nodiscard]] MotionSettings ReadMotion(const JsonValue* value, const MotionSettings fallback = {}) {
    if (value == nullptr || !value->IsObject()) {
        return NormalizeMotionSettings(fallback);
    }
    MotionSettings motion = fallback;
    motion.distance_scale = ReadNumber(*value, "distance_scale", motion.distance_scale);
    motion.animation_time_ms = ReadNumber(*value, "animation_time_ms", motion.animation_time_ms);
    motion.acceleration_window_ms = ReadNumber(
        *value, "acceleration_window_ms", motion.acceleration_window_ms);
    motion.acceleration_max = ReadNumber(*value, "acceleration_max", motion.acceleration_max);
    motion.tail_to_head_ratio = ReadNumber(*value, "tail_to_head_ratio", motion.tail_to_head_ratio);
    motion.easing_enabled = ReadBoolean(*value, "easing_enabled", motion.easing_enabled);
    return NormalizeMotionSettings(motion);
}

[[nodiscard]] JsonValue WriteMotion(const MotionSettings& source) {
    const MotionSettings motion = NormalizeMotionSettings(source);
    return JsonValue::Object{
        {"acceleration_max", motion.acceleration_max},
        {"acceleration_window_ms", motion.acceleration_window_ms},
        {"animation_time_ms", motion.animation_time_ms},
        {"distance_scale", motion.distance_scale},
        {"easing_enabled", motion.easing_enabled},
        {"tail_to_head_ratio", motion.tail_to_head_ratio},
    };
}

void NormalizeCollection(AppSettings& settings) {
    std::set<std::string, std::less<>> excluded;
    for (const auto& item : settings.excluded_apps) {
        std::string key = NormalizeExecutableKey(item);
        if (!key.empty()) {
            excluded.insert(std::move(key));
        }
    }
    settings.excluded_apps.assign(excluded.begin(), excluded.end());

    std::set<std::string, std::less<>> profile_keys;
    std::vector<AppProfile> profiles;
    profiles.reserve(settings.profiles.size());
    for (auto& profile : settings.profiles) {
        profile.executable = NormalizeExecutableKey(profile.executable);
        profile.motion = NormalizeMotionSettings(profile.motion);
        if (!profile.executable.empty() && profile_keys.insert(profile.executable).second) {
            profiles.push_back(std::move(profile));
        }
    }
    settings.profiles = std::move(profiles);
}

}  // namespace

AppSettings DefaultSettings() {
    return AppSettings{};
}

SettingsParseResult ParseSettings(const std::string_view json) noexcept {
    try {
        const JsonValue root = ParseJson(json);
        if (!root.IsObject()) {
            return {.value = std::nullopt, .error = "settings root must be a JSON object"};
        }

        const JsonValue* schema = root.Find("schema_version");
        if (schema == nullptr || !schema->IsNumber()) {
            return {.value = std::nullopt, .error = "schema_version is required"};
        }
        const int schema_version = static_cast<int>(schema->AsNumber());
        if (schema_version != kSettingsSchemaVersion) {
            return {.value = std::nullopt, .error = "unsupported settings schema_version"};
        }

        AppSettings result = DefaultSettings();
        result.schema_version = schema_version;
        result.enabled = ReadBoolean(root, "enabled", result.enabled);
        result.motion = ReadMotion(root.Find("motion"), result.motion);

        if (const JsonValue* input = root.Find("input"); input != nullptr && input->IsObject()) {
            result.horizontal_smoothing = ReadBoolean(
                *input, "horizontal_smoothing", result.horizontal_smoothing);
            result.shift_for_horizontal = ReadBoolean(
                *input, "shift_for_horizontal", result.shift_for_horizontal);
            result.reverse_direction = ReadBoolean(*input, "reverse_direction", result.reverse_direction);
            result.pass_through_ctrl = ReadBoolean(*input, "pass_through_ctrl", result.pass_through_ctrl);
            result.pass_through_alt = ReadBoolean(*input, "pass_through_alt", result.pass_through_alt);
            result.bypass_high_resolution = ReadBoolean(
                *input, "bypass_high_resolution", result.bypass_high_resolution);
        }

        if (const JsonValue* system = root.Find("system"); system != nullptr && system->IsObject()) {
            result.start_with_windows = ReadBoolean(
                *system, "start_with_windows", result.start_with_windows);
            result.show_tray_icon = ReadBoolean(*system, "show_tray_icon", result.show_tray_icon);
        }

        if (const JsonValue* excluded = root.Find("excluded_apps");
            excluded != nullptr && excluded->IsArray()) {
            for (const auto& value : excluded->AsArray()) {
                if (value.IsString()) {
                    result.excluded_apps.push_back(value.AsString());
                }
            }
        }

        if (const JsonValue* profiles = root.Find("profiles"); profiles != nullptr && profiles->IsArray()) {
            for (const auto& value : profiles->AsArray()) {
                if (!value.IsObject()) {
                    continue;
                }
                AppProfile profile;
                profile.executable = ReadString(value, "executable");
                profile.enabled = ReadBoolean(value, "enabled", profile.enabled);
                profile.compatibility_mode = ReadBoolean(
                    value, "compatibility_mode", profile.compatibility_mode);
                profile.motion = ReadMotion(value.Find("motion"), result.motion);
                result.profiles.push_back(std::move(profile));
            }
        }

        NormalizeCollection(result);
        return {.value = std::move(result), .error = {}};
    } catch (const JsonParseError& error) {
        return {
            .value = std::nullopt,
            .error = "JSON parse error at byte " + std::to_string(error.Offset()) + ": " + error.what(),
        };
    } catch (const std::exception& error) {
        return {.value = std::nullopt, .error = error.what()};
    }
}

std::string SerializeSettings(const AppSettings& source, const bool pretty) {
    AppSettings settings = source;
    settings.schema_version = kSettingsSchemaVersion;
    settings.motion = NormalizeMotionSettings(settings.motion);
    NormalizeCollection(settings);

    JsonValue::Array excluded;
    excluded.reserve(settings.excluded_apps.size());
    for (const auto& executable : settings.excluded_apps) {
        excluded.emplace_back(executable);
    }

    JsonValue::Array profiles;
    profiles.reserve(settings.profiles.size());
    for (const auto& profile : settings.profiles) {
        profiles.emplace_back(JsonValue::Object{
            {"compatibility_mode", profile.compatibility_mode},
            {"enabled", profile.enabled},
            {"executable", profile.executable},
            {"motion", WriteMotion(profile.motion)},
        });
    }

    const JsonValue root{JsonValue::Object{
        {"enabled", settings.enabled},
        {"excluded_apps", std::move(excluded)},
        {"input", JsonValue::Object{
            {"bypass_high_resolution", settings.bypass_high_resolution},
            {"horizontal_smoothing", settings.horizontal_smoothing},
            {"pass_through_alt", settings.pass_through_alt},
            {"pass_through_ctrl", settings.pass_through_ctrl},
            {"reverse_direction", settings.reverse_direction},
            {"shift_for_horizontal", settings.shift_for_horizontal},
        }},
        {"motion", WriteMotion(settings.motion)},
        {"profiles", std::move(profiles)},
        {"schema_version", settings.schema_version},
        {"system", JsonValue::Object{
            {"show_tray_icon", settings.show_tray_icon},
            {"start_with_windows", settings.start_with_windows},
        }},
    }};
    return SerializeJson(root, pretty);
}

std::string NormalizeExecutableKey(const std::string_view value) {
    const std::size_t separator = value.find_last_of("/\\");
    const std::string_view basename = separator == std::string_view::npos ? value : value.substr(separator + 1);
    std::string result;
    result.reserve(basename.size());
    for (const char raw_character : basename) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character >= 'A' && character <= 'Z') {
            result.push_back(static_cast<char>(character - 'A' + 'a'));
        } else if (character > 0x20U) {
            result.push_back(static_cast<char>(character));
        }
    }
    return result;
}

ResolvedAppPolicy ResolveAppPolicy(const AppSettings& settings, const std::string_view executable) {
    const std::string key = NormalizeExecutableKey(executable);
    const auto excluded = std::find(settings.excluded_apps.begin(), settings.excluded_apps.end(), key);
    if (excluded != settings.excluded_apps.end()) {
        return {
            .smooth_enabled = false,
            .compatibility_mode = false,
            .motion = NormalizeMotionSettings(settings.motion),
        };
    }

    const auto profile = std::find_if(
        settings.profiles.begin(),
        settings.profiles.end(),
        [&key](const AppProfile& candidate) {
            return NormalizeExecutableKey(candidate.executable) == key;
        });
    if (profile != settings.profiles.end()) {
        return {
            .smooth_enabled = settings.enabled && profile->enabled && !profile->compatibility_mode,
            .compatibility_mode = profile->compatibility_mode,
            .motion = NormalizeMotionSettings(profile->motion),
        };
    }

    return {
        .smooth_enabled = settings.enabled,
        .compatibility_mode = false,
        .motion = NormalizeMotionSettings(settings.motion),
    };
}

}  // namespace smootheverything
