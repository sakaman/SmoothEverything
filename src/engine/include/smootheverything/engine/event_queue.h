#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace smootheverything::engine {

template <typename T, std::size_t Capacity>
class SpscQueue final {
    static_assert(Capacity >= 2, "SPSC queue needs at least two slots");
    static_assert(std::is_nothrow_copy_assignable_v<T>, "SPSC values must copy without throwing");

public:
    [[nodiscard]] bool TryPush(const T& value) noexcept {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = Increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        values_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool TryPop(T& value) noexcept {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;
        }
        value = values_[tail];
        tail_.store(Increment(tail), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool Empty() const noexcept {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }

private:
    [[nodiscard]] static constexpr std::size_t Increment(const std::size_t value) noexcept {
        return (value + 1U) % Capacity;
    }

    alignas(64) std::array<T, Capacity> values_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace smootheverything::engine
