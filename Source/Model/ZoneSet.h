#pragma once

#include "Model/Sample.h"

#include <memory>
#include <string>
#include <vector>

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
