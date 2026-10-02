#include "SynthEngine.h"

#include "Params/ParamSnapshotCache.h"

#include <juce_audio_basics/juce_audio_basics.h>

// non-RT: prepare may allocate (architecture 5.3 - buffer allocation and state
// reset happen here, off the audio thread). M2 wires the Tone voices in.
void SynthEngine::prepare (double sampleRate, int) noexcept
{
    voice.prepare (sampleRate);
}

void SynthEngine::releaseResources() noexcept
{
    voice.reset();
}

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

// RT-safe. Temporary single-voice note routing until VoiceManager (M3-01).
void SynthEngine::startVoice (int note, float velocity) noexcept
{
    const auto* reclaimer = snapshotReclaimer.load (std::memory_order_acquire);

    if (reclaimer == nullptr)
        return;

    if (const auto* active = reclaimer->activeForAudio())
        voice.startNote (active, 0, note, velocity, voiceSeed++);
}

// RT-safe
void SynthEngine::process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept
{
    refreshActiveSnapshot();
    voice.beginBlock();

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

        // Zero-copy note parsing (M0-05 / M1-F07): no MidiMessage construction
        // on the audio thread, so long messages can never allocate here.
        if (metadata.numBytes >= 3)
        {
            const auto status = metadata.data[0] & 0xf0;
            const auto note = metadata.data[1] & 0x7f;
            const auto data2 = metadata.data[2] & 0x7f;

            if (status == 0x90 && data2 > 0)
                startVoice (note, (float) data2 / 127.0f);
            else if (status == 0x80 || (status == 0x90 && data2 == 0))
                voice.release();
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
        if (buses.l[bus] != nullptr)
            juce::FloatVectorOperations::clear (buses.l[bus] + startSample, numSamples);

        if (buses.r[bus] != nullptr)
            juce::FloatVectorOperations::clear (buses.r[bus] + startSample, numSamples);
    }

    if (voice.finished())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        BusBuffers cursor;

        for (int bus = 0; bus < kNumOutputBuses; ++bus)
        {
            cursor.l[bus] = buses.l[bus] != nullptr ? buses.l[bus] + startSample + i : nullptr;
            cursor.r[bus] = buses.r[bus] != nullptr ? buses.r[bus] + startSample + i : nullptr;
        }

        voice.updateModulators();
        const auto sample = voice.processTVA (voice.processTVF (voice.processWG()));
        voice.addToBus (sample, cursor);
    }
}
