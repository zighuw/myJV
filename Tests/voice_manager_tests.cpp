#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Engine/VoiceManager.h"
#include "Model/Sample.h"
#include "Model/ZoneSet.h"
#include "Params/ParamSnapshotCache.h"
#include "Params/ParameterIDs.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace juce;

constexpr double kSampleRate = 48000.0;

ToneSnapshot makeTone (bool toneSwitch = true, float level = 127.0f)
{
    ToneSnapshot tone;
    tone.wg.toneSwitch = toneSwitch;
    tone.wg.waveGain = 1;
    tone.wg.keyLow = 0;
    tone.wg.keyHigh = 127;
    tone.wg.velLow = 1;
    tone.wg.velHigh = 127;
    tone.wg.pitchKeyfollow = 100.0f;
    tone.tvf.type = 1;   // LPF
    tone.tvf.cutoff = 127.0f;
    tone.tva.level = level;

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

std::shared_ptr<Sample> makeSample (int frames = 48000)
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, frames);
    auto* data = sample->data.getWritePointer (0);

    for (int i = 0; i < frames; ++i)
        data[i] = (float) (0.5 * std::sin (6.283185307179586 * 100.0 * (double) i / kSampleRate));

    return sample;
}

std::shared_ptr<const ZoneSet> makeZoneSet (std::shared_ptr<const Sample> sample,
                                            int keyLow = 0, int keyHigh = 127)
{
    auto zoneSet = std::make_shared<ZoneSet>();
    Zone zone;
    zone.sample = std::move (sample);
    zone.keyLow = keyLow;
    zone.keyHigh = keyHigh;
    zone.velLow = 1;
    zone.velHigh = 127;
    zone.loopMode = LoopMode::Sustain;
    zone.loop.start = 0;
    zone.loop.end = zone.sample->data.getNumSamples();
    zoneSet->zones.push_back (zone);
    return zoneSet;
}

// A published PatchRuntime with tone 0 wired to a full-range zone.
struct Runtime
{
    Runtime (const ToneSnapshot& tone = makeTone(),
             std::shared_ptr<const ZoneSet> zones = makeZoneSet (makeSample()))
    {
        runtime = std::make_shared<PatchRuntime>();
        runtime->snapshot.tones[0] = tone;

        if (zones != nullptr)
        {
            runtime->zoneSets[0] = std::move (zones);
            runtime->rawZoneSets[0] = runtime->zoneSets[0].get();
        }

        reclaimer.publish (runtime);
    }

    AssetReclaimer reclaimer;
    std::shared_ptr<PatchRuntime> runtime;
};

struct BusFixture
{
    explicit BusFixture (int samples = 512)
    {
        for (auto& buffer : buffers)
        {
            buffer.setSize (2, samples);
            buffer.clear();
        }
    }

    BusBuffers make()
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

int activeNotesWithMidiNote (const VoiceManager& manager, int midiNote)
{
    int count = 0;

    for (int i = 0; i < kMaxNotes; ++i)
        if (manager.note (i).active && manager.note (i).midiNote == midiNote)
            ++count;

    return count;
}
}

TEST_CASE ("voice manager allocates one voice per enabled tone")
{
    auto runtime = std::make_shared<PatchRuntime>();
    runtime->snapshot.tones[0] = makeTone();
    runtime->snapshot.tones[1] = makeTone();

    auto zones = makeZoneSet (makeSample());
    runtime->zoneSets[0] = zones;
    runtime->rawZoneSets[0] = zones.get();
    runtime->zoneSets[1] = zones;
    runtime->rawZoneSets[1] = zones.get();

    AssetReclaimer reclaimer;
    reclaimer.publish (runtime);

    VoiceManager manager;
    manager.prepare (kSampleRate);

    REQUIRE (manager.startNote (*runtime, 60, 100, 0, 1) == 2);
    REQUIRE (manager.activeNoteCount() == 1);
    REQUIRE (manager.activeVoiceCount() == 2);
    REQUIRE (manager.note (0).voiceCount == 2);
}

