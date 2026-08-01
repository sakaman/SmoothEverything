#pragma once

#include "smootheverything/engine/input_hook.h"
#include "smootheverything/engine/motion_worker.h"
#include "smootheverything/engine/pipe_server.h"
#include "smootheverything/engine/runtime_state.h"
#include "smootheverything/engine/settings_store.h"
#include "smootheverything/engine/target_policy.h"

#include <windows.h>
#include <shellapi.h>

#include <mutex>
#include <string>
#include <string_view>

namespace smootheverything::engine {

class EngineApp final {
public:
    EngineApp(
        HINSTANCE instance,
        SettingsStore store,
        AppSettings settings,
        std::string load_error,
        bool sync_autostart);
    ~EngineApp();

    EngineApp(const EngineApp&) = delete;
    EngineApp& operator=(const EngineApp&) = delete;

    [[nodiscard]] int Run() noexcept;

private:
    enum : UINT {
        kTrayCallbackMessage = WM_APP + 1,
        kSettingsChangedMessage = WM_APP + 2,
    };

    enum : UINT {
        kCommandToggle = 1001,
        kCommandSettings = 1002,
        kCommandExit = 1003,
        kHotkeyToggle = 2001,
    };

    [[nodiscard]] static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM word,
        LPARAM data) noexcept;
    [[nodiscard]] LRESULT OnWindowMessage(UINT message, WPARAM word, LPARAM data) noexcept;

    [[nodiscard]] bool CreateMessageWindow() noexcept;
    void DestroyMessageWindow() noexcept;
    void RefreshTrayIcon() noexcept;
    void RemoveTrayIcon() noexcept;
    void ShowTrayMenu() noexcept;
    void OpenSettings() noexcept;
    void ToggleEnabled() noexcept;
    [[nodiscard]] bool ApplySettings(AppSettings settings, std::string& error) noexcept;
    [[nodiscard]] std::string HandlePipeRequest(std::string_view request);
    [[nodiscard]] std::string StateResponse() const;

    HINSTANCE instance_{nullptr};
    HWND window_{nullptr};
    HANDLE instance_mutex_{nullptr};
    UINT taskbar_created_message_{0};
    NOTIFYICONDATAW tray_{};
    bool tray_added_{false};
    SettingsStore store_;
    RuntimeState runtime_;
    TargetPolicyCache policies_;
    MotionWorker worker_;
    InputHook hook_;
    PipeServer pipe_;
    mutable std::mutex settings_write_mutex_;
    std::string last_error_;
    bool sync_autostart_{true};
};

}  // namespace smootheverything::engine
