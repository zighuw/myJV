#include "SynthEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

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

void SynthEngine::setMidiEventSink (MidiEventSink* sink) noexcept
{
    midiSink.store (sink, std::memory_order_release);
}

// RT-safe
void SynthEngine::process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept
{
    int currentSample = 0;

    for (const auto metadata : midi)
    {
        const auto eventSample = metadata.samplePosition;

        if (eventSample < 0 || eventSample >= numSamples)
            continue;

        if (eventSample > currentSample)
        {
            renderSegment (buses, currentSample, eventSample - currentSample);
            currentSample = eventSample;
        }

        if (auto* sink = midiSink.load (std::memory_order_acquire))
            sink->handleMidiEvent (metadata);
    }

    if (currentSample < numSamples)
        renderSegment (buses, currentSample, numSamples - currentSample);
}

// RT-safe
void SynthEngine::renderSegment (const BusBuffers& buses, int startSample, int numSamples) noexcept
{
    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        if (buses.l[bus] == nullptr || buses.r[bus] == nullptr)
            continue;

        const auto phaseDelta = juce::MathConstants<double>::twoPi * kTestToneFrequencies[bus] / sampleRate;
        auto phase = phases[bus];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = kTestToneLevel * static_cast<float> (std::sin (phase));

            buses.l[bus][startSample + i] = value;
            buses.r[bus][startSample + i] = value;

            phase += phaseDelta;

            while (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        phases[bus] = phase;
    }
}
