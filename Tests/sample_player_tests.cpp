#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "DSP/SamplePlayer.h"
#include "Model/ZoneSet.h"
#include "Params/Calibration.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace juce;

constexpr double kPlayerSampleRate = 48000.0;

Sample makeSineSample (int frames, double sampleRate = kPlayerSampleRate, float frequency = 1000.0f)
{
    Sample sample;
    sample.data.setSize (1, frames);

    for (int i = 0; i < frames; ++i)
        sample.data.setSample (0, i, 0.5f * std::sin (2.0f * MathConstants<float>::pi * frequency
                                                      * (float) i / (float) sampleRate));

    sample.sourceSampleRate = sampleRate;
    sample.rootKey = 60;
    sample.name = "test-sine";
    return sample;
}

Sample makeRampSample (int frames)
{
    Sample sample;
    sample.data.setSize (1, frames);

    for (int i = 0; i < frames; ++i)
        sample.data.setSample (0, i, (float) i * 0.006f);

    sample.sourceSampleRate = kPlayerSampleRate;
    sample.rootKey = 60;
    return sample;
}

Sample makeConstantSample (int frames, float left, float right = 0.0f, bool stereo = false)
{
    Sample sample;
    sample.data.setSize (stereo ? 2 : 1, frames);

    for (int i = 0; i < frames; ++i)
    {
        sample.data.setSample (0, i, left);

        if (stereo)
            sample.data.setSample (1, i, right);
    }

    sample.sourceSampleRate = kPlayerSampleRate;
    sample.rootKey = 60;
    return sample;
}

Zone makeZone (LoopMode mode = LoopMode::Off, int loopStart = 0, int loopEnd = 0,
               int crossfade = 0, bool reverse = false)
{
    Zone zone;
    zone.loopMode = mode;
    zone.loop.start = loopStart;
    zone.loop.end = loopEnd;
    zone.loop.crossfadeSamples = crossfade;
    zone.reverse = reverse;
    return zone;
}

std::vector<float> render (SamplePlayer& player, int numSamples)
{
    std::vector<float> output ((std::size_t) numSamples);

    for (int i = 0; i < numSamples; ++i)
        output[(std::size_t) i] = player.getNextSample();

    return output;
}

int countUntilFinished (SamplePlayer& player, int safetyLimit = 100000)
{
    int count = 0;

    while (! player.hasFinished() && count < safetyLimit)
    {
        player.getNextSample();
        ++count;
    }

    return count;
}

float maxAdjacentDelta (const std::vector<float>& samples)
{
    float maximum = 0.0f;

    for (std::size_t i = 1; i < samples.size(); ++i)
        maximum = jmax (maximum, std::abs (samples[i] - samples[i - 1]));

    return maximum;
}

int fadeInSamples()
{
    return roundToInt (Calibration::kSampleFadeInMs * 0.001 * kPlayerSampleRate);
}
}

TEST_CASE ("one-shot playback follows the pitch ratio")
{
    const auto sample = makeSineSample (480);
    const Zone zone;

    SamplePlayer unison;
    unison.start (sample, zone, 60, kPlayerSampleRate);
    CHECK (countUntilFinished (unison) == 480);

    SamplePlayer octaveUp;
    octaveUp.start (sample, zone, 72, kPlayerSampleRate);
    CHECK (countUntilFinished (octaveUp) == 240);

    SamplePlayer octaveDown;
    octaveDown.start (sample, zone, 48, kPlayerSampleRate);
    CHECK (countUntilFinished (octaveDown) == 960);
}

TEST_CASE ("root key override and tuning shift the playback ratio")
{
    const auto sample = makeSineSample (480);

    auto overrideZone = makeZone();
    overrideZone.rootKeyOverride = 48;
    SamplePlayer overridePlayer;
    overridePlayer.start (sample, overrideZone, 60, kPlayerSampleRate);
    CHECK (countUntilFinished (overridePlayer) == 240);

    auto coarseZone = makeZone();
    coarseZone.coarseTune = 12.0f;
    SamplePlayer coarsePlayer;
    coarsePlayer.start (sample, coarseZone, 60, kPlayerSampleRate);
    CHECK (countUntilFinished (coarsePlayer) == 240);

    auto fineZone = makeZone();
    fineZone.fineTune = 1200.0f;
    SamplePlayer finePlayer;
    finePlayer.start (sample, fineZone, 60, kPlayerSampleRate);
    CHECK (countUntilFinished (finePlayer) == 240);
}

