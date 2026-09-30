#include "Model/SampleLibrary.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <set>
#include <utility>

namespace
{
std::string normalisedRelativePath (const juce::File& root, const juce::File& file)
{
    return file.getRelativePathFrom (root).replaceCharacter ('\\', '/').toStdString();
}

// SHA-256 of a file, with cooperative cancellation. JUCE has no incremental
// SHA256 update(), so the stream is wrapped instead: once shouldCancel() fires
// the wrapper reports EOF and the caller discards the partial digest rather than
// writing a wrong hash into the index (code-review I-7).
std::string hashFile (const juce::File& file, const std::function<bool()>& shouldCancel, bool& cancelled)
{
    cancelled = false;

    class CancellableInputStream final : public juce::InputStream
    {
    public:
        CancellableInputStream (const juce::File& sourceFile, const std::function<bool()>& cancelCheckIn)
            : source (sourceFile), cancelCheck (cancelCheckIn)
        {
        }

        bool wasCancelled() const noexcept { return cancelled; }
        bool openedOk() const noexcept { return source.openedOk(); }

        int read (void* buffer, int maxBytes) override
        {
            if (cancelCheck != nullptr && cancelCheck())
            {
                cancelled = true;
                return 0;   // report EOF; the digest is thrown away below
            }

            return source.read (buffer, maxBytes);
        }

        juce::int64 getTotalLength() override { return source.getTotalLength(); }
        bool isExhausted() override { return cancelled || source.isExhausted(); }
        juce::int64 getPosition() override { return source.getPosition(); }
        bool setPosition (juce::int64 newPosition) override { return source.setPosition (newPosition); }

    private:
        juce::FileInputStream source;
        const std::function<bool()>& cancelCheck;
        bool cancelled = false;
    };

    CancellableInputStream stream (file, shouldCancel);

    if (! stream.openedOk())
        return juce::SHA256 (file).toHexString().toStdString();   // unchanged fallback

    const juce::SHA256 digest (stream);

    if (stream.wasCancelled())
    {
        cancelled = true;
        return {};
    }

    return digest.toHexString().toStdString();
}

juce::File resolveEntryFile (const juce::File& root, const LibraryEntry& entry)
{
    // Explicit branch (self-documenting): juce::File::getChildFile would also
    // return an absolute child unchanged, making external/internal resolution
    // behaviourally identical here.
    return entry.external ? juce::File (juce::String (entry.path))
                          : root.getChildFile (juce::String (entry.path));
}

struct ScanOutcome
{
    ScanResult result;
    std::vector<LibraryEntry> entries;
};

int removeOrphanedThumbnails (const juce::File& root, const std::vector<LibraryEntry>& entries,
                              double minimumAgeSeconds)
{
    const auto directory = root.getChildFile (kThumbnailsDirectoryName);

    if (! directory.isDirectory())
        return 0;

    std::set<juce::String> referenced;

    for (const auto& entry : entries)
        if (! entry.thumbnailPath.empty())
            referenced.insert (juce::String (entry.thumbnailPath).replaceCharacter ('\\', '/'));

    const auto now = juce::Time::getCurrentTime();
    int removed = 0;

    for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*"))
    {
        if (! file.hasFileExtension (".png"))
            continue;

        const auto relative = file.getRelativePathFrom (root).replaceCharacter ('\\', '/');

        if (referenced.count (relative) != 0)
            continue;

        if ((now - file.getLastModificationTime()).inSeconds() < minimumAgeSeconds)
            continue;

        if (file.deleteFile())
            ++removed;
    }

    return removed;
}

ScanOutcome performScan (const juce::File& root,
                         std::vector<LibraryEntry> existingEntries,
                         const juce::Thread* thread)
{
    ScanOutcome outcome;
    outcome.entries = std::move (existingEntries);

    if (! root.isDirectory())
        return outcome;

    const auto shouldStop = [thread] { return thread != nullptr && thread->threadShouldExit(); };

    std::vector<std::pair<std::string, juce::File>> files;

    for (const auto& file : root.findChildFiles (juce::File::findFiles, true, "*",
                                                 juce::File::FollowSymlinks::no))
    {
        if (shouldStop())
            return outcome;

        if (isSupportedAudioFile (file.getFileName()))
            files.emplace_back (normalisedRelativePath (root, file), file);
    }

    std::sort (files.begin(), files.end(),
               [] (const auto& a, const auto& b) { return a.first < b.first; });

    outcome.result.filesFound = (int) files.size();

    std::map<std::string, std::string> claimedHashes;   // hash -> canonical path
    std::set<std::string> existingPaths;
    std::vector<LibraryEntry> kept;
    kept.reserve (outcome.entries.size());

    // Existing entries keep their canonical claim so entry metadata survives rescans.
    for (auto& entry : outcome.entries)
    {
        if (shouldStop())
            return outcome;

        existingPaths.insert (entry.path);

        const auto file = resolveEntryFile (root, entry);

        if (file.existsAsFile())
        {
            bool cancelled = false;
            const auto hash = hashFile (file, shouldStop, cancelled);

            if (cancelled)
                return outcome;   // aborted: nothing of this scan may be applied

            if (! claimedHashes.emplace (hash, entry.path).second)
            {
                ++outcome.result.duplicatesSkipped;
                ++outcome.result.entriesDropped;
                continue;
            }

            if (hash != entry.fileHash)
            {
                entry.fileHash = hash;
                ++outcome.result.entriesUpdated;
            }
        }
        // Missing entries keep their place without claiming their hash, so a
        // present file with the same contents can still be indexed (relocation
        // candidate) instead of being silently suppressed.

        kept.push_back (entry);
    }

    for (const auto& [path, file] : files)
    {
        if (shouldStop())
            return outcome;

        if (existingPaths.count (path) != 0)
            continue;

        bool cancelled = false;
        const auto hash = hashFile (file, shouldStop, cancelled);

        if (cancelled)
            return outcome;   // aborted: nothing of this scan may be applied

        if (! claimedHashes.emplace (hash, path).second)
        {
            ++outcome.result.duplicatesSkipped;
            continue;
        }

        LibraryEntry entry;
        entry.path = path;
        entry.fileHash = hash;
        kept.push_back (entry);
        ++outcome.result.entriesAdded;
    }

    for (const auto& entry : kept)
        if (! resolveEntryFile (root, entry).existsAsFile())
            outcome.result.missingPaths.push_back (entry.path);

    outcome.result.thumbnailsRemoved = removeOrphanedThumbnails (root, kept, kThumbnailOrphanGraceSeconds);

    outcome.entries = std::move (kept);
    outcome.result.succeeded = true;
    return outcome;
}
}

