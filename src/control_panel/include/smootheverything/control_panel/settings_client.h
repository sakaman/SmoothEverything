#pragma once

#include "smootheverything/control_panel/localization.h"
#include "smootheverything/settings.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace smootheverything::control_panel {

enum class SessionStatus {
    Connecting,
    Disconnected,
    Connected,
    Applied,
    SavedOffline,
    SaveFailed,
    UnableToStart,
    WaitingForEngine,
    Saving,
    OfflineLocal,
};

struct EngineDiagnostics final {
    std::int64_t physical_events{};
    std::int64_t smoothed_events{};
    std::int64_t passed_events{};
    std::int64_t injected_events{};
    std::int64_t injected_delta{};
    std::int64_t queue_overflows{};
    std::int64_t injection_failures{};
    std::int64_t target_changes{};
    std::int64_t settings_generation{};
};

struct SessionState final {
    AppSettings settings{DefaultSettings()};
    EngineDiagnostics diagnostics{};
    bool online{};
    SessionStatus status{SessionStatus::Connecting};
    std::wstring last_error;
};

class SettingsClient final {
public:
    explicit SettingsClient(const Localizer& localizer);

    [[nodiscard]] const SessionState& State() const noexcept;
    [[nodiscard]] SessionState& MutableState() noexcept;
    [[nodiscard]] const std::filesystem::path& SettingsPath() const noexcept;

    [[nodiscard]] bool Refresh(unsigned long timeout_ms = 1200);
    [[nodiscard]] bool Apply(unsigned long timeout_ms = 1200);
    [[nodiscard]] bool StartSiblingEngine();
    [[nodiscard]] bool SaveLocal();

private:
    [[nodiscard]] bool LoadLocal();
    [[nodiscard]] bool SendRequest(
        std::string_view request,
        std::string& response,
        unsigned long timeout_ms);
    [[nodiscard]] bool ApplyResponse(std::string_view response, bool replace_settings);
    void SetWin32Error(std::wstring_view operation, unsigned long error);

    const Localizer& localizer_;
    SessionState state_{};
    std::filesystem::path settings_path_;
};

[[nodiscard]] std::wstring Utf8ToWide(std::string_view value);
[[nodiscard]] std::string WideToUtf8(std::wstring_view value);

}  // namespace smootheverything::control_panel
