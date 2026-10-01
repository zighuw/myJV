#include <catch2/catch_test_macros.hpp>

#include "Engine/AssetReclaimer.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace
{
std::shared_ptr<PatchRuntime> makeRuntime (std::shared_ptr<const ZoneSet> zoneSet = nullptr)
{
    auto runtime = std::make_shared<PatchRuntime>();

    if (zoneSet != nullptr)
    {
        runtime->zoneSets[0] = zoneSet;
        runtime->rawZoneSets[0] = zoneSet.get();
    }

    return runtime;
}

std::shared_ptr<const ZoneSet> makeZoneSet (const std::shared_ptr<const Sample>& sample)
{
    auto zoneSet = std::make_shared<ZoneSet>();
    Zone zone;
    zone.sample = sample;
    zoneSet->zones.push_back (zone);
    return zoneSet;
}
}

TEST_CASE ("reclaimer starts empty")
{
    AssetReclaimer reclaimer;

    CHECK (reclaimer.activeForAudio() == nullptr);
    CHECK (reclaimer.oldestAssetInUse() == 0);
    CHECK (reclaimer.pendingCount() == 0);
    CHECK (reclaimer.retiredCount() == 0);
    CHECK (reclaimer.collect() == 0);
}

TEST_CASE ("publish assigns monotonic ids and activates the newest runtime")
{
    AssetReclaimer reclaimer;

    const auto* first = reclaimer.publish (makeRuntime());
    const auto* second = reclaimer.publish (makeRuntime());
    const auto* third = reclaimer.publish (makeRuntime());

    REQUIRE (first != nullptr);
    REQUIRE (second != nullptr);
    REQUIRE (third != nullptr);
    CHECK (first->id == 1);
    CHECK (second->id == 2);
    CHECK (third->id == 3);
    CHECK (reclaimer.activeForAudio() == third);
    CHECK (reclaimer.pendingCount() == 3);
    CHECK (reclaimer.retiredCount() == 2);
}

TEST_CASE ("collect reclaims nothing while oldestAssetInUse is zero")
{
    AssetReclaimer reclaimer (0.0);
    reclaimer.publish (makeRuntime());
    reclaimer.publish (makeRuntime());

    CHECK (reclaimer.collect() == 0);
    CHECK (reclaimer.pendingCount() == 2);
    CHECK (reclaimer.retiredCount() == 1);
}

TEST_CASE ("collect reclaims retired runtimes older than oldestAssetInUse")
{
    AssetReclaimer reclaimer (0.0);

    auto first = makeRuntime();
    std::weak_ptr<PatchRuntime> weakFirst = first;
    reclaimer.publish (std::move (first));
    reclaimer.publish (makeRuntime());
    reclaimer.publish (makeRuntime());

    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);
    CHECK (reclaimer.collect() == 2);
    CHECK (weakFirst.expired());
    CHECK (reclaimer.pendingCount() == 1);
    CHECK (reclaimer.retiredCount() == 0);
}

TEST_CASE ("only runtimes older than oldestAssetInUse are reclaimed")
{
    AssetReclaimer reclaimer (0.0);

    reclaimer.publish (makeRuntime());   // id 1
    reclaimer.publish (makeRuntime());   // id 2
    reclaimer.publish (makeRuntime());   // id 3

    reclaimer.updateOldestAssetInUse (2);
    CHECK (reclaimer.collect() == 1);    // id 1 only
    CHECK (reclaimer.pendingCount() == 2);

    reclaimer.updateOldestAssetInUse (3);
    CHECK (reclaimer.collect() == 1);    // id 2
    CHECK (reclaimer.pendingCount() == 1);
}

TEST_CASE ("the active runtime is never reclaimed")
{
    AssetReclaimer reclaimer (0.0);

    reclaimer.publish (makeRuntime());
    reclaimer.publish (makeRuntime());

    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);
    CHECK (reclaimer.collect() == 1);
    CHECK (reclaimer.collect() == 0);

    for (int i = 0; i < 5; ++i)
    {
        CHECK (reclaimer.collect() == 0);
        CHECK (reclaimer.pendingCount() == 1);
        CHECK (reclaimer.retiredCount() == 0);
    }
}

TEST_CASE ("collect keeps runtimes within the grace period")
{
    AssetReclaimer reclaimer (10000.0);   // no timing dependence in this phase

    auto first = makeRuntime();
    std::weak_ptr<PatchRuntime> weakFirst = first;
    reclaimer.publish (std::move (first));
    reclaimer.publish (makeRuntime());
    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);

    CHECK (reclaimer.collect() == 0);
    CHECK_FALSE (weakFirst.expired());
}

TEST_CASE ("collect reclaims after the grace period")
{
    AssetReclaimer reclaimer (30.0);

    auto first = makeRuntime();
    std::weak_ptr<PatchRuntime> weakFirst = first;
    reclaimer.publish (std::move (first));
    reclaimer.publish (makeRuntime());
    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);

    std::this_thread::sleep_for (std::chrono::milliseconds (80));

    CHECK (reclaimer.collect() == 1);
    CHECK (weakFirst.expired());
}

