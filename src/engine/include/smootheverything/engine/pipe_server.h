#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <string_view>

namespace smootheverything::engine {

inline constexpr wchar_t kEnginePipeName[] = L"\\\\.\\pipe\\SmoothEverything.Engine.v1";

class PipeServer final {
public:
    using RequestHandler = std::function<std::string(std::string_view)>;

    explicit PipeServer(RequestHandler handler, std::wstring pipe_name = kEnginePipeName);
    ~PipeServer();

    PipeServer(const PipeServer&) = delete;
    PipeServer& operator=(const PipeServer&) = delete;

    [[nodiscard]] bool Start() noexcept;
    void Stop() noexcept;
    [[nodiscard]] bool Running() const noexcept;

private:
    [[nodiscard]] static DWORD WINAPI ThreadEntry(void* context) noexcept;
    [[nodiscard]] DWORD Run() noexcept;
    [[nodiscard]] bool ServeClient(HANDLE pipe) noexcept;
    [[nodiscard]] bool ReadRequest(HANDLE pipe, std::string& request) noexcept;
    [[nodiscard]] bool WriteResponse(HANDLE pipe, std::string_view response) noexcept;
    [[nodiscard]] bool WaitForIo(HANDLE pipe, OVERLAPPED& operation, DWORD& transferred) noexcept;

    RequestHandler handler_;
    std::wstring pipe_name_;
    HANDLE stop_event_{nullptr};
    HANDLE thread_{nullptr};
    std::atomic<bool> running_{false};
};

}  // namespace smootheverything::engine
