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

        for (int bus = 0; bus < 3; ++bus)
        {
            if (! includeAuxiliaryBuses && bus > 0)
                continue;

            buses.l[bus] = buffers[bus].getWritePointer (0);
            buses.r[bus] = buffers[bus].getWritePointer (1);
        }

        return buses;
    }

    AudioBuffer<float> buffers[3];
};

double measureFrequency (const AudioBuffer<float>& buffer)
{
    const auto* data = buffer.getReadPointer (0);
    const auto numSamples = buffer.getNumSamples();
    int risingCrossings = 0;

    for (int i = 1; i < numSamples; ++i)
        if (data[i - 1] <= 0.0f && data[i] > 0.0f)
            ++risingCrossings;

    return risingCrossings * kSampleRate / numSamples;
}

float expectedRms()
{
    return kTestToneLevel / std::sqrt (2.0f);
}
}

TEST_CASE ("plugin exposes three stereo output buses")
{
    const auto buses = MyJVProcessor::createBuses();

    REQUIRE (buses.outputLayouts.size() == 3);
    REQUIRE (buses.outputLayouts[0].busName == "Main");
    REQUIRE (buses.outputLayouts[1].busName == "Out1");
    REQUIRE (buses.outputLayouts[2].busName == "Out2");

    for (const auto& bus : buses.outputLayouts)
    {
        REQUIRE (bus.defaultLayout == AudioChannelSet::stereo());
        REQUIRE (bus.isActivatedByDefault);
    }
}

TEST_CASE ("engine renders the test tone on each bus")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers(), kNumSamples);

    for (int bus = 0; bus < 3; ++bus)
    {
        INFO ("bus " << bus);

        REQUIRE (measureFrequency (fixture.buffers[bus]) == Catch::Approx (kExpectedFrequencies[bus]).margin (2.0));
        REQUIRE (fixture.buffers[bus].getRMSLevel (0, 0, kNumSamples) == Catch::Approx (expectedRms()).margin (0.002f));
        REQUIRE (fixture.buffers[bus].getMagnitude (0, 0, kNumSamples)
                 == Catch::Approx (fixture.buffers[bus].getMagnitude (1, 0, kNumSamples)).margin (1.0e-6f));
    }
}

TEST_CASE ("engine tolerates missing auxiliary buses")
{
    BusFixture fixture;
    SynthEngine engine;
    engine.prepare (kSampleRate, kNumSamples);

    engine.process (fixture.makeBusBuffers (false), kNumSamples);

    REQUIRE (measureFrequency (fixture.buffers[0]) == Catch::Approx (kExpectedFrequencies[0]).margin (2.0));
    REQUIRE (fixture.buffers[1].getMagnitude (0, 0, kNumSamples) == 0.0f);
    REQUIRE (fixture.buffers[2].getMagnitude (0, 0, kNumSamples) == 0.0f);
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

    for (int bus = 0; bus < 3; ++bus)
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