TEST_CASE ("voice manager keeps one voice per note and prefers free voices")
{
    Runtime runtime;
    VoiceManager manager;
    manager.prepare (kSampleRate);

    REQUIRE (manager.startNote (*runtime.runtime, 48, 100, 0, 1) == 1);
    REQUIRE (manager.startNote (*runtime.runtime, 55, 100, 1, 2) == 1);
    REQUIRE (manager.startNote (*runtime.runtime, 67, 100, 0, 3) == 1);

    REQUIRE (manager.activeNoteCount() == 3);
    REQUIRE (manager.activeVoiceCount() == 3);
    REQUIRE (manager.hardTakeoverCount() == 0);
    REQUIRE (manager.note (0).runtimeId == runtime.runtime->id);
}

TEST_CASE ("voice manager keeps a note with no matching zone silent and free")
{
    Runtime runtime { makeTone(), makeZoneSet (makeSample(), 0, 10) };   // zone covers 0..10 only
    VoiceManager manager;
    manager.prepare (kSampleRate);

    REQUIRE (manager.startNote (*runtime.runtime, 60, 100, 0, 1) == 0);
    REQUIRE (manager.activeNoteCount() == 0);
    REQUIRE (manager.activeVoiceCount() == 0);
}

TEST_CASE ("voice manager never exceeds the fixed voice pool")
{
    Runtime runtime;
    VoiceManager manager;
    manager.prepare (kSampleRate);

    for (int i = 0; i < 200; ++i)
        manager.startNote (*runtime.runtime, i % 128, 100, 0, (std::uint64_t) i + 1);

    REQUIRE (manager.activeVoiceCount() <= kMaxVoices);
    REQUIRE (manager.activeNoteCount() <= kMaxNotes);
    REQUIRE (manager.hardTakeoverCount() > 0);
}

TEST_CASE ("the LAST policy steals the earliest note")
{
    Runtime runtime;
    VoiceManager manager;
    manager.prepare (kSampleRate);

    for (int i = 0; i < kMaxVoices; ++i)
        manager.startNote (*runtime.runtime, i, 100, 0, (std::uint64_t) i + 1);

    REQUIRE (manager.activeVoiceCount() == kMaxVoices);

    // The pool is full: the next note must replace the earliest one (midiNote 0).
    manager.startNote (*runtime.runtime, 100, 100, 0, 9999);

    CHECK (activeNotesWithMidiNote (manager, 0) == 0);
    CHECK (activeNotesWithMidiNote (manager, 100) == 1);
}

TEST_CASE ("the LOUDEST policy steals the quietest note")
{
    Runtime quiet { makeTone (true, 1.0f) };
    Runtime loud { makeTone (true, 127.0f) };
    loud.runtime->snapshot.common.voicePriority = 1;   // LOUDEST

    VoiceManager manager;
    manager.prepare (kSampleRate);

    manager.startNote (*quiet.runtime, 10, 100, 0, 1);

    for (int i = 0; i < kMaxVoices - 1; ++i)
        manager.startNote (*loud.runtime, 20 + i, 100, 0, (std::uint64_t) i + 2);

    REQUIRE (manager.activeVoiceCount() == kMaxVoices);

    // Sound every voice once, then capture the resulting per-note levels.
    {
        BusFixture bus (1);
        manager.beginBlock();
        manager.render (bus.make(), 0, 1);
        manager.beginBlock();
    }

    manager.startNote (*loud.runtime, 99, 100, 0, 9999);   // full pool: steal the quietest

    CHECK (activeNotesWithMidiNote (manager, 10) == 0);
    CHECK (activeNotesWithMidiNote (manager, 99) == 1);
}

TEST_CASE ("a repeated note-off does not cancel a HOLD countdown")
{
    auto tone = makeTone();
    tone.wg.toneDelayMode = 1;   // HOLD
    tone.wg.toneDelayTime = 64.0f;

    Runtime runtime { tone };
    VoiceManager manager;
    manager.prepare (kSampleRate);

    REQUIRE (manager.startNote (*runtime.runtime, 60, 100, 0, 1) == 1);

    BusFixture bus (64);

    for (int i = 0; i < 4; ++i)
    {
        manager.beginBlock();
        manager.render (bus.make(), 0, 64);
    }

    REQUIRE (manager.activeVoiceCount() == 1);

    // First note-off starts the countdown; the second must be a no-op (R4-M1).
    manager.releaseNote (60, 0);
    manager.releaseNote (60, 0);

    manager.beginBlock();
    manager.render (bus.make(), 0, 8);

    CHECK (manager.activeVoiceCount() == 1);
}

