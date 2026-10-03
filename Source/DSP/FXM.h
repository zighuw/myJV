#pragma once

// Frequency cross modulation (architecture 5.11, basic version): the previous
// output drives a one-pole feedback path (Color sets its brightness) and the
// result is returned as a read-position offset for the sample player. The 2x
// oversampled/character-curve refinement lands in M4-07. RT-safe.
class FXM
{
public:
    FXM() = default;

    void reset() noexcept;

    // RT-safe. depth 0..127, color 1..7.
    void setParameters (bool enabled, int color, float depth) noexcept;

    // RT-safe. Returns the phase modulation offset (in samples) for the next
    // sample player read.
    float process (float input) noexcept;

private:
    bool on = false;
    float feedback = 0.0f;
    float colorGain = 1.0f;
    float depthNorm = 0.0f;
};
