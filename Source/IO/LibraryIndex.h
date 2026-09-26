#pragma once

#include <juce_core/juce_core.h>

#include "Model/SampleLibrary.h"

#include <vector>

class LibraryIndex
{
public:
    static constexpr int kSchemaVersion = 1;

    // Tolerant: a missing, corrupt or future-version file clears entries and
    // returns false. Unknown fields inside an entry are ignored; entries with an
    // empty hash, an absolute path or a ".." path segment are skipped silently.
    static bool load (const juce::File& file, std::vector<LibraryEntry>& entries);

    // Atomic: writes to a temporary file and replaces the target. Creates the
    // parent directory when needed.
    static bool save (const juce::File& file, const std::vector<LibraryEntry>& entries);
};
