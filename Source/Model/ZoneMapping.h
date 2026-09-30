#pragma once

#include "Model/SampleLibrary.h"
#include "Model/ZoneSet.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Filename-based note parsing and automatic Zone generation (architecture 6.2).
// Pure and message-thread friendly; the editor owns the resulting draft.
namespace ZoneMapping
{
// Parses a note name from a filename (with optional extension). A token must
// start a word, i.e. sit at index 0 or after a character that is not a letter
// or digit, so the letter+digit pairs inside "Piano_C4_Take2.wav" are not read
// as E2. Among the remaining tokens the one that ends last wins,
// e.g. "Piano_C#4_v2.wav" -> 61. C4 = MIDI 60 (scientific pitch), sharps are
// '#', '+', 's'; flats are 'b'; octave -1..9. Returns -1 when no valid note
// exists or the result is outside MIDI 0..127.
inline int parseRootKeyFromFileName (const juce::String& fileName)
{
    if (fileName.isEmpty())
        return -1;

    auto stem = fileName;
    const auto lastSlash = juce::jmax (stem.lastIndexOfChar ('/'), stem.lastIndexOfChar ('\\'));

    if (lastSlash >= 0)
        stem = stem.substring (lastSlash + 1);

    const auto lastDot = stem.lastIndexOfChar ('.');

    if (lastDot > 0)
        stem = stem.substring (0, lastDot);

    stem = stem.toLowerCase();

    const auto length = stem.length();
    int bestEnd = -1;
    int bestStart = -1;
    int bestKey = -1;

    for (int i = 0; i < length; ++i)
    {
        // Token boundary (code-review I-2): a note name must start a word. Inside
        // a word the letter+digit pairs of "take2"/"mic2"/"Analog2" would
        // otherwise be read as e2/c2/g2 and override (or invent) the root key.
        if (i > 0 && juce::CharacterFunctions::isLetterOrDigit (stem[i - 1]))
            continue;

        int semitone = -1;

        switch (stem[i])
        {
            case 'c': semitone = 0; break;
            case 'd': semitone = 2; break;
            case 'e': semitone = 4; break;
            case 'f': semitone = 5; break;
            case 'g': semitone = 7; break;
            case 'a': semitone = 9; break;
            case 'b': semitone = 11; break;
            default: continue;
        }

        auto j = i + 1;

        if (j < length && (stem[j] == '#' || stem[j] == '+' || stem[j] == 's'))
        {
            ++semitone;
            ++j;
        }
        else if (j < length && stem[j] == 'b')
        {
            --semitone;
            ++j;
        }

        auto negative = false;

        if (j < length && stem[j] == '-')
        {
            negative = true;
            ++j;
        }

        if (j >= length || ! juce::CharacterFunctions::isDigit (stem[j]))
            continue;

        auto octave = 0;

        while (j < length && juce::CharacterFunctions::isDigit (stem[j]))
        {
            if (octave < 1000)
                octave = octave * 10 + (stem[j] - '0');

            ++j;
        }

        if (negative)
            octave = -octave;

        if (j < length && juce::CharacterFunctions::isLetterOrDigit (stem[j]))
            continue;

        const auto key = 12 * (octave + 1) + semitone;

        if (key < kMidiNoteMin || key > kMidiNoteMax)
            continue;

        // Prefer the token that ends last; break ties with the longest token
        // (smallest start) so "Db2" parses as D-flat rather than B. Both tokens
        // are already known to start a word (see the boundary check above).
        if (j > bestEnd || (j == bestEnd && (bestStart < 0 || i < bestStart)))
        {
            bestEnd = j;
            bestStart = i;
            bestKey = key;
        }
    }

    return bestKey;
}

struct AutoMapOptions
{
    // Optional decoded-sample resolver (usually SampleLibrary::findSample).
    std::function<std::shared_ptr<const Sample> (const std::string& fileHash)> resolveSample;
};

// Builds a draft ZoneSet: named entries become single-key zones (root key from
// the filename); several entries mapping to the same key split [1..127] into
// equal velocity layers (sorted by path); unnamed entries form one full-range
// group that is split the same way.
inline ZoneSet buildAutoMappedZoneSet (const std::vector<LibraryEntry>& entries,
                                       const AutoMapOptions& options = {})
{
    ZoneSet zoneSet;
    zoneSet.name = "Auto-Mapped";

    std::map<int, std::vector<const LibraryEntry*>> namedGroups;
    std::vector<const LibraryEntry*> unnamed;

    for (const auto& entry : entries)
    {
        const auto key = parseRootKeyFromFileName (juce::String (entry.path));

        if (key >= 0)
            namedGroups[key].push_back (&entry);
        else
            unnamed.push_back (&entry);
    }

    const auto resolve = [&options] (const LibraryEntry& entry)
    {
        return options.resolveSample ? options.resolveSample (entry.fileHash) : nullptr;
    };

    const auto addGroup = [&] (std::vector<const LibraryEntry*> group, int keyLow, int keyHigh, int rootKeyOverride)
    {
        std::sort (group.begin(), group.end(),
                   [] (const LibraryEntry* a, const LibraryEntry* b) { return a->path < b->path; });

        const auto count = juce::jmin ((int) group.size(), kMidiVelocityMax);

        for (int i = 0; i < count; ++i)
        {
            const auto& entry = *group[(std::size_t) i];

            Zone zone;
            zone.sample = resolve (entry);
            zone.keyLow = keyLow;
            zone.keyHigh = keyHigh;
            zone.velLow = kMidiVelocityMin + (i * kMidiVelocityMax) / count;
            zone.velHigh = ((i + 1) * kMidiVelocityMax) / count;
            zone.rootKeyOverride = rootKeyOverride;
            zone.loop = entry.loop;   // Zone::loop is the effective region (ADR-014)
            zoneSet.zones.push_back (std::move (zone));
        }
    };

    // The unnamed full-range group is added first so the named single-key zones
    // are drawn (and hit-tested) last and stay selectable.
    if (! unnamed.empty())
        addGroup (unnamed, kMidiNoteMin, kMidiNoteMax, -1);

    for (const auto& [key, group] : namedGroups)
        addGroup (group, key, key, key);

    return zoneSet;
}
}