TEST_CASE ("unity playback reproduces the sample after the fade-in")
{
    const auto sample = makeSineSample (480);
    SamplePlayer player;
    player.start (sample, makeZone(), 60, kPlayerSampleRate);

    const auto output = render (player, 300);

    for (int i = fadeInSamples(); i < 300; ++i)
        CHECK (output[(std::size_t) i] == Catch::Approx (sample.data.getSample (0, i)).margin (1.0e-5f));
}

TEST_CASE ("hermite interpolation tracks a shifted sine")
{
    const auto sample = makeSineSample (2048);
    SamplePlayer player;
    player.start (sample, makeZone(), 67, kPlayerSampleRate);

    const auto ratio = (float) std::exp2 (7.0 / 12.0);
    const auto output = render (player, 800);

    float maximumError = 0.0f;

    for (int i = fadeInSamples(); i < 800; ++i)
    {
        const auto expected = 0.5f * std::sin (2.0f * MathConstants<float>::pi * 1000.0f * ratio
                                               * (float) i / (float) kPlayerSampleRate);
        maximumError = jmax (maximumError, std::abs (output[(std::size_t) i] - expected));
    }

    CHECK (maximumError < 0.01f);
}

TEST_CASE ("forward and sustain loops wrap the region")
{
    const auto sample = makeSineSample (480);

    for (auto mode : { LoopMode::Forward, LoopMode::Sustain })
    {
        SamplePlayer player;
        player.start (sample, makeZone (mode, 100, 200), 60, kPlayerSampleRate);

        const auto output = render (player, 400);

        REQUIRE (player.isActive());
        CHECK (output[200] == Catch::Approx (output[100]).margin (1.0e-6f));
        CHECK (output[250] == Catch::Approx (output[150]).margin (1.0e-6f));
    }
}

TEST_CASE ("invalid loop regions fall back to one-shot playback")
{
    const auto sample = makeSineSample (480);

    SamplePlayer reversed;
    reversed.start (sample, makeZone (LoopMode::Forward, 200, 100), 60, kPlayerSampleRate);
    CHECK (countUntilFinished (reversed) == 480);

    SamplePlayer beyondEnd;
    beyondEnd.start (sample, makeZone (LoopMode::Forward, 100, 9999), 60, kPlayerSampleRate);
    CHECK (countUntilFinished (beyondEnd) == 480);

    SamplePlayer emptyLoop;
    emptyLoop.start (sample, makeZone (LoopMode::Forward, 100, 100), 60, kPlayerSampleRate);
    CHECK (countUntilFinished (emptyLoop) == 480);
}

TEST_CASE ("crossfade removes the loop discontinuity")
{
    const auto sample = makeRampSample (480);

    SamplePlayer plain;
    plain.start (sample, makeZone (LoopMode::Forward, 100, 200, 0), 60, kPlayerSampleRate);
    const auto plainOutput = render (plain, 400);

    SamplePlayer crossfaded;
    crossfaded.start (sample, makeZone (LoopMode::Forward, 100, 200, 32), 60, kPlayerSampleRate);
    const auto crossfadedOutput = render (crossfaded, 400);

    CHECK (maxAdjacentDelta (plainOutput) > 0.5f);
    CHECK (maxAdjacentDelta (crossfadedOutput) < 0.1f);

    const auto mix = 31.0f / 32.0f;
    const auto expected = sample.data.getSample (0, 199) * (1.0f - mix)
                          + sample.data.getSample (0, 131) * mix;
    CHECK (crossfadedOutput[199] == Catch::Approx (expected).margin (1.0e-5f));
}

