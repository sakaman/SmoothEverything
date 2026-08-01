#include "smootheverything/engine/pipe_server.h"

#include <windows.h>
#include <sddl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef PIPE_REJECT_REMOTE_CLIENTS
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008
#endif

namespace smootheverything::engine {
namespace {

constexpr std::size_t kMaximumRequestBytes = 1024U * 1024U;

struct LocalFreeDeleter final {
    void operator()(void* value) const noexcept {
        if (value != nullptr) {
            LocalFree(value);
        }
    }
};

using LocalPointer = std::unique_ptr<void, LocalFreeDeleter>;

class UniqueHandle final {
public:
    explicit UniqueHandle(const HANDLE value = nullptr) noexcept : value_(value) {}
    ~UniqueHandle() {
        if (value_ != nullptr && value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    [[nodiscard]] HANDLE Get() const noexcept { return value_; }
    [[nodiscard]] bool Valid() const noexcept { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }

private:
    HANDLE value_;
};

[[nodiscard]] LocalPointer CurrentUserSecurityDescriptor() noexcept {
    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
        return {};
    }
    const UniqueHandle token(raw_token);

    DWORD bytes = 0;
    static_cast<void>(GetTokenInformation(token.Get(), TokenUser, nullptr, 0, &bytes));
    if (bytes == 0) {
        return {};
    }
    std::vector<std::byte> buffer(bytes);
    if (!GetTokenInformation(token.Get(), TokenUser, buffer.data(), bytes, &bytes)) {
        return {};
    }
    const auto* user = reinterpret_cast<const TOKEN_USER*>(buffer.data());

    wchar_t* raw_sid = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &raw_sid) || raw_sid == nullptr) {
        return {};
    }
    const LocalPointer sid(raw_sid);
    const std::wstring sddl = L"D:P(A;;GA;;;" + std::wstring(raw_sid) + L")";

    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
        return {};
    }
    return LocalPointer(descriptor);
}

}  // namespace

PipeServer::PipeServer(RequestHandler handler, std::wstring pipe_name)
    : handler_(std::move(handler)), pipe_name_(std::move(pipe_name)) {}

PipeServer::~PipeServer() {
    Stop();
}

bool PipeServer::Start() noexcept {
    if (thread_ != nullptr) {
        return running_.load(std::memory_order_acquire);
    }
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_event_ == nullptr) {
        return false;
    }
    thread_ = CreateThread(nullptr, 0, &PipeServer::ThreadEntry, this, 0, nullptr);
    if (thread_ == nullptr) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
        return false;
    }
    running_.store(true, std::memory_order_release);
    return true;
}

void PipeServer::Stop() noexcept {
    if (stop_event_ != nullptr) {
        SetEvent(stop_event_);
    }
    if (thread_ != nullptr) {
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (stop_event_ != nullptr) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }
    running_.store(false, std::memory_order_release);
}

bool PipeServer::Running() const noexcept {
    return running_.load(std::memory_order_acquire);
}

DWORD WINAPI PipeServer::ThreadEntry(void* context) noexcept {
    return static_cast<PipeServer*>(context)->Run();
}

DWORD PipeServer::Run() noexcept {
    const LocalPointer descriptor = CurrentUserSecurityDescriptor();
    if (!descriptor) {
        running_.store(false, std::memory_order_release);
        return ERROR_ACCESS_DENIED;
    }
    SECURITY_ATTRIBUTES security{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = descriptor.get(),
        .bInheritHandle = FALSE,
    };

    while (WaitForSingleObject(stop_event_, 0) != WAIT_OBJECT_0) {
        const UniqueHandle pipe(CreateNamedPipeW(
            pipe_name_.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1,
            64U * 1024U,
            64U * 1024U,
            0,
            &security));
        if (!pipe.Valid()) {
            running_.store(false, std::memory_order_release);
            return GetLastError();
        }

        const UniqueHandle connect_event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!connect_event.Valid()) {
            running_.store(false, std::memory_order_release);
            return GetLastError();
        }
        OVERLAPPED connect{};
        connect.hEvent = connect_event.Get();
        BOOL connected = ConnectNamedPipe(pipe.Get(), &connect);
        if (!connected) {
            const DWORD error = GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                connected = TRUE;
            } else if (error == ERROR_IO_PENDING) {
                DWORD ignored = 0;
                connected = WaitForIo(pipe.Get(), connect, ignored) ? TRUE : FALSE;
            }
        }

        if (connected) {
            static_cast<void>(ServeClient(pipe.Get()));
            FlushFileBuffers(pipe.Get());
            DisconnectNamedPipe(pipe.Get());
        }
    }
    running_.store(false, std::memory_order_release);
    return 0;
}

bool PipeServer::ServeClient(const HANDLE pipe) noexcept {
    std::string request;
    if (!ReadRequest(pipe, request)) {
        return false;
    }

    std::string response;
    try {
        response = handler_(request);
    } catch (...) {
        response = R"({"ok":false,"error":"internal engine error"})";
    }
    if (response.empty() || response.back() != '\n') {
        response.push_back('\n');
    }
    return WriteResponse(pipe, response);
}

bool PipeServer::ReadRequest(const HANDLE pipe, std::string& request) noexcept {
    std::array<char, 4096> buffer{};
    while (request.size() <= kMaximumRequestBytes) {
        const UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event.Valid()) {
            return false;
        }
        OVERLAPPED operation{};
        operation.hEvent = event.Get();
        DWORD transferred = 0;
        const BOOL immediate = ReadFile(
            pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &transferred, &operation);
        if (!immediate) {
            const DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING || !WaitForIo(pipe, operation, transferred)) {
                return false;
            }
        }
        if (transferred == 0) {
            return false;
        }
        request.append(buffer.data(), transferred);
        const std::size_t newline = request.find('\n');
        if (newline != std::string::npos) {
            request.resize(newline);
            return true;
        }
    }
    return false;
}

bool PipeServer::WriteResponse(const HANDLE pipe, const std::string_view response) noexcept {
    std::size_t offset = 0;
    while (offset < response.size()) {
        const UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!event.Valid()) {
            return false;
        }
        OVERLAPPED operation{};
        operation.hEvent = event.Get();
        const std::size_t remaining = response.size() - offset;
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(remaining, 64U * 1024U));
        DWORD transferred = 0;
        const BOOL immediate = WriteFile(pipe, response.data() + offset, request, &transferred, &operation);
        if (!immediate) {
            const DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING || !WaitForIo(pipe, operation, transferred)) {
                return false;
            }
        }
        if (transferred == 0) {
            return false;
        }
        offset += transferred;
    }
    return true;
}

bool PipeServer::WaitForIo(
    const HANDLE pipe,
    OVERLAPPED& operation,
    DWORD& transferred) noexcept {
    const std::array<HANDLE, 2> handles{stop_event_, operation.hEvent};
    const DWORD wait = WaitForMultipleObjects(
        static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);
    if (wait == WAIT_OBJECT_0) {
        CancelIoEx(pipe, &operation);
        static_cast<void>(GetOverlappedResult(pipe, &operation, &transferred, TRUE));
        return false;
    }
    if (wait != WAIT_OBJECT_0 + 1) {
        CancelIoEx(pipe, &operation);
        return false;
    }
    return GetOverlappedResult(pipe, &operation, &transferred, FALSE) != FALSE;
}

}  // namespace smootheverything::engine
