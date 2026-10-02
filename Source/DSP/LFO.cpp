#include "LFO.h"

#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kTwoPi = 6.283185307179586f;

double rateToHz (float rate) noexcept
{
    const auto value = std::clamp (rate, 0.0f, 127.0f);
    return Calibration::kLfoRateMinHz
           * std::pow (Calibration::kLfoRateMaxHz / Calibration::kLfoRateMinHz, (double) value / 127.0);
}

double syncHz (float rate, double tempoBpm) noexcept
{
    const auto value = std::clamp (rate, 0.0f, 127.0f);
    const auto index = (int) std::lround ((double) value / 127.0 * 17.0);
    const auto beats = Calibration::kLfoSyncBeats[std::clamp (index, 0, 17)];
    const auto tempo = tempoBpm > 0.0 ? tempoBpm : 120.0;
    return (tempo / 60.0) / beats;
}

int timeToSamples (float time, double maxMs, double sampleRate) noexcept
{
    const auto value = std::clamp (time, 0.0f, 127.0f);
    const auto ms = maxMs * std::pow ((double) value / 127.0, 2.0);
    return (int) std::min (std::max (0.0, std::round (ms * sampleRate / 1000.0)), 2147483647.0);
}
}

void LFO::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

// RT-safe
void LFO::start (const LfoSettings& newSettings, std::uint64_t seed, double tempoBpm) noexcept
{
    settings = newSettings;

    if (settings.keyTrig)
        phase = 0.0f;

    frequency = settings.sync ? syncHz (settings.rate, tempoBpm) : rateToHz (settings.rate);
    offset = std::clamp ((float) settings.levelOffset * Calibration::kLfoLevelOffsetScale, -1.0f, 1.0f);

    delayRemaining = timeToSamples (settings.delayTime, Calibration::kLfoDelayMaxMs, sampleRate);
    fadeSamples = timeToSamples (settings.fadeTime, Calibration::kLfoFadeMaxMs, sampleRate);
    fadeElapsed = 0;

    random.reseed (seed);

    if (settings.wave == 5)   // S&H
        sampleHold = random.nextBipolar();
    else if (settings.wave == 6)   // Random
    {
        randomPrevious = random.nextBipolar();
        randomTarget = random.nextBipolar();
    }
    else if (settings.wave == 7)   // Chaotic
        chaosX = 0.2f + 0.6f * random.nextFloat();

    active = true;
    currentOutput = offset;
}

// RT-safe
void LFO::reset() noexcept
{
    active = false;
    currentOutput = 0.0f;
    phase = 0.0f;
    frequency = 0.0;
    delayRemaining = 0;
    fadeSamples = 0;
    fadeElapsed = 0;
}

// RT-safe
float LFO::process() noexcept
{
    if (! active)
        return currentOutput;

    if (delayRemaining > 0)
    {
        --delayRemaining;
        currentOutput = offset;
        return currentOutput;
    }

    const auto wave = waveformValue();

    if (fadeElapsed < fadeSamples)
        ++fadeElapsed;

    currentOutput = offset + fadeValue() * wave;

    phase += (float) (frequency / sampleRate);

    if (phase >= 1.0f)
    {
        phase -= std::floor (phase);
        advanceCycle();
    }

    return currentOutput;
}

float LFO::waveformValue() const noexcept
{
    switch (settings.wave)
    {
        case 1:   // SAW (rising)
            return 2.0f * phase - 1.0f;

        case 2:   // SQUARE
            return phase < 0.5f ? 1.0f : -1.0f;

        case 3:   // TRIANGLE (peak at phase 0.5)
            return 1.0f - 4.0f * std::abs (phase - 0.5f);

        case 4:   // TRAPEZOID (clipped triangle, plateau at +-1)
            return std::clamp (2.0f * (1.0f - 4.0f * std::abs (phase - 0.5f)), -1.0f, 1.0f);

        case 5:   // SAMPLE & HOLD
            return sampleHold;

        case 6:   // RANDOM (linear interpolation between cycle values)
            return randomPrevious + (randomTarget - randomPrevious) * phase;

        case 7:   // CHAOTIC (logistic map, bipolar output)
            return 2.0f * chaosX - 1.0f;

        default:  // SINE
            return std::sin (kTwoPi * phase);
    }
}

float LFO::fadeValue() const noexcept
{
    if (fadeSamples <= 0)
        return 1.0f;

    const auto progress = (float) fadeElapsed / (float) fadeSamples;

    if (settings.fadeMode == 1)
    {
        const auto curve = (float) Calibration::kLfoFadeCurve;
        return (1.0f - std::exp (-curve * progress)) / (1.0f - std::exp (-curve));
    }

    return progress;
}

void LFO::advanceCycle() noexcept
{
    switch (settings.wave)
    {
        case 5:
            sampleHold = random.nextBipolar();
            break;

        case 6:
            randomPrevious = randomTarget;
            randomTarget = random.nextBipolar();
            break;

        case 7:
            chaosX = (float) Calibration::kChaosR * chaosX * (1.0f - chaosX);
            break;

        default:
            break;
    }
}
