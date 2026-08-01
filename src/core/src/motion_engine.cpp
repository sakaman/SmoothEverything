#include "smootheverything/motion_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace smootheverything {
namespace {

[[nodiscard]] int Sign(const int value) noexcept {
    return (value > 0) - (value < 0);
}

}  // namespace

MotionSettings NormalizeMotionSettings(MotionSettings settings) noexcept {
    settings.distance_scale = std::clamp(settings.distance_scale, 0.1, 4.0);
    settings.animation_time_ms = std::clamp(settings.animation_time_ms, 1.0, 2000.0);
    settings.acceleration_window_ms = std::clamp(settings.acceleration_window_ms, 0.0, 500.0);
    settings.acceleration_max = std::clamp(settings.acceleration_max, 1.0, 20.0);
    settings.tail_to_head_ratio = std::clamp(settings.tail_to_head_ratio, 0.1, 10.0);
    return settings;
}

MotionEngine::MotionEngine(MotionSettings settings) : settings_(NormalizeMotionSettings(settings)) {}

void MotionEngine::SetSettings(MotionSettings settings) {
    settings_ = NormalizeMotionSettings(settings);
}

const MotionSettings& MotionEngine::Settings() const noexcept {
    return settings_;
}

void MotionEngine::Push(const ScrollAxis axis, const int wheel_delta, const double timestamp_ms) {
    if (wheel_delta == 0 || !std::isfinite(timestamp_ms)) {
        return;
    }

    AxisState& state = StateFor(axis);
    const int direction = Sign(wheel_delta);
    if (state.direction != 0 && state.direction != direction && !state.impulses.empty()) {
        CancelOutstanding(state);
        state.acceleration = 1.0;
        state.last_input_ms = -1.0;
    }

    state.direction = direction;
    const double acceleration = NextAcceleration(state, timestamp_ms);
    const double scaled = static_cast<double>(wheel_delta) * settings_.distance_scale * acceleration;
    std::int64_t total_delta = static_cast<std::int64_t>(std::llround(scaled));
    if (total_delta == 0) {
        total_delta = static_cast<std::int64_t>(direction);
    }

    state.impulses.push_back(Impulse{
        .start_ms = timestamp_ms,
        .duration_ms = settings_.animation_time_ms,
        .total_delta = total_delta,
        .emitted_position = 0.0,
    });
    state.accepted_delta += total_delta;
    state.last_input_ms = timestamp_ms;
}

MotionFrame MotionEngine::Sample(const double timestamp_ms) {
    if (!std::isfinite(timestamp_ms)) {
        return MotionFrame{.active = Active()};
    }

    MotionFrame frame;
    frame.vertical_delta = SampleAxis(vertical_, timestamp_ms);
    frame.horizontal_delta = SampleAxis(horizontal_, timestamp_ms);
    frame.active = Active();
    return frame;
}

void MotionEngine::Reset() noexcept {
    CancelOutstanding(vertical_);
    CancelOutstanding(horizontal_);
}

void MotionEngine::ResetAxis(const ScrollAxis axis) noexcept {
    CancelOutstanding(StateFor(axis));
}

bool MotionEngine::Active() const noexcept {
    return !vertical_.impulses.empty() || !horizontal_.impulses.empty();
}

MotionDiagnostics MotionEngine::Diagnostics() const noexcept {
    return MotionDiagnostics{
        .accepted_vertical_delta = vertical_.accepted_delta,
        .accepted_horizontal_delta = horizontal_.accepted_delta,
        .emitted_vertical_delta = vertical_.emitted_delta,
        .emitted_horizontal_delta = horizontal_.emitted_delta,
        .cancelled_vertical_delta = vertical_.cancelled_delta,
        .cancelled_horizontal_delta = horizontal_.cancelled_delta,
        .vertical_acceleration = vertical_.acceleration,
        .horizontal_acceleration = horizontal_.acceleration,
    };
}