TEST_CASE ("reverse loops stay continuous with crossfade")
{
    const auto sample = makeRampSample (480);

    SamplePlayer plain;
    plain.start (sample, makeZone (LoopMode::Forward, 100, 200, 0, true), 60, kPlayerSampleRate);
    const auto plainOutput = render (plain, 600);

    SamplePlayer crossfaded;
    crossfaded.start (sample, makeZone (LoopMode::Forward, 100, 200, 32, true), 60, kPlayerSampleRate);
    const auto crossfadedOutput = render (crossfaded, 600);

    CHECK (maxAdjacentDelta (plainOutput) > 0.5f);
    CHECK (maxAdjacentDelta (crossfadedOutput) < 0.1f);
}

TEST_CASE ("crossfade length is clamped to half the loop")
{
    const auto sample = makeRampSample (480);

    SamplePlayer player;
    player.start (sample, makeZone (LoopMode::Forward, 100, 200, 1000), 60, kPlayerSampleRate);

    const auto output = render (player, 400);

    CHECK (player.isActive());
    CHECK (maxAdjacentDelta (output) < 0.2f);
}

TEST_CASE ("reverse playback mirrors forward playback")
{
    const auto sample = makeSineSample (480);

    SamplePlayer forward;
    forward.start (sample, makeZone(), 60, kPlayerSampleRate);
    const auto forwardOutput = render (forward, 480);

    SamplePlayer reverse;
    reverse.start (sample, makeZone (LoopMode::Off, 0, 0, 0, true), 60, kPlayerSampleRate);
    const auto reverseOutput = render (reverse, 480);

    for (int i = fadeInSamples(); i < 400; ++i)
        CHECK (reverseOutput[(std::size_t) i] == Catch::Approx (forwardOutput[(std::size_t) (479 - i)]).margin (1.0e-5f));
}

TEST_CASE ("reverse loops wrap at the loop start")
{
    const auto sample = makeSineSample (480);
    SamplePlayer player;
    player.start (sample, makeZone (LoopMode::Forward, 100, 200, 0, true), 60, kPlayerSampleRate);

    const auto output = render (player, 500);

    REQUIRE (player.isActive());
    // Positions run 479, 478, ... ; index 380 lands on 199 after wrapping at 100.
    CHECK (output[380] == Catch::Approx (sample.data.getSample (0, 199)).margin (1.0e-5f));
    CHECK (output[381] == Catch::Approx (sample.data.getSample (0, 198)).margin (1.0e-5f));
}

TEST_CASE ("note-on fade-in ramps linearly at any sample rate")
{
    const auto sample = makeConstantSample (2000, 1.0f);

    SamplePlayer player;
    player.start (sample, makeZone(), 60, kPlayerSampleRate);
    const auto output = render (player, 200);

    const auto fadeSamples = fadeInSamples();
    REQUIRE (fadeSamples > 1);

    CHECK (output[0] == 0.0f);

    for (int i = 1; i < fadeSamples; ++i)
        CHECK (output[(std::size_t) i] > output[(std::size_t) (i - 1)]);

    CHECK (output[(std::size_t) (fadeSamples - 1)] == Catch::Approx ((float) (fadeSamples - 1) / (float) fadeSamples).margin (1.0e-4f));
    CHECK (output[(std::size_t) fadeSamples] == Catch::Approx (1.0f).margin (1.0e-5f));

    SamplePlayer fast;
    fast.start (sample, makeZone(), 60, kPlayerSampleRate * 2.0);
    const auto fastOutput = render (fast, 200);
    const auto fastFadeSamples = fadeInSamples() * 2;

    CHECK (fastOutput[(std::size_t) (fastFadeSamples - 1)] < 1.0f);
    CHECK (fastOutput[(std::size_t) fastFadeSamples] == Catch::Approx (1.0f).margin (1.0e-5f));
}

TEST_CASE ("stereo samples are downmixed and mono samples pass through")
{
    const auto stereo = makeConstantSample (500, 1.0f, 0.0f, true);
    SamplePlayer stereoPlayer;
    stereoPlayer.start (stereo, makeZone(), 60, kPlayerSampleRate);
    const auto stereoOutput = render (stereoPlayer, 200);
    CHECK (stereoOutput[(std::size_t) fadeInSamples()] == Catch::Approx (0.5f).margin (1.0e-6f));

    const auto mono = makeConstantSample (500, 0.25f);
    SamplePlayer monoPlayer;
    monoPlayer.start (mono, makeZone(), 60, kPlayerSampleRate);
    const auto monoOutput = render (monoPlayer, 200);
    CHECK (monoOutput[(std::size_t) fadeInSamples()] == Catch::Approx (0.25f).margin (1.0e-6f));
}

