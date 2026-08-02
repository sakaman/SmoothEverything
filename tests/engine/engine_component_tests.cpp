#include "smootheverything/engine/control_panel_lifecycle.h"
#include "smootheverything/engine/motion_worker.h"
#include "smootheverything/engine/pipe_server.h"
#include "smootheverything/engine/runtime_state.h"
#include "smootheverything/engine/settings_store.h"
#include "smootheverything/engine/target_policy.h"
#include "smootheverything/settings.h"

#include <windows.h>

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using smootheverything::DefaultSettings;
using smootheverything::engine::RequestControlPanelClose;
using smootheverything::engine::MotionWorker;
using smootheverything::engine::PipeServer;
using smootheverything::engine::RuntimeState;
using smootheverything::engine::SettingsStore;
using smootheverything::engine::TargetPolicyCache;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

int control_panel_close_count = 0;

LRESULT CALLBACK ControlPanelWindowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM word,
    const LPARAM data) {
    if (message == WM_CLOSE) {
        ++control_panel_close_count;
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, word, data);
}

[[nodiscard]] std::wstring TestDirectory() {
    std::array<wchar_t, MAX_PATH> temporary{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(temporary.size()), temporary.data());
    if (length == 0 || static_cast<std::size_t>(length) >= temporary.size()) {
        throw std::runtime_error("GetTempPathW failed");
    }
    return std::wstring(temporary.data(), length)
        + L"SmoothEverything.Tests."
        + std::to_wstring(GetCurrentProcessId())
        + L"."
        + std::to_wstring(GetTickCount64());
}

void SettingsStoreWritesAtomicallyAndRejectsCorruption() {
    const std::wstring directory = TestDirectory();
    Require(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "create test directory");
    const std::wstring path = directory + L"\\settings.json";
    SettingsStore store(path);

    auto settings = DefaultSettings();
    settings.motion.animation_time_ms = 222.0;
    settings.excluded_apps = {"game.exe"};
    std::string error;
    Require(store.Save(settings, error), "save settings: " + error);

    const auto loaded = store.Load();
    Require(loaded.found && loaded.valid, "saved settings must load");
    Require(loaded.settings.motion.animation_time_ms == 222.0, "saved duration must round trip");
    Require(loaded.settings.excluded_apps.size() == 1, "saved exclusions must round trip");
    Require(GetFileAttributesW((path + L".tmp").c_str()) == INVALID_FILE_ATTRIBUTES, "temporary file must be replaced");

    const HANDLE file = CreateFileW(
        path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    Require(file != INVALID_HANDLE_VALUE, "open settings for corruption case");
    constexpr char invalid[] = "{broken";
    DWORD written = 0;
    const BOOL write_ok = WriteFile(file, invalid, static_cast<DWORD>(sizeof(invalid) - 1U), &written, nullptr);
    CloseHandle(file);
    Require(write_ok != FALSE && written == sizeof(invalid) - 1U, "write corrupt settings");
    const auto corrupt = store.Load();
    Require(corrupt.found && !corrupt.valid && !corrupt.error.empty(), "corrupt settings must fail closed");

    DeleteFileW(path.c_str());
    RemoveDirectoryW(directory.c_str());
}

void PipeServerAcceptsOnlyBoundedLineRequests() {
    const std::wstring pipe_name = L"\\\\.\\pipe\\SmoothEverything.Tests."
        + std::to_wstring(GetCurrentProcessId());
    PipeServer server(
        [](const std::string_view request) { return std::string("echo:") + std::string(request); },
        pipe_name);
    Require(server.Start(), "pipe server must start");

    HANDLE pipe = INVALID_HANDLE_VALUE;
    const std::uint64_t deadline = GetTickCount64() + 3000U;
    while (GetTickCount64() < deadline) {
        pipe = CreateFileW(
            pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) {
            break;
        }
        Sleep(10);
    }
    Require(pipe != INVALID_HANDLE_VALUE, "same-user pipe client must connect");

    constexpr char request[] = "hello\n";
    DWORD transferred = 0;
    Require(
        WriteFile(pipe, request, static_cast<DWORD>(sizeof(request) - 1U), &transferred, nullptr) != FALSE,
        "pipe request write");
    std::array<char, 128> response{};
    Require(
        ReadFile(pipe, response.data(), static_cast<DWORD>(response.size()), &transferred, nullptr) != FALSE,
        "pipe response read");
    CloseHandle(pipe);
    server.Stop();

    Require(std::string(response.data(), transferred) == "echo:hello\n", "pipe request/response framing");
    Require(!server.Running(), "pipe server must stop cleanly");
}

void TargetPolicyRefreshesOnSettingsGeneration() {
    RuntimeState runtime(DefaultSettings());
    TargetPolicyCache cache(runtime);
    const DWORD process_id = GetCurrentProcessId();
    const auto first = cache.Resolve(process_id, GetTickCount64());
    Require(!first.executable.empty(), "current executable must resolve");
    Require(first.policy.smooth_enabled, "default current-process policy must smooth");

    auto settings = *runtime.Settings();
    settings.excluded_apps = {first.executable};
    runtime.UpdateSettings(std::move(settings));
    cache.Invalidate();
    const auto second = cache.Resolve(process_id, GetTickCount64());
    Require(!second.policy.smooth_enabled, "new settings generation must invalidate cached policy");
}

void MotionWorkerStartsAndStopsWithoutInput() {
    RuntimeState runtime(DefaultSettings());
    TargetPolicyCache cache(runtime);
    MotionWorker worker(runtime, cache);
    Require(worker.Start(), "motion worker must start");
    worker.NotifySettingsChanged();
    worker.Stop();
}

void EngineShutdownClosesEveryControlPanelWindow() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const std::wstring class_name = L"SmoothEverything.Tests.ControlPanel." +
        std::to_wstring(GetCurrentProcessId());

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.lpfnWndProc = &ControlPanelWindowProcedure;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name.c_str();
    Require(RegisterClassExW(&window_class) != 0, "register test control panel class");

    const HWND first = CreateWindowExW(
        0, class_name.c_str(), L"", WS_OVERLAPPED, 0, 0, 1, 1,
        nullptr, nullptr, instance, nullptr);
    const HWND second = CreateWindowExW(
        0, class_name.c_str(), L"", WS_OVERLAPPED, 0, 0, 1, 1,
        nullptr, nullptr, instance, nullptr);
    Require(first != nullptr && second != nullptr, "create test control panel windows");

    control_panel_close_count = 0;
    Require(RequestControlPanelClose(class_name.c_str()) == 2, "request every panel close");

    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    Require(control_panel_close_count == 2, "every panel receives WM_CLOSE");
    Require(IsWindow(first) == FALSE && IsWindow(second) == FALSE, "every panel window closes");
    Require(UnregisterClassW(class_name.c_str(), instance) != FALSE, "unregister test panel class");
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests{
        {"atomic settings store", SettingsStoreWritesAtomicallyAndRejectsCorruption},
        {"same-user named pipe", PipeServerAcceptsOnlyBoundedLineRequests},
        {"target policy generation", TargetPolicyRefreshesOnSettingsGeneration},
        {"motion worker lifecycle", MotionWorkerStartsAndStopsWithoutInput},
        {"control panel shutdown", EngineShutdownClosesEveryControlPanelWindow},
    };

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
        std::cerr << failures << " engine test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << tests.size() << " engine test(s) passed\n";
    return EXIT_SUCCESS;
}