double MotionEngine::EvaluateProgress(
    const double unit_time,
    const double tail_to_head_ratio,
    const bool easing_enabled) noexcept {
    const double time = std::clamp(unit_time, 0.0, 1.0);
    if (!easing_enabled) {
        return time;
    }

    const double ratio = std::clamp(tail_to_head_ratio, 0.1, 10.0);
    const double denominator = 1.0 + ((ratio - 1.0) * time);
    const double warped = denominator > std::numeric_limits<double>::epsilon()
        ? (ratio * time) / denominator
        : time;
    const double squared = warped * warped;
    const double cubed = squared * warped;
    return cubed * ((warped * ((6.0 * warped) - 15.0)) + 10.0);
}

MotionEngine::AxisState& MotionEngine::StateFor(const ScrollAxis axis) noexcept {
    return axis == ScrollAxis::Vertical ? vertical_ : horizontal_;
}

const MotionEngine::AxisState& MotionEngine::StateFor(const ScrollAxis axis) const noexcept {
    return axis == ScrollAxis::Vertical ? vertical_ : horizontal_;
}

int MotionEngine::SampleAxis(AxisState& state, const double timestamp_ms) {
    double newly_available = 0.0;

    for (auto& impulse : state.impulses) {
        const double unit_time = (timestamp_ms - impulse.start_ms) / impulse.duration_ms;
        const double progress = EvaluateProgress(
            unit_time,
            settings_.tail_to_head_ratio,
            settings_.easing_enabled);
        const double desired_position = static_cast<double>(impulse.total_delta) * progress;
        newly_available += desired_position - impulse.emitted_position;
        impulse.emitted_position = desired_position;
    }

    state.fractional_delta += newly_available;
    int whole_delta = 0;
    if (state.fractional_delta >= 1.0) {
        whole_delta = static_cast<int>(std::floor(state.fractional_delta));
    } else if (state.fractional_delta <= -1.0) {
        whole_delta = static_cast<int>(std::ceil(state.fractional_delta));
    }
    state.fractional_delta -= static_cast<double>(whole_delta);

    while (!state.impulses.empty()) {
        const auto& impulse = state.impulses.front();
        if (timestamp_ms < impulse.start_ms + impulse.duration_ms) {
            break;
        }
        state.impulses.pop_front();
    }

    if (state.impulses.empty() && std::abs(state.fractional_delta) > 1e-9) {
        const int correction = static_cast<int>(std::llround(state.fractional_delta));
        whole_delta += correction;
        state.fractional_delta = 0.0;
    }

    state.emitted_delta += static_cast<std::int64_t>(whole_delta);
    return whole_delta;
}

void MotionEngine::CancelOutstanding(AxisState& state) noexcept {
    double outstanding = state.fractional_delta;
    for (const auto& impulse : state.impulses) {
        outstanding += static_cast<double>(impulse.total_delta) - impulse.emitted_position;
    }
    state.cancelled_delta += static_cast<std::int64_t>(std::llround(outstanding));
    state.impulses.clear();
    state.fractional_delta = 0.0;
    state.direction = 0;
}

double MotionEngine::NextAcceleration(AxisState& state, const double timestamp_ms) const noexcept {
    if (state.last_input_ms < 0.0 || settings_.acceleration_window_ms <= 0.0) {
        state.acceleration = 1.0;
        return state.acceleration;
    }

    const double elapsed = std::max(0.0, timestamp_ms - state.last_input_ms);
    if (elapsed > settings_.acceleration_window_ms) {
        state.acceleration = 1.0;
        return state.acceleration;
    }

    const double proximity = 1.0 - (elapsed / settings_.acceleration_window_ms);
    const double increment = std::max(0.15, proximity * 0.75);
    state.acceleration = std::min(settings_.acceleration_max, state.acceleration + increment);
    return state.acceleration;
}

}  // namespace smootheverything
