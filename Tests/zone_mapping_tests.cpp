#include <catch2/catch_test_macros.hpp>

#include "Model/SampleLibrary.h"
#include "Model/ZoneMapping.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
LibraryEntry makeEntry (const std::string& path, const std::string& hash = "hash")
{
    LibraryEntry entry;
    entry.path = path;
    entry.fileHash = hash;
    entry.sourceSampleRate = 48000.0;
    entry.lengthSamples = 4800;
    return entry;
}
}

TEST_CASE ("note names parse with C4 = 60")
{
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Piano_C4.wav") == 60);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Piano_C#4.wav") == 61);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Piano_Db4.wav") == 61);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Piano_Cs4.wav") == 61);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("A4.aif") == 69);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("C-1.flac") == 0);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("C#-1.flac") == 1);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("G9.wav") == 127);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("pad_c3.WAV") == 48);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Bass-Db2.wav") == 37);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Kick_C4_v2.wav") == 60);
}

TEST_CASE ("invalid note names are rejected")
{
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Kick.wav") == -1);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("Snare_C.wav") == -1);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("TooHigh_G10.wav") == -1);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("TooLow_C-2.wav") == -1);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("C4x.wav") == -1);
    CHECK (ZoneMapping::parseRootKeyFromFileName ("") == -1);
    CHECK (ZoneMapping::parseRootKeyFromFileName (".wav") == -1);
}

TEST_CASE ("automap is empty for an empty library")
{
    ZoneMapping::AutoMapOptions options;
    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet ({}, options);

    CHECK (zoneSet.zones.empty());
}

TEST_CASE ("automap creates single-key zones with root key overrides")
{
    const std::vector<LibraryEntry> entries { makeEntry ("Piano_C4.wav", "aa"), makeEntry ("Strings_A4.wav", "bb") };

    ZoneMapping::AutoMapOptions options;
    options.resolveSample = [] (const std::string& hash)
    {
        auto sample = std::make_shared<Sample>();
        sample->fileHash = hash;
        return sample;
    };

    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet (entries, options);

    REQUIRE (zoneSet.zones.size() == 2);

    CHECK (zoneSet.zones[0].keyLow == 60);
    CHECK (zoneSet.zones[0].keyHigh == 60);
    CHECK (zoneSet.zones[0].keyLow == zoneSet.zones[0].keyHigh);
    CHECK (zoneSet.zones[0].rootKeyOverride == 60);
    CHECK (zoneSet.zones[0].velLow == 1);
    CHECK (zoneSet.zones[0].velHigh == 127);
    REQUIRE (zoneSet.zones[0].sample != nullptr);
    CHECK (zoneSet.zones[0].sample->fileHash == "aa");

    CHECK (zoneSet.zones[1].keyLow == 69);
    CHECK (zoneSet.zones[1].rootKeyOverride == 69);
}

TEST_CASE ("automap splits velocity layers for the same key")
{
    auto hard = makeEntry ("Piano_C4_hard.wav", "c");
    auto mid = makeEntry ("Piano_C4_mid.wav", "b");
    auto soft = makeEntry ("Piano_C4_soft.wav", "a");
    hard.loop.start = 10;
    hard.loop.end = 20;
    hard.loop.crossfadeSamples = 5;

    const std::vector<LibraryEntry> entries { hard, soft, mid };

    ZoneMapping::AutoMapOptions options;
    options.resolveSample = [] (const std::string& hash)
    {
        auto sample = std::make_shared<Sample>();
        sample->fileHash = hash;
        return sample;
    };

    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet (entries, options);

    REQUIRE (zoneSet.zones.size() == 3);

    // Sorted by path: hard < mid < soft.
    CHECK (zoneSet.zones[0].velLow == 1);
    CHECK (zoneSet.zones[0].velHigh == 42);
    CHECK (zoneSet.zones[1].velLow == 43);
    CHECK (zoneSet.zones[1].velHigh == 84);
    CHECK (zoneSet.zones[2].velLow == 85);
    CHECK (zoneSet.zones[2].velHigh == 127);

    REQUIRE (zoneSet.zones[0].sample != nullptr);
    REQUIRE (zoneSet.zones[1].sample != nullptr);
    REQUIRE (zoneSet.zones[2].sample != nullptr);
    CHECK (zoneSet.zones[0].sample->fileHash == "c");
    CHECK (zoneSet.zones[1].sample->fileHash == "b");
    CHECK (zoneSet.zones[2].sample->fileHash == "a");

    // The effective loop region is carried over from the library entry.
    CHECK (zoneSet.zones[0].loop.start == 10);
    CHECK (zoneSet.zones[0].loop.end == 20);
    CHECK (zoneSet.zones[0].loop.crossfadeSamples == 5);

    for (const auto& zone : zoneSet.zones)
    {
        CHECK (zone.keyLow == 60);
        CHECK (zone.keyHigh == 60);
        CHECK (zone.rootKeyOverride == 60);
    }
}

