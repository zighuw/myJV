#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Plugin/MyJVProcessor.h"

#include <cmath>
#include <memory>
#include <string>

namespace
{
using namespace juce;

using BusesLayout = AudioProcessor::BusesLayout;

constexpr double kSampleRate = 48000.0;
constexpr int kNumSamples = 48000;

struct BusFixture
{
    explicit BusFixture (int samples = kNumSamples)
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize (2, samples);
            buffer.clear();
        }
    }

    BusBuffers makeBusBuffers()
    {
        BusBuffers buses;

        for (int bus = 0; bus < kNumOutputBuses; ++bus)
        {
            buses.l[bus] = buffers[bus].getWritePointer (0);
            buses.r[bus] = buffers[bus].getWritePointer (1);
        }

        return buses;
    }

    AudioBuffer<float> buffers[kNumOutputBuses];
};

void fillBuffer (AudioBuffer<float>& buffer, float value)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        FloatVectorOperations::fill (buffer.getWritePointer (channel), value, buffer.getNumSamples());
}

std::shared_ptr<Sample> makeRuntimeSample (double frequency = 100.0, int frames = kNumSamples)
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, frames);
    auto* data = sample->data.getWritePointer (0);

    for (int i = 0; i < frames; ++i)
        data[i] = (float) (0.5 * std::sin (6.283185307179586 * frequency * (double) i / kSampleRate));

    return sample;
}

ToneSnapshot makeRuntimeTone()
{
    ToneSnapshot tone;
    tone.wg.toneSwitch = true;
    tone.wg.keyLow = 0;
    tone.wg.keyHigh = 127;
    tone.wg.velLow = 1;
    tone.wg.velHigh = 127;
    tone.wg.pitchKeyfollow = 100.0f;
    tone.tvf.type = 1;
    tone.tvf.cutoff = 127.0f;
    tone.tva.level = 127.0f;

    for (int i = 0; i < 4; ++i)
    {
        tone.pEnv.level[i] = 127.0f;
        tone.tvf.fEnv.level[i] = 127.0f;
    }

    for (int i = 0; i < 3; ++i)
        tone.tva.aEnv.level[i] = 127.0f;

    tone.pan.position = 64.0f;
    tone.output.level = 127.0f;
    tone.lfo[0].rate = 64.0f;
    tone.lfo[1].rate = 64.0f;
    return tone;
}

struct EngineRuntime
{
    EngineRuntime()
    {
        auto zoneSet = std::make_shared<ZoneSet>();
        Zone zone;
        zone.sample = makeRuntimeSample();
        zone.loopMode = LoopMode::Sustain;
        zone.loop.start = 0;
        zone.loop.end = zone.sample->data.getNumSamples();
        zoneSet->zones.push_back (zone);

        runtime = std::make_shared<PatchRuntime>();
        runtime->snapshot.tones[0] = makeRuntimeTone();
        runtime->zoneSets[0] = zoneSet;
        runtime->rawZoneSets[0] = zoneSet.get();
        reclaimer.publish (runtime);
    }

    AssetReclaimer reclaimer;
    std::shared_ptr<PatchRuntime> runtime;
};

