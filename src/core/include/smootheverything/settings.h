#pragma once

#include "smootheverything/motion_engine.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace smootheverything {

inline constexpr int kSettingsSchemaVersion = 1;

struct AppProfile final {
    std::string executable;
    bool enabled{true};
    bool compatibility_mode{false};
    MotionSettings motion{};
};

struct AppSettings final {
    int schema_version{kSettingsSchemaVersion};
    bool enabled{true};
    MotionSettings motion{};
    bool horizontal_smoothing{true};
    bool shift_for_horizontal{true};
    bool reverse_direction{false};
    bool pass_through_ctrl{true};
    bool pass_through_alt{true};
    bool bypass_high_resolution{true};
    bool start_with_windows{false};
    bool show_tray_icon{true};
    std::string ui_language{"system"};
    std::vector<std::string> excluded_apps;
    std::vector<AppProfile> profiles;
};

struct SettingsParseResult final {
    std::optional<AppSettings> value;
    std::string error;
};

struct ResolvedAppPolicy final {
    bool smooth_enabled{true};
    bool compatibility_mode{false};
    MotionSettings motion{};
};

[[nodiscard]] AppSettings DefaultSettings();
[[nodiscard]] SettingsParseResult ParseSettings(std::string_view json) noexcept;
[[nodiscard]] std::string SerializeSettings(const AppSettings& settings, bool pretty = true);
[[nodiscard]] std::string NormalizeExecutableKey(std::string_view value);
[[nodiscard]] std::string NormalizeUiLanguage(std::string_view value);
[[nodiscard]] ResolvedAppPolicy ResolveAppPolicy(
    const AppSettings& settings,
    std::string_view executable);

}  // namespace smootheverything
