#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "DSP/Smoother.h"
#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Engine/ToneVoice.h"
#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
using namespace juce;

constexpr double kSampleRate = 48000.0;

ToneSnapshot makeTone()
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
        tone.tva.aEnv.time[i] = 0.0f;

    for (int i = 0; i < 3; ++i)
        tone.tva.aEnv.level[i] = 127.0f;

    tone.pan.position = 64.0f;
    tone.output.level = 127.0f;
    return tone;
}

std::shared_ptr<Sample> makeSample (double frequency = 100.0, int frames = 44100)
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

std::shared_ptr<Sample> makeDcSample (float value = 0.5f, int frames = 44100)
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, frames);
    auto* data = sample->data.getWritePointer (0);

    for (int i = 0; i < frames; ++i)
        data[i] = value;

    return sample;
}

std::shared_ptr<const ZoneSet> makeZoneSet (std::shared_ptr<const Sample> sample)
{
    auto zoneSet = std::make_shared<ZoneSet>();
    Zone zone;
    zone.sample = std::move (sample);
    zone.loopMode = LoopMode::Sustain;
    zone.loop.start = 0;
    zone.loop.end = zone.sample->data.getNumSamples();
    zoneSet->zones.push_back (zone);
    return zoneSet;
}

struct ToneFixture
{
    ToneFixture (const ToneSnapshot& tone, std::shared_ptr<const ZoneSet> zones)
    {
        runtime = std::make_shared<PatchRuntime>();
        runtime->snapshot.tones[0] = tone;
        runtime->zoneSets[0] = std::move (zones);
        runtime->rawZoneSets[0] = runtime->zoneSets[0].get();
    }

    const PatchRuntime* get() const noexcept { return runtime.get(); }

    std::shared_ptr<PatchRuntime> runtime;
};

float maxAdjacentDelta (const std::vector<float>& values)
{
    float maximum = 0.0f;

    for (std::size_t i = 1; i < values.size(); ++i)
        maximum = std::max (maximum, std::abs (values[i] - values[i - 1]));

    return maximum;
}
}

TEST_CASE ("the smoother converges with the configured time constant")
{
    Smoother smoother;
    smoother.prepare (kSampleRate, 10.0);
    smoother.reset (0.0f);
    smoother.setTarget (1.0f);

    const auto coefficient = 1.0 - std::exp (-1.0 / (10.0 * 0.001 * kSampleRate));
    REQUIRE (smoother.process() == Catch::Approx (coefficient).margin (1.0e-5));

    for (int i = 0; i < 480 - 1; ++i)
        smoother.process();

    REQUIRE (smoother.current() == Catch::Approx (1.0 - std::exp (-1.0)).margin (1.0e-3));
    REQUIRE (smoother.current() <= 1.0f);
    REQUIRE (smoother.isSmoothing());

    for (int i = 0; i < 4800; ++i)
        smoother.process();

    REQUIRE (smoother.current() == Catch::Approx (1.0f).margin (1.0e-4));

    for (int i = 0; i < 6000; ++i)
        smoother.process();

    REQUIRE (smoother.current() == 1.0f);   // snapped to the target
    REQUIRE_FALSE (smoother.isSmoothing());
}

TEST_CASE ("reset snaps and the approach stays monotonic")
{
    Smoother smoother;
    smoother.prepare (kSampleRate, 5.0);
    smoother.reset (0.5f);
    REQUIRE (smoother.current() == 0.5f);

    smoother.setTarget (0.0f);
    float previous = smoother.current();

    for (int i = 0; i < 1000; ++i)
    {
        const auto value = smoother.process();
        REQUIRE (value <= previous + 1.0e-7f);
        REQUIRE (value >= 0.0f);
        previous = value;
    }

    smoother.reset (0.2f);
    REQUIRE (smoother.current() == 0.2f);
    REQUIRE_FALSE (smoother.isSmoothing());
}

