#pragma once

#include <juce_events/juce_events.h>

#include "Model/Sample.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct LibraryEntry
{
    std::string relativePath;      // '/' separated, relative to the library root
    std::string fileHash;          // SHA-256 hex (lowercase) of the file contents
    int rootKey = 60;
    LoopInfo loop;
    double sourceSampleRate = 48000.0;
    std::int64_t lengthSamples = 0;
    std::string thumbnailPath;     // '/' separated, relative to the library root
};

inline constexpr const char* kThumbnailsDirectoryName = "thumbnails";

// Import/scan whitelist (architecture 1.2: WAV/AIFF/FLAC).
inline constexpr const char* kSupportedAudioExtensions[] { ".wav", ".aif", ".aiff", ".flac" };

inline bool isSupportedAudioFile (const juce::String& fileName)
{
    const auto extension = fileName.fromLastOccurrenceOf (".", true, false).toLowerCase();

    for (const auto* supported : kSupportedAudioExtensions)
        if (extension == supported)
            return true;

    return false;
}

// Orphaned thumbnail files younger than this are kept so an in-flight import
// cannot lose its just-written cache file to a concurrent scan.
inline constexpr double kThumbnailOrphanGraceSeconds = 300.0;

struct ScanResult
{
    bool succeeded = false;
    int filesFound = 0;
    int entriesAdded = 0;
    int entriesUpdated = 0;
    int duplicatesSkipped = 0;
    int thumbnailsRemoved = 0;
    std::vector<std::string> missingPaths;
};

// Non-RT only. The audio thread never touches the library; it reads samples
// through a ZoneSet's raw pointers. All public methods are message-thread-only;
// index mutations (and the sample cache) are not allowed while a background
// scan is in flight. The last reference to a removed sample is released on the
// message thread (architecture 3.3 rule 6).
class SampleLibrary : public juce::ChangeBroadcaster
{
public:
    SampleLibrary();
    ~SampleLibrary() override;

    SampleLibrary (const SampleLibrary&) = delete;
    SampleLibrary& operator= (const SampleLibrary&) = delete;

    // Message thread only; must not be called while a scan is in flight.
    void setRootDirectory (const juce::File& newRootDirectory);
    juce::File getRootDirectory() const;

    // Message thread only; must not be called while a scan is in flight.
    void setEntries (std::vector<LibraryEntry> newEntries);
    const std::vector<LibraryEntry>& getEntries() const noexcept;

    // Message thread only; must not be called while a scan is in flight.
    // Merges a synchronous scan into the index. Every successful scan also
    // removes orphaned thumbnail files older than kThumbnailOrphanGraceSeconds.
    ScanResult scanNow();

    // Message thread only; deletes thumbnail cache files that no entry
    // references and that are at least minimumAgeSeconds old. Also runs
    // automatically after each successful scan with the default grace period.
    int cleanupOrphanedThumbnails (double minimumAgeSeconds = kThumbnailOrphanGraceSeconds);

    // Starts a background scan. A call while a scan is already in flight is
    // ignored. The result is applied on the message thread, then onScanComplete
    // runs and ChangeBroadcaster listeners are notified. The completion callback
    // must not destroy the library.
    void startScan();
    bool isScanning() const noexcept;

    // Message thread only; waits for a scan to complete naturally (no
    // cancellation) and applies a pending result.
    void waitForScanToFinish();

    // Imported sample cache (published by the importer, M1-03). Message thread
    // only: the importer marshals publication through the message thread so the
    // final release also happens there (architecture 3.3 rule 6). The library
    // holds one reference; removing it does not free samples still referenced
    // by zones or runtimes.
    void addSample (const std::string& fileHash, std::shared_ptr<const Sample> sample);
    std::shared_ptr<const Sample> findSample (const std::string& fileHash) const;
    void removeSample (const std::string& fileHash);

    std::function<void (const ScanResult&)> onScanComplete;

private:
    class ScanThread;

    struct PendingScan
    {
        std::mutex mutex;
        ScanResult result;
        std::vector<LibraryEntry> entries;
        bool ready = false;
        bool cancelled = false;
        bool applied = false;
    };

    void applyPending (const std::shared_ptr<PendingScan>& pending);

    JUCE_DECLARE_WEAK_REFERENCEABLE (SampleLibrary)

    juce::File rootDirectory;
    std::vector<LibraryEntry> entries;
    std::map<std::string, std::shared_ptr<const Sample>> samples;
    std::unique_ptr<ScanThread> scanThread;
    std::shared_ptr<PendingScan> pendingScan;
    std::atomic<bool> scanning { false };
};
