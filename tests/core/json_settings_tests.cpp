#include "smootheverything/json.h"
#include "smootheverything/settings.h"
#include "smootheverything/engine/event_queue.h"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace motion_tests {
std::vector<std::pair<std::string, std::function<void()>>> Cases();
}

namespace {

using smootheverything::AppProfile;
using smootheverything::DefaultSettings;
using smootheverything::JsonParseError;
using smootheverything::ParseJson;
using smootheverything::ParseSettings;
using smootheverything::ResolveAppPolicy;
using smootheverything::SerializeJson;
using smootheverything::SerializeSettings;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void JsonRoundTripsUtf8AndEscapes() {
    const std::string source = R"({"emoji":"\ud83d\ude80","line":"a\nb","name":"smooth 🚀"})";
    const auto parsed = ParseJson(source);
    Require(parsed.Find("emoji") != nullptr && parsed.Find("emoji")->AsString() == "🚀", "surrogate pair");
    Require(parsed.Find("line") != nullptr && parsed.Find("line")->AsString() == "a\nb", "escaped newline");

    const std::string serialized = SerializeJson(parsed, false);
    const auto reparsed = ParseJson(serialized);
    Require(reparsed.Find("name") != nullptr && reparsed.Find("name")->AsString() == "smooth 🚀", "UTF-8 round trip");
}

void JsonRejectsMalformedAndDuplicateInput() {
    bool malformed = false;
    try {
        static_cast<void>(ParseJson(R"({"a":01})"));
    } catch (const JsonParseError&) {
        malformed = true;
    }
    Require(malformed, "leading-zero number must be rejected");

    bool duplicate = false;
    try {
        static_cast<void>(ParseJson(R"({"a":1,"a":2})"));
    } catch (const JsonParseError&) {
        duplicate = true;
    }
    Require(duplicate, "duplicate keys must be rejected");
}

void SettingsRoundTripNormalizesProfilesAndApps() {
    auto settings = DefaultSettings();
    settings.motion.animation_time_ms = 280.0;
    settings.ui_language = "zh-CN";
    settings.excluded_apps = {"C:\\Apps\\GAME.EXE", "game.exe", "  "};
    settings.profiles = {
        AppProfile{.executable = "C:\\Tools\\EDITOR.EXE", .enabled = true},
        AppProfile{.executable = "editor.exe", .enabled = false},
    };

    const auto parsed = ParseSettings(SerializeSettings(settings));
    Require(parsed.value.has_value(), "serialized settings must parse");
    Require(parsed.value->excluded_apps.size() == 1, "excluded apps must be deduplicated");
    Require(parsed.value->excluded_apps.front() == "game.exe", "excluded app key must normalize");
    Require(parsed.value->profiles.size() == 1, "profiles must be deduplicated");
    Require(parsed.value->profiles.front().executable == "editor.exe", "profile key must normalize");
    Require(std::abs(parsed.value->motion.animation_time_ms - 280.0) < 1e-12, "motion value round trip");
    Require(parsed.value->ui_language == "zh-CN", "UI language round trip");
}

void SettingsClampMotionAndIgnoreUnknownFields() {
    const auto parsed = ParseSettings(R"({
        "schema_version": 1,
        "enabled": true,
        "future_field": {"ignored": true},
        "system": {"ui_language": "fr-FR"},
        "motion": {
            "distance_scale": 99,
            "animation_time_ms": -1,
            "acceleration_max": 100,
            "tail_to_head_ratio": 0
        }
    })");
    Require(parsed.value.has_value(), "known schema with future fields must parse");
    Require(std::abs(parsed.value->motion.distance_scale - 4.0) < 1e-12, "distance clamp");
    Require(std::abs(parsed.value->motion.animation_time_ms - 1.0) < 1e-12, "duration clamp");
    Require(std::abs(parsed.value->motion.acceleration_max - 20.0) < 1e-12, "acceleration clamp");
    Require(std::abs(parsed.value->motion.tail_to_head_ratio - 0.1) < 1e-12, "tail ratio clamp");
    Require(parsed.value->ui_language == "system", "unsupported UI language must normalize");
}

void SettingsRejectMissingOrUnsupportedSchema() {
    Require(!ParseSettings(R"({"enabled":true})").value.has_value(), "missing schema must fail");
    Require(!ParseSettings(R"({"schema_version":2})").value.has_value(), "future schema must fail closed");
    Require(!ParseSettings("[]").value.has_value(), "non-object root must fail");
}

void AppPolicyHonorsExclusionsAndExactProfiles() {
    auto settings = DefaultSettings();
    settings.excluded_apps = {"game.exe"};
    settings.profiles = {
        AppProfile{
            .executable = "editor.exe",
            .enabled = true,
            .compatibility_mode = false,
            .motion = {.animation_time_ms = 180.0},
        },
        AppProfile{
            .executable = "legacy.exe",
            .enabled = true,
            .compatibility_mode = true,
        },
    };

    Require(!ResolveAppPolicy(settings, "C:\\Games\\GAME.EXE").smooth_enabled, "exclusion wins");
    const auto editor = ResolveAppPolicy(settings, "EDITOR.EXE");
    Require(editor.smooth_enabled, "enabled profile must smooth");
    Require(std::abs(editor.motion.animation_time_ms - 180.0) < 1e-12, "profile motion wins");
    const auto legacy = ResolveAppPolicy(settings, "legacy.exe");
    Require(!legacy.smooth_enabled && legacy.compatibility_mode, "compatibility profile passes through");
    Require(ResolveAppPolicy(settings, "other.exe").smooth_enabled, "default policy applies");
}

void SpscQueueIsBoundedAndPreservesOrder() {
    smootheverything::engine::SpscQueue<int, 4> queue;
    Require(queue.Empty(), "new queue must be empty");
    Require(queue.TryPush(1), "first push");
    Require(queue.TryPush(2), "second push");
    Require(queue.TryPush(3), "third push");
    Require(!queue.TryPush(4), "one slot remains reserved to distinguish full from empty");

    int value = 0;
    Require(queue.TryPop(value) && value == 1, "FIFO first value");
    Require(queue.TryPush(4), "queue must wrap after pop");
    Require(queue.TryPop(value) && value == 2, "FIFO second value");
    Require(queue.TryPop(value) && value == 3, "FIFO third value");
    Require(queue.TryPop(value) && value == 4, "FIFO wrapped value");
    Require(!queue.TryPop(value) && queue.Empty(), "empty queue must not pop");
}

}  // namespace

int main() {
    auto tests = motion_tests::Cases();
    tests.emplace_back("JSON UTF-8 and escapes", JsonRoundTripsUtf8AndEscapes);
    tests.emplace_back("JSON malformed input", JsonRejectsMalformedAndDuplicateInput);
    tests.emplace_back("settings round trip and normalization", SettingsRoundTripNormalizesProfilesAndApps);
    tests.emplace_back("settings clamp and forward fields", SettingsClampMotionAndIgnoreUnknownFields);
    tests.emplace_back("settings schema validation", SettingsRejectMissingOrUnsupportedSchema);
    tests.emplace_back("per-app policy", AppPolicyHonorsExclusionsAndExactProfiles);
    tests.emplace_back("bounded SPSC queue", SpscQueueIsBoundedAndPreservesOrder);

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << tests.size() << " test(s) passed\n";
    return EXIT_SUCCESS;
}
