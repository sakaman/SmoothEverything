#include "smootheverything/motion_engine.h"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace motion_tests {

using smootheverything::MotionEngine;
using smootheverything::MotionSettings;
using smootheverything::ScrollAxis;

void Require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void RequireNear(const double actual, const double expected, const double tolerance, const std::string& message) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            message + ": expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

std::pair<int, int> Drain(MotionEngine& engine, const double begin_ms, const double end_ms, const double step_ms) {
    int vertical = 0;
    int horizontal = 0;
    for (double now = begin_ms; now <= end_ms; now += step_ms) {
        const auto frame = engine.Sample(now);
        vertical += frame.vertical_delta;
        horizontal += frame.horizontal_delta;
    }
    return {vertical, horizontal};
}

void CurveHasStableEndpointsAndIsMonotonic() {
    RequireNear(MotionEngine::EvaluateProgress(-1.0, 3.0, true), 0.0, 1e-12, "curve start clamps");
    RequireNear(MotionEngine::EvaluateProgress(2.0, 3.0, true), 1.0, 1e-12, "curve end clamps");

    double previous = 0.0;
    for (int index = 1; index <= 1000; ++index) {
        const double unit = static_cast<double>(index) / 1000.0;
        const double current = MotionEngine::EvaluateProgress(unit, 3.0, true);
        Require(current >= previous, "curve must be monotonic");
        previous = current;
    }

    const double epsilon = 1e-5;
    const double start_slope = MotionEngine::EvaluateProgress(epsilon, 3.0, true) / epsilon;
    const double end_slope =
        (1.0 - MotionEngine::EvaluateProgress(1.0 - epsilon, 3.0, true)) / epsilon;
    Require(start_slope < 0.01, "curve must start with near-zero velocity");
    Require(end_slope < 0.01, "curve must end with near-zero velocity");
}

void SingleImpulseConservesDelta() {
    MotionSettings settings;
    settings.animation_time_ms = 360.0;
    settings.acceleration_max = 1.0;
    MotionEngine engine(settings);

    engine.Push(ScrollAxis::Vertical, 120, 0.0);
    const auto [vertical, horizontal] = Drain(engine, 0.0, 500.0, 4.0);
    Require(vertical == 120, "single vertical impulse must conserve delta");
    Require(horizontal == 0, "vertical impulse must not leak to horizontal axis");
    Require(!engine.Active(), "completed impulse must become idle");
}

void OverlappingAxesConserveIndependently() {
    MotionSettings settings;
    settings.animation_time_ms = 200.0;
    settings.acceleration_max = 1.0;
    MotionEngine engine(settings);

    engine.Push(ScrollAxis::Vertical, 120, 0.0);
    engine.Push(ScrollAxis::Vertical, 120, 20.0);
    engine.Push(ScrollAxis::Horizontal, -120, 30.0);

    const auto [vertical, horizontal] = Drain(engine, 0.0, 400.0, 2.0);
    Require(vertical == 240, "overlapping vertical impulses must conserve their sum");
    Require(horizontal == -120, "horizontal impulse must conserve sign and delta");
}

void AccelerationCapsAndResetsAfterTheWindow() {
    MotionSettings settings;
    settings.animation_time_ms = 100.0;
    settings.acceleration_window_ms = 70.0;
    settings.acceleration_max = 3.0;
    MotionEngine engine(settings);

    for (int index = 0; index < 20; ++index) {
        engine.Push(ScrollAxis::Vertical, 120, static_cast<double>(index * 10));
    }
    RequireNear(engine.Diagnostics().vertical_acceleration, 3.0, 1e-12, "acceleration must cap");

    engine.Push(ScrollAxis::Vertical, 120, 1000.0);
    RequireNear(engine.Diagnostics().vertical_acceleration, 1.0, 1e-12, "acceleration must reset after idle");
}

void DirectionReversalCancelsOldMomentum() {
    MotionSettings settings;
    settings.animation_time_ms = 400.0;
    settings.acceleration_max = 1.0;
    MotionEngine engine(settings);

    engine.Push(ScrollAxis::Vertical, 120, 0.0);
    const auto first = engine.Sample(80.0);
    Require(first.vertical_delta > 0, "initial motion must follow initial direction");

    engine.Push(ScrollAxis::Vertical, -120, 81.0);
    int after_reversal = 0;
    for (double now = 81.0; now <= 600.0; now += 4.0) {
        const auto frame = engine.Sample(now);
        Require(frame.vertical_delta <= 0, "old positive momentum must not leak after reversal");
        after_reversal += frame.vertical_delta;
    }
    Require(after_reversal == -120, "reversed impulse must conserve its own delta");
    Require(engine.Diagnostics().cancelled_vertical_delta > 0, "cancelled momentum must be observable");
}

void ResetIsFailOpenAndLeavesNoTail() {
    MotionEngine engine;
    engine.Push(ScrollAxis::Vertical, 120, 0.0);
    engine.Push(ScrollAxis::Horizontal, 120, 0.0);
    static_cast<void>(engine.Sample(20.0));

    engine.Reset();
    Require(!engine.Active(), "reset must clear active impulses");
    const auto frame = engine.Sample(1000.0);
    Require(frame.vertical_delta == 0 && frame.horizontal_delta == 0, "reset must leave no tail");
}

std::vector<std::pair<std::string, std::function<void()>>> Cases() {
    return {
        {"curve endpoints and monotonicity", CurveHasStableEndpointsAndIsMonotonic},
        {"single impulse delta conservation", SingleImpulseConservesDelta},
        {"overlapping axes delta conservation", OverlappingAxesConserveIndependently},
        {"acceleration cap and reset", AccelerationCapsAndResetsAfterTheWindow},
        {"direction reversal", DirectionReversalCancelsOldMomentum},
        {"reset clears tail", ResetIsFailOpenAndLeavesNoTail},
    };
}

}  // namespace motion_tests