class BusTestProcessor final : public AudioProcessor
{
public:
    BusTestProcessor()
        : AudioProcessor (MyJVProcessor::createBuses())
    {
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return MyJVProcessor::isLayoutSupported (layouts);
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const String getName() const override { return "BusTestProcessor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

BusesLayout makeLayout (const AudioChannelSet& main, const AudioChannelSet& out1, const AudioChannelSet& out2)
{
    BusesLayout layout;
    layout.outputBuses.add (main);
    layout.outputBuses.add (out1);
    layout.outputBuses.add (out2);
    return layout;
}

class RecordingSink final : public MidiEventSink
{
public:
    struct Event
    {
        int samplePosition = 0;
        int numBytes = 0;
        juce::uint8 bytes[3] {};
    };

    void handleMidiEvent (const juce::MidiMessageMetadata& event) noexcept override
    {
        if (numEvents >= maxEvents)
            return;

        auto& recorded = events[numEvents++];
        recorded.samplePosition = event.samplePosition;
        recorded.numBytes = juce::jmin (3, event.numBytes);

        for (int i = 0; i < recorded.numBytes; ++i)
            recorded.bytes[i] = event.data[i];
    }

    static constexpr int maxEvents = 16;

    Event events[maxEvents];
    int numEvents = 0;
};
}

TEST_CASE ("plugin exposes three stereo output buses")
{
    const auto buses = MyJVProcessor::createBuses();

    REQUIRE (buses.outputLayouts.size() == kNumOutputBuses);
    REQUIRE (buses.outputLayouts[0].busName == "Main");
    REQUIRE (buses.outputLayouts[1].busName == "Out1");
    REQUIRE (buses.outputLayouts[2].busName == "Out2");

    for (const auto& bus : buses.outputLayouts)
    {
        REQUIRE (bus.defaultLayout == AudioChannelSet::stereo());
        REQUIRE (bus.isActivatedByDefault);
    }
}

TEST_CASE ("bus layout predicate enforces the supported layouts")
{
    REQUIRE (MyJVProcessor::isLayoutSupported (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::stereo(), AudioChannelSet::stereo())));
    REQUIRE (MyJVProcessor::isLayoutSupported (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::disabled(), AudioChannelSet::stereo())));
    REQUIRE (MyJVProcessor::isLayoutSupported (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::disabled(), AudioChannelSet::disabled())));
    REQUIRE_FALSE (MyJVProcessor::isLayoutSupported (makeLayout (AudioChannelSet::disabled(), AudioChannelSet::stereo(), AudioChannelSet::stereo())));
    REQUIRE_FALSE (MyJVProcessor::isLayoutSupported (makeLayout (AudioChannelSet::mono(), AudioChannelSet::stereo(), AudioChannelSet::stereo())));
    REQUIRE_FALSE (MyJVProcessor::isLayoutSupported (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::mono(), AudioChannelSet::stereo())));
}

TEST_CASE ("bus buffer mapping follows the enabled output buses")
{
    BusTestProcessor processor;

    SECTION ("all buses enabled")
    {
        REQUIRE (processor.setBusesLayout (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::stereo(), AudioChannelSet::stereo())));

        AudioBuffer<float> buffer (6, 64);
        buffer.clear();

        const auto buses = MyJVProcessor::buildBusBuffers (processor, buffer);

        REQUIRE (buses.l[0] == buffer.getWritePointer (0));
        REQUIRE (buses.r[0] == buffer.getWritePointer (1));
        REQUIRE (buses.l[1] == buffer.getWritePointer (2));
        REQUIRE (buses.r[1] == buffer.getWritePointer (3));
        REQUIRE (buses.l[2] == buffer.getWritePointer (4));
        REQUIRE (buses.r[2] == buffer.getWritePointer (5));
    }

    SECTION ("Out1 disabled")
    {
        REQUIRE (processor.setBusesLayout (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::disabled(), AudioChannelSet::stereo())));

        AudioBuffer<float> buffer (4, 64);
        buffer.clear();

        const auto buses = MyJVProcessor::buildBusBuffers (processor, buffer);

        REQUIRE (buses.l[0] == buffer.getWritePointer (0));
        REQUIRE (buses.r[0] == buffer.getWritePointer (1));
        REQUIRE (buses.l[1] == nullptr);
        REQUIRE (buses.r[1] == nullptr);
        REQUIRE (buses.l[2] == buffer.getWritePointer (2));
        REQUIRE (buses.r[2] == buffer.getWritePointer (3));
    }

    SECTION ("Out1 and Out2 disabled")
    {
        REQUIRE (processor.setBusesLayout (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::disabled(), AudioChannelSet::disabled())));

        AudioBuffer<float> buffer (2, 64);
        buffer.clear();

        const auto buses = MyJVProcessor::buildBusBuffers (processor, buffer);

        REQUIRE (buses.l[0] == buffer.getWritePointer (0));
        REQUIRE (buses.r[0] == buffer.getWritePointer (1));
        REQUIRE (buses.l[1] == nullptr);
        REQUIRE (buses.r[1] == nullptr);
        REQUIRE (buses.l[2] == nullptr);
        REQUIRE (buses.r[2] == nullptr);
    }
}

