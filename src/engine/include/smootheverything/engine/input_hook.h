#pragma once

#include "smootheverything/engine/motion_worker.h"
#include "smootheverything/engine/runtime_state.h"
#include "smootheverything/engine/target_policy.h"

#include <windows.h>

#include <atomic>

namespace smootheverything::engine {

class InputHook final {
public:
    InputHook(RuntimeState& runtime, TargetPolicyCache& policies, MotionWorker& worker);
    ~InputHook();

    InputHook(const InputHook&) = delete;
    InputHook& operator=(const InputHook&) = delete;

    [[nodiscard]] bool Start() noexcept;
    void Stop() noexcept;

private:
    [[nodiscard]] static DWORD WINAPI ThreadEntry(void* context) noexcept;
    [[nodiscard]] DWORD Run() noexcept;
    [[nodiscard]] static LRESULT CALLBACK HookProcedure(int code, WPARAM message, LPARAM data) noexcept;
    [[nodiscard]] LRESULT OnMouse(int code, WPARAM message, LPARAM data) noexcept;
    [[nodiscard]] LRESULT Pass(int code, WPARAM message, LPARAM data) noexcept;

    static std::atomic<InputHook*> active_instance_;

    RuntimeState& runtime_;
    TargetPolicyCache& policies_;
    MotionWorker& worker_;
    HANDLE ready_event_{nullptr};
    HANDLE thread_{nullptr};
    DWORD thread_id_{0};
    HHOOK hook_{nullptr};
    std::atomic<bool> start_succeeded_{false};
};

}  // namespace smootheverything::engine
