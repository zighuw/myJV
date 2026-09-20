#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct BusBuffers
{
    float* l[3] {};
    float* r[3] {};
};

class SynthEngine
{
public:
    SynthEngine() = default;

    void prepare (double newSampleRate, int maxBlockSize) noexcept;
    void releaseResources() noexcept;

    // RT-safe
    void process (const BusBuffers& buses, int numSamples) noexcept;

private:
    static constexpr int kNumBuses = 3;

    double sampleRate = 48000.0;
    double phases[kNumBuses] {};
};
