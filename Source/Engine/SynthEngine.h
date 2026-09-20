#pragma once

inline constexpr int kNumOutputBuses = 3;

struct BusBuffers
{
    float* l[kNumOutputBuses] {};
    float* r[kNumOutputBuses] {};
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
    double sampleRate = 48000.0;
    double phases[kNumOutputBuses] {};
};
