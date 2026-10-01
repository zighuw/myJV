#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "DSP/Envelope.h"
#include "Params/Calibration.h"

#include <cmath>
#include <utility>

namespace
{
constexpr double kSampleRate = 1000.0;   // keeps the 20 s cases cheap

EnvelopeSettings makeSettings (float t1 = 0.0f, float l1 = 127.0f,
                               float t2 = 0.0f, float l2 = 127.0f,
                               float t3 = 0.0f, float l3 = 127.0f,
                               float t4 = 0.0f, float l4 = 0.0f)
{
    EnvelopeSettings settings;
    settings.time[0] = t1;
    settings.time[1] = t2;
    settings.time[2] = t3;
    settings.time[3] = t4;
    settings.level[0] = l1;
    settings.level[1] = l2;
    settings.level[2] = l3;
    settings.level[3] = l4;
    return settings;
}

double timeMs (float t)
{
    return Calibration::kEnvTimeMinMs
           * std::pow (Calibration::kEnvTimeMaxMs / Calibration::kEnvTimeMinMs, (double) t / 127.0);
}

int segmentSamples (double sampleRate, double ms)
{
    return (int) std::lround (std::max (1.0, ms * sampleRate / 1000.0));
}

int runUntilStageChange (Envelope& env)
{
    const auto stage = env.stage();
    int count = 0;

    while (env.stage() == stage && count < 2000000)
    {
        env.process();
        ++count;
    }

    return count;
}

int runUntilFinished (Envelope& env)
{
    int count = 0;

    while (! env.isFinished() && count < 2000000)
    {
        env.process();
        ++count;
    }

    return count;
}
}

TEST_CASE ("the envelope maps t 0..127 to 1 ms..20 s at any sample rate")
{
    for (const double sampleRate : { 44100.0, 48000.0 })
    {
        for (const float t : { 0.0f, 64.0f })
        {
            Envelope env;
            env.prepare (sampleRate);
            env.start (makeSettings (t), 60, 1.0f);

            INFO ("sampleRate " << sampleRate << " t " << t);
            REQUIRE (runUntilStageChange (env) == segmentSamples (sampleRate, timeMs (t)));
        }
    }

    Envelope env;
    env.prepare (kSampleRate);
    env.start (makeSettings (127.0f), 60, 1.0f);
    REQUIRE (runUntilStageChange (env) == segmentSamples (kSampleRate, timeMs (127.0f)));
}

TEST_CASE ("each segment follows the exponential approach coefficient")
{
    Envelope env;
    env.prepare (kSampleRate);
    env.start (makeSettings (64.0f), 60, 1.0f);

    const auto samples = segmentSamples (kSampleRate, timeMs (64.0f));
    const auto coefficient = 1.0f - std::exp (-(float) Calibration::kEnvCurve / (float) samples);

    REQUIRE (env.level() == 0.0f);
    env.process();
    REQUIRE (env.level() == Catch::Approx (coefficient).margin (1.0e-6));

    float previous = env.level();
    bool monotonic = true;
    bool bounded = true;

    while (env.stage() == Envelope::Stage::Attack)
    {
        env.process();
        monotonic = monotonic && env.level() >= previous - 1.0e-6f;
        bounded = bounded && env.level() <= 1.0f + 1.0e-6f;
        previous = env.level();
    }

    CHECK (monotonic);
    CHECK (bounded);
    CHECK (env.level() == 127.0f * Calibration::kEnvLevelScale);   // the segment end snaps exactly
}