TEST_CASE ("pitch offset semitones shift the playback ratio")
{
    const auto sample = makeSineSample (480);

    SamplePlayer preset;
    preset.setPitchOffsetSemitones (12.0f);
    preset.start (sample, makeZone(), 60, kPlayerSampleRate);
    CHECK (countUntilFinished (preset) == 240);

    SamplePlayer midPlay;
    midPlay.start (sample, makeZone(), 60, kPlayerSampleRate);
    render (midPlay, 100);
    midPlay.setPitchOffsetSemitones (12.0f);
    CHECK (countUntilFinished (midPlay) == 190);
}

TEST_CASE ("stopped and exhausted players stay silent")
{
    const auto sample = makeSineSample (64);

    SamplePlayer stopped;
    stopped.start (sample, makeZone(), 60, kPlayerSampleRate);
    stopped.stop();
    CHECK_FALSE (stopped.isActive());
    CHECK (stopped.hasFinished());
    CHECK (stopped.getNextSample() == 0.0f);

    SamplePlayer exhausted;
    exhausted.start (sample, makeZone(), 60, kPlayerSampleRate);
    countUntilFinished (exhausted);
    CHECK (exhausted.hasFinished());
    CHECK (exhausted.getNextSample() == 0.0f);
}

TEST_CASE ("sample rate ratio participates in the pitch ratio")
{
    const auto sample = makeSineSample (480, 96000.0);

    SamplePlayer slowerEngine;
    slowerEngine.start (sample, makeZone(), 60, kPlayerSampleRate);
    CHECK (countUntilFinished (slowerEngine) == 240);

    const auto fastSample = makeSineSample (480, kPlayerSampleRate);
    SamplePlayer fasterEngine;
    fasterEngine.start (fastSample, makeZone(), 60, 96000.0);
    CHECK (countUntilFinished (fasterEngine) == 960);
}

TEST_CASE ("high pitch ratios wrap tiny loops inside the region")
{
    const auto sample = makeRampSample (480);
    SamplePlayer player;
    player.start (sample, makeZone (LoopMode::Forward, 100, 103), 84, kPlayerSampleRate);   // +24 semitones

    const auto output = render (player, 120);

    REQUIRE (player.isActive());

    const float expected[3] { sample.data.getSample (0, 100),
                              sample.data.getSample (0, 101),
                              sample.data.getSample (0, 102) };

    for (int i = 75; i < 120; ++i)
        CHECK (output[(std::size_t) i] == Catch::Approx (expected[(i - 25) % 3]).margin (1.0e-6f));
}

TEST_CASE ("loop boundaries at the sample edges are handled")
{
    const auto sample = makeSineSample (480);

    SamplePlayer fullLoop;
    fullLoop.start (sample, makeZone (LoopMode::Forward, 0, 480), 60, kPlayerSampleRate);
    const auto fullOutput = render (fullLoop, 600);
    CHECK (fullLoop.isActive());
    CHECK (fullOutput[480] == Catch::Approx (sample.data.getSample (0, 0)).margin (1.0e-6f));
    CHECK (fullOutput[481] == Catch::Approx (sample.data.getSample (0, 1)).margin (1.0e-6f));

    SamplePlayer twoFrameLoop;
    twoFrameLoop.start (sample, makeZone (LoopMode::Forward, 10, 12, 0), 60, kPlayerSampleRate);
    const auto twoFrameOutput = render (twoFrameLoop, 90);
    CHECK (twoFrameLoop.isActive());

    for (int i = 72; i < 90; ++i)
    {
        const auto& expected = (i - 12) % 2 == 0 ? sample.data.getSample (0, 10)
                                                 : sample.data.getSample (0, 11);
        CHECK (twoFrameOutput[(std::size_t) i] == Catch::Approx (expected).margin (1.0e-6f));
    }

    SamplePlayer negativeStart;
    negativeStart.start (sample, makeZone (LoopMode::Forward, -5, 200), 60, kPlayerSampleRate);
    CHECK (countUntilFinished (negativeStart) == 480);
}