TEST_CASE ("engine clears the mapped buses of a disabled-aux layout")
{
    BusTestProcessor processor;
    REQUIRE (processor.setBusesLayout (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::disabled(), AudioChannelSet::stereo())));

    AudioBuffer<float> buffer (4, kNumSamples);
    fillBuffer (buffer, 1.0f);

    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.process (MyJVProcessor::buildBusBuffers (processor, buffer), MidiBuffer(), kNumSamples);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        CHECK (buffer.getMagnitude (channel, 0, kNumSamples) == 0.0f);
}

TEST_CASE ("engine clears every output bus")
{
    BusFixture fixture;

    for (auto& buffer : fixture.buffers)
        fillBuffer (buffer, 1.0f);

    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers(), MidiBuffer(), kNumSamples);

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        INFO ("bus " << bus);
        CHECK (fixture.buffers[bus].getMagnitude (0, 0, kNumSamples) == 0.0f);
        CHECK (fixture.buffers[bus].getMagnitude (1, 0, kNumSamples) == 0.0f);
    }
}

TEST_CASE ("engine tolerates partially mapped buses")
{
    AudioBuffer<float> leftOnly (1, 256);
    AudioBuffer<float> rightOnly (1, 256);
    leftOnly.clear();
    rightOnly.clear();

    BusBuffers buses;
    buses.l[0] = leftOnly.getWritePointer (0);
    buses.r[1] = rightOnly.getWritePointer (0);

    SynthEngine engine;
    engine.prepare (kSampleRate, 256);
    engine.process (buses, MidiBuffer(), 256);

    REQUIRE (leftOnly.getMagnitude (0, 0, 256) == 0.0f);
    REQUIRE (rightOnly.getMagnitude (0, 0, 256) == 0.0f);
}

TEST_CASE ("engine process with zero samples is a no-op")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers(), MidiBuffer(), 0);

    for (const auto& buffer : fixture.buffers)
        REQUIRE (buffer.getMagnitude (0, 0, kNumSamples) == 0.0f);
}

TEST_CASE ("engine dispatches midi events at sample-accurate positions")
{
    BusFixture fixture;
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setMidiEventSink (&sink);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 0);
    midi.addEvent (MidiMessage::noteOn (1, 62, 0.8f), 12345);
    midi.addEvent (MidiMessage::noteOff (1, 60), kNumSamples - 1);

    engine.process (fixture.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (sink.numEvents == 3);
    REQUIRE (sink.events[0].samplePosition == 0);
    REQUIRE (sink.events[1].samplePosition == 12345);
    REQUIRE (sink.events[2].samplePosition == kNumSamples - 1);
    REQUIRE (sink.events[0].bytes[0] == 0x90);
    REQUIRE (sink.events[1].bytes[1] == 62);
    REQUIRE (sink.events[2].bytes[0] == 0x80);
}

TEST_CASE ("engine preserves buffer order for events at the same position")
{
    BusFixture fixture;
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setMidiEventSink (&sink);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::controllerEvent (1, 1, 10), 500);
    midi.addEvent (MidiMessage::controllerEvent (1, 1, 20), 500);
    midi.addEvent (MidiMessage::controllerEvent (1, 1, 30), 500);

    engine.process (fixture.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (sink.numEvents == 3);
    REQUIRE (sink.events[0].samplePosition == 500);
    REQUIRE (sink.events[1].samplePosition == 500);
    REQUIRE (sink.events[2].samplePosition == 500);
    REQUIRE (sink.events[0].bytes[2] == 10);
    REQUIRE (sink.events[1].bytes[2] == 20);
    REQUIRE (sink.events[2].bytes[2] == 30);
}