TEST_CASE ("the envelope runs attack, decay, sustain-ramp and then holds")
{
    Envelope env;
    env.prepare (kSampleRate);
    env.start (makeSettings (10.0f, 127.0f, 20.0f, 64.0f, 30.0f, 32.0f, 40.0f, 0.0f), 60, 1.0f);

    runUntilStageChange (env);
    REQUIRE (env.stage() == Envelope::Stage::Decay);
    REQUIRE (env.level() == Catch::Approx (1.0f).margin (1.0e-6));

    const auto decay = runUntilStageChange (env);
    REQUIRE (env.stage() == Envelope::Stage::SustainRamp);
    REQUIRE (decay == segmentSamples (kSampleRate, timeMs (20.0f)));
    REQUIRE (env.level() == Catch::Approx (64.0f / 127.0f).margin (1.0e-6));

    const auto ramp = runUntilStageChange (env);
    REQUIRE (env.stage() == Envelope::Stage::Hold);
    REQUIRE (ramp == segmentSamples (kSampleRate, timeMs (30.0f)));
    REQUIRE (env.level() == Catch::Approx (32.0f / 127.0f).margin (1.0e-6));

    for (int i = 0; i < 100; ++i)
    {
        env.process();
        REQUIRE (env.stage() == Envelope::Stage::Hold);
        REQUIRE (env.level() == Catch::Approx (32.0f / 127.0f).margin (1.0e-6));
    }

    CHECK (env.isActive());
}

TEST_CASE ("release starts from the current level without a jump")
{
    Envelope env;
    env.prepare (kSampleRate);
    env.start (makeSettings (127.0f, 127.0f, 0.0f, 0.0f, 0.0f, 127.0f, 64.0f, 0.0f), 60, 1.0f);

    for (int i = 0; i < 100; ++i)
        env.process();

    const auto before = env.level();
    REQUIRE (before > 0.0f);
    REQUIRE (before < 0.5f);

    env.release();
    REQUIRE (env.stage() == Envelope::Stage::Release);

    const auto releaseSamples = segmentSamples (kSampleRate, timeMs (64.0f));
    const auto coefficient = 1.0f - std::exp (-(float) Calibration::kEnvCurve / (float) releaseSamples);

    env.process();
    const auto after = env.level();
    REQUIRE (after == Catch::Approx (before * (1.0f - coefficient)).margin (1.0e-6));
    REQUIRE (before - after <= before * coefficient + 1.0e-6f);

    int calls = 1;
    while (! env.isFinished())
    {
        env.process();
        ++calls;
    }

    REQUIRE (calls == releaseSamples);
    CHECK (env.level() == Catch::Approx (0.0f).margin (1.0e-6));
}

TEST_CASE ("retriggering during release continues from the current level")
{
    Envelope env;
    env.prepare (kSampleRate);
    const auto settings = makeSettings (127.0f, 127.0f, 0.0f, 0.0f, 0.0f, 127.0f, 64.0f, 0.0f);

    env.start (settings, 60, 1.0f);

    for (int i = 0; i < 200; ++i)
        env.process();

    env.release();

    for (int i = 0; i < 20; ++i)
        env.process();

    const auto before = env.level();
    REQUIRE (before > 0.0f);

    env.start (settings, 60, 1.0f);
    REQUIRE (env.stage() == Envelope::Stage::Attack);

    env.process();
    const auto after = env.level();

    const auto attackSamples = segmentSamples (kSampleRate, timeMs (127.0f));
    const auto coefficient = 1.0f - std::exp (-(float) Calibration::kEnvCurve / (float) attackSamples);

    REQUIRE (after == Catch::Approx (before + (1.0f - before) * coefficient).margin (1.0e-6));
    CHECK (after > before);
}

