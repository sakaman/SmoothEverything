#include "smootheverything/engine/settings_store.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <cstddef>
#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace smootheverything::engine {
namespace {

class UniqueHandle final {
public:
    explicit UniqueHandle(const HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    [[nodiscard]] HANDLE Get() const noexcept { return handle_; }
    [[nodiscard]] bool Valid() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }
    void Close() noexcept {
        if (Valid()) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE handle_;
};

[[nodiscard]] std::string WindowsError(const DWORD code) {
    wchar_t* message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        0,
        reinterpret_cast<wchar_t*>(&message),
        0,
        nullptr);
    if (length == 0 || message == nullptr) {
        return "Windows error " + std::to_string(code);
    }

    const int utf8_size = WideCharToMultiByte(
        CP_UTF8, 0, message, static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(std::max(0, utf8_size)), '\0');
    if (utf8_size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(length), result.data(), utf8_size, nullptr, nullptr);
    }
    LocalFree(message);
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

[[nodiscard]] std::wstring ParentDirectory(const std::wstring& path) {
    const std::size_t separator = path.find_last_of(L"/\\");
    return separator == std::wstring::npos ? std::wstring{} : path.substr(0, separator);
}

}  // namespace

SettingsStore::SettingsStore(std::wstring path) : path_(std::move(path)) {}

std::wstring SettingsStore::DefaultPath() {
    PWSTR local_app_data = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local_app_data);
    if (FAILED(result) || local_app_data == nullptr) {
        return L"settings.json";
    }
    std::wstring path(local_app_data);
    CoTaskMemFree(local_app_data);
    path += L"\\SmoothEverything\\settings.json";
    return path;
}

const std::wstring& SettingsStore::Path() const noexcept {
    return path_;
}

SettingsLoadResult SettingsStore::Load() const noexcept {
    SettingsLoadResult result{
        .settings = DefaultSettings(),
        .found = false,
        .valid = true,
        .error = {},
    };
    const UniqueHandle file(CreateFileW(
        path_.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (!file.Valid()) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return result;
        }
        result.valid = false;
        result.error = "cannot open settings: " + WindowsError(error);
        return result;
    }

    result.found = true;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.Get(), &size) || size.QuadPart < 0 || size.QuadPart > 1024 * 1024) {
        result.valid = false;
        result.error = "settings file is unreadable or exceeds 1 MiB";
        return result;
    }

    std::string json(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t offset = 0;
    while (offset < json.size()) {
        const std::size_t remaining = json.size() - offset;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
        DWORD read = 0;
        if (!ReadFile(file.Get(), json.data() + offset, request, &read, nullptr)) {
            result.valid = false;
            result.error = "cannot read settings: " + WindowsError(GetLastError());
            return result;
        }
        if (read == 0) {
            break;
        }
        offset += read;
    }
    json.resize(offset);

    const SettingsParseResult parsed = ParseSettings(json);
    if (!parsed.value.has_value()) {
        result.valid = false;
        result.error = parsed.error;
        return result;
    }
    result.settings = *parsed.value;
    return result;
}

bool SettingsStore::Save(const AppSettings& settings, std::string& error) const noexcept {
    const std::wstring directory = ParentDirectory(path_);
    if (!directory.empty()) {
        const int create_result = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
        if (create_result != ERROR_SUCCESS && create_result != ERROR_ALREADY_EXISTS && create_result != ERROR_FILE_EXISTS) {
            error = "cannot create settings directory: " + WindowsError(static_cast<DWORD>(create_result));
            return false;
        }
    }

    const std::string json = SerializeSettings(settings, true);
    const std::wstring temporary = path_ + L".tmp";
    UniqueHandle file(CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr));
    if (!file.Valid()) {
        error = "cannot create temporary settings: " + WindowsError(GetLastError());
        return false;
    }

    std::size_t offset = 0;
    while (offset < json.size()) {
        const std::size_t remaining = json.size() - offset;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
        DWORD written = 0;
        if (!WriteFile(file.Get(), json.data() + offset, request, &written, nullptr) || written == 0) {
            error = "cannot write settings: " + WindowsError(GetLastError());
            return false;
        }
        offset += written;
    }
    if (!FlushFileBuffers(file.Get())) {
        error = "cannot flush settings: " + WindowsError(GetLastError());
        return false;
    }
    file.Close();

    if (!MoveFileExW(
            temporary.c_str(),
            path_.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "cannot replace settings: " + WindowsError(GetLastError());
        return false;
    }
    error.clear();
    return true;
}

}  // namespace smootheverything::engine
