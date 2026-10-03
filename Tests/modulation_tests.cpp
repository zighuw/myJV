#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/ModulationMatrix.h"
#include "Engine/ToneVoice.h"
#include "Params/Calibration.h"
#include "Params/ParamSnapshot.h"

#include <cmath>
#include <memory>
#include <vector>

namespace
{
using Ctrl = ToneSnapshot::Ctrl;

constexpr int kBlockSamples = 480;   // 10 ms at 48 kHz

ModulationMatrix makeMatrix (float sampleRate = 48000.0)
{
    ModulationMatrix matrix;
    matrix.prepare (sampleRate);
    return matrix;
}

ToneSnapshot::Ctrl makeCtrl (std::initializer_list<int> dests, std::initializer_list<int> depths)
{
    ToneSnapshot::Ctrl ctrl;
    int i = 0;

    for (const auto dest : dests)
        ctrl.dest[i++] = dest;

    i = 0;

    for (const auto depth : depths)
        ctrl.depth[i++] = depth;

    return ctrl;
}
}

TEST_CASE ("the matrix resolves cc, pitch bend and aftertouch sources")
{
    auto matrix = makeMatrix();
    ModulationInput input;
    input.sourceIndex[0] = 0;    // CC1
    input.sourceIndex[1] = 94;   // CC95
    input.sourceIndex[2] = 95;   // pitch bend
    input.cc[1] = 0.25f;
    input.cc[95] = 0.75f;
    input.pitchBend = 1.0f;

    for (int i = 0; i < 100; ++i)
        matrix.update (input, kBlockSamples);

    REQUIRE (matrix.groupValue (0) == Catch::Approx (0.25f).margin (0.01));
    REQUIRE (matrix.groupValue (1) == Catch::Approx (0.75f).margin (0.01));
    REQUIRE (matrix.groupValue (2) == Catch::Approx (1.0f).margin (0.01));

    // Pitch bend is centred: raw 0.5. Group 1 is fixed to CC1, so use group 2.
    auto bendOnly = makeMatrix();
    ModulationInput bendInput;
    bendInput.sourceIndex[1] = 95;
    bendInput.pitchBend = 0.0f;

    for (int i = 0; i < 100; ++i)
        bendOnly.update (bendInput, kBlockSamples);

    REQUIRE (bendOnly.groupValue (1) == Catch::Approx (0.5f).margin (0.01));

    auto atOnly = makeMatrix();
    ModulationInput atInput;
    atInput.sourceIndex[1] = 96;
    atInput.aftertouch = 0.8f;

    for (int i = 0; i < 100; ++i)
        atOnly.update (atInput, kBlockSamples);

    REQUIRE (atOnly.groupValue (1) == Catch::Approx (0.8f).margin (0.01));
}

TEST_CASE ("group one always follows cc1")
{
    auto matrix = makeMatrix();
    ModulationInput input;
    input.sourceIndex[0] = 96;   // must be ignored
    input.aftertouch = 1.0f;
    input.cc[1] = 0.5f;

    for (int i = 0; i < 100; ++i)
        matrix.update (input, kBlockSamples);

    REQUIRE (matrix.groupValue (0) == Catch::Approx (0.5f).margin (0.01));
}

TEST_CASE ("hold latches the last value and peak keeps the maximum")
{
    const auto converge = [] (ModulationMatrix& matrix, const ModulationInput& input)
    {
        for (int i = 0; i < 100; ++i)
            matrix.update (input, kBlockSamples);
    };

    // HOLD follows new non-zero values (including downwards) and keeps the last
    // value when the source returns to zero.
    auto hold = makeMatrix();
    ModulationInput holdInput;
    holdInput.sourceIndex[0] = 0;
    holdInput.holdPeakMode[0] = 0;   // HOLD

    holdInput.cc[1] = 0.8f;
    converge (hold, holdInput);
    REQUIRE (hold.groupValue (0) == Catch::Approx (0.8f).margin (0.01));

    holdInput.cc[1] = 0.2f;
    converge (hold, holdInput);
    REQUIRE (hold.groupValue (0) == Catch::Approx (0.2f).margin (0.01));

    holdInput.cc[1] = 0.0f;
    converge (hold, holdInput);
    REQUIRE (hold.groupValue (0) == Catch::Approx (0.2f).margin (0.01));

    // PEAK keeps the maximum even when the source falls.
    auto peak = makeMatrix();
    ModulationInput peakInput;
    peakInput.sourceIndex[0] = 0;
    peakInput.holdPeakMode[0] = 1;   // PEAK

    peakInput.cc[1] = 0.4f;
    converge (peak, peakInput);
    peakInput.cc[1] = 0.9f;
    converge (peak, peakInput);
    REQUIRE (peak.groupValue (0) == Catch::Approx (0.9f).margin (0.01));

    peakInput.cc[1] = 0.2f;
    converge (peak, peakInput);
    REQUIRE (peak.groupValue (0) == Catch::Approx (0.9f).margin (0.01));

    peakInput.cc[1] = 0.0f;
    converge (peak, peakInput);
    REQUIRE (peak.groupValue (0) == Catch::Approx (0.9f).margin (0.01));
}

