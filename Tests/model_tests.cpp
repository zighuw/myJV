#include <catch2/catch_test_macros.hpp>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Model/Sample.h"
#include "Model/ZoneSet.h"

#include <memory>
#include <type_traits>

namespace
{
using namespace juce;

std::shared_ptr<const Sample> makeSample (int rootKey = 60, double sourceSampleRate = 48000.0)
{
    auto sample = std::make_shared<Sample>();
    sample->rootKey = rootKey;
    sample->sourceSampleRate = sourceSampleRate;
    return sample;
}
}

TEST_CASE ("sample defaults match the model specification")
{
    const Sample sample;

    CHECK (sample.data.getNumChannels() == 0);
    CHECK (sample.data.getNumSamples() == 0);
    CHECK (sample.sourceSampleRate == 48000.0);
    CHECK (sample.rootKey == 60);
    CHECK (sample.embeddedLoop.start == 0);
    CHECK (sample.embeddedLoop.end == 0);
    CHECK (sample.embeddedLoop.crossfadeSamples == 0);
    CHECK (sample.name.empty());
    CHECK (sample.fileHash.empty());
}

TEST_CASE ("sample stores mono and stereo audio data")
{
    Sample mono;
    mono.data.setSize (1, 4);
    mono.data.setSample (0, 0, 0.25f);

    CHECK (mono.data.getNumChannels() == 1);
    CHECK (mono.data.getNumSamples() == 4);
    CHECK (mono.data.getSample (0, 0) == 0.25f);

    Sample stereo;
    stereo.data.setSize (2, 8);
    stereo.data.clear();
    stereo.data.setSample (1, 7, -0.5f);

    CHECK (stereo.data.getNumChannels() == 2);
    CHECK (stereo.data.getSample (0, 7) == 0.0f);
    CHECK (stereo.data.getSample (1, 7) == -0.5f);
}

TEST_CASE ("sample keeps the decoded loop region")
{
    Sample sample;
    sample.embeddedLoop.start = 120;
    sample.embeddedLoop.end = 480;
    sample.embeddedLoop.crossfadeSamples = 32;

    CHECK (sample.embeddedLoop.start == 120);
    CHECK (sample.embeddedLoop.end == 480);
    CHECK (sample.embeddedLoop.crossfadeSamples == 32);
}

TEST_CASE ("zone defaults match the model specification")
{
    const Zone zone;

    CHECK (zone.sample == nullptr);
    CHECK (zone.keyLow == 0);
    CHECK (zone.keyHigh == 127);
    CHECK (zone.velLow == 1);
    CHECK (zone.velHigh == 127);
    CHECK (zone.rootKeyOverride == -1);
    CHECK (zone.coarseTune == 0.0f);
    CHECK (zone.fineTune == 0.0f);
    CHECK (zone.gainDb == 0.0f);
    CHECK (zone.pan == 64);
    CHECK (zone.reverse == false);
    CHECK (zone.loopMode == LoopMode::Sustain);
    CHECK (zone.loop.start == 0);
    CHECK (zone.loop.end == 0);
    CHECK (zone.loop.crossfadeSamples == 0);
}

TEST_CASE ("zone stores key, velocity and loop boundaries verbatim")
{
    Zone zone;
    zone.keyLow = 127;
    zone.keyHigh = 0;
    zone.velLow = 0;
    zone.velHigh = 127;
    zone.rootKeyOverride = 127;
    zone.loop.start = 10;
    zone.loop.end = 5;

    CHECK (zone.keyLow == 127);
    CHECK (zone.keyHigh == 0);
    CHECK (zone.velLow == 0);
    CHECK (zone.velHigh == 127);
    CHECK (zone.rootKeyOverride == 127);
    CHECK (zone.loop.start == 10);
    CHECK (zone.loop.end == 5);
}

TEST_CASE ("zone loop mode round-trips every mode")
{
    Zone zone;

    for (auto mode : { LoopMode::Off, LoopMode::Forward, LoopMode::Sustain })
    {
        zone.loopMode = mode;
        CHECK (zone.loopMode == mode);
    }
}

