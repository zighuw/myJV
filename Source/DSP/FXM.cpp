#include "FXM.h"

#include "Params/Calibration.h"

#include <algorithm>

// RT-safe
void FXM::reset() noexcept
{
    feedback = 0.0f;
}

// RT-safe
void FXM::setParameters (bool enabled, int color, float depth) noexcept
{
    on = enabled;
    colorGain = std::clamp ((float) color, 1.0f, 7.0f) / 7.0f;
    depthNorm = std::clamp (depth, 0.0f, 127.0f) / 127.0f;
}

// RT-safe
float FXM::process (float input) noexcept
{
    feedback += (input - feedback) * colorGain;

    if (! on)
        return 0.0f;

    return depthNorm * feedback * (float) Calibration::kFxmDepthSamples;
}
