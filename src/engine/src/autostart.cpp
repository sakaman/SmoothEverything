#include "smootheverything/engine/autostart.h"

#include <windows.h>

#include <array>
#include <string>

namespace smootheverything::engine {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"SmoothEverything";

[[nodiscard]] std::string ErrorCode(const LSTATUS status) {
    return "registry operation failed with code " + std::to_string(status);
}

}  // namespace

bool SetStartWithWindows(const bool enabled, std::string& error) noexcept {
    HKEY key = nullptr;
    const LSTATUS opened = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKey,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);
    if (opened != ERROR_SUCCESS) {
        error = ErrorCode(opened);
        return false;
    }

    LSTATUS result = ERROR_SUCCESS;
    if (enabled) {
        std::array<wchar_t, 32'768> path{};
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0 || static_cast<std::size_t>(length) >= path.size()) {
            RegCloseKey(key);
            error = "cannot resolve engine executable path";
            return false;
        }
        const std::wstring command = L"\"" + std::wstring(path.data(), length) + L"\" --background";
        result = RegSetValueExW(
            key,
            kValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1U) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);

    if (result != ERROR_SUCCESS) {
        error = ErrorCode(result);
        return false;
    }
    error.clear();
    return true;
}

}  // namespace smootheverything::engine
