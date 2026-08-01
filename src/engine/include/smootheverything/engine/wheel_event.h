#pragma once

#include "smootheverything/motion_engine.h"

#include <windows.h>

#include <cstdint>

namespace smootheverything::engine {

inline constexpr ULONG_PTR kInjectedInputMarker = static_cast<ULONG_PTR>(0x53454D4FU);  // "SEMO"

enum class WheelEventKind : std::uint8_t {
    Smooth,
    ResolvePolicy,
};

struct WheelEvent final {
    WheelEventKind kind{WheelEventKind::ResolvePolicy};
    ScrollAxis axis{ScrollAxis::Vertical};
    int delta{0};
    std::uint64_t timestamp_ms{0};
    HWND target_window{nullptr};
    DWORD process_id{0};
};

}  // namespace smootheverything::engine