TEST_CASE ("engine skips midi events outside the block")
{
    BusFixture fixture;
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setMidiEventSink (&sink);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), -1);
    midi.addEvent (MidiMessage::noteOn (1, 61, 0.8f), kNumSamples);
    midi.addEvent (MidiMessage::noteOn (1, 62, 0.8f), kNumSamples + 100);
    midi.addEvent (MidiMessage::noteOn (1, 63, 0.8f), 100);

    engine.process (fixture.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (sink.numEvents == 1);
    REQUIRE (sink.events[0].samplePosition == 100);
    REQUIRE (sink.events[0].bytes[1] == 63);
}

TEST_CASE ("engine handles a sink being cleared")
{
    BusFixture fixture;
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setMidiEventSink (&sink);
    engine.setMidiEventSink (nullptr);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 100);

    engine.process (fixture.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (sink.numEvents == 0);
}

TEST_CASE ("engine dispatches nothing for a zero-length block")
{
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setMidiEventSink (&sink);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 0);

    BusBuffers buses;
    engine.process (buses, midi, 0);

    REQUIRE (sink.numEvents == 0);
}

TEST_CASE ("engine processes events without a configured sink")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 100);

    engine.process (fixture.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (fixture.buffers[0].getMagnitude (0, 0, kNumSamples) == 0.0f);
}

// M2-05 restores the non-silent comparison required by code-review I-4: both
// engines drive the same active runtime; the only difference is that the extra
// controller event forces an additional subblock split, which must not change a
// single sample of the rendered audio.
TEST_CASE ("midi event splitting leaves the rendered audio unchanged")
{
    EngineRuntime activeRuntime;

    BusFixture reference;
    SynthEngine referenceEngine;
    referenceEngine.prepare (kSampleRate, kNumSamples);
    referenceEngine.setParamSnapshotSource (nullptr, &activeRuntime.reclaimer);

    MidiBuffer referenceMidi;
    referenceMidi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 0);
    referenceMidi.addEvent (MidiMessage::noteOff (1, 60), kNumSamples - 1);
    referenceEngine.process (reference.makeBusBuffers(), referenceMidi, kNumSamples);

    BusFixture split;
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setParamSnapshotSource (nullptr, &activeRuntime.reclaimer);
    engine.setMidiEventSink (&sink);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 0);
    midi.addEvent (MidiMessage::controllerEvent (1, 1, 64), 12345);
    midi.addEvent (MidiMessage::noteOff (1, 60), kNumSamples - 1);

    engine.process (split.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (sink.numEvents == 3);
    REQUIRE (reference.buffers[0].getMagnitude (0, 0, kNumSamples) > 0.01f);

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        float maxDifference = 0.0f;

        for (int i = 0; i < kNumSamples; ++i)
            maxDifference = jmax (maxDifference, std::abs (split.buffers[bus].getSample (0, i)
                                                          - reference.buffers[bus].getSample (0, i)));

        REQUIRE (maxDifference <= 1.0e-6f);
    }
}

TEST_CASE ("the engine renders the active runtime's first tone")
{
    EngineRuntime activeRuntime;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setParamSnapshotSource (nullptr, &activeRuntime.reclaimer);

    BusFixture buffers;
    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0);
    engine.process (buffers.makeBusBuffers(), midi, kNumSamples);

    CHECK (buffers.buffers[0].getMagnitude (0, 0, kNumSamples) > 0.01f);
    CHECK (buffers.buffers[1].getMagnitude (0, 0, kNumSamples) == 0.0f);
    CHECK (buffers.buffers[2].getMagnitude (0, 0, kNumSamples) == 0.0f);
}

