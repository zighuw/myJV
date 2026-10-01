#include "Envelope.h"

#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>

namespace
{
float normalizedSensitivity (int value) noexcept
{
    return std::clamp ((float) value / 63.0f, -1.0f, 1.0f);
}

float velocityCurveValue (int curve, float velocity) noexcept
{
    switch (curve)
    {
        case 1:   // EXP1
            return velocity * velocity;
        case 2:   // EXP2
        {
            const auto square = velocity * velocity;
            return square * square;
        }
        case 3:   // EXP3
        {
            const auto square = velocity * velocity;
            const auto fourth = square * square;
            return fourth * fourth;
        }
        case 4:   // LOG1
            return std::sqrt (velocity);
        case 5:   // LOG2
            return std::sqrt (std::sqrt (velocity));
        case 6:   // LOG3
            return std::sqrt (std::sqrt (std::sqrt (velocity)));
        default:  // LINEAR
            return velocity;
    }
}

// Bounded velocity gain: 0 -> 1, +63 -> the full curve, -63 -> its mirror.
float velocityGainFor (int curve, int sensitivity, float velocity) noexcept
{
    const auto curved = velocityCurveValue (curve, velocity);
    const auto sens = normalizedSensitivity (sensitivity);

    return sens >= 0.0f ? 1.0f - sens * (1.0f - curved)
                        : 1.0f + sens * curved;
}

// Positive keyfollow shortens segment times for higher notes; note 60 is neutral.
float keyScaleFor (int keyfollow, int midiNote) noexcept
{
    return std::pow (2.0f, -normalizedSensitivity (keyfollow) * (float) (midiNote - 60) / 12.0f);
}

// Positive sensitivity: soft velocities take longer, hard velocities shorter.
float velocityTimeScaleFor (int sensitivity, float velocity) noexcept
{
    return std::pow (2.0f, -normalizedSensitivity (sensitivity)
                               * (float) Calibration::kEnvVelTimeOctaves
                               * (2.0f * velocity - 1.0f));
}

double timeMsFor (float time) noexcept
{
    const auto value = std::clamp (time, 0.0f, 127.0f);
    return Calibration::kEnvTimeMinMs
           * std::pow (Calibration::kEnvTimeMaxMs / Calibration::kEnvTimeMinMs, (double) value / 127.0);
}
}

void Envelope::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

// RT-safe
void Envelope::start (const EnvelopeSettings& newSettings, int midiNote, float velocity) noexcept
{
    settings = newSettings;

    const auto vel = std::clamp (velocity, 0.0f, 1.0f);
    velocityGain = velocityGainFor (settings.velocityCurve, settings.velocitySens, vel);
    keyScale = keyScaleFor (settings.timeKeyfollow, midiNote);
    velTime1Scale = velocityTimeScaleFor (settings.velTime1Sens, vel);
    velTime4Scale = velocityTimeScaleFor (settings.velTime4Sens, vel);

    enterStage (Stage::Attack, effectiveLevel (0),
                timeMsFor (settings.time[0]) * keyScale * velTime1Scale);
}

// RT-safe
void Envelope::release() noexcept
{
    if (currentStage == Stage::Idle || currentStage == Stage::Release || currentStage == Stage::Finished)
        return;

    enterStage (Stage::Release, effectiveLevel (3),
                timeMsFor (settings.time[3]) * keyScale * velTime4Scale);
}

// RT-safe
void Envelope::reset() noexcept
{
    currentStage = Stage::Idle;
    currentLevel = 0.0f;
    stageTarget = 0.0f;
    coefficient = 0.0f;
    samplesRemaining = 0;
}

// RT-safe
float Envelope::process() noexcept
{
    switch (currentStage)
    {
        case Stage::Idle:
        case Stage::Hold:
        case Stage::Finished:
            return currentLevel;

        case Stage::Attack:
        case Stage::Decay:
        case Stage::SustainRamp:
        case Stage::Release:
            break;
    }

    currentLevel += (stageTarget - currentLevel) * coefficient;

    if (--samplesRemaining <= 0)
    {
        currentLevel = stageTarget;

        switch (currentStage)
        {
            case Stage::Attack:
                enterStage (Stage::Decay, effectiveLevel (1), timeMsFor (settings.time[1]) * keyScale);
                break;
            case Stage::Decay:
                enterStage (Stage::SustainRamp, effectiveLevel (2), timeMsFor (settings.time[2]) * keyScale);
                break;
            case Stage::SustainRamp:
                currentStage = Stage::Hold;
                break;
            case Stage::Release:
                currentStage = Stage::Finished;
                break;
            default:
                break;
        }
    }

    return currentLevel;
}

bool Envelope::isActive() const noexcept
{
    return currentStage != Stage::Idle && currentStage != Stage::Finished;
}

bool Envelope::isFinished() const noexcept
{
    return currentStage == Stage::Finished;
}

float Envelope::effectiveLevel (int index) const noexcept
{
    return std::clamp (settings.level[index] * Calibration::kEnvLevelScale * velocityGain, 0.0f, 1.0f);
}

void Envelope::enterStage (Stage stage, float target, double ms) noexcept
{
    currentStage = stage;
    stageTarget = target;

    const auto samples = std::max (1.0, std::round (ms * sampleRate / 1000.0));
    samplesRemaining = (int) std::min (samples, 2147483647.0);
    coefficient = 1.0f - std::exp (-(float) Calibration::kEnvCurve / (float) samples);
}
