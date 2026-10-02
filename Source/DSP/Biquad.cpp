#include "Biquad.h"

#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kFixedQ = 1.0f;
}

void Biquad::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    setPeaking (1000.0f, kFixedQ, 0.0f);
    reset();
}

// RT-safe
void Biquad::reset() noexcept
{
    x1 = 0.0f;
    x2 = 0.0f;
    y1 = 0.0f;
    y2 = 0.0f;
}

// RT-safe
void Biquad::setPeaking (float centreHz, float q, float gainDb) noexcept
{
    const auto maxHz = (float) (0.49 * sampleRate);
    currentCentreHz = std::clamp (centreHz, (float) Calibration::kCutoffMinHz, maxHz);
    currentGainDb = gainDb;

    const auto quality = std::max (q, 0.05f);
    const auto A = std::pow (10.0f, currentGainDb / 40.0f);
    const auto w0 = 2.0 * kPi * (double) currentCentreHz / sampleRate;
    const auto cosine = std::cos (w0);
    const auto alpha = std::sin (w0) / (2.0 * (double) quality);
    const auto a0 = 1.0 + alpha / (double) A;

    b0 = (float) ((1.0 + alpha * (double) A) / a0);
    b1 = (float) (-2.0 * cosine / a0);
    b2 = (float) ((1.0 - alpha * (double) A) / a0);
    a1 = (float) (-2.0 * cosine / a0);
    a2 = (float) ((1.0 - alpha / (double) A) / a0);
}

// RT-safe
void Biquad::setParameters (float cutoffParam, float resonanceParam) noexcept
{
    const auto cutoffValue = std::clamp (cutoffParam, 0.0f, 127.0f);
    const auto resonanceValue = std::clamp (resonanceParam, 0.0f, 127.0f);

    const auto hz = Calibration::kCutoffMinHz
                    * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz,
                                (double) cutoffValue / 127.0);
    const auto gain = (double) resonanceValue / 127.0 * Calibration::kPkgGainDb;

    setPeaking ((float) hz, kFixedQ, (float) gain);
}

// RT-safe
float Biquad::process (float in) noexcept
{
    const auto out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    x2 = x1;
    x1 = in;
    y2 = y1;
    y1 = out;

    return out;
}
