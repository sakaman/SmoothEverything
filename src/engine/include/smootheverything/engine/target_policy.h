#pragma once

#include "smootheverything/engine/runtime_state.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace smootheverything::engine {

struct ResolvedTarget final {
    DWORD process_id{0};
    std::uint64_t settings_generation{0};
    std::uint64_t resolved_at_ms{0};
    std::string executable;
    ResolvedAppPolicy policy{};
};

class TargetPolicyCache final {
public:
    explicit TargetPolicyCache(RuntimeState& runtime);

    [[nodiscard]] bool TryGet(DWORD process_id, std::uint64_t now_ms, ResolvedTarget& result) const noexcept;
    [[nodiscard]] ResolvedTarget Resolve(DWORD process_id, std::uint64_t now_ms);
    void MarkPassThrough(DWORD process_id, std::uint64_t now_ms);
    void Invalidate();

private:
    using PolicyMap = std::unordered_map<DWORD, ResolvedTarget>;

    [[nodiscard]] static std::string ExecutableName(DWORD process_id);
    void Publish(ResolvedTarget target);

    RuntimeState& runtime_;
    std::shared_ptr<const PolicyMap> policies_{std::make_shared<const PolicyMap>()};
    std::mutex writer_mutex_;
};

}  // namespace smootheverything::engine
