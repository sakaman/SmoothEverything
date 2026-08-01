#include "smootheverything/engine/target_policy.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace smootheverything::engine {
namespace {

constexpr std::uint64_t kPolicyLifetimeMs = 30'000;
constexpr std::size_t kMaximumCachedProcesses = 128;

class UniqueHandle final {
public:
    explicit UniqueHandle(const HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        if (handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    [[nodiscard]] HANDLE Get() const noexcept { return handle_; }
    [[nodiscard]] bool Valid() const noexcept { return handle_ != nullptr; }

private:
    HANDLE handle_;
};

[[nodiscard]] std::string Utf8FromWide(const std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }
    return result;
}

}  // namespace

TargetPolicyCache::TargetPolicyCache(RuntimeState& runtime) : runtime_(runtime) {}

bool TargetPolicyCache::TryGet(
    const DWORD process_id,
    const std::uint64_t now_ms,
    ResolvedTarget& result) const noexcept {
    const auto policies = std::atomic_load_explicit(&policies_, std::memory_order_acquire);
    const auto iterator = policies->find(process_id);
    if (iterator == policies->end()) {
        return false;
    }
    const ResolvedTarget& candidate = iterator->second;
    if (candidate.settings_generation != runtime_.SettingsGeneration()
        || now_ms < candidate.resolved_at_ms
        || now_ms - candidate.resolved_at_ms > kPolicyLifetimeMs) {
        return false;
    }
    result = candidate;
    return true;
}

ResolvedTarget TargetPolicyCache::Resolve(const DWORD process_id, const std::uint64_t now_ms) {
    ResolvedTarget result{
        .process_id = process_id,
        .settings_generation = runtime_.SettingsGeneration(),
        .resolved_at_ms = now_ms,
        .executable = ExecutableName(process_id),
    };
    if (result.executable.empty()) {
        result.policy.smooth_enabled = false;
    } else {
        result.policy = ResolveAppPolicy(*runtime_.Settings(), result.executable);
    }
    Publish(result);
    return result;
}

void TargetPolicyCache::MarkPassThrough(const DWORD process_id, const std::uint64_t now_ms) {
    ResolvedTarget result{
        .process_id = process_id,
        .settings_generation = runtime_.SettingsGeneration(),
        .resolved_at_ms = now_ms,
        .executable = ExecutableName(process_id),
        .policy = {
            .smooth_enabled = false,
            .compatibility_mode = true,
            .motion = runtime_.Settings()->motion,
        },
    };
    Publish(std::move(result));
}

void TargetPolicyCache::Invalidate() {
    std::lock_guard lock(writer_mutex_);
    std::atomic_store_explicit(
        &policies_, std::make_shared<const PolicyMap>(), std::memory_order_release);
}

std::string TargetPolicyCache::ExecutableName(const DWORD process_id) {
    const UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
    if (!process.Valid()) {
        return {};
    }

    std::array<wchar_t, 32'768> buffer{};
    DWORD size = static_cast<DWORD>(buffer.size());
    if (!QueryFullProcessImageNameW(process.Get(), 0, buffer.data(), &size) || size == 0) {
        return {};
    }
    const std::wstring_view path(buffer.data(), static_cast<std::size_t>(size));
    const std::size_t separator = path.find_last_of(L"/\\");
    const std::wstring_view basename = separator == std::wstring_view::npos ? path : path.substr(separator + 1);
    return NormalizeExecutableKey(Utf8FromWide(basename));
}

void TargetPolicyCache::Publish(ResolvedTarget target) {
    std::lock_guard lock(writer_mutex_);
    const auto current = std::atomic_load_explicit(&policies_, std::memory_order_acquire);
    auto updated = std::make_shared<PolicyMap>(*current);
    if (updated->size() >= kMaximumCachedProcesses && !updated->contains(target.process_id)) {
        const auto oldest = std::min_element(
            updated->begin(),
            updated->end(),
            [](const auto& left, const auto& right) {
                return left.second.resolved_at_ms < right.second.resolved_at_ms;
            });
        if (oldest != updated->end()) {
            updated->erase(oldest);
        }
    }
    (*updated)[target.process_id] = std::move(target);
    std::atomic_store_explicit(
        &policies_, std::shared_ptr<const PolicyMap>(std::move(updated)), std::memory_order_release);
}

}  // namespace smootheverything::engine
