#pragma once

#include "smootheverything/app_contract.h"

#include <windows.h>

namespace smootheverything::control_panel {

class SingleInstance final {
public:
    explicit SingleInstance(
        const wchar_t* mutex_name = kControlPanelInstanceMutexName) noexcept;
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] bool AlreadyRunning() const noexcept;

private:
    HANDLE mutex_{};
    bool already_running_{};
};

[[nodiscard]] bool ActivateExistingControlPanelWindow() noexcept;

}  // namespace smootheverything::control_panel
