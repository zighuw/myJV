#pragma once

#include "DSP/DeterministicRandom.h"

#include <cstdint>

// Plain parameter view for one LFO, filled by the voice layer from an
// LfoSnapshot.
struct LfoSettings
{
    int   wave = 0;          // 0..7: SINE/SAW/SQUARE/TRIANGLE/TRAPEZOID/S&H/RANDOM/CHAOTIC
    bool  keyTrig = false;   // Note-On resets the phase
    float rate = 64.0f;      // 0..127
    int   levelOffset = 0;   // -63..63 (DC shift)
    float delayTime = 0.0f;  // 0..127
    int   fadeMode = 0;      // 0 LINEAR / 1 EXPONENTIAL
    float fadeTime = 0.0f;   // 0..127
    bool  sync = false;      // Ext Sync (host BPM / patch default tempo)
};

// Per-voice LFO (architecture 5.8): 8 waveforms, key trigger, free or
// tempo-synced rate, level offset and a delay + fade-in stage. Random waveforms
// (S&H, Random, Chaotic) are driven by a seeded DeterministicRandom so renders
// are reproducible. start/reset/process are RT-safe.
class LFO
{
public:
    LFO() = default;

    // Non-RT: stores the sample rate and resets the LFO.
    void prepare (double sampleRate) noexcept;

    // RT-safe. Note-On. `tempoBpm` is used only when settings.sync is set.
    void start (const LfoSettings& settings, std::uint64_t seed, double tempoBpm) noexcept;

    // RT-safe. Live frequency multiplier from the control matrix (2^octaves).
    // start() resets it to 1.
    void setRateMultiplier (float multiplier) noexcept;

    // RT-safe. Returns to an inactive, silent state.
    void reset() noexcept;

    // RT-safe. Advances one sample and returns the output.
    float process() noexcept;

    float value() const noexcept { return currentOutput; }
    double frequencyHz() const noexcept { return frequency; }
    bool isActive() const noexcept { return active; }

private:
    float waveformValue() const noexcept;
    float fadeValue() const noexcept;
    void advanceCycle() noexcept;

    LfoSettings settings;
    double sampleRate = 48000.0;
    double frequency = 0.0;
    float rateMultiplier = 1.0f;
    float phase = 0.0f;             // [0, 1)
    float offset = 0.0f;
    float currentOutput = 0.0f;
    bool active = false;
    int delayRemaining = 0;
    int fadeSamples = 0;
    int fadeElapsed = 0;

    DeterministicRandom random;

    // Waveform state.
    float sampleHold = 0.0f;
    float randomPrevious = 0.0f;
    float randomTarget = 0.0f;
    float chaosX = 0.5f;
};