class SampleLibrary::ScanThread final : public juce::Thread
{
public:
    ScanThread (SampleLibrary& ownerToUse, juce::File root,
                std::vector<LibraryEntry> existingEntries,
                std::shared_ptr<PendingScan> pendingResult)
        : juce::Thread ("myJV library scan"),
          library (ownerToUse),
          rootDirectory (std::move (root)),
          entries (std::move (existingEntries)),
          pending (std::move (pendingResult))
    {
    }

    void run() override
    {
        auto outcome = performScan (rootDirectory, std::move (entries), this);

        {
            const std::lock_guard lock (pending->mutex);

            if (threadShouldExit())
            {
                pending->cancelled = true;
                return;
            }

            pending->result = outcome.result;
            pending->entries = std::move (outcome.entries);
            pending->ready = true;
        }

        const juce::WeakReference<SampleLibrary> weakLibrary { &library };
        juce::MessageManager::callAsync ([weakLibrary, pending = pending]
        {
            if (auto* self = weakLibrary.get())
                self->applyPending (pending);
        });
    }

private:
    SampleLibrary& library;
    juce::File rootDirectory;
    std::vector<LibraryEntry> entries;
    std::shared_ptr<PendingScan> pending;
};

SampleLibrary::SampleLibrary() = default;

SampleLibrary::~SampleLibrary()
{
    if (scanThread != nullptr)
    {
        scanThread->stopThread (-1);
        scanThread.reset();
    }
}