TEST_CASE ("model types stay aggregates and loop mode values stay stable")
{
    static_assert (std::is_aggregate_v<Sample>);
    static_assert (std::is_aggregate_v<Zone>);
    static_assert (std::is_aggregate_v<ZoneSet>);
    static_assert (static_cast<int> (LoopMode::Off) == 0);
    static_assert (static_cast<int> (LoopMode::Forward) == 1);
    static_assert (static_cast<int> (LoopMode::Sustain) == 2);

    CHECK (std::is_aggregate_v<Sample>);
    CHECK (std::is_aggregate_v<Zone>);
    CHECK (std::is_aggregate_v<ZoneSet>);
    CHECK (static_cast<int> (LoopMode::Off) == 0);
    CHECK (static_cast<int> (LoopMode::Forward) == 1);
    CHECK (static_cast<int> (LoopMode::Sustain) == 2);
}

TEST_CASE ("zone sample references are shared pointers to const samples")
{
    using MutableRef = std::shared_ptr<Sample>;
    using ConstRef = std::shared_ptr<const Sample>;

    static_assert (std::is_convertible_v<MutableRef, ConstRef>);
    static_assert (! std::is_convertible_v<ConstRef, MutableRef>);

    const ConstRef ref = makeSample();
    REQUIRE (ref != nullptr);
    CHECK (std::is_const_v<std::remove_reference_t<decltype (*ref)>>);
    CHECK (ref->rootKey == 60);
}

TEST_CASE ("zones share a sample through shared_ptr ownership")
{
    auto sample = makeSample (36, 44100.0);

    Zone low;
    low.sample = sample;
    Zone high;
    high.sample = sample;

    CHECK (low.sample.get() == high.sample.get());
    CHECK (sample.use_count() == 3);

    sample.reset();

    REQUIRE (low.sample != nullptr);
    CHECK (low.sample->rootKey == 36);
    CHECK (low.sample->sourceSampleRate == 44100.0);
    CHECK (low.sample.use_count() == 2);
}

TEST_CASE ("zone set defaults to an empty named collection")
{
    const ZoneSet set;

    CHECK (set.name.empty());
    CHECK (set.zones.empty());
}

TEST_CASE ("zone set keeps zones in insertion order")
{
    ZoneSet set;
    set.name = "Piano";

    for (int i = 0; i < 3; ++i)
    {
        Zone zone;
        zone.keyLow = i * 10;
        zone.keyHigh = i * 10 + 9;
        set.zones.push_back (zone);
    }

    REQUIRE (set.zones.size() == 3);
    CHECK (set.name == "Piano");
    CHECK (set.zones[0].keyLow == 0);
    CHECK (set.zones[1].keyLow == 10);
    CHECK (set.zones[2].keyLow == 20);
    CHECK (set.zones[2].keyHigh == 29);
}

TEST_CASE ("zone set keeps referenced samples alive after the source zone is destroyed")
{
    ZoneSet set;

    {
        Zone zone;
        zone.sample = makeSample (48);
        set.zones.push_back (zone);
    }

    REQUIRE (set.zones.size() == 1);
    REQUIRE (set.zones.front().sample != nullptr);
    CHECK (set.zones.front().sample->rootKey == 48);
    CHECK (set.zones.front().sample.use_count() == 1);
}

TEST_CASE ("multiple zones can reference the same sample")
{
    auto sample = makeSample (60);

    ZoneSet set;

    for (int i = 0; i < 2; ++i)
    {
        Zone zone;
        zone.sample = sample;
        zone.keyLow = i * 64;
        zone.keyHigh = i * 64 + 63;
        set.zones.push_back (zone);
    }

    REQUIRE (set.zones.size() == 2);
    CHECK (set.zones[0].sample.get() == set.zones[1].sample.get());
    CHECK (set.zones[0].sample.use_count() == 3);
}