TEST_CASE ("velocity curves and sensitivity set the level scale")
{
    struct Case
    {
        int curve;
        float velocity;
        int sens;
        float expected;
    };

    const Case cases[]
    {
        { 0, 0.5f, 0, 1.0f },
        { 0, 0.5f, 63, 0.5f },
        { 0, 0.5f, -63, 0.5f },
        { 0, 0.25f, 63, 0.25f },
        { 0, 0.25f, -63, 0.75f },
        { 1, 0.5f, 63, 0.25f },
        { 1, 0.5f, -63, 0.75f },
        { 2, 0.5f, 63, 0.0625f },
        { 3, 0.5f, 63, 0.00390625f },
        { 4, 0.25f, 63, 0.5f },
        { 5, 0.25f, 63, 0.70710678f },
        { 5, 0.25f, -63, 0.29289322f },
        { 6, 0.25f, 63, 0.84089642f },
        { 6, 0.25f, -63, 0.15910358f },
    };

    for (const auto& c : cases)
    {
        INFO ("curve " << c.curve << " velocity " << c.velocity << " sens " << c.sens);

        Envelope env;
        env.prepare (kSampleRate);

        auto settings = makeSettings (0.0f, 127.0f);
        settings.velocityCurve = c.curve;
        settings.velocitySens = c.sens;
        env.start (settings, 60, c.velocity);

        REQUIRE (env.velocityLevelScale() == Catch::Approx (c.expected).margin (1.0e-5));

        env.process();   // the 1 ms attack snaps within one sample at 1 kHz
        REQUIRE (env.level() == Catch::Approx (c.expected).margin (1.0e-5));
    }
}

TEST_CASE ("velocity time sensitivity scales attack and release times")
{
    const auto attackSamples = [] (int sens, float velocity)
    {
        Envelope env;
        env.prepare (kSampleRate);
        auto settings = makeSettings (64.0f, 127.0f);
        settings.velTime1Sens = sens;
        env.start (settings, 60, velocity);
        return runUntilStageChange (env);
    };

    const auto expected = [] (double factor)
    {
        return segmentSamples (kSampleRate, timeMs (64.0f) * factor);
    };

    const auto base = attackSamples (0, 1.0f);
    REQUIRE (base == expected (1.0));
    REQUIRE (attackSamples (0, 1.0f / 127.0f) == base);
    REQUIRE (attackSamples (63, 1.0f) == expected (0.5));
    REQUIRE (attackSamples (63, 1.0f / 127.0f) == expected (std::pow (2.0, 1.0 - 2.0 / 127.0)));
    REQUIRE (attackSamples (-63, 1.0f) == expected (2.0));
    REQUIRE (attackSamples (-63, 1.0f / 127.0f) == expected (std::pow (2.0, -(1.0 - 2.0 / 127.0))));

    // Only T1 is scaled by velTime1Sens: the decay segment is unchanged.
    const auto attackAndDecay = [] (int sens)
    {
        Envelope env;
        env.prepare (kSampleRate);
        auto settings = makeSettings (64.0f, 127.0f, 64.0f, 64.0f);
        settings.velTime1Sens = sens;
        env.start (settings, 60, 1.0f);
        const auto attack = runUntilStageChange (env);
        const auto decay = runUntilStageChange (env);
        return std::pair { attack, decay };
    };

    const auto neutral = attackAndDecay (0);
    const auto scaled = attackAndDecay (63);
    REQUIRE (scaled.first == expected (0.5));
    REQUIRE (scaled.second == neutral.second);

    // T4 is scaled by velTime4Sens only.
    const auto releaseSamples = [] (int sens, float velocity)
    {
        Envelope env;
        env.prepare (kSampleRate);
        auto settings = makeSettings (0.0f, 127.0f, 0.0f, 127.0f, 0.0f, 127.0f, 64.0f, 0.0f);
        settings.velTime4Sens = sens;
        env.start (settings, 60, velocity);
        env.process();
        env.release();
        return runUntilFinished (env);
    };

    const auto releaseBase = releaseSamples (0, 1.0f);
    REQUIRE (releaseBase == segmentSamples (kSampleRate, timeMs (64.0f)));
    REQUIRE (releaseSamples (63, 1.0f) == segmentSamples (kSampleRate, timeMs (64.0f) * 0.5));
    REQUIRE (releaseSamples (-63, 1.0f) == segmentSamples (kSampleRate, timeMs (64.0f) * 2.0));
}

