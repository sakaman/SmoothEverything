#include "smootheverything/control_panel/settings_client.h"

#include "smootheverything/json.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <system_error>
#include <utility>
#include <vector>

namespace smootheverything::control_panel {
namespace {

constexpr wchar_t kPipePath[] = LR"(\\.\pipe\SmoothEverything.Engine.v1)";
constexpr std::size_t kMaximumMessageBytes = 1024U * 1024U;

class UniqueHandle final {
public:
    explicit UniqueHandle(const HANDLE value = INVALID_HANDLE_VALUE) noexcept : value_(value) {}
    ~UniqueHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    [[nodiscard]] HANDLE Get() const noexcept { return value_; }
    [[nodiscard]] bool Valid() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE value_;
};

[[nodiscard]] bool TransferOverlapped(
    const HANDLE pipe,
    const bool write,
    void* buffer,
    const DWORD bytes,
    DWORD& transferred,
    const DWORD timeout_ms) noexcept {
    const UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.Valid()) {
        return false;
    }
    OVERLAPPED operation{};
    operation.hEvent = event.Get();
    const BOOL immediate = write
        ? WriteFile(pipe, buffer, bytes, &transferred, &operation)
        : ReadFile(pipe, buffer, bytes, &transferred, &operation);
    if (immediate) {
        return true;
    }
    if (GetLastError() != ERROR_IO_PENDING) {
        return false;
    }
    const DWORD wait = WaitForSingleObject(event.Get(), timeout_ms);
    if (wait != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &operation);
        static_cast<void>(GetOverlappedResult(pipe, &operation, &transferred, TRUE));
        SetLastError(wait == WAIT_TIMEOUT ? ERROR_SEM_TIMEOUT : ERROR_OPERATION_ABORTED);
        return false;
    }
    return GetOverlappedResult(pipe, &operation, &transferred, FALSE) != FALSE;
}

[[nodiscard]] std::int64_t ReadCounter(
    const JsonValue& object,
    const std::string_view key) noexcept {
    const JsonValue* value = object.Find(key);
    if (value == nullptr || !value->IsNumber() || !std::isfinite(value->AsNumber())) {
        return 0;
    }
    const double number = value->AsNumber();
    if (number <= static_cast<double>(std::numeric_limits<std::int64_t>::min())) {
        return std::numeric_limits<std::int64_t>::min();
    }
    if (number >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return static_cast<std::int64_t>(number);
}

[[nodiscard]] std::wstring Win32Message(const DWORD error) {
    wchar_t* raw = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&raw),
        0,
        nullptr);
    if (length == 0 || raw == nullptr) {
        return L"错误代码 " + std::to_wstring(error);
    }
    std::wstring message(raw, length);
    LocalFree(raw);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

[[nodiscard]] bool WriteAll(const HANDLE file, const std::string_view bytes) noexcept {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(remaining, std::numeric_limits<DWORD>::max()));
        DWORD written = 0;
        if (!WriteFile(file, bytes.data() + offset, chunk, &written, nullptr) || written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

}  // namespace

std::wstring Utf8ToWide(const std::string_view value) {
    if (value.empty()) {
        return {};
    }
    const int source_length = static_cast<int>(std::min<std::size_t>(
        value.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), source_length, nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            source_length,
            result.data(),
            length) != length) {
        return {};
    }
    return result;
}

std::string WideToUtf8(const std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int source_length = static_cast<int>(std::min<std::size_t>(
        value.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), source_length, nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            source_length,
            result.data(),
            length,
            nullptr,
            nullptr) != length) {
        return {};
    }
    return result;
}

SettingsClient::SettingsClient() {
    PWSTR local_app_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local_app_data)) &&
        local_app_data != nullptr) {
        settings_path_ = std::filesystem::path(local_app_data) /
            L"SmoothEverything" / L"settings.json";
        CoTaskMemFree(local_app_data);
    } else {
        std::array<wchar_t, MAX_PATH> temporary{};
        const DWORD length = GetTempPathW(static_cast<DWORD>(temporary.size()), temporary.data());
        const std::wstring_view base(
            temporary.data(),
            std::min<std::size_t>(length, temporary.size() - 1U));
        settings_path_ = std::filesystem::path(base) / L"SmoothEverything" / L"settings.json";
    }
    static_cast<void>(LoadLocal());
}

const SessionState& SettingsClient::State() const noexcept {
    return state_;
}

