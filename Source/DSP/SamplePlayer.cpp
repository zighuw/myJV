#include "DSP/SamplePlayer.h"

#include "Params/Calibration.h"

#include <cmath>

namespace
{
float hermite (const float* data, int numSamples, double position) noexcept
{
    const auto index = (int) std::floor (position);
    const auto fraction = (float) (position - (double) index);

    const auto i0 = juce::jlimit (0, numSamples - 1, index - 1);
    const auto i1 = juce::jlimit (0, numSamples - 1, index);
    const auto i2 = juce::jlimit (0, numSamples - 1, index + 1);
    const auto i3 = juce::jlimit (0, numSamples - 1, index + 2);

    const auto x0 = data[i0];
    const auto x1 = data[i1];
    const auto x2 = data[i2];
    const auto x3 = data[i3];

    const auto c0 = x1;
    const auto c1 = 0.5f * (x2 - x0);
    const auto c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
    const auto c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);

    return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
}
}

void SamplePlayer::start (const Sample& sample, const Zone& zone, int note, double engineSampleRate) noexcept
{
    active = false;
    finished = true;
    left = nullptr;
    right = nullptr;
    numSamples = 0;
    stereo = false;

    const auto frames = sample.data.getNumSamples();

    if (sample.data.getNumChannels() <= 0 || frames <= 0
        || ! (engineSampleRate > 0.0) || ! (sample.sourceSampleRate > 0.0))
        return;

    const auto rootKey = zone.rootKeyOverride >= 0 ? zone.rootKeyOverride : sample.rootKey;
    const auto semitones = (double) (note - rootKey)
                           + (double) zone.coarseTune
                           + (double) zone.fineTune / 100.0;

    baseRatio = (sample.sourceSampleRate / engineSampleRate) * std::exp2 (semitones / 12.0);
    reverse = zone.reverse;

    numSamples = frames;
    stereo = sample.data.getNumChannels() > 1;
    left = sample.data.getReadPointer (0);
    right = stereo ? sample.data.getReadPointer (1) : left;

    loopStart = zone.loop.start;
    loopEnd = zone.loop.end;
    loopEnabled = zone.loopMode != LoopMode::Off
                  && loopStart >= 0 && loopEnd > loopStart && loopEnd <= numSamples;
    crossfadeSamples = loopEnabled ? juce::jlimit (0, (loopEnd - loopStart) / 2, zone.loop.crossfadeSamples) : 0;

    const auto fadeSamples = juce::jmax (1, juce::roundToInt (Calibration::kSampleFadeInMs * 0.001 * engineSampleRate));
    fadeInGain = 0.0f;
    fadeInStep = 1.0f / (float) fadeSamples;

    position = reverse ? (double) (numSamples - 1) : 0.0;
    active = true;
    finished = false;
}

void SamplePlayer::stop() noexcept
{
    active = false;
    finished = true;
}

void SamplePlayer::setPitchOffsetSemitones (float semitones) noexcept
{
    pitchScale = std::exp2 (semitones / 12.0f);
}

bool SamplePlayer::isActive() const noexcept
{
    return active;
}

bool SamplePlayer::hasFinished() const noexcept
{
    return finished;
}

float SamplePlayer::interpolate (double positionToRead) const noexcept
{
    const auto mono = hermite (left, numSamples, positionToRead);

    if (! stereo)
        return mono;

    return 0.5f * (mono + hermite (right, numSamples, positionToRead));
}

float SamplePlayer::sampleForPosition (double positionToRead) const noexcept
{
    auto value = interpolate (positionToRead);

    if (loopEnabled && crossfadeSamples > 0)
    {
        const auto crossfade = (double) crossfadeSamples;

        if (! reverse)
        {
            const auto regionStart = (double) loopEnd - crossfade;

            if (positionToRead >= regionStart)
            {
                const auto mix = juce::jlimit (0.0f, 1.0f, (float) ((positionToRead - regionStart) / crossfade));
                const auto headPosition = (double) loopStart + (positionToRead - regionStart);
                value = value * (1.0f - mix) + interpolate (headPosition) * mix;
            }
        }
        else
        {
            const auto regionEnd = (double) loopStart + crossfade;

            if (positionToRead <= regionEnd)
            {
                const auto mix = juce::jlimit (0.0f, 1.0f, (float) ((regionEnd - positionToRead) / crossfade));
                const auto tailPosition = (double) loopEnd - crossfade + (positionToRead - (double) loopStart);
                value = value * (1.0f - mix) + interpolate (tailPosition) * mix;
            }
        }
    }

    return value;
}

float SamplePlayer::getNextSample() noexcept
{
    if (! active)
        return 0.0f;

    if (loopEnabled)
    {
        // Steady-state span: the crossfade region is skipped after each wrap, so
        // the modulo must use loopLength - crossfade, otherwise large increments
        // (tiny loops at high pitch) can escape the region.
        const auto span = (double) (loopEnd - loopStart - crossfadeSamples);

        if (span > 0.0)
        {
            if (! reverse && position >= (double) loopEnd)
                position = (double) loopStart + (double) crossfadeSamples
                           + std::fmod (position - (double) loopEnd, span);
            else if (reverse && position < (double) loopStart)
                position = (double) loopEnd - (double) crossfadeSamples
                           - std::fmod ((double) loopStart - position, span);
        }
    }
    else
    {
        if ((! reverse && position >= (double) numSamples) || (reverse && position < 0.0))
        {
            active = false;
            finished = true;
            return 0.0f;
        }
    }

    auto value = sampleForPosition (position);

    value *= fadeInGain;

    if (fadeInGain < 1.0f)
        fadeInGain = juce::jmin (1.0f, fadeInGain + fadeInStep);

    position += baseRatio * (double) pitchScale * (reverse ? -1.0 : 1.0);

    if (! loopEnabled
        && ((! reverse && position >= (double) numSamples) || (reverse && position < 0.0)))
    {
        active = false;
        finished = true;
    }

    return value;
}
