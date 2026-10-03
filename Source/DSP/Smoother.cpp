#include "Smoother.h"

#include <cmath>

void Smoother::prepare (double sampleRate, double smoothingMs) noexcept
{
    const auto rate = sampleRate > 0.0 ? sampleRate : 48000.0;

    if (smoothingMs <= 0.0)
        coefficient = 1.0f;
    else
        coefficient = (float) (1.0 - std::exp (-1.0 / (smoothingMs * 0.001 * rate)));

    reset (0.0f);
}

// RT-safe
void Smoother::reset (float value) noexcept
{
    currentValue = value;
    target = value;
}

// RT-safe
void Smoother::snapToTarget() noexcept
{
    currentValue = target;
}

// RT-safe
void Smoother::setTarget (float newTarget) noexcept
{
    target = newTarget;
}

// RT-safe
float Smoother::process() noexcept
{
    currentValue += (target - currentValue) * coefficient;

    if (std::abs (target - currentValue) < 1.0e-4f)
        currentValue = target;

    return currentValue;
}

bool Smoother::isSmoothing() const noexcept
{
    return std::abs (target - currentValue) > 1.0e-4f;
}
