#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Engine/ToneVoice.h"
#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace
{
using namespace juce;

constexpr double kSampleRate = 48000.0;
constexpr int kFrames = 4800;

ToneSnapshot makeDefaultTone (float cutoffParam = 127.0f, float resonance = 0.0f)
{
    ToneSnapshot tone;
    tone.wg.toneSwitch = true;
    tone.wg.waveGain = 1;
    tone.wg.keyLow = 0;
    tone.wg.keyHigh = 127;
    tone.wg.velLow = 1;
    tone.wg.velHigh = 127;
    tone.wg.pitchKeyfollow = 100.0f;
    tone.tvf.type = 1;   // LPF
    tone.tvf.cutoff = cutoffParam;
    tone.tvf.resonance = resonance;
    tone.tva.level = 127.0f;

    for (int i = 0; i < 4; ++i)
    {
        tone.tva.aEnv.time[i] = 0.0f;
        tone.pEnv.time[i] = 0.0f;
        tone.pEnv.level[i] = 127.0f;
        tone.tvf.fEnv.time[i] = 0.0f;
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

std::shared_ptr<Sample> makeSineSample (double frequency = 100.0, int frames = kFrames)
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

std::shared_ptr<Sample> makeDcSample (float value = 0.5f, int frames = kFrames)
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

std::shared_ptr<const ZoneSet> makeZoneSet (std::shared_ptr<const Sample> sample,
                                            LoopMode loopMode = LoopMode::Off)
{
    auto zoneSet = std::make_shared<ZoneSet>();
    Zone zone;
    zone.sample = std::move (sample);
    zone.loopMode = loopMode;
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

        if (zones != nullptr)
        {
            runtime->zoneSets[0] = std::move (zones);
            runtime->rawZoneSets[0] = runtime->zoneSets[0].get();
        }
    }

    const PatchRuntime* get() const noexcept { return runtime.get(); }

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

std::vector<float> renderVoice (ToneVoice& voice, int samples)
{
    std::vector<float> out;
    out.reserve ((std::size_t) samples);

    for (int i = 0; i < samples; ++i)
    {
        if (i % 16 == 0)
            voice.beginBlock();

        voice.updateModulators();
        out.push_back (voice.processTVA (voice.processTVF (voice.processWG())));
    }

    return out;
}

int samplesUntilFinished (ToneVoice& voice, int limit = 200000)
{
    int count = 0;

    while (! voice.finished() && count < limit)
    {
        if (count % 16 == 0)
            voice.beginBlock();

        voice.updateModulators();
        (void) voice.processTVA (voice.processTVF (voice.processWG()));
        ++count;
    }

    return count;
}

float rms (const std::vector<float>& values)
{
    double sum = 0.0;

    for (const auto value : values)
        sum += (double) value * (double) value;

    return values.empty() ? 0.0f : (float) std::sqrt (sum / (double) values.size());
}

int samplesUntilFinishedForNote (const ToneSnapshot& tone, std::shared_ptr<const Sample> sample, int note)
{
    ToneFixture fixture (tone, makeZoneSet (std::move (sample)));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, note, 1.0f, 1);
    return samplesUntilFinished (voice);
}
}

TEST_CASE ("the tone voice renders a one-shot sample and frees itself")
{
    ToneFixture fixture (makeDefaultTone(), makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    REQUIRE (voice.state() == ToneVoice::State::Active);

    const auto rendered = renderVoice (voice, 2000);
    CHECK (rms (rendered) > 0.05f);

    const auto remaining = samplesUntilFinished (voice);
    CHECK (voice.state() == ToneVoice::State::Free);
    CHECK (2000 + remaining <= kFrames + 200);
}

TEST_CASE ("an octave up halves the playback time")
{
    const auto tone = makeDefaultTone();
    const auto atSixty = samplesUntilFinishedForNote (tone, makeSineSample(), 60);
    const auto atSeventyTwo = samplesUntilFinishedForNote (tone, makeSineSample(), 72);

    REQUIRE (atSeventyTwo == Catch::Approx (atSixty / 2.0).epsilon (0.03));
}

TEST_CASE ("envelope and lfo timing follow the engine sample rate")
{
    constexpr double sampleRate = 44100.0;
    auto sample = makeSineSample (100.0, 44100);
    sample->sourceSampleRate = sampleRate;

    const auto mappedMs = [] (float time)
    {
        return Calibration::kEnvTimeMinMs
               * std::pow (Calibration::kEnvTimeMaxMs / Calibration::kEnvTimeMinMs, (double) time / 127.0);
    };

    // A-ENV release T4 = 64 maps to ~147 ms at the engine rate.
    auto tone = makeDefaultTone();
    tone.tva.aEnv.time[3] = 64.0f;

    ToneFixture fixture (tone, makeZoneSet (sample, LoopMode::Sustain));
    ToneVoice voice;
    voice.prepare (sampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    for (int i = 0; i < 100; ++i)
    {
        voice.updateModulators();
        (void) voice.processTVA (voice.processTVF (voice.processWG()));
    }

    voice.release();

    int releaseSamples = 0;

    while (! voice.finished() && releaseSamples < 200000)
    {
        if (releaseSamples % 16 == 0)
            voice.beginBlock();

        voice.updateModulators();
        (void) voice.processTVA (voice.processTVF (voice.processWG()));
        ++releaseSamples;
    }

    const auto expectedRelease = (int) std::lround (mappedMs (64.0f) * sampleRate / 1000.0);
    REQUIRE (releaseSamples == Catch::Approx (expectedRelease).epsilon (0.02));

    // LFO delay T = 64 starts the vibrato after the mapped time.
    auto lfoTone = makeDefaultTone();
    lfoTone.wg.pitchLfo1Depth = 127.0f;
    lfoTone.lfo[0].rate = 127.0f;
    lfoTone.lfo[0].delayTime = 64.0f;

    ToneFixture lfoFixture (lfoTone, makeZoneSet (sample));
    ToneVoice lfoVoice;
    lfoVoice.prepare (sampleRate);
    lfoVoice.startNote (lfoFixture.get(), 0, 60, 1.0f, 1);

    int delaySamples = 0;

    while (delaySamples < 200000
           && std::abs (lfoVoice.currentPitchOffsetSemitones()) < 0.05f)
    {
        if (delaySamples % 16 == 0)
            lfoVoice.beginBlock();

        lfoVoice.updateModulators();
        ++delaySamples;
    }

    const auto expectedDelay = (int) std::lround (Calibration::kLfoDelayMaxMs
                                                  * std::pow (64.0 / 127.0, 2.0)
                                                  * sampleRate / 1000.0);
    REQUIRE (delaySamples == Catch::Approx (expectedDelay).epsilon (0.03));
}

TEST_CASE ("p-env depth bends the pitch from note-on")
{
    auto tone = makeDefaultTone();
    tone.pEnv.depth = 63;   // +12 semitones at full envelope

    ToneFixture fixture (tone, makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
    voice.beginBlock();

    float maximum = 0.0f;

    for (int i = 0; i < 2000; ++i)
    {
        voice.updateModulators();
        maximum = std::max (maximum, voice.currentPitchOffsetSemitones());
    }

    REQUIRE (maximum > 5.0f);

    // The bend must reach the audio, not only the diagnostic sum.
    auto flat = makeDefaultTone();
    ToneFixture flatFixture (flat, makeZoneSet (makeSineSample (100.0, 24000)));
    ToneVoice flatVoice;
    flatVoice.prepare (kSampleRate);
    flatVoice.startNote (flatFixture.get(), 0, 60, 1.0f, 1);

    auto bent = makeDefaultTone();
    bent.pEnv.depth = 63;
    ToneFixture bentFixture (bent, makeZoneSet (makeSineSample (100.0, 24000)));
    ToneVoice bentVoice;
    bentVoice.prepare (kSampleRate);
    bentVoice.startNote (bentFixture.get(), 0, 60, 1.0f, 1);

    REQUIRE (renderVoice (bentVoice, 1000) != renderVoice (flatVoice, 1000));
}

TEST_CASE ("pitch lfo depth vibrates the pitch")
{
    auto tone = makeDefaultTone();
    tone.wg.pitchLfo1Depth = 127.0f;
    tone.lfo[0].rate = 127.0f;   // 20 Hz

    ToneFixture fixture (tone, makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
    voice.beginBlock();

    float minimum = 100.0f;
    float maximum = -100.0f;

    for (int i = 0; i < 4800; ++i)
    {
        voice.updateModulators();
        minimum = std::min (minimum, voice.currentPitchOffsetSemitones());
        maximum = std::max (maximum, voice.currentPitchOffsetSemitones());
    }

    REQUIRE (maximum > 0.5f);
    REQUIRE (minimum < -0.5f);

    // Vibrato must reach the audio.
    auto flat = makeDefaultTone();
    ToneFixture flatFixture (flat, makeZoneSet (makeSineSample (100.0, 24000)));
    ToneVoice flatVoice;
    flatVoice.prepare (kSampleRate);
    flatVoice.startNote (flatFixture.get(), 0, 60, 1.0f, 1);

    auto vibrato = makeDefaultTone();
    vibrato.wg.pitchLfo1Depth = 127.0f;
    vibrato.lfo[0].rate = 127.0f;
    ToneFixture vibratoFixture (vibrato, makeZoneSet (makeSineSample (100.0, 24000)));
    ToneVoice vibratoVoice;
    vibratoVoice.prepare (kSampleRate);
    vibratoVoice.startNote (vibratoFixture.get(), 0, 60, 1.0f, 1);

    REQUIRE (renderVoice (vibratoVoice, 1000) != renderVoice (flatVoice, 1000));
}

TEST_CASE ("release lets the voice finish while the note is held")
{
    auto tone = makeDefaultTone();

    ToneFixture fixture (tone, makeZoneSet (makeSineSample (100.0, 24000), LoopMode::Sustain));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    renderVoice (voice, 1000);
    REQUIRE (voice.state() == ToneVoice::State::Active);

    voice.release();
    REQUIRE (voice.state() == ToneVoice::State::Releasing);

    const auto remaining = samplesUntilFinished (voice, 20000);
    CHECK (voice.state() == ToneVoice::State::Free);
    CHECK (remaining < 500);   // the 1 ms A-ENV release has to stop the loop

    ToneFixture heldFixture (tone, makeZoneSet (makeSineSample (100.0, 24000), LoopMode::Sustain));
    ToneVoice heldVoice;
    heldVoice.prepare (kSampleRate);
    heldVoice.startNote (heldFixture.get(), 0, 60, 1.0f, 1);
    renderVoice (heldVoice, 5000);
    CHECK (heldVoice.state() == ToneVoice::State::Active);   // without release the loop keeps sounding
}

TEST_CASE ("random pitch is deterministic per seed")
{
    auto tone = makeDefaultTone();
    tone.wg.randomPitch = 127.0f;

    const auto render = [&tone] (std::uint64_t seed)
    {
        ToneFixture fixture (tone, makeZoneSet (makeSineSample()));
        ToneVoice voice;
        voice.prepare (kSampleRate);
        voice.startNote (fixture.get(), 0, 60, 1.0f, seed);
        return renderVoice (voice, 1000);
    };

    const auto first = render (42);
    const auto same = render (42);
    const auto other = render (43);

    REQUIRE (first == same);
    REQUIRE (first != other);
}

TEST_CASE ("the filter types route the signal")
{
    const auto renderType = [] (int type, std::shared_ptr<const Sample> sample, float cutoff, float resonance)
    {
        auto tone = makeDefaultTone (cutoff, resonance);
        tone.tvf.type = type;

        ToneFixture fixture (tone, makeZoneSet (std::move (sample)));
        ToneVoice voice;
        voice.prepare (kSampleRate);
        voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
        return rms (renderVoice (voice, 4000));
    };

    const auto highSample = [] { return makeSineSample (6000.0); };
    const auto flat = renderType (0, highSample(), 0.0f, 0.0f);
    const auto lowpass = renderType (1, highSample(), 0.0f, 0.0f);
    const auto highpass = renderType (3, highSample(), 0.0f, 0.0f);

    REQUIRE (lowpass < flat * 0.2f);
    REQUIRE (highpass > flat * 0.5f);

    // BPF centred on the sample frequency (cutoff 105 maps to ~6 kHz, Q = 0.5).
    const auto bandpass = renderType (2, makeSineSample (6000.0), 105.0f, 0.0f);
    REQUIRE (bandpass > flat * 0.3f);
    REQUIRE (bandpass < flat * 0.7f);

    // PKG boosts its centre frequency.
    const auto pkg = renderType (4, makeSineSample (1000.0), 72.0f, 127.0f);
    const auto pkgFlat = renderType (0, makeSineSample (1000.0), 72.0f, 127.0f);
    REQUIRE (pkg > pkgFlat * 2.5f);
}

TEST_CASE ("cutoff tracks keyfollow and the filter envelope")
{
    auto tone = makeDefaultTone (64.0f);
    tone.tvf.cutoffKeyfollow = 100.0f;

    const auto cutoffAt = [&tone] (int note)
    {
        ToneFixture fixture (tone, makeZoneSet (makeSineSample()));
        ToneVoice voice;
        voice.prepare (kSampleRate);
        voice.startNote (fixture.get(), 0, note, 1.0f, 1);
        voice.beginBlock();
        voice.updateModulators();
        return voice.currentCutoffHz();
    };

    REQUIRE (cutoffAt (72) == Catch::Approx (2.0f * cutoffAt (60)).epsilon (0.02));

    auto enveloped = makeDefaultTone (64.0f);
    enveloped.tvf.fEnv.depth = 63;
    enveloped.tvf.fEnv.time[0] = 64.0f;   // ~147 ms attack

    ToneFixture fixture (enveloped, makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    float minimum = 1.0e9f;
    float maximum = 0.0f;

    for (int i = 0; i < 10000; ++i)
    {
        if (i % 16 == 0)
            voice.beginBlock();

        voice.updateModulators();
        minimum = std::min (minimum, voice.currentCutoffHz());
        maximum = std::max (maximum, voice.currentCutoffHz());
    }

    CHECK (maximum > minimum * 8.0f);
}

TEST_CASE ("lfo depth modulates the filter cutoff")
{
    auto tone = makeDefaultTone (64.0f);
    tone.tvf.lfo1Depth = 127.0f;
    tone.lfo[0].rate = 127.0f;   // 20 Hz

    ToneFixture fixture (tone, makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

    float minimum = 1.0e9f;
    float maximum = 0.0f;
    bool finite = true;

    for (int i = 0; i < 4800; ++i)
    {
        if (i % 16 == 0)
            voice.beginBlock();

        voice.updateModulators();
        finite = finite && std::isfinite (voice.currentCutoffHz());
        minimum = std::min (minimum, voice.currentCutoffHz());
        maximum = std::max (maximum, voice.currentCutoffHz());
    }

    CHECK (finite);
    REQUIRE (maximum > minimum * 4.0f);
}

TEST_CASE ("tone level hot updates scale without resetting the phase")
{
    const auto tone = makeDefaultTone();

    const auto render = [&tone] (bool changeLevel)
    {
        ToneFixture fixture (tone, makeZoneSet (makeSineSample (100.0, 24000)));
        ToneVoice voice;
        voice.prepare (kSampleRate);
        voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

        std::vector<float> out;

        for (int i = 0; i < 2000; ++i)
        {
            if (i % 16 == 0)
                voice.beginBlock();

            if (changeLevel && i == 1000)
                fixture.runtime->snapshot.tones[0].tva.level = 64.0f;

            voice.updateModulators();
            out.push_back (voice.processTVA (voice.processTVF (voice.processWG())));
        }

        return out;
    };

    const auto reference = render (false);
    const auto changed = render (true);

    for (int i = 200; i < 900; ++i)
        REQUIRE (changed[(std::size_t) i] == Catch::Approx (reference[(std::size_t) i]).margin (1.0e-6));

    for (int i = 1100; i < 1900; ++i)
    {
        if (std::abs (reference[(std::size_t) i]) > 0.05f)
        {
            REQUIRE (changed[(std::size_t) i] / reference[(std::size_t) i]
                     == Catch::Approx (64.0f / 127.0f).margin (0.02));
            REQUIRE ((changed[(std::size_t) i] > 0.0f) == (reference[(std::size_t) i] > 0.0f));
        }
    }
}

TEST_CASE ("pan and output assignment place the voice on the buses")
{
    const auto renderBuses = [] (float panPosition, int assign)
    {
        auto tone = makeDefaultTone();
        tone.pan.position = panPosition;
        tone.output.assign = assign;

        ToneFixture fixture (tone, makeZoneSet (makeDcSample()));
        ToneVoice voice;
        voice.prepare (kSampleRate);
        voice.startNote (fixture.get(), 0, 60, 1.0f, 1);

        BusFixture buffer (512);
        const auto buses = buffer.make();

        for (int i = 0; i < 512; ++i)
        {
            if (i % 16 == 0)
                voice.beginBlock();

            BusBuffers cursor;

            for (int bus = 0; bus < kNumOutputBuses; ++bus)
            {
                cursor.l[bus] = buses.l[bus] != nullptr ? buses.l[bus] + i : nullptr;
                cursor.r[bus] = buses.r[bus] != nullptr ? buses.r[bus] + i : nullptr;
            }

            voice.updateModulators();
            voice.addToBus (voice.processTVA (voice.processTVF (voice.processWG())), cursor);
        }

        return buffer;
    };

    const auto centre = renderBuses (64.0f, 0);
    REQUIRE (centre.buffers[0].getMagnitude (0, 0, 512) > 0.2f);
    REQUIRE (centre.buffers[0].getMagnitude (1, 0, 512) > 0.2f);

    const auto left = renderBuses (0.0f, 0);
    REQUIRE (left.buffers[0].getMagnitude (0, 0, 512) > 0.2f);
    REQUIRE (left.buffers[0].getMagnitude (1, 0, 512) < 0.02f);

    const auto right = renderBuses (127.0f, 0);
    REQUIRE (right.buffers[0].getMagnitude (0, 0, 512) < 0.02f);
    REQUIRE (right.buffers[0].getMagnitude (1, 0, 512) > 0.2f);

    const auto out1 = renderBuses (64.0f, 1);
    REQUIRE (out1.buffers[0].getMagnitude (0, 0, 512) == 0.0f);
    REQUIRE (out1.buffers[1].getMagnitude (0, 0, 512) > 0.2f);
    REQUIRE (out1.buffers[2].getMagnitude (0, 0, 512) == 0.0f);

    const auto out2 = renderBuses (64.0f, 2);
    REQUIRE (out2.buffers[0].getMagnitude (0, 0, 512) == 0.0f);
    REQUIRE (out2.buffers[1].getMagnitude (0, 0, 512) == 0.0f);
    REQUIRE (out2.buffers[2].getMagnitude (0, 0, 512) > 0.2f);
}

TEST_CASE ("kill fades out quickly and frees the voice")
{
    ToneFixture fixture (makeDefaultTone(), makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
    renderVoice (voice, 100);

    voice.kill();
    REQUIRE (voice.state() == ToneVoice::State::KillFading);

    int count = 0;

    while (! voice.finished() && count < 100000)
    {
        voice.updateModulators();
        (void) voice.processTVA (voice.processTVF (voice.processWG()));
        ++count;
    }

    CHECK (voice.state() == ToneVoice::State::Free);
    CHECK (count <= (int) std::ceil (Calibration::kKillFadeMs * kSampleRate / 1000.0) + 2);
    CHECK (voice.currentGain() == 0.0f);
}

TEST_CASE ("no zone or a tone switch off stays silent and free")
{
    ToneVoice voice;
    voice.prepare (kSampleRate);

    auto noZones = std::make_shared<PatchRuntime>();
    noZones->snapshot.tones[0] = makeDefaultTone();
    voice.startNote (noZones.get(), 0, 60, 1.0f, 1);
    CHECK (voice.finished());
    CHECK (voice.processWG() == 0.0f);

    auto switchedOff = makeDefaultTone();
    switchedOff.wg.toneSwitch = false;
    ToneFixture offFixture (switchedOff, makeZoneSet (makeSineSample()));
    voice.startNote (offFixture.get(), 0, 60, 1.0f, 1);
    CHECK (voice.finished());

    auto emptyZoneSet = std::make_shared<ZoneSet>();
    emptyZoneSet->zones.push_back (Zone {});
    ToneFixture emptyFixture (makeDefaultTone(), emptyZoneSet);
    voice.startNote (emptyFixture.get(), 0, 60, 1.0f, 1);
    CHECK (voice.finished());
}

TEST_CASE ("retriggering restarts the sample")
{
    ToneFixture fixture (makeDefaultTone(), makeZoneSet (makeSineSample()));
    ToneVoice voice;
    voice.prepare (kSampleRate);
    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
    renderVoice (voice, 1000);
    REQUIRE (voice.state() == ToneVoice::State::Active);

    voice.startNote (fixture.get(), 0, 60, 1.0f, 1);
    const auto remaining = samplesUntilFinished (voice);

    CHECK (remaining >= kFrames - 100);
    CHECK (remaining <= kFrames + 200);
}