TEST_CASE ("abrupt level and pan changes stay click free")
{
    auto tone = makeTone();
    ToneFixture fixture (tone, makeZoneSet (makeDcSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    std::vector<float> output;
    output.reserve (20000);

    for (int block = 0; block < 20; ++block)
    {
        if (block == 5)
            fixture.runtime->snapshot.tones[0].tva.level = 0.0f;

        if (block == 10)
            fixture.runtime->snapshot.tones[0].tva.level = 127.0f;

        if (block == 15)
            fixture.runtime->snapshot.tones[0].pan.position = 127.0f;

        voice.beginBlock();

        for (int i = 0; i < 512; ++i)
        {
            voice.updateModulators();
            output.push_back (voice.processTVA (voice.processTVF (voice.processWG())));
        }
    }

    REQUIRE (maxAdjacentDelta (output) < 0.02f);
}

TEST_CASE ("abrupt cutoff and resonance changes stay click free")
{
    auto tone = makeTone();
    ToneFixture fixture (tone, makeZoneSet (makeSample (1000.0)));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    std::vector<float> output;
    output.reserve (20000);
    bool finite = true;

    for (int block = 0; block < 20; ++block)
    {
        if (block == 5)
            fixture.runtime->snapshot.tones[0].tvf.cutoff = 0.0f;

        if (block == 10)
            fixture.runtime->snapshot.tones[0].tvf.cutoff = 127.0f;

        if (block == 15)
            fixture.runtime->snapshot.tones[0].tvf.resonance = 127.0f;

        voice.beginBlock();

        for (int i = 0; i < 512; ++i)
        {
            voice.updateModulators();
            const auto value = voice.processTVA (voice.processTVF (voice.processWG()));
            finite = finite && std::isfinite (value);
            output.push_back (value);
        }
    }

    REQUIRE (finite);
    REQUIRE (maxAdjacentDelta (output) < 0.05f);
}

TEST_CASE ("note-on snaps the parameter smoothers")
{
    auto tone = makeTone();
    tone.output.level = 64.0f;

    ToneFixture fixture (tone, makeZoneSet (makeDcSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
    voice.beginBlock();

    float last = 0.0f;

    for (int i = 0; i < 200; ++i)
    {
        voice.updateModulators();
        last = voice.processTVA (voice.processTVF (voice.processWG()));
    }

    // 0.5 DC x 64/127 x equal-power centre (0.707) without any start ramp.
    REQUIRE (std::abs (last) > 0.15f);
}

TEST_CASE ("the engine keeps hot parameter sweeps click free")
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, 48000);
    auto* data = sample->data.getWritePointer (0);

    for (int i = 0; i < 48000; ++i)
        data[i] = (float) (0.5 * std::sin (6.283185307179586 * 200.0 * (double) i / kSampleRate));

    auto zoneSet = makeZoneSet (sample);

    auto runtime = std::make_shared<PatchRuntime>();
    runtime->snapshot.tones[0] = makeTone();
    runtime->zoneSets[0] = zoneSet;
    runtime->rawZoneSets[0] = zoneSet.get();

    AssetReclaimer reclaimer;
    reclaimer.publish (runtime);

    SynthEngine engine;
    engine.prepare (kSampleRate, 512);
    engine.setParamSnapshotSource (nullptr, &reclaimer);

    AudioBuffer<float> buffer[3];
    BusBuffers buses;

    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        buffer[bus].setSize (2, 512);
        buffer[bus].clear();
        buses.l[bus] = buffer[bus].getWritePointer (0);
        buses.r[bus] = buffer[bus].getWritePointer (1);
    }

    MidiBuffer noteOn;
    noteOn.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0);
    engine.process (buses, noteOn, 512);

    const MidiBuffer empty;
    std::vector<float> output;
    output.reserve (12 * 512);
    bool finite = true;

    for (int block = 0; block < 12; ++block)
    {
        runtime->snapshot.tones[0].tva.level = (block % 2 == 0) ? 127.0f : 32.0f;
        runtime->snapshot.tones[0].tvf.cutoff = (block % 2 == 0) ? 100.0f : 20.0f;

        for (auto& busBuffer : buffer)
            busBuffer.clear();

        engine.process (buses, empty, 512);

        for (int i = 0; i < 512; ++i)
        {
            const auto value = buffer[0].getSample (0, i);
            finite = finite && std::isfinite (value);
            output.push_back (value);
        }
    }

    REQUIRE (finite);
    REQUIRE (output.size() > 1000);
    REQUIRE (maxAdjacentDelta (output) < 0.1f);
}
