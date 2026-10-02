#pragma once

#include "Engine/ToneVoice.h"

#include <atomic>
#include <cstdint>

namespace juce
{
class MidiBuffer;
struct MidiMessageMetadata;
}

class AssetReclaimer;
class ParamSnapshotCache;

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

    void prepare (double, int) noexcept;
    void releaseResources() noexcept;

    void setMidiEventSink (MidiEventSink* sink) noexcept;

    // Non-RT wiring; call while the audio callback is stopped (prepareToPlay).
    // A null source (either pointer) makes the per-block refresh a silent no-op
    // on the audio thread. Until the patch publishing pipeline lands (M3-01)
    // the processor wires the cache with no reclaimer, so no runtime is ever
    // active and the running snapshot stays idle.
    void setParamSnapshotSource (const ParamSnapshotCache* cache, const AssetReclaimer* reclaimer) noexcept;

    // RT-safe
    void process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept;

    // Diagnostics: temporary voice modulator cadence (one per rendered sample).
    std::uint64_t modulationUpdateCount() const noexcept { return voice.modulationUpdateCount(); }

private:
    // RT-safe
    void refreshActiveSnapshot() noexcept;

    // RT-safe. Temporary single-voice path until VoiceManager lands (M3-01).
    void startVoice (int note, float velocity) noexcept;

    // RT-safe
    void renderSegment (const BusBuffers& buses, int startSample, int numSamples) noexcept;

    std::atomic<MidiEventSink*> midiSink { nullptr };
    std::atomic<const ParamSnapshotCache*> snapshotCache { nullptr };
    std::atomic<const AssetReclaimer*> snapshotReclaimer { nullptr };

    ToneVoice voice;
    std::uint64_t voiceSeed = 1;
};
