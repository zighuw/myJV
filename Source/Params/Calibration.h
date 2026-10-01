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
}
