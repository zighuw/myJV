#pragma once

// First-order lowpass parameter smoother (architecture 4.3): step changes are
// approached exponentially with a fixed time constant, removing zipper noise
// from block-rate parameter updates. prepare() is non-RT; the rest is RT-safe.
class Smoother
{
public:
    Smoother() = default;

    // Non-RT. smoothingMs <= 0 makes process() jump straight to the target.
    void prepare (double sampleRate, double smoothingMs) noexcept;

    // RT-safe. Snaps current and target to `value`.
    void reset (float value = 0.0f) noexcept;

    // RT-safe. Snaps the current value to the current target (Note-On).
    void snapToTarget() noexcept;

    // RT-safe.
    void setTarget (float target) noexcept;

    // RT-safe. Advances one sample toward the target.
    float process() noexcept;

    float current() const noexcept { return currentValue; }
    bool isSmoothing() const noexcept;

private:
    float currentValue = 0.0f;
    float target = 0.0f;
    float coefficient = 1.0f;
};
