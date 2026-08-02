#pragma once

#include "smootheverything/app_contract.h"

#include <cstddef>

namespace smootheverything::engine {

[[nodiscard]] std::size_t RequestControlPanelClose(
    const wchar_t* window_class_name = kControlPanelWindowClassName) noexcept;

}  // namespace smootheverything::engine
