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
constexpr double kExpectedFrequencies[3] { 440.0, 660.0, 880.0 };
constexpr float kTestToneLevel = 0.1f;

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

    BusBuffers makeBusBuffers (bool includeAuxiliaryBuses = true)
    {
        BusBuffers buses;

        for (int bus = 0; bus < kNumOutputBuses; ++bus)
        {
            if (! includeAuxiliaryBuses && bus > 0)
                continue;

            buses.l[bus] = buffers[bus].getWritePointer (0);
            buses.r[bus] = buffers[bus].getWritePointer (1);
        }

        return buses;
    }

    AudioBuffer<float> buffers[kNumOutputBuses];
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

double measureFrequency (const AudioBuffer<float>& buffer, double sampleRate, int channel = 0)
{
    const auto* data = buffer.getReadPointer (channel);
    const auto numSamples = buffer.getNumSamples();
    int risingCrossings = 0;

    for (int i = 1; i < numSamples; ++i)
        if (data[i - 1] <= 0.0f && data[i] > 0.0f)
            ++risingCrossings;

    return risingCrossings * sampleRate / numSamples;
}

float expectedRms()
{
    return kTestToneLevel / std::sqrt (2.0f);
}
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

TEST_CASE ("engine renders into the mapped buses of a disabled-aux layout")
{
    BusTestProcessor processor;
    REQUIRE (processor.setBusesLayout (makeLayout (AudioChannelSet::stereo(), AudioChannelSet::disabled(), AudioChannelSet::stereo())));

    AudioBuffer<float> buffer (4, kNumSamples);
    buffer.clear();

    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.process (MyJVProcessor::buildBusBuffers (processor, buffer), kNumSamples);

    REQUIRE (measureFrequency (buffer, kSampleRate, 0) == Catch::Approx (kExpectedFrequencies[0]).margin (2.0));
    REQUIRE (measureFrequency (buffer, kSampleRate, 2) == Catch::Approx (kExpectedFrequencies[2]).margin (2.0));
    REQUIRE (measureFrequency (buffer, kSampleRate, 2) != Catch::Approx (kExpectedFrequencies[1]).margin (2.0));
}

TEST_CASE ("engine renders the test tone on each bus")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers(), kNumSamples);

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        INFO ("bus " << bus);

        REQUIRE (measureFrequency (fixture.buffers[bus], kSampleRate) == Catch::Approx (kExpectedFrequencies[bus]).margin (2.0));
        REQUIRE (fixture.buffers[bus].getRMSLevel (0, 0, kNumSamples) == Catch::Approx (expectedRms()).margin (0.002f));

        for (int i = 0; i < jmin (kNumSamples, 1024); ++i)
            REQUIRE (fixture.buffers[bus].getSample (0, i) == fixture.buffers[bus].getSample (1, i));
    }
}

TEST_CASE ("test tone frequency is sample-rate independent")
{
    constexpr double kAlternateSampleRate = 44100.0;

    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kAlternateSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers(), kNumSamples);

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
        REQUIRE (measureFrequency (fixture.buffers[bus], kAlternateSampleRate) == Catch::Approx (kExpectedFrequencies[bus]).margin (2.0));
}

TEST_CASE ("engine tolerates missing auxiliary buses")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers (false), kNumSamples);

    REQUIRE (measureFrequency (fixture.buffers[0], kSampleRate) == Catch::Approx (kExpectedFrequencies[0]).margin (2.0));
    REQUIRE (fixture.buffers[1].getMagnitude (0, 0, kNumSamples) == 0.0f);
    REQUIRE (fixture.buffers[2].getMagnitude (0, 0, kNumSamples) == 0.0f);
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
    engine.process (buses, 256);

    REQUIRE (leftOnly.getMagnitude (0, 0, 256) == 0.0f);
    REQUIRE (rightOnly.getMagnitude (0, 0, 256) == 0.0f);
}

TEST_CASE ("engine process with zero samples is a no-op")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers(), 0);

    for (const auto& buffer : fixture.buffers)
        REQUIRE (buffer.getMagnitude (0, 0, kNumSamples) == 0.0f);
}

TEST_CASE ("test tone phase is continuous across blocks")
{
    constexpr int kHalfSamples = kNumSamples / 2;

    BusFixture reference;
    SynthEngine referenceEngine;
    referenceEngine.prepare (kSampleRate, kNumSamples);
    referenceEngine.process (reference.makeBusBuffers(), kNumSamples);

    AudioBuffer<float> firstHalf (2, kHalfSamples);
    AudioBuffer<float> secondHalf (2, kHalfSamples);

    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    BusBuffers buses;
    buses.l[0] = firstHalf.getWritePointer (0);
    buses.r[0] = firstHalf.getWritePointer (1);
    engine.process (buses, kHalfSamples);

    buses.l[0] = secondHalf.getWritePointer (0);
    buses.r[0] = secondHalf.getWritePointer (1);
    engine.process (buses, kHalfSamples);

    for (int i = 0; i < kHalfSamples; ++i)
    {
        REQUIRE (firstHalf.getSample (0, i) == Catch::Approx (reference.buffers[0].getSample (0, i)).margin (1.0e-6f));
        REQUIRE (secondHalf.getSample (0, i) == Catch::Approx (reference.buffers[0].getSample (0, kHalfSamples + i)).margin (1.0e-6f));
    }
}

TEST_CASE ("render test tone wavs", "[.wav]")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);
    engine.process (fixture.makeBusBuffers(), kNumSamples);

    const char* const names[3] { "main", "out1", "out2" };
    WavAudioFormat format;

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        const auto path = File (String (MYJV_REPO_DIR) + "/REVIEWS/M0-04/test-tone-" + names[bus] + ".wav");

        auto fileStream = path.createOutputStream();
        REQUIRE (fileStream != nullptr);
        REQUIRE (fileStream->openedOk());

        std::unique_ptr<OutputStream> stream = std::move (fileStream);

        auto writer = format.createWriterFor (stream, AudioFormatWriterOptions()
                                                          .withSampleRate (kSampleRate)
                                                          .withNumChannels (2)
                                                          .withBitsPerSample (16));
        REQUIRE (writer != nullptr);

        REQUIRE (writer->writeFromAudioSampleBuffer (fixture.buffers[bus], 0, kNumSamples / 2));
    }
}
