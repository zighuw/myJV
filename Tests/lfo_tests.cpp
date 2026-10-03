#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "DSP/DeterministicRandom.h"
#include "DSP/LFO.h"
#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
constexpr double kShapeRate = 160.0;   // rate 127 -> 20 Hz -> exact 0.125 phase step
constexpr double kSampleRate = 1000.0;

enum Wave
{
    Sine = 0,
    Saw = 1,
    Square = 2,
    Triangle = 3,
    Trapezoid = 4,
    SampleHold = 5,
    Random = 6,
    Chaotic = 7
};

LfoSettings makeSettings (int wave = Sine, float rate = 64.0f)
{
    LfoSettings settings;
    settings.wave = wave;
    settings.rate = rate;
    return settings;
}

std::vector<float> run (LFO& lfo, int samples)
{
    std::vector<float> out;
    out.reserve ((std::size_t) samples);

    for (int i = 0; i < samples; ++i)
        out.push_back (lfo.process());

    return out;
}

std::vector<float> captureCycle (int wave)
{
    LFO lfo;
    lfo.prepare (kShapeRate);
    lfo.start (makeSettings (wave, 127.0f), 1, 120.0);
    return run (lfo, 8);
}
}

TEST_CASE ("the deterministic random generator reproduces its sequence per seed")
{
    DeterministicRandom first (42), same (42), other (43);

    std::vector<float> sequenceFirst, sequenceSame, sequenceOther;

    for (int i = 0; i < 100; ++i)
    {
        sequenceFirst.push_back (first.nextFloat());
        sequenceSame.push_back (same.nextFloat());
        sequenceOther.push_back (other.nextFloat());
    }

    REQUIRE (sequenceFirst == sequenceSame);
    REQUIRE (sequenceFirst != sequenceOther);

    for (const auto value : sequenceFirst)
    {
        CHECK (value >= 0.0f);
        CHECK (value < 1.0f);
    }

    DeterministicRandom bipolar (7);

    for (int i = 0; i < 100; ++i)
    {
        const auto value = bipolar.nextBipolar();
        CHECK (value >= -1.0f);
        CHECK (value < 1.0f);
    }
}

