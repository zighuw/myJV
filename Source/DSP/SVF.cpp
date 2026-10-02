#include "SVF.h"

#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
}

void SVF::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    setCoefficients (1000.0f, 1.0f);
    reset();
}

// RT-safe
void SVF::reset() noexcept
{
    ic1eq = 0.0f;
    ic2eq = 0.0f;
}

// RT-safe
void SVF::setCoefficients (float cutoffHz, float q) noexcept
{
    const auto maxHz = 0.49f * (float) sampleRate;
    currentCutoffHz = std::clamp (cutoffHz, (float) Calibration::kCutoffMinHz, maxHz);
    currentQ = std::max (q, 0.05f);

    g = std::tan (kPi * currentCutoffHz / (float) sampleRate);
    k = 1.0f / currentQ;
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

// RT-safe
void SVF::setParameters (float cutoffParam, float resonanceParam) noexcept
{
    const auto cutoffValue = std::clamp (cutoffParam, 0.0f, 127.0f);
    const auto resonanceValue = std::clamp (resonanceParam, 0.0f, 127.0f);

    const auto hz = Calibration::kCutoffMinHz
                    * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz,
                                (double) cutoffValue / 127.0);
    const auto q = 0.5 * std::pow (Calibration::kResonanceMaxQ / 0.5, (double) resonanceValue / 127.0);

    setCoefficients ((float) hz, (float) q);
}

// RT-safe
SVF::Outputs SVF::update (float in) noexcept
{
    const auto v3 = in - ic2eq;
    const auto v1 = a1 * ic1eq + a2 * v3;
    const auto v2 = ic2eq + a2 * ic1eq + a3 * v3;

    const auto limit = Calibration::kFilterStateLimit;
    ic1eq = std::clamp (2.0f * v1 - ic1eq, -limit, limit);
    ic2eq = std::clamp (2.0f * v2 - ic2eq, -limit, limit);

    return { v2, v1, in - k * v1 - v2 };
}

// RT-safe
float SVF::processLowpass (float in) noexcept
{
    return update (in).low;
}

// RT-safe
float SVF::processBandpass (float in) noexcept
{
    return update (in).band;
}

// RT-safe
float SVF::processHighpass (float in) noexcept
{
    return update (in).high;
}
