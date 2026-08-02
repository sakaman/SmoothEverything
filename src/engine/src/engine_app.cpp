#include "smootheverything/engine/engine_app.h"

#include "smootheverything/engine/autostart.h"
#include "smootheverything/engine/control_panel_lifecycle.h"
#include "smootheverything/engine/resource.h"
#include "smootheverything/json.h"
#include "smootheverything/settings.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>

namespace smootheverything::engine {
namespace {

constexpr wchar_t kWindowClassName[] = L"SmoothEverything.Engine.Window.v1";
constexpr wchar_t kInstanceMutexName[] = L"Local\\SmoothEverything.Engine.v1";

[[nodiscard]] JsonValue ErrorResponse(const std::string& message) {
    return JsonValue::Object{{"error", message}, {"ok", false}};
}

[[nodiscard]] std::wstring ParentDirectory(const std::wstring& path) {
    const std::size_t separator = path.find_last_of(L"/\\");
    return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
}

}  // namespace

EngineApp::EngineApp(
    const HINSTANCE instance,
    SettingsStore store,
    AppSettings settings,
    std::string load_error,
    const bool sync_autostart)
    : instance_(instance),
      store_(std::move(store)),
      runtime_(std::move(settings)),
      policies_(runtime_),
      worker_(runtime_, policies_),
      hook_(runtime_, policies_, worker_),
      pipe_([this](const std::string_view request) { return HandlePipeRequest(request); }),
      last_error_(std::move(load_error)),
      sync_autostart_(sync_autostart) {}

EngineApp::~EngineApp() {
    RemoveTrayIcon();
    pipe_.Stop();
    hook_.Stop();
    worker_.Stop();
    DestroyMessageWindow();
    if (instance_mutex_ != nullptr) {
        CloseHandle(instance_mutex_);
        instance_mutex_ = nullptr;
    }
}

int EngineApp::Run() noexcept {
    instance_mutex_ = CreateMutexW(nullptr, FALSE, kInstanceMutexName);
    if (instance_mutex_ == nullptr) {
        return 10;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }
    if (!CreateMessageWindow()) {
        return 11;
    }
    if (!worker_.Start()) {
        return 12;
    }
    if (!hook_.Start()) {
        return 13;
    }
    const auto settings = runtime_.Settings();
    if (!settings->show_tray_icon) {
        tray_added_ = false;
    } else {
        RefreshTrayIcon();
    }
    static_cast<void>(RegisterHotKey(window_, kHotkeyToggle, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'S'));

    if (sync_autostart_) {
        std::string startup_error;
        if (!SetStartWithWindows(settings->start_with_windows, startup_error) && last_error_.empty()) {
            last_error_ = std::move(startup_error);
        }
    }
    if (!pipe_.Start()) {
        return 14;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnregisterHotKey(window_, kHotkeyToggle);
    RemoveTrayIcon();
    pipe_.Stop();
    hook_.Stop();
    worker_.Stop();
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK EngineApp::WindowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM word,
    const LPARAM data) noexcept {
    EngineApp* app = reinterpret_cast<EngineApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(data);
        app = static_cast<EngineApp*>(create->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app == nullptr ? DefWindowProcW(window, message, word, data) : app->OnWindowMessage(message, word, data);
}

LRESULT EngineApp::OnWindowMessage(const UINT message, const WPARAM word, const LPARAM data) noexcept {
    if (message == taskbar_created_message_) {
        tray_added_ = false;
        RefreshTrayIcon();
        return 0;
    }
    switch (message) {
        case kTrayCallbackMessage: {
            const UINT notification = LOWORD(data);
            if (notification == WM_CONTEXTMENU || notification == WM_RBUTTONUP) {
                ShowTrayMenu();
            } else if (notification == NIN_SELECT || notification == WM_LBUTTONUP) {
                OpenSettings();
            }
            return 0;
        }
        case kSettingsChangedMessage:
            RefreshTrayIcon();
            return 0;
        case WM_HOTKEY:
            if (word == kHotkeyToggle) {
                ToggleEnabled();
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(word)) {
                case kCommandToggle: ToggleEnabled(); break;
                case kCommandSettings: OpenSettings(); break;
                case kCommandExit: Shutdown(); break;
                default: break;
            }
            return 0;
        case WM_CLOSE:
            Shutdown();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window_, message, word, data);
    }
}

bool EngineApp::CreateMessageWindow() noexcept {
    HICON application_icon = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(IDI_SMOOTHEVERYTHING),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (application_icon == nullptr) {
        application_icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    HICON small_application_icon = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(IDI_SMOOTHEVERYTHING),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (small_application_icon == nullptr) {
        small_application_icon = application_icon;
    }
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.lpfnWndProc = &EngineApp::WindowProcedure;
    window_class.hInstance = instance_;
    window_class.hIcon = application_icon;
    window_class.hIconSm = small_application_icon;
    window_class.lpszClassName = kWindowClassName;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    window_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"SmoothEverything Engine",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        return false;
    }
    taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
    return true;
}

void EngineApp::DestroyMessageWindow() noexcept {
    if (window_ != nullptr && IsWindow(window_)) {
        DestroyWindow(window_);
    }
    window_ = nullptr;
    UnregisterClassW(kWindowClassName, instance_);
}

void EngineApp::RefreshTrayIcon() noexcept {
    if (window_ == nullptr) {
        return;
    }
    const auto settings = runtime_.Settings();
    if (!settings->show_tray_icon) {
        RemoveTrayIcon();
        return;
    }

    tray_ = {};
    tray_.cbSize = sizeof(NOTIFYICONDATAW);
    tray_.hWnd = window_;
    tray_.uID = 1;
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = kTrayCallbackMessage;
    tray_.hIcon = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(IDI_SMOOTHEVERYTHING),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (tray_.hIcon == nullptr) {
        tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    const std::wstring tooltip = settings->enabled
        ? L"SmoothEverything — enabled"
        : L"SmoothEverything — paused";
    wcsncpy_s(tray_.szTip, std::size(tray_.szTip), tooltip.c_str(), _TRUNCATE);

    if (!tray_added_) {
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
        if (tray_added_) {
            tray_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &tray_);
        }
    } else {
        Shell_NotifyIconW(NIM_MODIFY, &tray_);
    }
}

void EngineApp::RemoveTrayIcon() noexcept {
    if (tray_added_) {
        Shell_NotifyIconW(NIM_DELETE, &tray_);
        tray_added_ = false;
    }
}

void EngineApp::ShowTrayMenu() noexcept {
    const HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    const auto settings = runtime_.Settings();
    AppendMenuW(
        menu,
        MF_STRING | (settings->enabled ? MF_CHECKED : MF_UNCHECKED),
        kCommandToggle,
        L"Enable smooth scrolling");
    AppendMenuW(menu, MF_STRING, kCommandSettings, L"Settings…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCommandExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    const int selected = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        cursor.x,
        cursor.y,
        0,
        window_,
        nullptr);
    const UINT command = selected > 0 ? static_cast<UINT>(selected) : 0U;
    DestroyMenu(menu);
    if (command != 0) {
        PostMessageW(window_, WM_COMMAND, command, 0);
    }
    PostMessageW(window_, WM_NULL, 0, 0);
}

void EngineApp::OpenSettings() noexcept {
    std::array<wchar_t, 32'768> module{};
    const DWORD length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length == 0 || static_cast<std::size_t>(length) >= module.size()) {
        return;
    }
    const std::wstring settings_path = ParentDirectory(std::wstring(module.data(), length))
        + L"\\SmoothEverything.ControlPanel.exe";
    if (GetFileAttributesW(settings_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }
    std::wstring command = L"\"" + settings_path + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(
            nullptr,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &startup,
            &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
}

void EngineApp::Shutdown() noexcept {
    static_cast<void>(RequestControlPanelClose());
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
}

void EngineApp::ToggleEnabled() noexcept {
    AppSettings settings = *runtime_.Settings();
    settings.enabled = !settings.enabled;
    std::string error;
    static_cast<void>(ApplySettings(std::move(settings), error));
}

bool EngineApp::ApplySettings(AppSettings settings, std::string& error) noexcept {
    std::lock_guard lock(settings_write_mutex_);
    if (!store_.Save(settings, error)) {
        last_error_ = error;
        return false;
    }

    const bool start_with_windows = settings.start_with_windows;
    runtime_.UpdateSettings(std::move(settings));
    policies_.Invalidate();
    worker_.NotifySettingsChanged();

    if (sync_autostart_) {
        std::string startup_error;
        if (!SetStartWithWindows(start_with_windows, startup_error)) {
            last_error_ = startup_error;
        } else {
            last_error_.clear();
        }
    }
    PostMessageW(window_, kSettingsChangedMessage, 0, 0);
    error.clear();
    return true;
}

std::string EngineApp::HandlePipeRequest(const std::string_view request) {
    try {
        const JsonValue root = ParseJson(request);
        if (!root.IsObject()) {
            return SerializeJson(ErrorResponse("request must be a JSON object"), false);
        }
        const JsonValue* operation = root.Find("op");
        if (operation == nullptr || !operation->IsString()) {
            return SerializeJson(ErrorResponse("op is required"), false);
        }

        if (operation->AsString() == "ping" || operation->AsString() == "get_state") {
            return StateResponse();
        }
        if (operation->AsString() == "open_settings") {
            PostMessageW(window_, WM_COMMAND, kCommandSettings, 0);
            return SerializeJson(JsonValue::Object{{"ok", true}}, false);
        }
        if (operation->AsString() == "shutdown") {
            PostMessageW(window_, WM_CLOSE, 0, 0);
            return SerializeJson(JsonValue::Object{{"ok", true}}, false);
        }
        if (operation->AsString() == "set_enabled") {
            const JsonValue* enabled = root.Find("enabled");
            if (enabled == nullptr || !enabled->IsBoolean()) {
                return SerializeJson(ErrorResponse("enabled must be a boolean"), false);
            }
            AppSettings settings = *runtime_.Settings();
            settings.enabled = enabled->AsBoolean();
            std::string error;
            if (!ApplySettings(std::move(settings), error)) {
                return SerializeJson(ErrorResponse(error), false);
            }
            return StateResponse();
        }
        if (operation->AsString() == "apply_settings") {
            const JsonValue* settings_value = root.Find("settings");
            if (settings_value == nullptr || !settings_value->IsObject()) {
                return SerializeJson(ErrorResponse("settings must be an object"), false);
            }
            const SettingsParseResult parsed = ParseSettings(SerializeJson(*settings_value, false));
            if (!parsed.value.has_value()) {
                return SerializeJson(ErrorResponse(parsed.error), false);
            }
            std::string error;
            if (!ApplySettings(*parsed.value, error)) {
                return SerializeJson(ErrorResponse(error), false);
            }
            return StateResponse();
        }
        return SerializeJson(ErrorResponse("unknown operation"), false);
    } catch (const JsonParseError& error) {
        return SerializeJson(
            ErrorResponse("JSON parse error at byte " + std::to_string(error.Offset())), false);
    } catch (const std::exception& error) {
        return SerializeJson(ErrorResponse(error.what()), false);
    }
}

std::string EngineApp::StateResponse() const {
    std::lock_guard lock(settings_write_mutex_);
    const RuntimeDiagnostics diagnostics = runtime_.Diagnostics();
    const JsonValue settings = ParseJson(SerializeSettings(*runtime_.Settings(), false));
    const JsonValue response{JsonValue::Object{
        {"diagnostics", JsonValue::Object{
            {"injected_delta", static_cast<double>(diagnostics.injected_delta)},
            {"injected_events", static_cast<double>(diagnostics.injected_events)},
            {"injection_failures", static_cast<double>(diagnostics.injection_failures)},
            {"passed_events", static_cast<double>(diagnostics.passed_events)},
            {"physical_events", static_cast<double>(diagnostics.physical_events)},
            {"queue_overflows", static_cast<double>(diagnostics.queue_overflows)},
            {"settings_generation", static_cast<double>(diagnostics.settings_generation)},
            {"smoothed_events", static_cast<double>(diagnostics.smoothed_events)},
            {"target_changes", static_cast<double>(diagnostics.target_changes)},
        }},
        {"error", last_error_},
        {"ok", true},
        {"settings", settings},
    }};
    return SerializeJson(response, false);
}

}  // namespace smootheverything::engine
