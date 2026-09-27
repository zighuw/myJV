#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

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
    BusFixture()
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize (2, kNumSamples);
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

TEST_CASE ("midi events do not alter the rendered audio")
{
    BusFixture reference;
    SynthEngine referenceEngine;
    referenceEngine.prepare (kSampleRate, kNumSamples);
    referenceEngine.process (reference.makeBusBuffers(), MidiBuffer(), kNumSamples);

    BusFixture split;
    RecordingSink sink;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.setMidiEventSink (&sink);

    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 0.8f), 0);
    midi.addEvent (MidiMessage::noteOn (1, 62, 0.8f), 12345);
    midi.addEvent (MidiMessage::noteOff (1, 60), kNumSamples - 1);

    engine.process (split.makeBusBuffers(), midi, kNumSamples);

    REQUIRE (sink.numEvents == 3);

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        float maxDifference = 0.0f;

        for (int i = 0; i < kNumSamples; ++i)
            maxDifference = jmax (maxDifference, std::abs (split.buffers[bus].getSample (0, i)
                                                          - reference.buffers[bus].getSample (0, i)));

        REQUIRE (maxDifference == 0.0f);
    }
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
