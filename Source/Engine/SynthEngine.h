#pragma once

#include "Engine/VoiceManager.h"

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
    // on the audio thread. The processor owns an AssetReclaimer and publishes
    // the first PatchRuntime once the library has been mapped (M3-01).
    void setParamSnapshotSource (const ParamSnapshotCache* cache, AssetReclaimer* reclaimer) noexcept;

    // RT-safe
    void process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept;

    // Diagnostics: total voice modulator updates (one per rendered sample per
    // sounding voice, M2-05 contract).
    std::uint64_t modulationUpdateCount() const noexcept { return manager.modulationUpdateCount(); }

private:
    // RT-safe
    void refreshActiveSnapshot() noexcept;

    // RT-safe. Routes a Note-On to the voice manager using the active runtime.
    void handleNoteOn (int note, int velocity, int channel) noexcept;

    // RT-safe.
    void handleNoteOff (int note, int channel) noexcept;

    // RT-safe. Publishes the tracked controller state to the voice manager.
    void refreshModulationInput (int numSamples) noexcept;

    // RT-safe. Reports the oldest in-use runtime id to the reclaimer (ADR-018).
    void reportOldestAssetInUse() noexcept;

    // RT-safe
    void renderSegment (const BusBuffers& buses, int startSample, int numSamples) noexcept;

    std::atomic<MidiEventSink*> midiSink { nullptr };
    std::atomic<const ParamSnapshotCache*> snapshotCache { nullptr };
    std::atomic<AssetReclaimer*> snapshotReclaimer { nullptr };

    VoiceManager manager;
    std::uint64_t noteSeed = 1;
    double sampleRate = 48000.0;
    std::uint64_t totalSamples = 0;
    std::uint64_t lastNoteOnSample = 0;
    bool hasLastNoteOn = false;

    float ccValues[128] {};
    float pitchBendValue = 0.0f;
    float aftertouchValue = 0.0f;
};