TEST_CASE ("restarting a player resets its playback state")
{
    const auto sample = makeSineSample (480);

    SamplePlayer player;
    player.start (sample, makeZone(), 60, kPlayerSampleRate);
    render (player, 100);

    player.start (sample, makeZone(), 60, kPlayerSampleRate);

    CHECK (player.isActive());
    CHECK (player.getNextSample() == 0.0f);   // fade-in restarts from zero
}

TEST_CASE ("reverse one-shot playback finishes after the first frame")
{
    const auto sample = makeSineSample (480);
    SamplePlayer player;
    player.start (sample, makeZone (LoopMode::Off, 0, 0, 0, true), 60, kPlayerSampleRate);

    CHECK (countUntilFinished (player) == 480);
}

TEST_CASE ("degenerate samples are handled safely")
{
    Sample empty;
    empty.data.setSize (0, 0);
    SamplePlayer emptyPlayer;
    emptyPlayer.start (empty, makeZone(), 60, kPlayerSampleRate);
    CHECK_FALSE (emptyPlayer.isActive());
    CHECK (emptyPlayer.getNextSample() == 0.0f);

    const auto zeroRate = makeConstantSample (64, 0.5f);
    Sample invalidRate;
    invalidRate.data = zeroRate.data;
    invalidRate.sourceSampleRate = 0.0;
    SamplePlayer invalidRatePlayer;
    invalidRatePlayer.start (invalidRate, makeZone(), 60, kPlayerSampleRate);
    CHECK_FALSE (invalidRatePlayer.isActive());

    const auto oneFrame = makeConstantSample (1, 0.5f);
    SamplePlayer single;
    single.start (oneFrame, makeZone(), 60, kPlayerSampleRate);
    CHECK (single.getNextSample() == 0.0f);
    single.getNextSample();
    CHECK (single.hasFinished());

    SamplePlayer stoppedBeforeStart;
    CHECK_FALSE (stoppedBeforeStart.isActive());
    CHECK (stoppedBeforeStart.getNextSample() == 0.0f);
}

TEST_CASE ("render sample player wavs", "[.wav]")
{
    const auto directory = File (String (MYJV_REPO_DIR) + "/REVIEWS/M1-04");
    REQUIRE (directory.createDirectory().wasOk());

    const auto sample = makeSineSample (4800, kPlayerSampleRate, 220.0f);

    struct Audition
    {
        const char* name;
        Zone zone;
        int note;
        float pitchOffset;
    };

    auto loopZone = makeZone (LoopMode::Forward, 1000, 3000, 128);
    auto reverseZone = makeZone (LoopMode::Off, 0, 0, 0, true);

    const Audition auditions[]
    {
        { "pitch-up", makeZone(), 67, 0.0f },
        { "pitch-down", makeZone(), 53, 0.0f },
        { "loop-crossfade", loopZone, 60, 0.0f },
        { "reverse", reverseZone, 60, 0.0f }
    };

    WavAudioFormat format;

    for (const auto& audition : auditions)
    {
        SamplePlayer player;
        player.setPitchOffsetSemitones (audition.pitchOffset);
        player.start (sample, audition.zone, audition.note, kPlayerSampleRate);

        const auto rendered = render (player, 4800);

        const auto path = directory.getChildFile (String ("sample-player-") + audition.name + ".wav");
        path.deleteFile();

        std::unique_ptr<OutputStream> stream = path.createOutputStream();
        REQUIRE (stream != nullptr);

        auto writer = format.createWriterFor (stream, AudioFormatWriterOptions()
                                                          .withSampleRate (kPlayerSampleRate)
                                                          .withNumChannels (2)
                                                          .withBitsPerSample (16));
        REQUIRE (writer != nullptr);

        AudioBuffer<float> buffer (2, (int) rendered.size());

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            buffer.setSample (0, i, rendered[(std::size_t) i]);
            buffer.setSample (1, i, rendered[(std::size_t) i]);
        }

        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()));
    }
}
