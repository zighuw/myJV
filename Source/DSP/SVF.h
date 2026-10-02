#pragma once

// TPT/ZDF state variable filter (architecture 5.5): one state update yields the
// 12 dB/oct lowpass, bandpass and highpass outputs. Coefficient changes are
// cheap and safe at any rate (the caller refreshes them at the control rate);
// setCoefficients/setParameters/reset/process* are RT-safe.
class SVF
{
public:
    SVF() = default;

    // Non-RT: stores the sample rate and resets the state.
    void prepare (double sampleRate) noexcept;

    // RT-safe.
    void reset() noexcept;

    // RT-safe. Direct coefficients (Hz / Q), clamped to the audible range and
    // the Nyquist guard.
    void setCoefficients (float cutoffHz, float q) noexcept;

    // RT-safe. Convenience mapping from the 0..127 tone parameters.
    void setParameters (float cutoffParam, float resonanceParam) noexcept;

    // RT-safe. Each call performs exactly one state update.
    float processLowpass (float in) noexcept;
    float processBandpass (float in) noexcept;
    float processHighpass (float in) noexcept;

    float cutoffHz() const noexcept { return currentCutoffHz; }
    float q() const noexcept { return currentQ; }

private:
    struct Outputs
    {
        float low;
        float band;
        float high;
    };

    Outputs update (float in) noexcept;

    double sampleRate = 48000.0;
    float currentCutoffHz = 1000.0f;
    float currentQ = 1.0f;
    float g = 0.0f;
    float k = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
};