TEST_CASE ("automap orders named groups by ascending root key")
{
    const std::vector<LibraryEntry> entries { makeEntry ("Pad_C4.wav", "aa"), makeEntry ("Pad_G3.wav", "bb") };

    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet (entries);

    REQUIRE (zoneSet.zones.size() == 2);
    CHECK (zoneSet.zones[0].rootKeyOverride == 55);   // G3
    CHECK (zoneSet.zones[1].rootKeyOverride == 60);   // C4
}

TEST_CASE ("automap caps velocity layers at 127")
{
    std::vector<LibraryEntry> entries;

    for (int i = 0; i < 130; ++i)
        entries.push_back (makeEntry ("Pad_C4_" + std::to_string (i) + ".wav", "h" + std::to_string (i)));

    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet (entries);

    REQUIRE (zoneSet.zones.size() == 127);
    CHECK (zoneSet.zones.front().velLow == 1);
    CHECK (zoneSet.zones.front().velHigh == 1);
    CHECK (zoneSet.zones.back().velLow == 127);
    CHECK (zoneSet.zones.back().velHigh == 127);

    for (const auto& zone : zoneSet.zones)
        CHECK (zone.velLow <= zone.velHigh);
}

TEST_CASE ("automap gives unnamed entries full-range velocity layers")
{
    const std::vector<LibraryEntry> entries { makeEntry ("Kick.wav", "aa"), makeEntry ("Snare.wav", "bb") };

    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet (entries);

    REQUIRE (zoneSet.zones.size() == 2);

    CHECK (zoneSet.zones[0].keyLow == 0);
    CHECK (zoneSet.zones[0].keyHigh == 127);
    CHECK (zoneSet.zones[0].velLow == 1);
    CHECK (zoneSet.zones[0].velHigh == 63);
    CHECK (zoneSet.zones[1].keyLow == 0);
    CHECK (zoneSet.zones[1].keyHigh == 127);
    CHECK (zoneSet.zones[1].velLow == 64);
    CHECK (zoneSet.zones[1].velHigh == 127);
    CHECK (zoneSet.zones[0].rootKeyOverride == -1);
}

TEST_CASE ("automap keeps named and unnamed entries separate")
{
    const std::vector<LibraryEntry> entries { makeEntry ("Piano_C4.wav", "aa"), makeEntry ("Noise.wav", "bb") };

    const auto zoneSet = ZoneMapping::buildAutoMappedZoneSet (entries);

    REQUIRE (zoneSet.zones.size() == 2);

    // The unnamed full-range group is added first so named zones stay selectable.
    CHECK (zoneSet.zones[0].keyLow == 0);
    CHECK (zoneSet.zones[0].keyHigh == 127);
    CHECK (zoneSet.zones[0].rootKeyOverride == -1);
    CHECK ((zoneSet.zones[0].sample == nullptr || zoneSet.zones[0].sample->fileHash == "bb"));
    CHECK (zoneSet.zones[1].keyLow == 60);
    CHECK (zoneSet.zones[1].keyHigh == 60);
}
