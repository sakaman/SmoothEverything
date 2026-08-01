#pragma once

#include "smootheverything/settings.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace smootheverything::engine {

struct RuntimeCounters final {
    std::atomic<std::uint64_t> physical_events{0};
    std::atomic<std::uint64_t> smoothed_events{0};
    std::atomic<std::uint64_t> passed_events{0};
    std::atomic<std::uint64_t> queue_overflows{0};
    std::atomic<std::uint64_t> injected_events{0};
    std::atomic<std::uint64_t> injected_delta{0};
    std::atomic<std::uint64_t> injection_failures{0};
    std::atomic<std::uint64_t> target_changes{0};
};

struct RuntimeDiagnostics final {
    std::uint64_t physical_events{0};
    std::uint64_t smoothed_events{0};
    std::uint64_t passed_events{0};
    std::uint64_t queue_overflows{0};
    std::uint64_t injected_events{0};
    std::uint64_t injected_delta{0};
    std::uint64_t injection_failures{0};
    std::uint64_t target_changes{0};
    std::uint64_t settings_generation{0};
};

class RuntimeState final {
public:
    explicit RuntimeState(AppSettings settings)
        : settings_(std::make_shared<const AppSettings>(std::move(settings))) {}

    [[nodiscard]] std::shared_ptr<const AppSettings> Settings() const noexcept {
        return std::atomic_load_explicit(&settings_, std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t SettingsGeneration() const noexcept {
        return settings_generation_.load(std::memory_order_acquire);
    }

    void UpdateSettings(AppSettings settings) {
        std::atomic_store_explicit(
            &settings_,
            std::make_shared<const AppSettings>(std::move(settings)),
            std::memory_order_release);
        settings_generation_.fetch_add(1, std::memory_order_acq_rel);
    }

    [[nodiscard]] RuntimeDiagnostics Diagnostics() const noexcept {
        return {
            .physical_events = counters.physical_events.load(std::memory_order_relaxed),
            .smoothed_events = counters.smoothed_events.load(std::memory_order_relaxed),
            .passed_events = counters.passed_events.load(std::memory_order_relaxed),
            .queue_overflows = counters.queue_overflows.load(std::memory_order_relaxed),
            .injected_events = counters.injected_events.load(std::memory_order_relaxed),
            .injected_delta = counters.injected_delta.load(std::memory_order_relaxed),
            .injection_failures = counters.injection_failures.load(std::memory_order_relaxed),
            .target_changes = counters.target_changes.load(std::memory_order_relaxed),
            .settings_generation = SettingsGeneration(),
        };
    }

    RuntimeCounters counters;

private:
    std::shared_ptr<const AppSettings> settings_;
    std::atomic<std::uint64_t> settings_generation_{1};
};

}  // namespace smootheverything::engine