void SampleLibrary::setRootDirectory (const juce::File& newRootDirectory)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    jassert (! isScanning());

    rootDirectory = newRootDirectory;
}

juce::File SampleLibrary::getRootDirectory() const
{
    return rootDirectory;
}

void SampleLibrary::setEntries (std::vector<LibraryEntry> newEntries)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    jassert (! isScanning());

    entries = std::move (newEntries);
}

const std::vector<LibraryEntry>& SampleLibrary::getEntries() const noexcept
{
    return entries;
}

ScanResult SampleLibrary::scanNow()
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    jassert (! isScanning());

    auto outcome = performScan (rootDirectory, entries, nullptr);
    entries = std::move (outcome.entries);
    return outcome.result;
}

int SampleLibrary::cleanupOrphanedThumbnails (double minimumAgeSeconds)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    return removeOrphanedThumbnails (rootDirectory, entries, juce::jmax (0.0, minimumAgeSeconds));
}

void SampleLibrary::startScan()
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    if (scanning.exchange (true))
        return;

    if (scanThread != nullptr)
    {
        scanThread->stopThread (-1);
        scanThread.reset();
    }

    auto pending = std::make_shared<PendingScan>();
    pendingScan = pending;
    scanThread = std::make_unique<ScanThread> (*this, rootDirectory, entries, std::move (pending));
    scanThread->startThread();
}

bool SampleLibrary::isScanning() const noexcept
{
    return scanning.load();
}

void SampleLibrary::waitForScanToFinish()
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    if (scanThread != nullptr)
    {
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000.0;

        while (scanThread->isThreadRunning() && juce::Time::getMillisecondCounterHiRes() < deadline)
            juce::Thread::sleep (1);

        if (scanThread->isThreadRunning())
            scanThread->stopThread (-1);

        scanThread.reset();
    }

    auto pending = pendingScan;
    pendingScan.reset();

    if (pending == nullptr)
        return;

    bool cancelled = false;
    {
        const std::lock_guard lock (pending->mutex);
        cancelled = pending->cancelled;
    }

    if (cancelled)
        scanning.store (false);
    else
        applyPending (pending);
}

bool SampleLibrary::cancelScan()
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    if (! scanning.load())
        return false;

    if (auto pending = pendingScan)
    {
        const std::lock_guard lock (pending->mutex);
        pending->cancelled = true;   // discard whatever the thread already produced
        pending->ready = false;
    }

    if (scanThread != nullptr)
    {
        scanThread->stopThread (-1);   // prompt: hashing checks the exit flag
        scanThread.reset();
    }

    pendingScan.reset();
    scanning.store (false);
    return true;
}

void SampleLibrary::applyPending (const std::shared_ptr<PendingScan>& pending)
{
    ScanResult result;

    {
        const std::lock_guard lock (pending->mutex);

        if (! pending->ready || pending->applied || pending->cancelled)
            return;

        pending->applied = true;
        entries = std::move (pending->entries);
        result = pending->result;
    }

    scanning.store (false);

    if (onScanComplete)
        onScanComplete (result);

    sendChangeMessage();
}

void SampleLibrary::addSample (const std::string& fileHash, std::shared_ptr<const Sample> sample)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    samples[fileHash] = std::move (sample);
}

std::shared_ptr<const Sample> SampleLibrary::findSample (const std::string& fileHash) const
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    const auto found = samples.find (fileHash);
    return found != samples.end() ? found->second : nullptr;
}

void SampleLibrary::removeSample (const std::string& fileHash)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    samples.erase (fileHash);
}
