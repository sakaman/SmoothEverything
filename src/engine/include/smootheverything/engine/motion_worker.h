#pragma once

#include "smootheverything/engine/event_queue.h"
#include "smootheverything/engine/runtime_state.h"
#include "smootheverything/engine/target_policy.h"
#include "smootheverything/engine/wheel_event.h"
#include "smootheverything/motion_engine.h"

#include <windows.h>

#include <atomic>
#include <cstdint>

namespace smootheverything::engine {

class MotionWorker final {
public:
    MotionWorker(RuntimeState& runtime, TargetPolicyCache& policies);
    ~MotionWorker();

    MotionWorker(const MotionWorker&) = delete;
    MotionWorker& operator=(const MotionWorker&) = delete;

    [[nodiscard]] bool Start() noexcept;
    void Stop() noexcept;
    [[nodiscard]] bool TryEnqueue(const WheelEvent& event) noexcept;
    void NotifySettingsChanged() noexcept;

private:
    [[nodiscard]] static DWORD WINAPI ThreadEntry(void* context) noexcept;
    [[nodiscard]] DWORD Run() noexcept;
    void DrainEvents(std::uint64_t now_ms) noexcept;
    void ProcessEvent(const WheelEvent& event, std::uint64_t now_ms) noexcept;
    void Tick(std::uint64_t now_ms) noexcept;
    void ScheduleNextTick() noexcept;
    void ClearGesture() noexcept;
    [[nodiscard]] bool TargetStillValid() const noexcept;
    [[nodiscard]] bool Inject(const MotionFrame& frame) noexcept;

    RuntimeState& runtime_;
    TargetPolicyCache& policies_;
    SpscQueue<WheelEvent, 1024> queue_;
    MotionEngine motion_;
    HANDLE stop_event_{nullptr};
    HANDLE wake_event_{nullptr};
    HANDLE timer_{nullptr};
    HANDLE thread_{nullptr};
    HWND active_target_{nullptr};
    DWORD active_process_id_{0};
    std::uint64_t observed_settings_generation_{0};
    std::atomic<bool> running_{false};
};

}  // namespace smootheverything::engine
