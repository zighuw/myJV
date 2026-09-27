#pragma once

#include "Model/ZoneSet.h"

// Single-voice sample playback: pitch ratio, 4-point Hermite interpolation,
// Off/Forward/Sustain loop modes with linear crossfade, reverse playback and a
// Note-On fade-in. Stereo samples are downmixed to the mono Tone chain.
class SamplePlayer
{
public:
    SamplePlayer() = default;

    // RT-safe
    void start (const Sample& sample, const Zone& zone, int note, double engineSampleRate) noexcept;
    void stop() noexcept;

    // Bend/mod/slide hook. The value persists across start() calls, so a pooled
    // voice must refresh it at Note-On.
    void setPitchOffsetSemitones (float semitones) noexcept;

    // RT-safe
    float getNextSample() noexcept;

    bool isActive() const noexcept;
    bool hasFinished() const noexcept;

private:
    // RT-safe
    float interpolate (double position) const noexcept;
    float sampleForPosition (double position) const noexcept;

    const float* left = nullptr;
    const float* right = nullptr;
    int numSamples = 0;
    bool stereo = false;
    double position = 0.0;
    double baseRatio = 1.0;
    float pitchScale = 1.0f;
    int loopStart = 0;
    int loopEnd = 0;
    bool loopEnabled = false;
    bool reverse = false;
    int crossfadeSamples = 0;
    float fadeInGain = 0.0f;
    float fadeInStep = 1.0f;
    bool active = false;
    bool finished = true;
};
