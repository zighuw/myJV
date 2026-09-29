#pragma once

#include "Model/SampleLibrary.h"

#include <cstdint>
#include <string>
#include <vector>

// Pure helpers for the Sampler panel, kept out of the editor so they are
// unit-testable (checklist item 8).
namespace SamplerUi
{
inline juce::String formatDuration (std::int64_t lengthSamples, double sampleRate)
{
    if (sampleRate <= 0.0 || lengthSamples <= 0)
        return "-";

    return juce::String ((double) lengthSamples / sampleRate, 2) + " s";
}

inline juce::String entryLabel (const LibraryEntry& entry, bool missing)
{
    juce::String text;

    if (entry.external)
        text << "[ext] ";

    text << entry.path
         << "   key " << entry.rootKey
         << "   " << formatDuration (entry.lengthSamples, entry.sourceSampleRate)
         << "   loop " << entry.loop.start << ".." << entry.loop.end;

    if (entry.loop.crossfadeSamples > 0)
        text << " +xf " << entry.loop.crossfadeSamples;

    if (missing)
        text << "   (missing)";

    return text;
}

inline bool containsHash (const std::vector<LibraryEntry>& entries, const std::string& fileHash)
{
    for (const auto& entry : entries)
        if (entry.fileHash == fileHash)
            return true;

    return false;
}

struct MergeOutcome
{
    bool added = false;
    bool updated = false;
    bool skippedDuplicate = false;
};

// Merges an imported entry into the index: a new hash appends; an existing hash
// is skipped unless replaceExisting is set (lazy-audition metadata refresh).
inline MergeOutcome mergeImportedEntry (std::vector<LibraryEntry>& entries,
                                        const LibraryEntry& incoming,
                                        bool replaceExisting)
{
    MergeOutcome outcome;

    for (auto& entry : entries)
    {
        if (entry.fileHash != incoming.fileHash)
            continue;

        if (replaceExisting)
        {
            entry = incoming;
            outcome.updated = true;
        }
        else
        {
            outcome.skippedDuplicate = true;
        }

        return outcome;
    }

    entries.push_back (incoming);
    outcome.added = true;
    return outcome;
}
}
