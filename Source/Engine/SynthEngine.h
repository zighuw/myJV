#pragma once

#include <atomic>

namespace juce
{
class MidiBuffer;
struct MidiMessageMetadata;
}

inline constexpr int kNumOutputBuses = 3;

struct BusBuffers
{
    float* l[kNumOutputBuses] {};
    float* r[kNumOutputBuses] {};
};

class MidiEventSink
{
public:
    virtual ~MidiEventSink() = default;

    // RT-safe
    virtual void handleMidiEvent (const juce::MidiMessageMetadata& event) noexcept = 0;
};

class SynthEngine
{
public:
    SynthEngine() = default;

    void prepare (double newSampleRate, int maxBlockSize) noexcept;
    void releaseResources() noexcept;

    void setMidiEventSink (MidiEventSink* sink) noexcept;

    // RT-safe
    void process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept;

private:
    // RT-safe
    void renderSegment (const BusBuffers& buses, int startSample, int numSamples) noexcept;

    std::atomic<MidiEventSink*> midiSink { nullptr };
    double sampleRate = 48000.0;
    double phases[kNumOutputBuses] {};
};
