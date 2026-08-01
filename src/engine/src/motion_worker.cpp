#include "smootheverything/engine/motion_worker.h"

#include <windows.h>

#include <array>
#include <cstdlib>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace smootheverything::engine {
namespace {

constexpr LONGLONG kTickInterval100Nanoseconds = 80'000;  // 8 ms / 125 Hz.

[[nodiscard]] HWND RootWindowAtCursor() noexcept {
    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return nullptr;
    }
    const HWND window = WindowFromPoint(cursor);
    return window == nullptr ? nullptr : GetAncestor(window, GA_ROOT);
}

}  // namespace

MotionWorker::MotionWorker(RuntimeState& runtime, TargetPolicyCache& policies)
    : runtime_(runtime), policies_(policies), motion_(runtime.Settings()->motion) {}

MotionWorker::~MotionWorker() {
    Stop();
}

bool MotionWorker::Start() noexcept {
    if (running_.load(std::memory_order_acquire)) {
        return true;
    }

    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wake_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    timer_ = CreateWaitableTimerExW(
        nullptr,
        nullptr,
        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
        TIMER_ALL_ACCESS);
    if (timer_ == nullptr) {
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }
    if (stop_event_ == nullptr || wake_event_ == nullptr || timer_ == nullptr) {
        Stop();
        return false;
    }

    ResetEvent(stop_event_);
    observed_settings_generation_ = runtime_.SettingsGeneration();
    thread_ = CreateThread(nullptr, 0, &MotionWorker::ThreadEntry, this, 0, nullptr);
    if (thread_ == nullptr) {
        Stop();
        return false;
    }
    running_.store(true, std::memory_order_release);
    return true;
}

void MotionWorker::Stop() noexcept {
    if (stop_event_ != nullptr) {
        SetEvent(stop_event_);
    }
    if (thread_ != nullptr) {
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (timer_ != nullptr) {
        CancelWaitableTimer(timer_);
        CloseHandle(timer_);
        timer_ = nullptr;
    }
    if (wake_event_ != nullptr) {
        CloseHandle(wake_event_);
        wake_event_ = nullptr;
    }
    if (stop_event_ != nullptr) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }
    running_.store(false, std::memory_order_release);
}

bool MotionWorker::TryEnqueue(const WheelEvent& event) noexcept {
    if (!running_.load(std::memory_order_acquire) || !queue_.TryPush(event)) {
        return false;
    }
    SetEvent(wake_event_);
    return true;
}

void MotionWorker::NotifySettingsChanged() noexcept {
    if (wake_event_ != nullptr) {
        SetEvent(wake_event_);
    }
}

DWORD WINAPI MotionWorker::ThreadEntry(void* context) noexcept {
    return static_cast<MotionWorker*>(context)->Run();
}

DWORD MotionWorker::Run() noexcept {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    const std::array<HANDLE, 3> handles{stop_event_, wake_event_, timer_};
    for (;;) {
        const DWORD wait = WaitForMultipleObjects(
            static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        if (wait == WAIT_FAILED) {
            break;
        }

        const std::uint64_t now_ms = GetTickCount64();
        if (runtime_.SettingsGeneration() != observed_settings_generation_) {
            observed_settings_generation_ = runtime_.SettingsGeneration();
            policies_.Invalidate();
            ClearGesture();
            motion_.SetSettings(runtime_.Settings()->motion);
        }

        DrainEvents(now_ms);
        Tick(now_ms);
        if (motion_.Active()) {
            ScheduleNextTick();
        }
    }
    ClearGesture();
    return 0;
}

void MotionWorker::DrainEvents(const std::uint64_t now_ms) noexcept {
    WheelEvent event;
    while (queue_.TryPop(event)) {
        ProcessEvent(event, now_ms);
    }
}

void MotionWorker::ProcessEvent(const WheelEvent& event, const std::uint64_t now_ms) noexcept {
    if (event.kind == WheelEventKind::ResolvePolicy) {
        static_cast<void>(policies_.Resolve(event.process_id, now_ms));
        return;
    }

    ResolvedTarget target;
    if (!policies_.TryGet(event.process_id, now_ms, target) || !target.policy.smooth_enabled) {
        return;
    }

    if (active_target_ != nullptr
        && (active_target_ != event.target_window || active_process_id_ != event.process_id)) {
        ClearGesture();
        runtime_.counters.target_changes.fetch_add(1, std::memory_order_relaxed);
    }
    active_target_ = event.target_window;
    active_process_id_ = event.process_id;
    motion_.SetSettings(target.policy.motion);

    int delta = event.delta;
    if (runtime_.Settings()->reverse_direction) {
        delta = -delta;
    }
    motion_.Push(event.axis, delta, static_cast<double>(event.timestamp_ms));
}

void MotionWorker::Tick(const std::uint64_t now_ms) noexcept {
    if (!motion_.Active()) {
        active_target_ = nullptr;
        active_process_id_ = 0;
        return;
    }
    if (!TargetStillValid()) {
        ClearGesture();
        runtime_.counters.target_changes.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const MotionFrame frame = motion_.Sample(static_cast<double>(now_ms));
    if ((frame.vertical_delta != 0 || frame.horizontal_delta != 0) && !Inject(frame)) {
        runtime_.counters.injection_failures.fetch_add(1, std::memory_order_relaxed);
        policies_.MarkPassThrough(active_process_id_, now_ms);
        ClearGesture();
    }
    if (!motion_.Active()) {
        active_target_ = nullptr;
        active_process_id_ = 0;
    }
}

void MotionWorker::ScheduleNextTick() noexcept {
    LARGE_INTEGER due_time{};
    due_time.QuadPart = -kTickInterval100Nanoseconds;
    static_cast<void>(SetWaitableTimer(timer_, &due_time, 0, nullptr, nullptr, FALSE));
}

void MotionWorker::ClearGesture() noexcept {
    motion_.Reset();
    active_target_ = nullptr;
    active_process_id_ = 0;
    if (timer_ != nullptr) {
        CancelWaitableTimer(timer_);
    }
}

bool MotionWorker::TargetStillValid() const noexcept {
    return active_target_ != nullptr && IsWindow(active_target_) && RootWindowAtCursor() == active_target_;
}

bool MotionWorker::Inject(const MotionFrame& frame) noexcept {
    std::array<INPUT, 2> inputs{};
    UINT count = 0;
    const auto append = [&inputs, &count](const int delta, const DWORD flag) {
        INPUT& input = inputs[count++];
        input.type = INPUT_MOUSE;
        input.mi.mouseData = static_cast<DWORD>(delta);
        input.mi.dwFlags = flag;
        input.mi.dwExtraInfo = kInjectedInputMarker;
    };
    if (frame.vertical_delta != 0) {
        append(frame.vertical_delta, MOUSEEVENTF_WHEEL);
    }
    if (frame.horizontal_delta != 0) {
        append(frame.horizontal_delta, MOUSEEVENTF_HWHEEL);
    }
    if (count == 0) {
        return true;
    }

    const UINT injected = SendInput(count, inputs.data(), static_cast<int>(sizeof(INPUT)));
    if (injected != count) {
        return false;
    }
    runtime_.counters.injected_events.fetch_add(count, std::memory_order_relaxed);
    const std::uint64_t absolute_delta = static_cast<std::uint64_t>(
        std::llabs(static_cast<long long>(frame.vertical_delta))
        + std::llabs(static_cast<long long>(frame.horizontal_delta)));
    runtime_.counters.injected_delta.fetch_add(absolute_delta, std::memory_order_relaxed);
    return true;
}

}  // namespace smootheverything::engine