SessionState& SettingsClient::MutableState() noexcept {
    return state_;
}

const std::filesystem::path& SettingsClient::SettingsPath() const noexcept {
    return settings_path_;
}

bool SettingsClient::Refresh(const unsigned long timeout_ms) {
    std::string response;
    if (!SendRequest(R"({"op":"get_state"})", response, timeout_ms) ||
        !ApplyResponse(response, true)) {
        state_.online = false;
        state_.status = L"未连接到引擎";
        return false;
    }
    state_.online = true;
    state_.status = L"引擎已连接";
    return true;
}

bool SettingsClient::Apply(const unsigned long timeout_ms) {
    const JsonValue settings = ParseJson(SerializeSettings(state_.settings, false));
    const std::string request = SerializeJson(JsonValue::Object{
        {"op", "apply_settings"},
        {"settings", settings},
    }, false);

    std::string response;
    if (SendRequest(request, response, timeout_ms) && ApplyResponse(response, true)) {
        state_.online = true;
        state_.status = L"已应用到引擎";
        return true;
    }

    const std::wstring request_error = state_.last_error;
    if (SaveLocal()) {
        state_.online = false;
        state_.status = L"已保存；引擎离线";
        state_.last_error = request_error;
        return true;
    }
    state_.online = false;
    state_.status = L"保存失败";
    return false;
}

bool SettingsClient::StartSiblingEngine() {
    std::vector<wchar_t> module(32768U, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length == 0 || length >= module.size()) {
        SetWin32Error(L"获取程序目录失败", GetLastError());
        return false;
    }

    const std::filesystem::path directory =
        std::filesystem::path(std::wstring_view(module.data(), length)).parent_path();
    const std::filesystem::path engine = directory / L"SmoothEverything.Engine.exe";
    if (!std::filesystem::exists(engine)) {
        state_.last_error = L"在设置程序同目录中找不到 SmoothEverything.Engine.exe";
        state_.status = L"无法启动引擎";
        return false;
    }

    std::wstring command = L"\"" + engine.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(
            engine.c_str(),
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            directory.c_str(),
            &startup,
            &process)) {
        SetWin32Error(L"启动引擎失败", GetLastError());
        state_.status = L"无法启动引擎";
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    state_.status = L"正在等待引擎…";
    return true;
}

bool SettingsClient::LoadLocal() {
    const HANDLE file = CreateFileW(
        settings_path_.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            state_.settings = DefaultSettings();
            return true;
        }
        SetWin32Error(L"无法读取本地配置", error);
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        size.QuadPart > static_cast<LONGLONG>(kMaximumMessageBytes)) {
        const DWORD error = GetLastError();
        CloseHandle(file);
        SetWin32Error(L"本地配置大小无效", error == ERROR_SUCCESS ? ERROR_FILE_TOO_LARGE : error);
        return false;
    }
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        DWORD read = 0;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
            bytes.size() - offset, std::numeric_limits<DWORD>::max()));
        if (!ReadFile(file, bytes.data() + offset, request, &read, nullptr) || read == 0) {
            const DWORD error = GetLastError();
            CloseHandle(file);
            SetWin32Error(L"无法读取本地配置", error);
            return false;
        }
        offset += read;
    }
    CloseHandle(file);

    const SettingsParseResult parsed = ParseSettings(bytes);
    if (!parsed.value.has_value()) {
        state_.last_error = L"无法解析本地配置：" + Utf8ToWide(parsed.error);
        state_.settings = DefaultSettings();
        return false;
    }
    state_.settings = *parsed.value;
    return true;
}

bool SettingsClient::SaveLocal() {
    std::error_code filesystem_error;
    std::filesystem::create_directories(settings_path_.parent_path(), filesystem_error);
    if (filesystem_error) {
        state_.last_error = L"无法创建配置目录：" +
            Utf8ToWide(filesystem_error.message());
        return false;
    }

    const std::filesystem::path temporary = settings_path_.wstring() + L".settings.tmp";
    const HANDLE file = CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        SetWin32Error(L"无法创建临时配置", GetLastError());
        return false;
    }
    const std::string json = SerializeSettings(state_.settings, true);
    const bool written = WriteAll(file, json) && FlushFileBuffers(file) != FALSE;
    const DWORD write_error = written ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!written) {
        DeleteFileW(temporary.c_str());
        SetWin32Error(L"无法写入配置", write_error);
        return false;
    }
    if (!MoveFileExW(
            temporary.c_str(),
            settings_path_.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temporary.c_str());
        SetWin32Error(L"无法替换配置", error);
        return false;
    }
    return true;
}

