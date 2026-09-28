#include <catch2/catch_test_macros.hpp>

#include "Plugin/SamplerUiHelpers.h"

#include <string>
#include <vector>

namespace
{
LibraryEntry makeEntry (const std::string& path, const std::string& hash)
{
    LibraryEntry entry;
    entry.path = path;
    entry.fileHash = hash;
    entry.rootKey = 36;
    entry.sourceSampleRate = 48000.0;
    entry.lengthSamples = 48000;
    entry.loop.start = 10;
    entry.loop.end = 20;
    return entry;
}
}

TEST_CASE ("sampler ui formats sample durations")
{
    CHECK (SamplerUi::formatDuration (48000, 48000.0) == "1.00 s");
    CHECK (SamplerUi::formatDuration (24000, 48000.0) == "0.50 s");
    CHECK (SamplerUi::formatDuration (0, 48000.0) == "-");
    CHECK (SamplerUi::formatDuration (48000, 0.0) == "-");
    CHECK (SamplerUi::formatDuration (-5, 48000.0) == "-");
}

TEST_CASE ("sampler ui labels include loop, crossfade, external and missing markers")
{
    auto internal = makeEntry ("Piano/C2.wav", "aa");
    internal.loop.crossfadeSamples = 5;

    const auto internalLabel = SamplerUi::entryLabel (internal, false);
    CHECK (internalLabel.contains ("Piano/C2.wav"));
    CHECK (internalLabel.contains ("key 36"));
    CHECK (internalLabel.contains ("1.00 s"));
    CHECK (internalLabel.contains ("loop 10..20"));
    CHECK (internalLabel.contains ("+xf 5"));
    CHECK_FALSE (internalLabel.contains ("[ext]"));
    CHECK_FALSE (internalLabel.contains ("(missing)"));

    auto external = makeEntry ("C:/Samples/Pad.wav", "bb");
    external.external = true;

    const auto externalLabel = SamplerUi::entryLabel (external, true);
    CHECK (externalLabel.contains ("[ext] "));
    CHECK (externalLabel.contains ("(missing)"));
    CHECK_FALSE (externalLabel.contains ("+xf"));
}

TEST_CASE ("sampler ui detects existing hashes")
{
    const std::vector<LibraryEntry> entries { makeEntry ("A.wav", "aa"), makeEntry ("B.wav", "bb") };

    CHECK (SamplerUi::containsHash (entries, "aa"));
    CHECK (SamplerUi::containsHash (entries, "bb"));
    CHECK_FALSE (SamplerUi::containsHash (entries, "cc"));
    CHECK_FALSE (SamplerUi::containsHash ({}, "aa"));
}

TEST_CASE ("sampler ui merge appends new hashes")
{
    std::vector<LibraryEntry> entries { makeEntry ("A.wav", "aa") };

    const auto outcome = SamplerUi::mergeImportedEntry (entries, makeEntry ("B.wav", "bb"), false);

    CHECK (outcome.added);
    CHECK_FALSE (outcome.updated);
    CHECK_FALSE (outcome.skippedDuplicate);
    REQUIRE (entries.size() == 2);
    CHECK (entries[1].path == "B.wav");
}

TEST_CASE ("sampler ui merge skips duplicates by default")
{
    std::vector<LibraryEntry> entries { makeEntry ("A.wav", "aa") };

    auto incoming = makeEntry ("Other/A.wav", "aa");
    incoming.rootKey = 60;

    const auto outcome = SamplerUi::mergeImportedEntry (entries, incoming, false);

    CHECK (outcome.skippedDuplicate);
    CHECK_FALSE (outcome.added);
    CHECK_FALSE (outcome.updated);
    REQUIRE (entries.size() == 1);
    CHECK (entries[0].path == "A.wav");
    CHECK (entries[0].rootKey == 36);   // existing metadata untouched
}

TEST_CASE ("sampler ui merge replaces duplicates when requested")
{
    std::vector<LibraryEntry> entries { makeEntry ("A.wav", "aa") };

    auto incoming = makeEntry ("Other/A.wav", "aa");
    incoming.rootKey = 60;

    const auto outcome = SamplerUi::mergeImportedEntry (entries, incoming, true);

    CHECK (outcome.updated);
    CHECK_FALSE (outcome.added);
    CHECK_FALSE (outcome.skippedDuplicate);
    REQUIRE (entries.size() == 1);
    CHECK (entries[0].path == "Other/A.wav");
    CHECK (entries[0].rootKey == 60);
}
