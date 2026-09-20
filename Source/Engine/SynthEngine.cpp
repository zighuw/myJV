#include "SynthEngine.h"

#include <cmath>

namespace
{
constexpr double kTestToneFrequencies[3] { 440.0, 660.0, 880.0 };
constexpr float kTestToneLevel = 0.1f;
}

void SynthEngine::prepare (double newSampleRate, int) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    for (auto& phase : phases)
        phase = 0.0;
}

void SynthEngine::releaseResources() noexcept {}

// RT-safe
void SynthEngine::process (const BusBuffers& buses, int numSamples) noexcept
{
    for (int bus = 0; bus < kNumBuses; ++bus)
    {
        if (buses.l[bus] == nullptr || buses.r[bus] == nullptr)
            continue;

        const auto phaseDelta = juce::MathConstants<double>::twoPi * kTestToneFrequencies[bus] / sampleRate;
        auto phase = phases[bus];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = kTestToneLevel * static_cast<float> (std::sin (phase));

            buses.l[bus][i] = value;
            buses.r[bus][i] = value;

            phase += phaseDelta;

            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        phases[bus] = phase;
    }
}