bool SettingsClient::SendRequest(
    const std::string_view request,
    std::string& response,
    const unsigned long timeout_ms) {
    if (request.size() + 1U > kMaximumMessageBytes) {
        state_.last_error = L"请求数据过大";
        return false;
    }
    if (!WaitNamedPipeW(kPipePath, timeout_ms)) {
        SetWin32Error(L"连接引擎失败", GetLastError());
        return false;
    }
    const UniqueHandle pipe(CreateFileW(
        kPipePath,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr));
    if (!pipe.Valid()) {
        SetWin32Error(L"打开引擎管道失败", GetLastError());
        return false;
    }

    std::string line(request);
    line.push_back('\n');
    std::size_t offset = 0;
    while (offset < line.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
            line.size() - offset, 64U * 1024U));
        DWORD written = 0;
        if (!TransferOverlapped(
                pipe.Get(),
                true,
                line.data() + offset,
                chunk,
                written,
                timeout_ms) || written == 0) {
            SetWin32Error(L"向引擎发送请求失败", GetLastError());
            return false;
        }
        offset += written;
    }

    response.clear();
    response.reserve(4096U);
    std::array<char, 4096> buffer{};
    while (response.size() <= kMaximumMessageBytes) {
        DWORD read = 0;
        if (!TransferOverlapped(
                pipe.Get(),
                false,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                read,
                timeout_ms) || read == 0) {
            SetWin32Error(L"读取引擎响应失败", GetLastError());
            return false;
        }
        response.append(buffer.data(), read);
        const std::size_t newline = response.find('\n');
        if (newline != std::string::npos) {
            response.resize(newline);
            if (!response.empty() && response.back() == '\r') {
                response.pop_back();
            }
            return !response.empty();
        }
    }
    state_.last_error = L"引擎响应超过 1 MiB 上限";
    return false;
}

bool SettingsClient::ApplyResponse(
    const std::string_view response,
    const bool replace_settings) {
    try {
        const JsonValue root = ParseJson(response);
        if (!root.IsObject()) {
            state_.last_error = L"引擎返回的数据不是 JSON 对象";
            return false;
        }
        const JsonValue* ok = root.Find("ok");
        const JsonValue* error = root.Find("error");
        state_.last_error = error != nullptr && error->IsString()
            ? Utf8ToWide(error->AsString())
            : std::wstring{};
        if (ok == nullptr || !ok->IsBoolean() || !ok->AsBoolean()) {
            if (state_.last_error.empty()) {
                state_.last_error = L"引擎拒绝了请求";
            }
            return false;
        }

        if (replace_settings) {
            const JsonValue* settings = root.Find("settings");
            if (settings != nullptr && settings->IsObject()) {
                const SettingsParseResult parsed = ParseSettings(SerializeJson(*settings, false));
                if (!parsed.value.has_value()) {
                    state_.last_error = L"引擎返回了无效配置：" + Utf8ToWide(parsed.error);
                    return false;
                }
                state_.settings = *parsed.value;
            }
        }

        const JsonValue* diagnostics = root.Find("diagnostics");
        if (diagnostics != nullptr && diagnostics->IsObject()) {
            state_.diagnostics = EngineDiagnostics{
                .physical_events = ReadCounter(*diagnostics, "physical_events"),
                .smoothed_events = ReadCounter(*diagnostics, "smoothed_events"),
                .passed_events = ReadCounter(*diagnostics, "passed_events"),
                .injected_events = ReadCounter(*diagnostics, "injected_events"),
                .injected_delta = ReadCounter(*diagnostics, "injected_delta"),
                .queue_overflows = ReadCounter(*diagnostics, "queue_overflows"),
                .injection_failures = ReadCounter(*diagnostics, "injection_failures"),
                .target_changes = ReadCounter(*diagnostics, "target_changes"),
                .settings_generation = ReadCounter(*diagnostics, "settings_generation"),
            };
        }
        return true;
    } catch (const std::exception& error) {
        state_.last_error = L"引擎返回了无效数据：" + Utf8ToWide(error.what());
        return false;
    }
}

void SettingsClient::SetWin32Error(
    const std::wstring_view operation,
    const unsigned long error) {
    state_.last_error = std::wstring(operation) + L"：" + Win32Message(error);
}

}  // namespace smootheverything::control_panel
