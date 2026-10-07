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

// Applies a decoded import to the library on the message thread. The decoded
// sample is ALWAYS cached, even when the index already holds the hash: the
// engine's runtime rebuild resolves zones through SampleLibrary::findSample, so
// a duplicate index entry must still get its sample or the tone stays silent
// (M3-01 C1). The index is rewritten only when the entry set actually changes.
inline MergeOutcome publishImportedSample (SampleLibrary& library,
                                           const LibraryEntry& entry,
                                           std::shared_ptr<const Sample> sample,
                                           bool replaceExisting)
{
    library.addSample (entry.fileHash, std::move (sample));

    auto entries = library.getEntries();
    const auto outcome = mergeImportedEntry (entries, entry, replaceExisting);

    if (! outcome.skippedDuplicate)
        library.setEntries (std::move (entries));

    return outcome;
}

// --- transient status line (code-review I-1) --------------------------------

// Import/scan feedback must stay readable: the 10 Hz status timer and the
// change listeners rewrite the label every 100 ms, so a transient message is
// only shown while it is younger than this window.
inline constexpr std::int64_t kStatusMessageDurationMs = 2000;

// First `maxChars` characters of a content hash, for compact status text.
inline juce::String shortHash (const std::string& fileHash, std::size_t maxChars = 8)
{
    return juce::String (fileHash.substr (0, juce::jmin (maxChars, fileHash.size())));
}

// Display policy for the status label: while a transient message is young
// enough it outranks the periodic statistics, afterwards the statistics come
// back. Pure so the editor's behaviour is unit-testable (checklist item 8).
class StatusLineState
{
public:
    // An empty message clears a pending one; a non-positive duration expires
    // immediately (nothing is shown).
    void setMessage (juce::String message, std::int64_t nowMs,
                     std::int64_t durationMs = kStatusMessageDurationMs)
    {
        if (message.isEmpty())
        {
            clear();
            return;
        }

        text = std::move (message);
        expiryMs = nowMs + juce::jmax ((std::int64_t) 0, durationMs);
    }

    void clear() noexcept
    {
        text.clear();
        expiryMs = 0;
    }

    bool hasActiveMessage (std::int64_t nowMs) const noexcept
    {
        return text.isNotEmpty() && nowMs < expiryMs;
    }

    const juce::String& message() const noexcept { return text; }

    juce::String resolve (const juce::String& statsText, std::int64_t nowMs) const
    {
        return hasActiveMessage (nowMs) ? text : statsText;
    }

private:
    juce::String text;
    std::int64_t expiryMs = 0;
};

// Message builders for the import/scan outcomes that must stay visible.
inline juce::String duplicateSkipMessage (const std::string& path)
{
    return "Already in library: " + juce::String (path);
}

inline juce::String importFailureMessage (const std::string& errorMessage)
{
    return "Import failed: " + juce::String (errorMessage);
}

inline juce::String importSuccessMessage (const std::string& path, const std::string& fileHash)
{
    return "Imported: " + juce::String (path) + " (" + shortHash (fileHash) + ")";
}

inline juce::String fileNotFoundMessage (const juce::String& fullPath)
{
    return "File not found: " + fullPath;
}

inline juce::String scanInProgressMessage()
{
    return "Scan in progress; import skipped";
}

inline juce::String scanAlreadyRunningMessage()
{
    return "Scan already in progress";
}

inline juce::String importInProgressMessage()
{
    return "Import in progress; try again";
}

// A scan dropped already-indexed entries because another entry claimed their
// content hash first; the files are still on disk, so the loss is reported
// (code-review I-6).
inline juce::String entriesDroppedMessage (int count)
{
    return "Scan dropped " + juce::String (count)
           + (count == 1 ? " duplicate entry" : " duplicate entries");
}
}
