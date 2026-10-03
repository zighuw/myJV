#include "SynthEngine.h"

#include "Params/Calibration.h"
#include "Params/ParamSnapshotCache.h"

#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

// non-RT: prepare may allocate (architecture 5.3 - buffer allocation and state
// reset happen here, off the audio thread). M2 wires the Tone voices in.
void SynthEngine::prepare (double newSampleRate, int) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    totalSamples = 0;
    lastNoteOnSample = 0;
    hasLastNoteOn = false;

    for (auto& value : ccValues)
        value = 0.0f;

    pitchBendValue = 0.0f;
    aftertouchValue = 0.0f;
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

    const auto* active = reclaimer->activeForAudio();

    if (active == nullptr)
        return;

    // Tone Delay KEY INTERVAL uses the previous Note-On spacing (M4-06 refines).
    auto intervalScale = 1.0f;

    if (hasLastNoteOn)
    {
        const auto reference = std::max (1.0, sampleRate * Calibration::kKeyIntervalReferenceMs / 1000.0);
        const auto interval = totalSamples > lastNoteOnSample ? totalSamples - lastNoteOnSample : 0;
        intervalScale = std::clamp ((float) ((double) interval / reference),
                                    Calibration::kKeyIntervalScaleMin,
                                    Calibration::kKeyIntervalScaleMax);
    }

    lastNoteOnSample = totalSamples;
    hasLastNoteOn = true;

    voice.startNote (active, 0, note, velocity, voiceSeed++, intervalScale);
}

// RT-safe
void SynthEngine::refreshModulationInput (int numSamples) noexcept
{
    ModulationInput input;

    for (int i = 0; i < 128; ++i)
        input.cc[i] = ccValues[i];

    input.pitchBend = pitchBendValue;
    input.aftertouch = aftertouchValue;
    input.sourceIndex[0] = 0;   // Control 1 is fixed to Modulation (CC1)

    const auto* reclaimer = snapshotReclaimer.load (std::memory_order_acquire);
    const auto* active = reclaimer != nullptr ? reclaimer->activeForAudio() : nullptr;

    if (active != nullptr)
    {
        const auto& common = active->snapshot.common;
        input.sourceIndex[1] = common.ctrlSource2;
        input.sourceIndex[2] = common.ctrlSource3;
        input.holdPeakMode[0] = common.ctrl1HoldPeak;
        input.holdPeakMode[1] = common.ctrl2HoldPeak;
        input.holdPeakMode[2] = common.ctrl3HoldPeak;
    }

    voice.setModulationInput (input, numSamples);
}

// RT-safe
void SynthEngine::process (const BusBuffers& buses, const juce::MidiBuffer& midi, int numSamples) noexcept
{
    refreshActiveSnapshot();
    refreshModulationInput (numSamples);
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

        // Zero-copy parsing (M0-05 / M1-F07): no MidiMessage construction on
        // the audio thread, so long messages can never allocate here.
        if (metadata.numBytes >= 2)
        {
            const auto status = metadata.data[0] & 0xf0;
            const auto data1 = metadata.data[1] & 0x7f;

            if (status == 0x90 && metadata.numBytes >= 3)
            {
                const auto velocity = metadata.data[2] & 0x7f;

                if (velocity > 0)
                    startVoice (data1, (float) velocity / 127.0f);
                else
                    voice.release();
            }
            else if (status == 0x80 && metadata.numBytes >= 3)
            {
                voice.release();
            }
            else if (status == 0xB0 && metadata.numBytes >= 3)
            {
                ccValues[data1] = (float) (metadata.data[2] & 0x7f) / 127.0f;
            }
            else if (status == 0xE0 && metadata.numBytes >= 3)
            {
                const auto value = ((int) (metadata.data[2] & 0x7f) << 7) | (int) data1;
                pitchBendValue = (float) (value - 8192) / 8192.0f;
            }
            else if (status == 0xD0)
            {
                aftertouchValue = (float) data1 / 127.0f;
            }
        }

        if (auto* sink = midiSink.load (std::memory_order_acquire))
            sink->handleMidiEvent (metadata);
    }

    if (currentSample < numSamples)
        renderSegment (buses, currentSample, numSamples - currentSample);

    totalSamples += (std::uint64_t) std::max (0, numSamples);
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