TEST_CASE ("the periodic waveforms follow their phase formulas")
{
    const float expectedSquare[] { 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    const float expectedTriangle[] { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, -0.5f };
    const float expectedTrapezoid[] { -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, -1.0f };

    const auto sine = captureCycle (Sine);

    for (int i = 0; i < 8; ++i)
        REQUIRE (sine[i] == Catch::Approx (std::sin (6.283185307179586 * (double) i * 0.125)).margin (1.0e-5));

    const auto saw = captureCycle (Saw);

    for (int i = 0; i < 8; ++i)
        REQUIRE (saw[i] == Catch::Approx (2.0f * (float) i * 0.125f - 1.0f).margin (1.0e-5));

    const auto square = captureCycle (Square);
    const auto triangle = captureCycle (Triangle);
    const auto trapezoid = captureCycle (Trapezoid);

    for (int i = 0; i < 8; ++i)
    {
        INFO ("phase step " << i);
        REQUIRE (square[i] == expectedSquare[i]);
        REQUIRE (triangle[i] == Catch::Approx (expectedTriangle[i]).margin (1.0e-5));
        REQUIRE (trapezoid[i] == Catch::Approx (expectedTrapezoid[i]).margin (1.0e-5));
    }

    // The periodic waveforms repeat with the phase step.
    const auto twoCycles = [] (int wave)
    {
        LFO lfo;
        lfo.prepare (kShapeRate);
        lfo.start (makeSettings (wave, 127.0f), 1, 120.0);
        return run (lfo, 16);
    };

    const auto sawTwo = twoCycles (Saw);

    for (int i = 0; i < 8; ++i)
        REQUIRE (sawTwo[i + 8] == Catch::Approx (sawTwo[i]).margin (1.0e-6));

    const auto squareTwo = twoCycles (Square);

    for (int i = 0; i < 8; ++i)
        REQUIRE (squareTwo[i + 8] == squareTwo[i]);
}

TEST_CASE ("rate maps 0..127 to the 0.05..20 Hz range")
{
    const auto expectedHz = [] (float rate)
    {
        return Calibration::kLfoRateMinHz
               * std::pow (Calibration::kLfoRateMaxHz / Calibration::kLfoRateMinHz, (double) rate / 127.0);
    };

    for (const float rate : { 0.0f, 64.0f, 127.0f })
    {
        LFO lfo;
        lfo.prepare (kSampleRate);
        lfo.start (makeSettings (Sine, rate), 1, 120.0);

        INFO ("rate " << rate);
        REQUIRE (lfo.frequencyHz() == Catch::Approx (expectedHz (rate)).epsilon (1.0e-9));
    }

    // 20 Hz at 160 Hz sample rate is 8 samples per cycle.
    LFO lfo;
    lfo.prepare (kShapeRate);
    lfo.start (makeSettings (Saw, 127.0f), 1, 120.0);

    int period = 0;
    float previous = lfo.process();

    for (int i = 1; i < 100; ++i)
    {
        const auto value = lfo.process();
        ++period;

        if (value < previous - 1.0f)
            break;

        previous = value;
    }

    REQUIRE (period == 8);
}

TEST_CASE ("key trigger resets the phase and free-run preserves it")
{
    LFO lfo;
    lfo.prepare (kSampleRate);

    auto settings = makeSettings (Square, 127.0f);
    settings.keyTrig = false;
    lfo.start (settings, 1, 120.0);

    for (int i = 0; i < 30; ++i)
        lfo.process();   // phase ~= 0.6 -> square is -1

    lfo.start (settings, 1, 120.0);
    REQUIRE (lfo.process() == Catch::Approx (-1.0f));

    settings.keyTrig = true;
    lfo.start (settings, 1, 120.0);
    REQUIRE (lfo.process() == Catch::Approx (1.0f));
}

TEST_CASE ("delay holds the offset and freezes the phase")
{
    LFO lfo;
    lfo.prepare (kSampleRate);

    auto settings = makeSettings (Square, 127.0f);
    settings.delayTime = 127.0f;   // 2000 ms -> 2000 samples at 1 kHz
    lfo.start (settings, 1, 120.0);

    const auto delaySamples = (int) std::lround (Calibration::kLfoDelayMaxMs * 1.0);

    for (int i = 0; i < delaySamples; ++i)
        REQUIRE (lfo.process() == 0.0f);

    REQUIRE (lfo.process() == Catch::Approx (1.0f));   // square at phase 0
}

TEST_CASE ("delay and fade combine without gaps")
{
    LFO lfo;
    lfo.prepare (kSampleRate);

    auto settings = makeSettings (Square, 127.0f);
    settings.delayTime = 20.0f;
    settings.fadeTime = 20.0f;
    lfo.start (settings, 1, 120.0);

    const auto mapped = [] (double maxMs)
    {
        return (int) std::lround (maxMs * std::pow (20.0 / 127.0, 2.0));
    };

    const auto delaySamples = mapped (Calibration::kLfoDelayMaxMs);
    const auto fadeSamples = mapped (Calibration::kLfoFadeMaxMs);

    for (int i = 0; i < delaySamples; ++i)
        REQUIRE (lfo.process() == 0.0f);

    float previous = 0.0f;

    for (int i = 0; i < fadeSamples; ++i)
    {
        const auto value = std::abs (lfo.process());
        REQUIRE (value >= previous - 1.0e-6f);
        previous = value;
    }

    REQUIRE (previous == Catch::Approx (1.0f).margin (1.0e-5));
}

TEST_CASE ("linear and exponential fades reach full scale at the fade time")
{
    const auto fadeSamplesFor = [] (float t)
    {
        return (int) std::lround (Calibration::kLfoFadeMaxMs * std::pow ((double) t / 127.0, 2.0));
    };

    const auto recordFade = [] (int mode, int samples)
    {
        LFO lfo;
        lfo.prepare (kSampleRate);
        auto settings = makeSettings (Square, 127.0f);
        settings.fadeMode = mode;
        settings.fadeTime = 90.0f;
        lfo.start (settings, 1, 120.0);

        std::vector<float> out;

        for (int i = 0; i < samples; ++i)
            out.push_back (std::abs (lfo.process()));

        return out;
    };

    const auto fadeSamples = fadeSamplesFor (90.0f);
    REQUIRE (fadeSamples > 10);
    REQUIRE (fadeSamples % 2 == 0);

    const auto linear = recordFade (0, fadeSamples);
    REQUIRE (linear[fadeSamples / 2 - 1] == Catch::Approx (0.5).margin (1.0e-4));
    REQUIRE (linear[fadeSamples - 1] == Catch::Approx (1.0f).margin (1.0e-5));

    for (std::size_t i = 1; i < linear.size(); ++i)
        REQUIRE (linear[i] >= linear[i - 1] - 1.0e-6f);

    const auto exponential = recordFade (1, fadeSamples);
    const auto expectedHalf = (1.0 - std::exp (-Calibration::kLfoFadeCurve * 0.5))
                              / (1.0 - std::exp (-Calibration::kLfoFadeCurve));
    REQUIRE (exponential[fadeSamples / 2 - 1] == Catch::Approx (expectedHalf).margin (1.0e-4));
    REQUIRE (exponential[fadeSamples - 1] == Catch::Approx (1.0f).margin (1.0e-5));
}

TEST_CASE ("level offset shifts the oscillation centre")
{
    LFO lfo;
    lfo.prepare (kShapeRate);

    auto settings = makeSettings (Square, 127.0f);
    settings.levelOffset = 63;
    lfo.start (settings, 1, 120.0);

    float minimum = 2.0f;
    float maximum = -2.0f;

    for (int i = 0; i < 8; ++i)
    {
        const auto value = lfo.process();
        minimum = std::min (minimum, value);
        maximum = std::max (maximum, value);
    }

    REQUIRE (minimum == Catch::Approx (0.0f).margin (1.0e-5));
    REQUIRE (maximum == Catch::Approx (2.0f).margin (1.0e-5));
}

TEST_CASE ("sample and hold updates its value once per cycle")
{
    LFO lfo;
    lfo.prepare (kShapeRate);
    lfo.start (makeSettings (SampleHold, 127.0f), 7, 120.0);

    const auto out = run (lfo, 24);

    for (int i = 1; i < 8; ++i)
        REQUIRE (out[i] == out[0]);

    for (int i = 9; i < 16; ++i)
        REQUIRE (out[i] == out[8]);

    for (int i = 17; i < 24; ++i)
        REQUIRE (out[i] == out[16]);

    CHECK (out[0] != out[8]);
    CHECK (out[8] != out[16]);
}

TEST_CASE ("the random waveform interpolates continuously")
{
    LFO lfo;
    lfo.prepare (kShapeRate);
    lfo.start (makeSettings (Random, 127.0f), 11, 120.0);

    const auto out = run (lfo, 64);

    for (std::size_t i = 1; i < out.size(); ++i)
    {
        REQUIRE (std::abs (out[i] - out[i - 1]) <= 0.25f + 1.0e-4f);
        REQUIRE (std::abs (out[i]) <= 1.0f);
    }
}

TEST_CASE ("the chaotic waveform is bounded and seed deterministic")
{
    const auto capture = [] (std::uint64_t seed)
    {
        LFO lfo;
        lfo.prepare (kShapeRate);
        lfo.start (makeSettings (Chaotic, 127.0f), seed, 120.0);
        return run (lfo, 400);
    };

    const auto first = capture (1);
    const auto same = capture (1);
    const auto other = capture (2);

    REQUIRE (first == same);
    REQUIRE (first != other);

    for (const auto value : first)
    {
        REQUIRE (value > -1.0f);
        REQUIRE (value < 1.0f);
    }
}

TEST_CASE ("ext sync maps rate to note divisions of the tempo")
{
    const auto hz = [] (float rate, double bpm)
    {
        LFO lfo;
        lfo.prepare (kSampleRate);
        auto settings = makeSettings (Saw, rate);
        settings.sync = true;
        lfo.start (settings, 1, bpm);
        return lfo.frequencyHz();
    };

    const auto beatHz = [] (int index, double bpm)
    {
        return (bpm / 60.0) / Calibration::kLfoSyncBeats[index];
    };

    REQUIRE (hz (0.0f, 120.0) == Catch::Approx (beatHz (0, 120.0)).epsilon (1.0e-12));
    REQUIRE (hz (127.0f, 120.0) == Catch::Approx (beatHz (17, 120.0)).epsilon (1.0e-12));
    REQUIRE (hz (45.0f, 120.0) == Catch::Approx (beatHz (6, 120.0)).epsilon (1.0e-12));
    REQUIRE (hz (45.0f, 60.0) == Catch::Approx (1.0));
    REQUIRE (hz (63.5f, 120.0) == Catch::Approx (beatHz (9, 120.0)).epsilon (1.0e-12));

    LFO lfo;
    lfo.prepare (kSampleRate);
    auto settings = makeSettings (Saw, 45.0f);
    lfo.start (settings, 1, 60.0);
    const auto freeHz = lfo.frequencyHz();
    lfo.start (settings, 1, 240.0);
    REQUIRE (lfo.frequencyHz() == Catch::Approx (freeHz));
}

TEST_CASE ("reset returns the lfo to idle")
{
    LFO lfo;
    lfo.prepare (kSampleRate);
    lfo.start (makeSettings (Square, 127.0f), 1, 120.0);

    REQUIRE (lfo.isActive());
    lfo.process();
    lfo.reset();

    CHECK_FALSE (lfo.isActive());
    CHECK (lfo.process() == 0.0f);
    lfo.process();
    CHECK (lfo.value() == 0.0f);
}

TEST_CASE ("every non-offset waveform stays within -1..1")
{
    for (int wave = 0; wave < 8; ++wave)
    {
        LFO lfo;
        lfo.prepare (kSampleRate);
        lfo.start (makeSettings (wave, 127.0f), 99, 120.0);

        for (int i = 0; i < 200; ++i)
        {
            const auto value = lfo.process();
            INFO ("wave " << wave << " sample " << i);
            REQUIRE (value >= -1.0f);
            REQUIRE (value <= 1.0f);
        }
    }
}

TEST_CASE ("the rate multiplier scales the lfo frequency")
{
    LFO lfo;
    lfo.prepare (kSampleRate);
    lfo.start (makeSettings (Saw, 127.0f), 1, 120.0);
    lfo.setRateMultiplier (2.0f);   // 20 Hz -> 40 Hz -> 25 samples per cycle at 1 kHz

    int period = 0;
    float previous = lfo.process();

    for (int i = 1; i < 200; ++i)
    {
        const auto value = lfo.process();
        ++period;

        if (value < previous - 1.0f)
            break;

        previous = value;
    }

    REQUIRE (period == Catch::Approx (25).margin (1));
}
