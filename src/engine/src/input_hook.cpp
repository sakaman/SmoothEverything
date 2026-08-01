#include "smootheverything/engine/input_hook.h"

#include "smootheverything/engine/wheel_event.h"

#include <windows.h>

#include <cmath>
#include <cstdint>

namespace smootheverything::engine {

std::atomic<InputHook*> InputHook::active_instance_{nullptr};

InputHook::InputHook(RuntimeState& runtime, TargetPolicyCache& policies, MotionWorker& worker)
    : runtime_(runtime), policies_(policies), worker_(worker) {}

InputHook::~InputHook() {
    Stop();
}

bool InputHook::Start() noexcept {
    if (thread_ != nullptr) {
        return start_succeeded_.load(std::memory_order_acquire);
    }

    InputHook* expected = nullptr;
    if (!active_instance_.compare_exchange_strong(expected, this, std::memory_order_acq_rel)) {
        return false;
    }
    ready_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ready_event_ == nullptr) {
        active_instance_.store(nullptr, std::memory_order_release);
        return false;
    }
    thread_ = CreateThread(nullptr, 0, &InputHook::ThreadEntry, this, 0, &thread_id_);
    if (thread_ == nullptr) {
        CloseHandle(ready_event_);
        ready_event_ = nullptr;
        active_instance_.store(nullptr, std::memory_order_release);
        return false;
    }

    if (WaitForSingleObject(ready_event_, 5000) != WAIT_OBJECT_0
        || !start_succeeded_.load(std::memory_order_acquire)) {
        Stop();
        return false;
    }
    return true;
}

void InputHook::Stop() noexcept {
    if (thread_id_ != 0) {
        PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
    }
    if (thread_ != nullptr) {
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (ready_event_ != nullptr) {
        CloseHandle(ready_event_);
        ready_event_ = nullptr;
    }
    thread_id_ = 0;
    start_succeeded_.store(false, std::memory_order_release);
    InputHook* expected = this;
    static_cast<void>(active_instance_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel));
}

DWORD WINAPI InputHook::ThreadEntry(void* context) noexcept {
    return static_cast<InputHook*>(context)->Run();
}

DWORD InputHook::Run() noexcept {
    MSG bootstrap{};
    static_cast<void>(PeekMessageW(&bootstrap, nullptr, WM_USER, WM_USER, PM_NOREMOVE));
    hook_ = SetWindowsHookExW(WH_MOUSE_LL, &InputHook::HookProcedure, GetModuleHandleW(nullptr), 0);
    start_succeeded_.store(hook_ != nullptr, std::memory_order_release);
    SetEvent(ready_event_);
    if (hook_ == nullptr) {
        return GetLastError();
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
    return 0;
}

LRESULT CALLBACK InputHook::HookProcedure(const int code, const WPARAM message, const LPARAM data) noexcept {
    InputHook* instance = active_instance_.load(std::memory_order_acquire);
    return instance == nullptr
        ? CallNextHookEx(nullptr, code, message, data)
        : instance->OnMouse(code, message, data);
}

LRESULT InputHook::OnMouse(const int code, const WPARAM message, const LPARAM data) noexcept {
    if (code != HC_ACTION || (message != WM_MOUSEWHEEL && message != WM_MOUSEHWHEEL)) {
        return Pass(code, message, data);
    }

    const auto* input = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
    if (input == nullptr
        || (input->flags & LLMHF_INJECTED) != 0
        || input->dwExtraInfo == kInjectedInputMarker) {
        return Pass(code, message, data);
    }

    runtime_.counters.physical_events.fetch_add(1, std::memory_order_relaxed);
    const auto settings = runtime_.Settings();
    if (!settings->enabled) {
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }

    const int delta = static_cast<int>(static_cast<short>(HIWORD(input->mouseData)));
    if (delta == 0
        || (settings->bypass_high_resolution && std::abs(delta) < WHEEL_DELTA)
        || (settings->pass_through_ctrl && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
        || (settings->pass_through_alt && (GetAsyncKeyState(VK_MENU) & 0x8000) != 0)) {
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }

    ScrollAxis axis = message == WM_MOUSEHWHEEL ? ScrollAxis::Horizontal : ScrollAxis::Vertical;
    if (axis == ScrollAxis::Vertical
        && settings->shift_for_horizontal
        && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
        axis = ScrollAxis::Horizontal;
    }
    if (axis == ScrollAxis::Horizontal && !settings->horizontal_smoothing) {
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }

    const HWND pointed = WindowFromPoint(input->pt);
    const HWND target = pointed == nullptr ? nullptr : GetAncestor(pointed, GA_ROOT);
    DWORD process_id = 0;
    if (target == nullptr || GetWindowThreadProcessId(target, &process_id) == 0 || process_id == 0) {
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }

    const std::uint64_t now_ms = GetTickCount64();
    ResolvedTarget resolved;
    if (!policies_.TryGet(process_id, now_ms, resolved)) {
        const WheelEvent probe{
            .kind = WheelEventKind::ResolvePolicy,
            .axis = axis,
            .delta = 0,
            .timestamp_ms = now_ms,
            .target_window = target,
            .process_id = process_id,
        };
        if (!worker_.TryEnqueue(probe)) {
            runtime_.counters.queue_overflows.fetch_add(1, std::memory_order_relaxed);
        }
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }
    if (!resolved.policy.smooth_enabled) {
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }

    const WheelEvent event{
        .kind = WheelEventKind::Smooth,
        .axis = axis,
        .delta = delta,
        .timestamp_ms = now_ms,
        .target_window = target,
        .process_id = process_id,
    };
    if (!worker_.TryEnqueue(event)) {
        runtime_.counters.queue_overflows.fetch_add(1, std::memory_order_relaxed);
        runtime_.counters.passed_events.fetch_add(1, std::memory_order_relaxed);
        return Pass(code, message, data);
    }

    runtime_.counters.smoothed_events.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

LRESULT InputHook::Pass(const int code, const WPARAM message, const LPARAM data) noexcept {
    return CallNextHookEx(hook_, code, message, data);
}

}  // namespace smootheverything::engine
