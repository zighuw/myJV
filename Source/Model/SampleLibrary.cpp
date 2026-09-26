#include "Model/SampleLibrary.h"

#include <juce_cryptography/juce_cryptography.h>

#include <algorithm>
#include <set>
#include <utility>

namespace
{
constexpr const char* kSupportedExtensions[] { ".wav", ".aif", ".aiff", ".flac" };

bool isSupportedAudioFile (const juce::String& fileName)
{
    const auto extension = fileName.fromLastOccurrenceOf (".", true, false).toLowerCase();

    for (const auto* supported : kSupportedExtensions)
        if (extension == supported)
            return true;

    return false;
}

std::string normalisedRelativePath (const juce::File& root, const juce::File& file)
{
    return file.getRelativePathFrom (root).replaceCharacter ('\\', '/').toStdString();
}

std::string hashFile (const juce::File& file)
{
    return juce::SHA256 (file).toHexString().toStdString();
}

struct ScanOutcome
{
    ScanResult result;
    std::vector<LibraryEntry> entries;
};

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

        existingPaths.insert (entry.relativePath);

        const auto file = root.getChildFile (juce::String (entry.relativePath));

        if (file.existsAsFile())
        {
            const auto hash = hashFile (file);

            if (! claimedHashes.emplace (hash, entry.relativePath).second)
            {
                ++outcome.result.duplicatesSkipped;
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

    for (const auto& [relativePath, file] : files)
    {
        if (shouldStop())
            return outcome;

        if (existingPaths.count (relativePath) != 0)
            continue;

        const auto hash = hashFile (file);

        if (! claimedHashes.emplace (hash, relativePath).second)
        {
            ++outcome.result.duplicatesSkipped;
            continue;
        }

        LibraryEntry entry;
        entry.relativePath = relativePath;
        entry.fileHash = hash;
        kept.push_back (entry);
        ++outcome.result.entriesAdded;
    }

    for (const auto& entry : kept)
        if (! root.getChildFile (juce::String (entry.relativePath)).existsAsFile())
            outcome.result.missingPaths.push_back (entry.relativePath);

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

void SampleLibrary::startScan()
{
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

void SampleLibrary::applyPending (const std::shared_ptr<PendingScan>& pending)
{
    ScanResult result;

    {
        const std::lock_guard lock (pending->mutex);

        if (! pending->ready || pending->applied)
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
