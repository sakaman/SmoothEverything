#pragma once

#include <string>

namespace smootheverything::engine {

[[nodiscard]] bool SetStartWithWindows(bool enabled, std::string& error) noexcept;

}  // namespace smootheverything::engine
