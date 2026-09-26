#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <string>

enum class LoopMode
{
    Off = 0,
    Forward,
    Sustain
};

// Loop region in sample frames, half-open [start, end); a region with
// start >= end is invalid and behaves as no loop (interpreted by SamplePlayer).
// crossfadeSamples == 0 means no crossfade.
struct LoopInfo
{
    int start = 0;
    int end = 0;
    int crossfadeSamples = 0;
};

// Immutable after publication (architecture 1.3 rule 3); shared through
// std::shared_ptr<const Sample> and referenced by one or more zones.
struct Sample
{
    juce::AudioBuffer<float> data;
    double sourceSampleRate = 48000.0;
    int rootKey = 60;
    LoopInfo embeddedLoop;   // file loop points at import; Zone::loop is the effective playback region
    std::string name, fileHash;
};
