#pragma once

namespace Calibration
{
// Note-On fade applied by the sample player to avoid start clicks.
inline constexpr double kSampleFadeInMs = 1.5;

// Envelope time mapping (architecture 4.4): ms = min * pow(max/min, t/127).
inline constexpr double kEnvTimeMinMs = 1.0;
inline constexpr double kEnvTimeMaxMs = 20000.0;

// Exponential approach coefficient used by every envelope segment: the level
// reaches ~99.3% of its target at the segment boundary, which then snaps
// exactly. Tuned by ear in M2-11.
inline constexpr double kEnvCurve = 5.0;

// Envelope level mapping (architecture 4.4): gain = l/127.
inline constexpr float kEnvLevelScale = 1.0f / 127.0f;

// Maximum velocity time-scaling span for Vel Time 1/4 Sens (+-1 octave).
inline constexpr double kEnvVelTimeOctaves = 1.0;

// LFO (architecture 5.8 / appendix C, tuned by ear in M2-11).
inline constexpr double kLfoRateMinHz = 0.05;
inline constexpr double kLfoRateMaxHz = 20.0;
inline constexpr double kChaosR = 3.9;
inline constexpr double kLfoDelayMaxMs = 2000.0;
inline constexpr double kLfoFadeMaxMs = 2000.0;
inline constexpr double kLfoFadeCurve = 3.0;
inline constexpr float kLfoLevelOffsetScale = 1.0f / 63.0f;

// Beats per LFO cycle for the 18 Ext Sync divisions, slowest to fastest:
// 1/1, 1/1., 1/1T, 1/2, 1/2., 1/2T, 1/4, 1/4., 1/4T, 1/8, 1/8., 1/8T,
// 1/16, 1/16., 1/16T, 1/32, 1/32., 1/32T
inline constexpr double kLfoSyncBeats[18]
{
    4.0, 6.0, 8.0 / 3.0, 2.0, 3.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0,
    0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125, 0.1875, 1.0 / 12.0
};

// TVF (architecture 5.5 / appendix C, tuned by ear in M2-11).
// cutoffHz = min * pow(max/min, c/127); Q = 0.5 * pow(kResonanceMaxQ/0.5, r/127).
inline constexpr double kCutoffMinHz = 20.0;
inline constexpr double kCutoffMaxHz = 20000.0;
inline constexpr double kResonanceMaxQ = 20.0;
inline constexpr double kPkgGainDb = 12.0;
inline constexpr int kFilterControlRate = 16;          // coefficient refresh, in samples (M2-05)
inline constexpr float kFilterStateLimit = 8.0f;       // SVF self-oscillation state guard

// ToneVoice modulation depths (architecture 5.4/5.7; provisional, tuned in M2-11).
inline constexpr double kPEnvDepthSemitones = 12.0;
inline constexpr double kLfoPitchSemitones = 1.0;
inline constexpr double kRandomPitchSemitones = 1.0;
inline constexpr double kFEnvDepthOctaves = 4.0;
inline constexpr double kLfoFilterDepthOctaves = 4.0;
inline constexpr float kRandomPan = 1.0f;
inline constexpr double kKillFadeMs = 5.0;
inline constexpr float kToneOutputLevelScale = 1.0f / 127.0f;

// WG (architecture 5.4/5.11; provisional, tuned in M2-11).
inline constexpr float kWaveGainDb[4] = { -6.0f, 0.0f, 6.0f, 12.0f };
inline constexpr double kFxmDepthSamples = 32.0;
inline constexpr double kToneDelayMinMs = 1.0;
inline constexpr double kToneDelayMaxMs = 2000.0;
inline constexpr double kNoteEndFadeMs = 1.0;
inline constexpr double kKeyIntervalReferenceMs = 250.0;
inline constexpr float kKeyIntervalScaleMin = 0.25f;
inline constexpr float kKeyIntervalScaleMax = 4.0f;

// Control matrix (architecture 5.9; provisional, tuned in M2-11).
inline constexpr double kMatrixPitchSemitones = 12.0;
inline constexpr double kMatrixCutoffOctaves = 4.0;
inline constexpr double kMatrixResonanceOctaves = 2.0;
inline constexpr double kMatrixLevelDb = 12.0;
inline constexpr double kMatrixLfoRateOctaves = 2.0;
inline constexpr double kMatrixLfoDepthRange = 1.0;
inline constexpr double kMatrixEnvLevelRange = 1.0;
inline constexpr double kModSmoothingMs = 10.0;
inline constexpr double kLfoAmpDepth = 1.0;

// Block-rate parameter smoothing (architecture 4.3; provisional, tuned in M2-11).
inline constexpr double kLevelSmoothingMs = 10.0;
inline constexpr double kPanSmoothingMs = 10.0;
inline constexpr double kOutputLevelSmoothingMs = 10.0;
inline constexpr double kCutoffSmoothingMs = 5.0;
inline constexpr double kResonanceSmoothingMs = 5.0;
}