TEST_CASE ("oldestAssetInUse never decreases")
{
    AssetReclaimer reclaimer (0.0);

    reclaimer.publish (makeRuntime());   // id 1
    reclaimer.publish (makeRuntime());   // id 2
    reclaimer.publish (makeRuntime());   // id 3

    reclaimer.updateOldestAssetInUse (3);
    reclaimer.updateOldestAssetInUse (1);   // stale report must not lower the value

    CHECK (reclaimer.oldestAssetInUse() == 3);
    CHECK (reclaimer.collect() == 2);
    CHECK (reclaimer.pendingCount() == 1);
}

TEST_CASE ("negative grace periods are clamped to zero")
{
    AssetReclaimer reclaimer (-5.0);

    reclaimer.publish (makeRuntime());
    reclaimer.publish (makeRuntime());
    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);

    CHECK (reclaimer.collect() == 1);
}

TEST_CASE ("reclaiming a runtime releases its zone and sample chain")
{
    AssetReclaimer reclaimer (0.0);

    auto sample = std::make_shared<Sample>();
    auto zoneSet = makeZoneSet (sample);

    std::weak_ptr<PatchRuntime> weakRuntime;
    std::weak_ptr<const ZoneSet> weakZoneSet = zoneSet;
    std::weak_ptr<const Sample> weakSample = sample;

    auto runtime = makeRuntime (zoneSet);
    weakRuntime = runtime;
    reclaimer.publish (std::move (runtime));
    reclaimer.publish (makeRuntime());
    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);

    CHECK (reclaimer.collect() == 1);
    CHECK (weakRuntime.expired());

    sample.reset();
    zoneSet.reset();

    CHECK (weakZoneSet.expired());
    CHECK (weakSample.expired());
}

TEST_CASE ("publishing null is ignored")
{
    AssetReclaimer reclaimer;
    const auto* first = reclaimer.publish (makeRuntime());

    CHECK (reclaimer.publish (nullptr) == first);
    CHECK (reclaimer.activeForAudio() == first);
    CHECK (reclaimer.pendingCount() == 1);
}

TEST_CASE ("publishing null on an empty reclaimer is a no-op")
{
    AssetReclaimer reclaimer;

    CHECK (reclaimer.publish (nullptr) == nullptr);
    CHECK (reclaimer.activeForAudio() == nullptr);
    CHECK (reclaimer.pendingCount() == 0);
}

TEST_CASE ("raw zone pointers mirror the owning shared pointers")
{
    AssetReclaimer reclaimer;
    const auto zoneSet = makeZoneSet (std::make_shared<Sample>());

    auto runtime = makeRuntime (zoneSet);

    for (int tone = 1; tone < kNumTones; ++tone)
        runtime->rawZoneSets[tone] = runtime->zoneSets[tone].get();

    const auto* published = reclaimer.publish (std::move (runtime));

    REQUIRE (published != nullptr);

    for (int tone = 0; tone < kNumTones; ++tone)
        CHECK (published->zoneSets[tone].get() == published->rawZoneSets[tone]);

    CHECK (published->zoneSets[0].get() == zoneSet.get());
}

TEST_CASE ("publish and collect race safely against the audio read loop")
{
    AssetReclaimer reclaimer (500.0);   // real grace; protocol keeps readers alive
    constexpr int kPublishes = 10000;

    std::atomic<bool> started { false };
    std::atomic<bool> stop { false };
    std::atomic<std::uint64_t> lastSeenId { 0 };

    std::thread reader ([&]
    {
        while (! started.load (std::memory_order_acquire))
        {
        }

        while (! stop.load (std::memory_order_relaxed))
        {
            if (const auto* runtime = reclaimer.activeForAudio())
            {
                const auto id = runtime->id;
                lastSeenId.store (id, std::memory_order_relaxed);
                reclaimer.updateOldestAssetInUse (id);   // engine-style report
            }
        }
    });

    started.store (true, std::memory_order_release);

    for (int i = 0; i < kPublishes; ++i)
    {
        reclaimer.publish (makeRuntime());
        reclaimer.collect();
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (5);

    while (lastSeenId.load (std::memory_order_relaxed) != (std::uint64_t) kPublishes
           && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for (std::chrono::microseconds (100));
    }

    stop.store (true);
    reader.join();

    CHECK (lastSeenId.load() == (std::uint64_t) kPublishes);
    CHECK (reclaimer.oldestAssetInUse() == (std::uint64_t) kPublishes);
    CHECK (reclaimer.activeForAudio()->id == (std::uint64_t) kPublishes);
}

TEST_CASE ("publish and collect handle sustained churn")
{
    AssetReclaimer reclaimer (0.0);

    for (int i = 0; i < 10000; ++i)
    {
        reclaimer.publish (makeRuntime());
        reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);
        reclaimer.collect();
    }

    CHECK (reclaimer.pendingCount() == 1);
    CHECK (reclaimer.retiredCount() == 0);
    CHECK (reclaimer.activeForAudio()->id == 10000);
}

