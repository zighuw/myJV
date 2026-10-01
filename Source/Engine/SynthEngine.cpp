#include "SynthEngine.h"

#include "Params/ParamSnapshotCache.h"

#include <juce_audio_basics/juce_audio_basics.h>

// non-RT: prepare may allocate (architecture 5.3 - buffer allocation and state
// reset happen here, off the audio thread). M2 wires the Tone voices in.
void SynthEngine::prepare (double, int) noexcept {}

void SynthEngine::releaseResources() noexcept {}

void SynthEngine::setMidiEventSink (MidiEventSink* sink) noexcept
{
    midiSink.store (sink, std::memory_order_release);
}

void SynthEngine::setParamSnapshotSource (const ParamSnapshotCache* cache, const AssetReclaimer* reclaimer) noexcept
{
    snapshotReclaimer.store (reclaimer, std::memory_order_release);
    snapshotCache.store (cache, std::memory_order_release);
}

// RT-safe. Architecture 5.3 step 2: refresh the active runtime's snapshot once
// per block, before the MIDI sub-block split. Retired runtimes keep their
// frozen snapshot (architecture 4.2 / Patch Remain).
void SynthEngine::refreshActiveSnapshot() noexcept
{
    const auto* cache = snapshotCache.load (std::memory_order_acquire);
    const auto* reclaimer = snapshotReclaimer.load (std::memory_order_acquire);

    if (cache == nullptr || reclaimer == nullptr)
        return;

    if (const auto* active = reclaimer->activeForAudio())
        cache->refresh (*active);
}

// RT-safe
void SynthEngine::process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept
{
    refreshActiveSnapshot();

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
    // The sampler is the only waveform source (architecture 1.3); until the
    // voice engine lands (M2) every segment is cleared to silence.
    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        if (buses.l[bus] != nullptr)
            juce::FloatVectorOperations::clear (buses.l[bus] + startSample, numSamples);

        if (buses.r[bus] != nullptr)
            juce::FloatVectorOperations::clear (buses.r[bus] + startSample, numSamples);
    }
}