TEST_CASE ("time keyfollow scales segment times with the played note")
{
    const auto attackSamples = [] (int keyfollow, int note)
    {
        Envelope env;
        env.prepare (kSampleRate);
        auto settings = makeSettings (64.0f, 127.0f);
        settings.timeKeyfollow = keyfollow;
        env.start (settings, note, 1.0f);
        return runUntilStageChange (env);
    };

    const auto expected = [] (double factor)
    {
        return segmentSamples (kSampleRate, timeMs (64.0f) * factor);
    };

    const auto neutral = attackSamples (0, 60);
    REQUIRE (attackSamples (63, 60) == neutral);
    REQUIRE (attackSamples (0, 72) == neutral);
    REQUIRE (attackSamples (63, 72) == expected (0.5));
    REQUIRE (attackSamples (63, 48) == expected (2.0));
    REQUIRE (attackSamples (-63, 72) == expected (2.0));
    REQUIRE (attackSamples (-63, 48) == expected (0.5));

    // Keyfollow scales every segment, not only the attack.
    const auto decaySamples = [] (int keyfollow, int note)
    {
        Envelope env;
        env.prepare (kSampleRate);
        auto settings = makeSettings (0.0f, 127.0f, 64.0f, 64.0f);
        settings.timeKeyfollow = keyfollow;
        env.start (settings, note, 1.0f);
        runUntilStageChange (env);
        return runUntilStageChange (env);
    };

    REQUIRE (decaySamples (63, 72) == expected (0.5));
    REQUIRE (decaySamples (-63, 48) == expected (0.5));
}

TEST_CASE ("a-env releases to zero while p/f envelopes may hold level 4")
{
    {
        Envelope env;
        env.prepare (kSampleRate);
        env.start (makeSettings (0.0f, 127.0f, 0.0f, 127.0f, 0.0f, 100.0f, 10.0f, 0.0f), 60, 1.0f);
        env.process();
        env.process();
        env.process();
        env.release();
        runUntilFinished (env);
        CHECK (env.level() == Catch::Approx (0.0f).margin (1.0e-6));
    }

    {
        Envelope env;
        env.prepare (kSampleRate);
        env.start (makeSettings (0.0f, 127.0f, 0.0f, 127.0f, 0.0f, 100.0f, 10.0f, 64.0f), 60, 1.0f);
        env.process();
        env.process();
        env.process();
        env.release();
        runUntilFinished (env);
        CHECK (env.level() == Catch::Approx (64.0f / 127.0f).margin (1.0e-6));
    }
}

TEST_CASE ("reset returns the envelope to idle and silence")
{
    Envelope env;
    env.prepare (kSampleRate);
    env.start (makeSettings (64.0f, 127.0f), 60, 1.0f);

    for (int i = 0; i < 100; ++i)
        env.process();

    REQUIRE (env.isActive());

    env.reset();
    CHECK (env.stage() == Envelope::Stage::Idle);
    CHECK (env.level() == 0.0f);
    CHECK_FALSE (env.isActive());
    CHECK_FALSE (env.isFinished());

    env.process();
    CHECK (env.level() == 0.0f);
}

TEST_CASE ("release and process are safe while idle or finished")
{
    Envelope env;
    env.prepare (kSampleRate);

    env.release();
    CHECK (env.stage() == Envelope::Stage::Idle);
    env.process();
    CHECK (env.level() == 0.0f);

    env.start (makeSettings (0.0f, 127.0f, 0.0f, 127.0f, 0.0f, 127.0f, 0.0f, 0.0f), 60, 1.0f);
    env.process();
    env.process();
    env.process();
    env.release();
    runUntilFinished (env);
    REQUIRE (env.isFinished());
    CHECK_FALSE (env.isActive());

    const auto held = env.level();
    env.release();
    env.process();
    CHECK (env.level() == held);
    CHECK (env.stage() == Envelope::Stage::Finished);
}