TEST_CASE ("an unowned published runtime is reclaimed once retired and reported")
{
    // Regression proof for the CI flake: publish() did not give the caller
    // custody. With a concurrent publisher + collector (grace 0) the runtime is
    // reclaimed here, so the raw pointer returned by publish() must not be
    // dereferenced afterwards (the old concurrent case did exactly that).
    AssetReclaimer reclaimer (0.0);

    std::weak_ptr<PatchRuntime> weak;
    {
        auto runtime = makeRuntime();
        weak = runtime;
        const auto* published = reclaimer.publish (std::move (runtime));
        REQUIRE (published != nullptr);
        CHECK (published->id == 1);
    }

    reclaimer.publish (makeRuntime());
    reclaimer.updateOldestAssetInUse (reclaimer.activeForAudio()->id);

    REQUIRE (reclaimer.collect() == 1);
    CHECK (weak.expired());
}

TEST_CASE ("two publishers and a collector keep one active entry and unique ids")
{
    AssetReclaimer reclaimer (0.0);
    constexpr int kPerThread = 2000;
    constexpr int kPublishers = 2;

    std::atomic<bool> start { false };
    std::atomic<bool> stop { false };
    std::atomic<int> violations { 0 };
    std::atomic<std::uint64_t> lastActiveId { 0 };

    // Written by one publisher each, read only after both have been joined.
    std::vector<std::uint64_t> ids[(std::size_t) kPublishers];

    const auto publisher = [&] (int index)
    {
        auto& recorded = ids[(std::size_t) index];

        while (! start.load (std::memory_order_acquire))
        {
        }

        std::uint64_t previous = 0;

        for (int i = 0; i < kPerThread; ++i)
        {
            // Keep this thread's own reference while reading the id: with the
            // concurrent collector and grace 0 the reclaimer may retire and
            // reclaim the just-published runtime before the check below, so
            // dereferencing publish()'s raw pointer without custody would be a
            // use-after-free (this was the CI flake).
            auto runtime = makeRuntime();
            const auto* published = reclaimer.publish (runtime);

            if (published == nullptr || runtime->id <= previous)
            {
                ++violations;   // a publisher must see strictly increasing ids
                return;
            }

            previous = runtime->id;
            recorded.push_back (previous);
        }
    };

    const auto collector = [&]
    {
        while (! start.load (std::memory_order_acquire))
        {
        }

        while (! stop.load (std::memory_order_relaxed))
        {
            // Read the retired count first: a publish only ever adds one entry
            // to each count, so pending - retired cannot shrink between the two
            // reads and "exactly one entry is not retired" is checked without a
            // combined accessor. Before the first publish there is no active
            // entry at all, so the invariant is vacuous and skipped.
            const auto retired = reclaimer.retiredCount();
            const auto pending = reclaimer.pendingCount();

            if (pending > 0 && pending - retired < 1)
                ++violations;

            if (const auto* active = reclaimer.activeForAudio())
            {
                const auto id = active->id;

                if (id < lastActiveId.load (std::memory_order_relaxed))
                    ++violations;   // the active id only ever moves forward

                lastActiveId.store (id, std::memory_order_relaxed);
                reclaimer.updateOldestAssetInUse (id);   // engine-style report
            }

            reclaimer.collect();
        }
    };

    std::thread first (publisher, 0);
    std::thread second (publisher, 1);
    std::thread collectorThread (collector);

    start.store (true, std::memory_order_release);

    first.join();
    second.join();

    stop.store (true, std::memory_order_relaxed);
    collectorThread.join();

    CHECK (violations.load() == 0);

    // Both publishers together must have produced exactly {1 .. 2 * kPerThread}:
    // unique, gapless and therefore strictly monotonic overall.
    std::vector<std::uint64_t> all;
    all.reserve ((std::size_t) kPublishers * kPerThread);

    for (const auto& recorded : ids)
        all.insert (all.end(), recorded.begin(), recorded.end());

    REQUIRE (all.size() == (std::size_t) kPublishers * kPerThread);
    std::sort (all.begin(), all.end());

    for (std::size_t i = 0; i < all.size(); ++i)
        CHECK (all[i] == (std::uint64_t) i + 1);

    // Deterministic quiescent check: report the newest id, reclaim everything
    // retired, and the one-active-entry invariant becomes exact.
    REQUIRE (reclaimer.activeForAudio() != nullptr);
    const auto newestId = reclaimer.activeForAudio()->id;

    CHECK (newestId == (std::uint64_t) (kPublishers * kPerThread));

    reclaimer.updateOldestAssetInUse (newestId);
    reclaimer.collect();

    CHECK (reclaimer.pendingCount() == 1);
    CHECK (reclaimer.retiredCount() == 0);
    CHECK (reclaimer.activeForAudio()->id == newestId);
}
