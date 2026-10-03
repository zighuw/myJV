#pragma once

#include "Params/ParamSnapshot.h"

// Control matrix (architecture 5.9): three groups, each with one resolved
// source and four destinations/depths per tone. The engine feeds raw MIDI
// controller state once per block; update() resolves the configured sources and
// applies Hold/Peak tracking plus one-pole smoothing; evaluate() maps the
// per-tone destination tables into the ToneVoice application points. All
// operations are RT-safe (fixed scalars, no allocation).
struct ModulationInput
{
    float cc[128] {};                  // raw 0..1 per controller
    float pitchBend = 0.0f;            // -1..1 (centre 0)
    float aftertouch = 0.0f;           // 0..1
    int sourceIndex[3] { 0, 0, 0 };    // group 1..3; group 1 is fixed to CC1
    int holdPeakMode[3] { 0, 0, 0 };   // 0 HOLD / 1 PEAK
};

struct ModulationOutputs
{
    float pitchSemitones = 0.0f;
    float cutoffOctaves = 0.0f;
    float resonanceOctaves = 0.0f;
    float levelDb = 0.0f;
    float pan = 0.0f;
    float lfoRateOctaves[2] {};
    float lfoPitchDepthScale[2] { 1.0f, 1.0f };
    float lfoCutoffDepthScale[2] { 1.0f, 1.0f };
    float lfoAmpDepthScale[2] { 1.0f, 1.0f };
    float lfoPanDepthScale[2] { 1.0f, 1.0f };
    float envLevelScale[3] { 1.0f, 1.0f, 1.0f };   // P / F / A
};

class ModulationMatrix
{
public:
    ModulationMatrix() = default;

    // Non-RT.
    void prepare (double sampleRate) noexcept;

    // RT-safe.
    void reset() noexcept;

    // RT-safe. Block-rate source resolution, Hold/Peak and smoothing.
    void update (const ModulationInput& input, int blockSamples) noexcept;

    // RT-safe. Stateless mapping of the per-tone 3x4 tables (each entry holds
    // its destination and depth).
    ModulationOutputs evaluate (const ToneSnapshot::Ctrl (&ctrl)[3]) const noexcept;

    // Diagnostics (tests).
    float groupValue (int group) const noexcept;

private:
    float rawSource (const ModulationInput& input, int group) const noexcept;

    double sampleRate = 48000.0;
    float smoothed[3] {};
    float latch[3] {};
};
