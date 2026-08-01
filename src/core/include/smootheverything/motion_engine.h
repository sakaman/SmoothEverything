#pragma once

#include <cstdint>
#include <deque>

namespace smootheverything {

enum class ScrollAxis : std::uint8_t {
    Vertical,
    Horizontal,
};

struct MotionSettings final {
    double distance_scale{1.0};
    double animation_time_ms{360.0};
    double acceleration_window_ms{70.0};
    double acceleration_max{7.0};
    double tail_to_head_ratio{3.0};
    bool easing_enabled{true};
};

struct MotionFrame final {
    int vertical_delta{0};
    int horizontal_delta{0};
    bool active{false};
};

struct MotionDiagnostics final {
    std::int64_t accepted_vertical_delta{0};
    std::int64_t accepted_horizontal_delta{0};
    std::int64_t emitted_vertical_delta{0};
    std::int64_t emitted_horizontal_delta{0};
    std::int64_t cancelled_vertical_delta{0};
    std::int64_t cancelled_horizontal_delta{0};
    double vertical_acceleration{1.0};
    double horizontal_acceleration{1.0};
};

[[nodiscard]] MotionSettings NormalizeMotionSettings(MotionSettings settings) noexcept;

class MotionEngine final {
public:
    explicit MotionEngine(MotionSettings settings = {});

    void SetSettings(MotionSettings settings);
    [[nodiscard]] const MotionSettings& Settings() const noexcept;

    void Push(ScrollAxis axis, int wheel_delta, double timestamp_ms);
    [[nodiscard]] MotionFrame Sample(double timestamp_ms);

    void Reset() noexcept;
    void ResetAxis(ScrollAxis axis) noexcept;
    [[nodiscard]] bool Active() const noexcept;
    [[nodiscard]] MotionDiagnostics Diagnostics() const noexcept;

    [[nodiscard]] static double EvaluateProgress(
        double unit_time,
        double tail_to_head_ratio,
        bool easing_enabled) noexcept;

private:
    struct Impulse final {
        double start_ms{0.0};
        double duration_ms{0.0};
        std::int64_t total_delta{0};
        double emitted_position{0.0};
    };

    struct AxisState final {
        std::deque<Impulse> impulses;
        double fractional_delta{0.0};
        double last_input_ms{-1.0};
        double acceleration{1.0};
        int direction{0};
        std::int64_t accepted_delta{0};
        std::int64_t emitted_delta{0};
        std::int64_t cancelled_delta{0};
    };

    [[nodiscard]] AxisState& StateFor(ScrollAxis axis) noexcept;
    [[nodiscard]] const AxisState& StateFor(ScrollAxis axis) const noexcept;
    [[nodiscard]] int SampleAxis(AxisState& state, double timestamp_ms);
    void CancelOutstanding(AxisState& state) noexcept;
    [[nodiscard]] double NextAcceleration(AxisState& state, double timestamp_ms) const noexcept;

    MotionSettings settings_;
    AxisState vertical_;
    AxisState horizontal_;
};

}  // namespace smootheverything
