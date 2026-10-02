#pragma once

// RBJ peaking EQ used by the TVF PKG type (architecture 5.5). Fixed Q in the
// parameter mapping; the caller may also set explicit coefficients.
// setPeaking/setParameters/reset/process are RT-safe (Direct Form I).
class Biquad
{
public:
    Biquad() = default;

    // Non-RT: stores the sample rate and resets the state.
    void prepare (double sampleRate) noexcept;

    // RT-safe.
    void reset() noexcept;

    // RT-safe. Centre frequency, quality factor and peak gain in dB.
    void setPeaking (float centreHz, float q, float gainDb) noexcept;

    // RT-safe. Convenience mapping from the 0..127 tone parameters
    // (fixed Q = 1, gain = resonance/127 * kPkgGainDb).
    void setParameters (float cutoffParam, float resonanceParam) noexcept;

    // RT-safe.
    float process (float in) noexcept;

    float centreHz() const noexcept { return currentCentreHz; }
    float gainDb() const noexcept { return currentGainDb; }

private:
    double sampleRate = 48000.0;
    float currentCentreHz = 1000.0f;
    float currentGainDb = 0.0f;
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float x1 = 0.0f;
    float x2 = 0.0f;
    float y1 = 0.0f;
    float y2 = 0.0f;
};
