#pragma once

// Plain parameter view for one envelope, filled by the voice layer from a
// ToneSnapshot shape (P/F/A). A-ENV callers must set level[3] = 0.
struct EnvelopeSettings
{
    float time[4] {};          // parameter values 0..127 (mapping in Calibration)
    float level[4] {};         // parameter values 0..127
    int velocityCurve = 0;     // 0..6: LINEAR / EXP1..3 / LOG1..3 (F/A; P-ENV passes 0)
    int velocitySens = 0;      // -63..63
    int timeKeyfollow = 0;     // -63..63
    int velTime1Sens = 0;      // -63..63, scales T1 only
    int velTime4Sens = 0;      // -63..63, scales T4 only
};

// Four-segment envelope (architecture 5.7):
//   Idle -> Attack(T1->L1) -> Decay(T2->L2) -> SustainRamp(T3->L3) -> Hold
//   Note-Off -> Release(T4->L4, starting from the current level)
//
// Every segment uses an exponential approach coefficient derived from its
// mapped time and Calibration::kEnvCurve, then snaps exactly to the target at
// the boundary. Fully trivially copyable and allocation free: start/release/
// process/reset are RT-safe.
class Envelope
{
public:
    enum class Stage : unsigned char
    {
        Idle,
        Attack,
        Decay,
        SustainRamp,
        Hold,
        Release,
        Finished
    };

    Envelope() = default;

    // Non-RT: stores the sample rate and resets the envelope.
    void prepare (double sampleRate) noexcept;

    // RT-safe. Note-On / retrigger: starts a new attack from the current level.
    void start (const EnvelopeSettings& settings, int midiNote, float velocity) noexcept;

    // RT-safe. Note-Off: releases from the current level (idempotent).
    void release() noexcept;

    // RT-safe. Returns to Idle at level 0.
    void reset() noexcept;

    // RT-safe. Advances one sample and returns the level.
    float process() noexcept;

    float level() const noexcept { return currentLevel; }

    // Level scaling implied by the current note velocity (diagnostics).
    float velocityLevelScale() const noexcept { return velocityGain; }

    Stage stage() const noexcept { return currentStage; }
    bool isActive() const noexcept;
    bool isFinished() const noexcept;

private:
    void enterStage (Stage stage, float target, double ms) noexcept;
    float effectiveLevel (int index) const noexcept;

    EnvelopeSettings settings;
    double sampleRate = 48000.0;
    Stage currentStage = Stage::Idle;
    float currentLevel = 0.0f;
    float stageTarget = 0.0f;
    float coefficient = 0.0f;
    int samplesRemaining = 0;
    float velocityGain = 1.0f;
    float keyScale = 1.0f;
    float velTime1Scale = 1.0f;
    float velTime4Scale = 1.0f;
};
