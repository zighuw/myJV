#pragma once

#include "Model/Sample.h"

#include <memory>
#include <string>
#include <vector>

// MIDI protocol bounds (model level, shared with the M2 call site).
inline constexpr int kMidiNoteMin = 0;
inline constexpr int kMidiNoteMax = 127;
inline constexpr int kMidiVelocityMin = 1;
inline constexpr int kMidiVelocityMax = 127;

struct Zone
{
    // Audio thread reads the raw const Sample* only; shared_ptr copies are banned on RT paths.
    std::shared_ptr<const Sample> sample;
    int keyLow = 0, keyHigh = 127;      // inclusive MIDI key range
    int velLow = 1, velHigh = 127;      // inclusive velocity range (1..127; note-on velocity is never 0)
    int rootKeyOverride = -1;           // -1 = use Sample::rootKey
    float coarseTune = 0.0f;            // semitones
    float fineTune = 0.0f;              // cents
    float gainDb = 0.0f;
    int pan = 64;                       // 0..127, 64 = centre
    bool reverse = false;
    LoopMode loopMode = LoopMode::Sustain;
    LoopInfo loop;                      // effective playback loop; Sample::embeddedLoop is import-time input only
};

// Immutable after publication (architecture 1.3 rule 3); owned by a
// PatchRuntime through std::shared_ptr<const ZoneSet>.
struct ZoneSet
{
    std::string name;
    std::vector<Zone> zones;
};

// RT-safe Note-On selection (architecture 5.4): filters on the Tone key and
// velocity ranges first (second clip), then on the zone's inclusive ranges, and
// prefers the narrowest key range, then the highest velLow, then the earliest
// zone. Note/velocity are MIDI values (note 0..127, velocity 1..127) - the M2
// call site converts the architecture's float vel with
// roundToInt(vel * 127) - and zones without a sample are skipped (placeholder
// ZoneSets are expected to be all-null; see ADR-017). The tone parameters are
// ordered keyLow, keyHigh, velLow, velHigh. Returns nullptr when nothing
// matches.
inline const Zone* selectZone (const ZoneSet& zoneSet, int note, int velocity,
                               int toneKeyLow = kMidiNoteMin, int toneKeyHigh = kMidiNoteMax,
                               int toneVelLow = kMidiVelocityMin, int toneVelHigh = kMidiVelocityMax) noexcept
{
    if (note < kMidiNoteMin || note > kMidiNoteMax
        || velocity < kMidiVelocityMin || velocity > kMidiVelocityMax)
        return nullptr;

    if (note < toneKeyLow || note > toneKeyHigh || velocity < toneVelLow || velocity > toneVelHigh)
        return nullptr;

    const Zone* best = nullptr;
    long long bestKeyWidth = 0;
    int bestVelLow = 0;

    for (const auto& zone : zoneSet.zones)
    {
        if (zone.sample == nullptr)
            continue;

        if (note < zone.keyLow || note > zone.keyHigh
            || velocity < zone.velLow || velocity > zone.velHigh)
            continue;

        const auto keyWidth = (long long) zone.keyHigh - (long long) zone.keyLow;

        if (best == nullptr || keyWidth < bestKeyWidth || (keyWidth == bestKeyWidth && zone.velLow > bestVelLow))
        {
            best = &zone;
            bestKeyWidth = keyWidth;
            bestVelLow = zone.velLow;
        }
    }

    return best;
}
