#include "ModulationMatrix.h"

#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>

namespace
{
float clampPositiveUnit (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

float depthScale (float norm) noexcept
{
    return std::clamp (1.0f + norm, 0.0f, 2.0f);
}
}

void ModulationMatrix::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

// RT-safe
void ModulationMatrix::reset() noexcept
{
    for (int group = 0; group < 3; ++group)
    {
        smoothed[group] = 0.0f;
        latch[group] = 0.0f;
    }
}

// RT-safe
float ModulationMatrix::rawSource (const ModulationInput& input, int group) const noexcept
{
    // Group 1 always follows Modulation (CC1); groups 2/3 use the patch
    // common source choice (0..94 -> CC1..CC95, 95 pitch bend, 96 aftertouch).
    const auto index = group == 0 ? 0 : std::clamp (input.sourceIndex[group], 0, 96);

    if (index <= 94)
        return clampPositiveUnit (input.cc[index + 1]);

    if (index == 95)
        return clampPositiveUnit ((input.pitchBend + 1.0f) * 0.5f);

    return clampPositiveUnit (input.aftertouch);
}

// RT-safe
void ModulationMatrix::update (const ModulationInput& input, int blockSamples) noexcept
{
    const auto blockMs = (double) std::max (0, blockSamples) / sampleRate * 1000.0;
    const auto coefficient = (float) std::clamp (1.0 - std::exp (-blockMs / Calibration::kModSmoothingMs),
                                                 0.0, 1.0);

    for (int group = 0; group < 3; ++group)
    {
        const auto raw = rawSource (input, group);

        if (input.holdPeakMode[group] == 1)   // PEAK: keep the maximum
            latch[group] = std::max (latch[group], raw);
        else if (raw > 0.0f)                  // HOLD: latch the last non-zero value
            latch[group] = raw;

        smoothed[group] += (latch[group] - smoothed[group]) * coefficient;
    }
}

// RT-safe
ModulationOutputs ModulationMatrix::evaluate (const ToneSnapshot::Ctrl (&ctrl)[3]) const noexcept
{
    ModulationOutputs outputs;

    for (int group = 0; group < 3; ++group)
    {
        const auto source = smoothed[group];

        for (int slot = 0; slot < 4; ++slot)
        {
            const auto dest = ctrl[group].dest[slot];

            if (dest <= 0 || dest > 22)
                continue;

            const auto norm = ((float) ctrl[group].depth[slot] / 63.0f) * source;

            if (dest == 1)        outputs.pitchSemitones += norm * (float) Calibration::kMatrixPitchSemitones;
            else if (dest == 2)   outputs.cutoffOctaves += norm * (float) Calibration::kMatrixCutoffOctaves;
            else if (dest == 3)   outputs.resonanceOctaves += norm * (float) Calibration::kMatrixResonanceOctaves;
            else if (dest == 4)   outputs.levelDb += norm * (float) Calibration::kMatrixLevelDb;
            else if (dest == 5)   outputs.pan += norm;
            else if (dest >= 6 && dest <= 15)
            {
                const auto lfo = dest >= 11 ? 1 : 0;
                const auto target = dest - (lfo == 0 ? 6 : 11);

                if (target == 0)       outputs.lfoRateOctaves[lfo] += norm * (float) Calibration::kMatrixLfoRateOctaves;
                else if (target == 1)  outputs.lfoPitchDepthScale[lfo] *= depthScale (norm);
                else if (target == 2)  outputs.lfoCutoffDepthScale[lfo] *= depthScale (norm);
                else if (target == 3)  outputs.lfoAmpDepthScale[lfo] *= depthScale (norm);
                else                   outputs.lfoPanDepthScale[lfo] *= depthScale (norm);
            }
            else if (dest == 17 || dest == 19 || dest == 21)
            {
                const auto envelope = dest == 17 ? 0 : (dest == 19 ? 1 : 2);
                outputs.envLevelScale[envelope] *= depthScale (norm);
            }

            // dest 16/18/20 (ENV_TIME) and 22 (TONE_DELAY_TIME) are deferred:
            // they stay inert until their application points are designed.
        }
    }

    return outputs;
}

// RT-safe
float ModulationMatrix::groupValue (int group) const noexcept
{
    const auto index = std::clamp (group, 0, 2);
    return smoothed[index];
}