TEST_CASE ("a HOLD note frees itself after the countdown and release")
{
    auto tone = makeTone();
    tone.wg.toneDelayMode = 1;   // HOLD
    tone.wg.toneDelayTime = 64.0f;

    Runtime runtime { tone };
    VoiceManager manager;
    manager.prepare (kSampleRate);

    REQUIRE (manager.startNote (*runtime.runtime, 60, 100, 0, 1) == 1);
    manager.releaseNote (60, 0);

    BusFixture bus (64);

    for (int block = 0; block < 4096 && manager.activeVoiceCount() > 0; ++block)
    {
        manager.beginBlock();
        manager.render (bus.make(), 0, 64);
    }

    CHECK (manager.activeVoiceCount() == 0);
}

TEST_CASE ("oldestRuntimeIdInUse reports the minimum live runtime id")
{
    AssetReclaimer reclaimer;

    const auto publish = [&reclaimer] (const ToneSnapshot& tone)
    {
        auto runtime = std::make_shared<PatchRuntime>();
        runtime->snapshot.tones[0] = tone;
        auto zones = makeZoneSet (makeSample());
        runtime->zoneSets[0] = zones;
        runtime->rawZoneSets[0] = zones.get();
        reclaimer.publish (runtime);
        return runtime;
    };

    auto runtimeA = publish (makeTone());
    auto runtimeB = publish (makeTone());

    REQUIRE (runtimeA->id == 1);
    REQUIRE (runtimeB->id == 2);

    VoiceManager manager;
    manager.prepare (kSampleRate);

    REQUIRE (manager.oldestRuntimeIdInUse() == 0);

    manager.startNote (*runtimeA, 60, 100, 0, 1);
    manager.startNote (*runtimeB, 62, 100, 0, 2);
    REQUIRE (manager.oldestRuntimeIdInUse() == 1);

    manager.releaseNote (60, 0);

    BusFixture bus (256);
    manager.beginBlock();
    manager.render (bus.make(), 0, 256);

    REQUIRE (manager.activeNoteCount() == 1);
    CHECK (manager.oldestRuntimeIdInUse() == 2);

    manager.releaseNote (62, 0);
    manager.beginBlock();
    manager.render (bus.make(), 0, 256);

    CHECK (manager.oldestRuntimeIdInUse() == 0);
}

TEST_CASE ("voice manager renders the sounding note and keeps the per-sample cadence")
{
    Runtime runtime;
    VoiceManager manager;
    manager.prepare (kSampleRate);

    BusFixture bus (256);
    manager.startNote (*runtime.runtime, 60, 100, 0, 1);
    manager.beginBlock();
    manager.render (bus.make(), 0, 256);

    REQUIRE (bus.buffers[0].getMagnitude (0, 0, 256) > 0.01f);
    CHECK (manager.modulationUpdateCount() == 256);
}

// R4-M6: the complete production chain (APVTS -> cache -> snapshot -> engine ->
// bus) with a real runtime, which the M2 engine/smoothing tests skipped.
namespace
{
class ChainProcessor final : public AudioProcessor
{
public:
    ChainProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Main", AudioChannelSet::stereo(), true))
    {
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const String getName() const override { return "ChainProcessor"; }
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
}

TEST_CASE ("the APVTS to bus full chain renders host midi (R4-M6)")
{
    ChainProcessor processor;
    AudioProcessorValueTreeState apvts { processor, nullptr, "PARAMETERS", createParameterLayout() };
    auto cache = ParamSnapshotCache::fromApvts (apvts);

    AssetReclaimer reclaimer;

    auto zoneSet = makeZoneSet (makeSample());
    auto runtime = std::make_shared<PatchRuntime>();
    runtime->zoneSets[0] = zoneSet;
    runtime->rawZoneSets[0] = zoneSet.get();
    cache.refresh (*runtime);   // initial snapshot from the live parameter set
    reclaimer.publish (runtime);

    SynthEngine engine;
    engine.prepare (kSampleRate, 512);
    engine.setParamSnapshotSource (&cache, &reclaimer);

    BusFixture bus (512);
    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, 60, 1.0f), 0);

    engine.process (bus.make(), midi, 512);

    REQUIRE (bus.buffers[0].getMagnitude (0, 0, 512) > 0.01f);
}
