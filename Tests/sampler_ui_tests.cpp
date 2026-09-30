#include <catch2/catch_test_macros.hpp>

#include "Plugin/MyJVEditor.h"
#include "Plugin/SamplerUiHelpers.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
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

// Stands in for the editor: the dispatch seam only needs a weak-referenceable
// owner whose lifetime the test controls (M1-F07 / code-review P-3).
class DispatchOwner
{
public:
    JUCE_DECLARE_WEAK_REFERENCEABLE (DispatchOwner)
};

// Records where a sample was finally released, so the test can prove it happens
// on the message thread.
struct ReleaseRecord
{
    std::atomic<int> count { 0 };
    std::atomic<std::thread::id> thread { std::thread::id {} };
};

std::shared_ptr<const Sample> makeProbedSample (const std::shared_ptr<ReleaseRecord>& record)
{
    return std::shared_ptr<const Sample> (new Sample(),
                                          [record] (const Sample* sample)
                                          {
                                              record->thread.store (std::this_thread::get_id());
                                              record->count.fetch_add (1);
                                              delete sample;
                                          });
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

// --- transient status line (code-review I-1) --------------------------------

TEST_CASE ("sampler ui keeps a transient status message until it expires")
{
    SamplerUi::StatusLineState state;
    const juce::String stats { "3 samples, 0 missing, 1 zones" };
    const juce::String message { "Imported: Kick.wav (a1b2c3d4)" };

    state.setMessage (message, 1000);

    CHECK (state.hasActiveMessage (1000));            // visible right after setting
    CHECK (state.hasActiveMessage (2999));            // still inside the window
    CHECK (state.resolve (stats, 1000) == message);
    CHECK (state.resolve (stats, 2999) == message);
    CHECK_FALSE (state.hasActiveMessage (3000));      // expired after the duration
    CHECK (state.resolve (stats, 3000) == stats);     // statistics come back
    CHECK (state.resolve (stats, 60000) == stats);
}

TEST_CASE ("sampler ui lets a later transient message replace the previous one")
{
    SamplerUi::StatusLineState state;
    const juce::String stats { "stats" };

    state.setMessage ("first", 1000, 5000);
    CHECK (state.resolve (stats, 1000) == "first");

    state.setMessage ("second", 1500);

    CHECK (state.resolve (stats, 1500) == "second");
    CHECK (state.resolve (stats, 3499) == "second");
    CHECK (state.resolve (stats, 3500) == stats);     // second's own window, not first's
}

TEST_CASE ("sampler ui clears transient status and ignores empty messages")
{
    SamplerUi::StatusLineState state;
    const juce::String stats { "stats" };

    state.setMessage ("boom", 1000);
    CHECK (state.hasActiveMessage (1000));

    state.clear();
    CHECK_FALSE (state.hasActiveMessage (1000));
    CHECK (state.resolve (stats, 1000) == stats);

    state.setMessage ({}, 1000);
    CHECK_FALSE (state.hasActiveMessage (1000));
    CHECK (state.resolve (stats, 1000) == stats);
}

TEST_CASE ("sampler ui transient status with a non-positive duration expires immediately")
{
    SamplerUi::StatusLineState state;
    const juce::String stats { "stats" };

    state.setMessage ("instant", 1000, 0);
    CHECK_FALSE (state.hasActiveMessage (1000));
    CHECK (state.resolve (stats, 1000) == stats);

    state.setMessage ("negative", 1000, -5);
    CHECK_FALSE (state.hasActiveMessage (1000));
    CHECK (state.resolve (stats, 1000) == stats);
}

TEST_CASE ("sampler ui shortens content hashes for display")
{
    CHECK (SamplerUi::shortHash ("0123456789abcdef") == "01234567");
    CHECK (SamplerUi::shortHash ("abc") == "abc");
    CHECK (SamplerUi::shortHash ("") == "");
    CHECK (SamplerUi::shortHash ("0123456789abcdef", 4) == "0123");
}

TEST_CASE ("sampler ui builds the transient messages for import and scan outcomes")
{
    CHECK (SamplerUi::duplicateSkipMessage ("Piano/C4.wav") == "Already in library: Piano/C4.wav");
    CHECK (SamplerUi::importFailureMessage ("unsupported format") == "Import failed: unsupported format");

    const auto success = SamplerUi::importSuccessMessage ("Kick.wav", "0123456789abcdef");
    CHECK (success.contains ("Kick.wav"));
    CHECK (success.contains ("01234567"));            // hash prefix identifies the entry
    CHECK_FALSE (success.contains ("89abcdef"));

    CHECK (SamplerUi::fileNotFoundMessage ("C:/nope.wav") == "File not found: C:/nope.wav");
    CHECK (SamplerUi::scanInProgressMessage().contains ("Scan"));
    CHECK (SamplerUi::scanAlreadyRunningMessage().contains ("Scan"));
    CHECK (SamplerUi::importInProgressMessage().contains ("Import"));
}

TEST_CASE ("sampler ui reports entries dropped by scan deduplication")
{
    CHECK (SamplerUi::entriesDroppedMessage (1) == "Scan dropped 1 duplicate entry");
    CHECK (SamplerUi::entriesDroppedMessage (2) == "Scan dropped 2 duplicate entries");
    CHECK (SamplerUi::entriesDroppedMessage (17) == "Scan dropped 17 duplicate entries");
}

// --- import dispatch thread contract (M1-F07 / code-review P-3) --------------

TEST_CASE ("import dispatch publishes the result on the message thread")
{
    const auto messageThread = std::this_thread::get_id();
    std::atomic<std::thread::id> consumerThread {};
    std::atomic<int> calls { 0 };

    ImportResult result;
    result.succeeded = true;

    std::thread worker ([&result, &consumerThread, &calls]
    {
        ImportDispatch::postToMessageThread ([] { return true; },
                                             [&consumerThread, &calls] (ImportResult)
                                             {
                                                 consumerThread.store (std::this_thread::get_id());
                                                 ++calls;
                                             },
                                             std::move (result));
    });

    worker.join();

    // The consumer is queued, not run on the posting thread.
    CHECK (calls.load() == 0);

    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    CHECK (calls.load() == 1);
    CHECK (consumerThread.load() == messageThread);
}

TEST_CASE ("import dispatch skips a dead owner but still releases the sample")
{
    auto owner = std::make_unique<DispatchOwner>();
    juce::WeakReference<DispatchOwner> weak (owner.get());

    auto record = std::make_shared<ReleaseRecord>();
    std::atomic<int> calls { 0 };

    ImportResult result;
    result.succeeded = true;
    result.sample = makeProbedSample (record);

    std::thread worker ([&result, weak, &calls]
    {
        ImportDispatch::postToMessageThread ([weak] { return weak.get() != nullptr; },
                                             [&calls] (ImportResult) { ++calls; },
                                             std::move (result));
    });

    owner.reset();   // the owner is gone before the message thread dispatches
    worker.join();

    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    CHECK (calls.load() == 0);                                     // no callback for a dead owner
    CHECK (record->count.load() == 1);                             // but the result was released
    CHECK (record->thread.load() == std::this_thread::get_id());   // ... on the message thread
}

TEST_CASE ("import dispatch hands the sample over without releasing it early")
{
    auto record = std::make_shared<ReleaseRecord>();
    ImportResult received;

    ImportResult result;
    result.succeeded = true;
    result.sample = makeProbedSample (record);

    ImportDispatch::postToMessageThread ([] { return true; },
                                         [&received] (ImportResult posted) { received = std::move (posted); },
                                         std::move (result));

    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    REQUIRE (received.sample != nullptr);
    CHECK (received.succeeded);
    CHECK (record->count.load() == 0);   // ownership moved to the consumer

    received.sample.reset();
    CHECK (record->count.load() == 1);   // released here, on the message thread
    CHECK (record->thread.load() == std::this_thread::get_id());
}
