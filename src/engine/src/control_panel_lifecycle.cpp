#include "smootheverything/engine/control_panel_lifecycle.h"

#include <windows.h>

#include <array>

namespace smootheverything::engine {
namespace {

struct CloseContext final {
    const wchar_t* window_class_name{};
    std::size_t requested{};
};

BOOL CALLBACK RequestWindowClose(const HWND window, const LPARAM data) noexcept {
    auto& context = *reinterpret_cast<CloseContext*>(data);
    std::array<wchar_t, 256> class_name{};
    if (GetClassNameW(window, class_name.data(), static_cast<int>(class_name.size())) == 0 ||
        lstrcmpW(class_name.data(), context.window_class_name) != 0) {
        return TRUE;
    }
    if (PostMessageW(window, WM_CLOSE, 0, 0) != FALSE) {
        ++context.requested;
    }
    return TRUE;
}

}  // namespace

std::size_t RequestControlPanelClose(const wchar_t* const window_class_name) noexcept {
    if (window_class_name == nullptr || window_class_name[0] == L'\0') {
        return 0;
    }
    CloseContext context{.window_class_name = window_class_name};
    EnumWindows(&RequestWindowClose, reinterpret_cast<LPARAM>(&context));
    return context.requested;
}

}  // namespace smootheverything::engine
