#pragma once

#include <juce_core/juce_core.h>

#include "Model/SampleLibrary.h"

#include <vector>

class LibraryIndex
{
public:
    static constexpr int kSchemaVersion = 1;

    // Tolerant: a missing, corrupt or future-version file clears entries and
    // returns false. Unknown fields inside an entry are ignored; entries with a
    // hash that is not 64 lowercase hex characters, a ".." path segment, a
    // non-absolute external path or an absolute/rooted internal path are skipped
    // silently. `external` must be a JSON boolean (fail-closed). A thumbnail path
    // that is not root-relative under "thumbnails/" is dropped (the entry is
    // kept). Downgrade note: an older binary reading a new index silently drops
    // external entries (absolute paths fail its relative check); schemaVersion
    // stays 1 until M3-05 revisits migration.
    static bool load (const juce::File& file, std::vector<LibraryEntry>& entries);

    // Atomic: writes to a temporary file and replaces the target. Creates the
    // parent directory when needed.
    static bool save (const juce::File& file, const std::vector<LibraryEntry>& entries);
};
