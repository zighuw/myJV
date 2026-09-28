#include <catch2/catch_test_macros.hpp>

#include "Model/ZoneSet.h"

#include <memory>

namespace
{
Zone makeZone (int keyLow, int keyHigh, int velLow, int velHigh, bool withSample = true)
{
    Zone zone;
    zone.keyLow = keyLow;
    zone.keyHigh = keyHigh;
    zone.velLow = velLow;
    zone.velHigh = velHigh;

    if (withSample)
        zone.sample = std::make_shared<Sample>();

    return zone;
}
}

TEST_CASE ("selects the matching zone")
{
    ZoneSet set;
    set.zones.push_back (makeZone (0, 127, 1, 127));

    const auto* selected = selectZone (set, 60, 100);

    REQUIRE (selected != nullptr);
    CHECK (selected == &set.zones[0]);
}

TEST_CASE ("returns null when nothing matches")
{
    ZoneSet empty;
    CHECK (selectZone (empty, 60, 100) == nullptr);

    ZoneSet set;
    set.zones.push_back (makeZone (60, 72, 1, 127));

    CHECK (selectZone (set, 59, 100) == nullptr);   // below the key range
    CHECK (selectZone (set, 73, 100) == nullptr);   // above the key range
    CHECK (selectZone (set, 60, 0) == nullptr);     // note-on velocity 0
    CHECK (selectZone (set, 60, 128) == nullptr);   // velocity out of MIDI range
    CHECK (selectZone (set, -1, 100) == nullptr);   // note out of MIDI range
    CHECK (selectZone (set, 128, 100) == nullptr);

    set.zones[0].velLow = 64;
    set.zones[0].velHigh = 100;
    CHECK (selectZone (set, 60, 63) == nullptr);    // below the velocity range
    CHECK (selectZone (set, 60, 101) == nullptr);   // above the velocity range
    CHECK (selectZone (set, 60, 64) == &set.zones[0]);
    CHECK (selectZone (set, 60, 100) == &set.zones[0]);
}

TEST_CASE ("prefers the narrowest key range")
{
    ZoneSet set;
    set.zones.push_back (makeZone (0, 127, 1, 127));   // width 127
    set.zones.push_back (makeZone (60, 72, 1, 127));   // width 12
    set.zones.push_back (makeZone (55, 90, 1, 127));   // width 35

    CHECK (selectZone (set, 60, 100) == &set.zones[1]);
}

TEST_CASE ("prefers the highest velLow among equal key ranges")
{
    ZoneSet set;
    set.zones.push_back (makeZone (60, 72, 1, 127));
    set.zones.push_back (makeZone (60, 72, 64, 127));
    set.zones.push_back (makeZone (60, 72, 32, 127));

    CHECK (selectZone (set, 60, 100) == &set.zones[1]);
    CHECK (selectZone (set, 60, 63) == &set.zones[2]);
}

TEST_CASE ("prefers the earliest zone when fully tied")
{
    ZoneSet set;
    set.zones.push_back (makeZone (60, 72, 1, 127));
    set.zones.push_back (makeZone (60, 72, 1, 127));

    CHECK (selectZone (set, 60, 100) == &set.zones[0]);
}

TEST_CASE ("skips zones without a sample")
{
    ZoneSet set;
    set.zones.push_back (makeZone (60, 61, 1, 127, false));   // narrowest, but empty
    set.zones.push_back (makeZone (60, 72, 1, 127, true));

    CHECK (selectZone (set, 60, 100) == &set.zones[1]);

    ZoneSet allEmpty;
    allEmpty.zones.push_back (makeZone (0, 127, 1, 127, false));
    CHECK (selectZone (allEmpty, 60, 100) == nullptr);
}

TEST_CASE ("applies the tone key and velocity ranges as a second clip")
{
    ZoneSet set;
    set.zones.push_back (makeZone (0, 127, 1, 127));

    CHECK (selectZone (set, 60, 100, 0, 127, 1, 127) == &set.zones[0]);
    CHECK (selectZone (set, 60, 100, 0, 59, 1, 127) == nullptr);     // tone key range excludes
    CHECK (selectZone (set, 60, 100, 61, 127, 1, 127) == nullptr);
    CHECK (selectZone (set, 60, 100, 0, 127, 101, 127) == nullptr);  // tone velocity range excludes
    CHECK (selectZone (set, 60, 100, 0, 127, 100, 127) == &set.zones[0]);
    CHECK (selectZone (set, 60, 100, 0, 127, 1, 100) == &set.zones[0]);   // inclusive tone velHigh
    CHECK (selectZone (set, 60, 101, 0, 127, 1, 100) == nullptr);
    CHECK (selectZone (set, 60, 100, 72, 60, 1, 127) == nullptr);    // inverted tone key range
    CHECK (selectZone (set, 60, 100, 0, 127, 127, 1) == nullptr);    // inverted tone velocity range
}

TEST_CASE ("rejects MIDI out-of-range inputs before filtering")
{
    ZoneSet set;
    set.zones.push_back (makeZone (-5, 5, 0, 127));   // malformed, would otherwise match

    // Tone ranges extended past MIDI make the MIDI guard observable.
    CHECK (selectZone (set, -1, 100, -10, 127, 1, 127) == nullptr);
    CHECK (selectZone (set, 0, 0, 0, 127, 0, 127) == nullptr);
    CHECK (selectZone (set, 0, 1) == &set.zones[0]);
}

TEST_CASE ("rejects zones with inverted key ranges")
{
    ZoneSet set;
    set.zones.push_back (makeZone (72, 60, 1, 127));

    CHECK (selectZone (set, 60, 100) == nullptr);
    CHECK (selectZone (set, 72, 100) == nullptr);
}

TEST_CASE ("velHigh filters but does not break ties")
{
    ZoneSet set;
    set.zones.push_back (makeZone (60, 72, 1, 80));
    set.zones.push_back (makeZone (60, 72, 1, 127));

    CHECK (selectZone (set, 60, 70) == &set.zones[0]);    // both match, earliest wins
    CHECK (selectZone (set, 60, 100) == &set.zones[1]);   // first filtered out by velHigh
}

TEST_CASE ("skips invalid zones before selecting a valid one")
{
    ZoneSet set;
    set.zones.push_back (makeZone (60, 61, 1, 127, false));   // narrowest but empty
    set.zones.push_back (makeZone (72, 60, 1, 127));          // inverted keys
    set.zones.push_back (makeZone (60, 72, 1, 127));

    CHECK (selectZone (set, 60, 100) == &set.zones[2]);
}

TEST_CASE ("handles single-key zones and range boundaries")
{
    ZoneSet set;
    set.zones.push_back (makeZone (60, 60, 1, 127));

    CHECK (selectZone (set, 60, 1) == &set.zones[0]);
    CHECK (selectZone (set, 60, 127) == &set.zones[0]);
    CHECK (selectZone (set, 59, 100) == nullptr);
    CHECK (selectZone (set, 61, 100) == nullptr);

    ZoneSet full;
    full.zones.push_back (makeZone (0, 127, 1, 127));
    CHECK (selectZone (full, 0, 1) == &full.zones[0]);
    CHECK (selectZone (full, 127, 127) == &full.zones[0]);
}