TEST_CASE ("hot parameter updates keep the sounding voice alive")
{
    EngineRuntime runtimeA;
    EngineRuntime runtimeB;

    SynthEngine engineA;
    SynthEngine engineB;
    engineA.prepare (kSampleRate, 512);
    engineB.prepare (kSampleRate, 512);
    engineA.setParamSnapshotSource (nullptr, &runtimeA.reclaimer);
    engineB.setParamSnapshotSource (nullptr, &runtimeB.reclaimer);

    BusFixture bufferA (512);
    BusFixture bufferB (512);
    MidiBuffer noteOn;
    noteOn.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0);

    engineA.process (bufferA.makeBusBuffers(), noteOn, 512);
    engineB.process (bufferB.makeBusBuffers(), noteOn, 512);

    const MidiBuffer empty;
    float magnitudeA = 0.0f;
    float magnitudeB = 0.0f;

    for (int block = 0; block < 6; ++block)
    {
        for (auto& buffer : bufferA.buffers)
            buffer.clear();

        for (auto& buffer : bufferB.buffers)
            buffer.clear();

        if (block == 3)
            runtimeB.runtime->snapshot.tones[0].tva.level = 64.0f;   // hot update for B only

        engineA.process (bufferA.makeBusBuffers(), empty, 512);
        engineB.process (bufferB.makeBusBuffers(), empty, 512);

        magnitudeA = bufferA.buffers[0].getMagnitude (0, 0, 512);
        magnitudeB = bufferB.buffers[0].getMagnitude (0, 0, 512);
        REQUIRE (magnitudeB > 0.001f);   // the update never drops the voice

        for (int i = 0; i < 512; ++i)
        {
            const auto a = bufferA.buffers[0].getSample (0, i);
            const auto b = bufferB.buffers[0].getSample (0, i);

            if (std::abs (a) > 0.01f)
                REQUIRE ((a > 0.0f) == (b > 0.0f));   // phase is preserved
        }
    }

    CHECK (magnitudeA > magnitudeB);   // the tone level change took effect
}

TEST_CASE ("updateModulators runs exactly once per rendered sample")
{
    EngineRuntime activeRuntime;
    SynthEngine engine;
    engine.prepare (kSampleRate, 512);
    engine.setParamSnapshotSource (nullptr, &activeRuntime.reclaimer);

    BusFixture buffer (512);
    MidiBuffer noteOn;
    noteOn.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0);

    const auto before = engine.modulationUpdateCount();
    engine.process (buffer.makeBusBuffers(), noteOn, 512);
    REQUIRE (engine.modulationUpdateCount() - before == 512);

    // Subblock splits must not change the per-sample cadence.
    for (auto& busBuffer : buffer.buffers)
        busBuffer.clear();

    MidiBuffer splitMidi;
    splitMidi.addEvent (MidiMessage::controllerEvent (1, 1, 64), 123);
    splitMidi.addEvent (MidiMessage::controllerEvent (1, 1, 65), 400);

    const auto beforeSplit = engine.modulationUpdateCount();
    engine.process (buffer.makeBusBuffers(), splitMidi, 512);
    REQUIRE (engine.modulationUpdateCount() - beforeSplit == 512);
}

TEST_CASE ("block splitting clears every segment")
{
    BusFixture fixture;

    for (auto& buffer : fixture.buffers)
        fillBuffer (buffer, 1.0f);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.5f), 100);
    midi.addEvent (MidiMessage::noteOff (1, 60), 20000);
    midi.addEvent (MidiMessage::controllerEvent (1, 1, 64), kNumSamples + 100);

    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.process (fixture.makeBusBuffers(), midi, kNumSamples);

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
        CHECK (fixture.buffers[bus].getMagnitude (0, 0, kNumSamples) == 0.0f);
}
