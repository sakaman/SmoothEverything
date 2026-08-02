#include "smootheverything/control_panel/single_instance.h"

namespace smootheverything::control_panel {

SingleInstance::SingleInstance(const wchar_t* const mutex_name) noexcept {
    mutex_ = CreateMutexW(nullptr, FALSE, mutex_name);
    already_running_ = mutex_ != nullptr && GetLastError() == ERROR_ALREADY_EXISTS;
}

SingleInstance::~SingleInstance() {
    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
    }
}

bool SingleInstance::Valid() const noexcept {
    return mutex_ != nullptr;
}

bool SingleInstance::AlreadyRunning() const noexcept {
    return already_running_;
}

bool ActivateExistingControlPanelWindow() noexcept {
    const HWND window = FindWindowW(kControlPanelWindowClassName, nullptr);
    if (window == nullptr) {
        return false;
    }
    ShowWindowAsync(window, IsIconic(window) ? SW_RESTORE : SW_SHOW);
    static_cast<void>(SetForegroundWindow(window));
    return true;
}

}  // namespace smootheverything::control_panel