TEST_CASE ("source smoothing converges without overshoot and follows the block length")
{
    auto matrix = makeMatrix();
    ModulationInput input;
    input.sourceIndex[0] = 0;
    input.cc[1] = 1.0f;

    const auto coefficient = 1.0 - std::exp (-(double) kBlockSamples / 48000.0 * 1000.0 / Calibration::kModSmoothingMs);
    matrix.update (input, kBlockSamples);
    REQUIRE (matrix.groupValue (0) == Catch::Approx (coefficient).margin (1.0e-4));

    for (int i = 0; i < 100; ++i)
        matrix.update (input, kBlockSamples);

    REQUIRE (matrix.groupValue (0) <= 1.0f);
    REQUIRE (matrix.groupValue (0) == Catch::Approx (1.0f).margin (0.01));

    // Half a block converges half as far.
    auto half = makeMatrix();
    half.update (input, kBlockSamples / 2);
    REQUIRE (half.groupValue (0) < matrix.groupValue (0));
}

TEST_CASE ("each destination maps its depth to the matching output")
{
    struct Case
    {
        int dest;
        int depth;
        float (*read) (const ModulationOutputs&);
        float expected;
    };

    const auto pitchRead = [] (const ModulationOutputs& o) { return o.pitchSemitones; };
    const auto cutoffRead = [] (const ModulationOutputs& o) { return o.cutoffOctaves; };
    const auto resonanceRead = [] (const ModulationOutputs& o) { return o.resonanceOctaves; };
    const auto levelRead = [] (const ModulationOutputs& o) { return o.levelDb; };
    const auto panRead = [] (const ModulationOutputs& o) { return o.pan; };
    const auto lfo1RateRead = [] (const ModulationOutputs& o) { return o.lfoRateOctaves[0]; };
    const auto lfo2RateRead = [] (const ModulationOutputs& o) { return o.lfoRateOctaves[1]; };
    const auto lfo1PitchRead = [] (const ModulationOutputs& o) { return o.lfoPitchDepthScale[0]; };
    const auto lfo2PanRead = [] (const ModulationOutputs& o) { return o.lfoPanDepthScale[1]; };
    const auto envLevelRead = [] (const ModulationOutputs& o) { return o.envLevelScale[2]; };

    const Case cases[]
    {
        { 1, 63, pitchRead, (float) Calibration::kMatrixPitchSemitones },
        { 1, -63, pitchRead, (float) -Calibration::kMatrixPitchSemitones },
        { 2, 63, cutoffRead, (float) Calibration::kMatrixCutoffOctaves },
        { 3, 63, resonanceRead, (float) Calibration::kMatrixResonanceOctaves },
        { 4, 63, levelRead, (float) Calibration::kMatrixLevelDb },
        { 5, -63, panRead, -1.0f },
        { 6, 63, lfo1RateRead, (float) Calibration::kMatrixLfoRateOctaves },
        { 11, 63, lfo2RateRead, (float) Calibration::kMatrixLfoRateOctaves },
        { 7, 63, lfo1PitchRead, 2.0f },
        { 15, -63, lfo2PanRead, 0.0f },
        { 21, 63, envLevelRead, 2.0f },
    };

    for (const auto& c : cases)
    {
        auto matrix = makeMatrix();
        ModulationInput input;
        input.sourceIndex[0] = 0;
        input.cc[1] = 1.0f;

        for (int i = 0; i < 200; ++i)
            matrix.update (input, kBlockSamples);

        Ctrl ctrl[3] {};
        ctrl[0] = makeCtrl ({ c.dest, 0, 0, 0 }, { c.depth, 0, 0, 0 });

        REQUIRE (matrix.groupValue (0) > 0.99f);

        const auto outputs = matrix.evaluate (ctrl);
        INFO ("dest " << c.dest << " depth " << c.depth);
        REQUIRE (c.read (outputs) == Catch::Approx (c.expected).margin (1.0e-3));
    }
}

TEST_CASE ("destinations add up and off is ignored")
{
    auto matrix = makeMatrix();
    ModulationInput input;
    input.sourceIndex[0] = 0;   // group 1: CC1
    input.cc[1] = 1.0f;
    input.sourceIndex[1] = 1;   // group 2: CC2
    input.cc[2] = 0.5f;

    for (int i = 0; i < 200; ++i)
        matrix.update (input, kBlockSamples);

    Ctrl ctrl[3] {};
    ctrl[0] = makeCtrl ({ 0, 1, 2, 3 }, { 63, 63, 63, 63 });
    ctrl[1] = makeCtrl ({ 1, 0, 0, 0 }, { 63, 0, 0, 0 });

    const auto outputs = matrix.evaluate (ctrl);
    REQUIRE (outputs.pitchSemitones == Catch::Approx (Calibration::kMatrixPitchSemitones + 0.5 * Calibration::kMatrixPitchSemitones).margin (0.02));
    REQUIRE (outputs.cutoffOctaves == Catch::Approx (Calibration::kMatrixCutoffOctaves).margin (0.02));
    REQUIRE (outputs.resonanceOctaves == Catch::Approx (Calibration::kMatrixResonanceOctaves).margin (0.02));
}

TEST_CASE ("the deferred env time and tone delay destinations stay inert")
{
    auto matrix = makeMatrix();
    ModulationInput input;
    input.sourceIndex[0] = 0;
    input.cc[1] = 1.0f;

    for (int i = 0; i < 200; ++i)
        matrix.update (input, kBlockSamples);

    Ctrl ctrl[3] {};
    ctrl[0] = makeCtrl ({ 16, 18, 20, 22 }, { 63, 63, 63, 63 });

    const auto outputs = matrix.evaluate (ctrl);
    REQUIRE (outputs.pitchSemitones == 0.0f);
    REQUIRE (outputs.cutoffOctaves == 0.0f);
    REQUIRE (outputs.levelDb == 0.0f);
    REQUIRE (outputs.pan == 0.0f);
}
