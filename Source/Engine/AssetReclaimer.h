#pragma once

#include "Model/ZoneSet.h"
#include "Params/ParamSnapshot.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

inline constexpr int kNumTones = 4;

// Default retire grace period (architecture 3.3 rule 4): protects audio blocks
// that may still be reading a just-retired runtime.
inline constexpr double kAssetRetireGraceMs = 500.0;

// Built on the message or a background thread and published exactly once
// through AssetReclaimer::publish (the id write must not race an audio read).
// Everything except the snapshot is read-only after publication: only the audio
// thread may refresh `snapshot` while the runtime is active; retired runtimes
// keep their frozen snapshot (architecture 3.3 rule 3). The zoneSets/
// rawZoneSets mirror is maintained by the builder before publication.
struct PatchRuntime
{
    std::uint64_t id = 0;                                  // AssetId, assigned by AssetReclaimer::publish
    mutable PatchSnapshot snapshot;                         // audio-thread refresh while active (M2-01)
    std::shared_ptr<const ZoneSet> zoneSets[kNumTones];     // owning chain (non-RT threads)
    const ZoneSet* rawZoneSets[kNumTones] = {};             // audio-thread read-only, mirrors zoneSets
};

// Message/background-thread publishing plus audio-thread reads (architecture
// 3.3/3.4). The audio thread never releases or refcounts: it reads
// activeForAudio() and reports oldestAssetInUse. Publication and collection are
// serialized by a non-RT mutex; the audio path is lock-free.
//
// oldestAssetInUse contract (audio thread, architecture 3.3 rule 4): report the
// smallest AssetId referenced by any live Note/ToneVoice; when no note is live,
// report the current active runtime's id; never report an id last observed from
// activeForAudio() until that runtime is actually in use. The value must not
// decrease, and 0 (initial) keeps every runtime alive. collect() may reclaim a
// retired runtime only when its id is strictly below the reported value, so a
// violating report can free a runtime that is still in use.
class AssetReclaimer
{
public:
    explicit AssetReclaimer (double graceMs = kAssetRetireGraceMs) noexcept;

    // Non-RT (message or background thread). Assigns the next AssetId, retires
    // the previous active runtime and atomically publishes the new one.
    // Publishing null is a no-op. Each runtime must be published exactly once.
    const PatchRuntime* publish (std::shared_ptr<PatchRuntime> next);

    // RT-safe. Reports the smallest AssetId in use (see the contract above);
    // the value only ever increases (monotonic clamp).
    void updateOldestAssetInUse (std::uint64_t id) noexcept;
    std::uint64_t oldestAssetInUse() const noexcept;

    // Non-RT (message thread). Returns the number of runtimes reclaimed; the
    // last shared_ptr release (and therefore the ZoneSet/Sample chain release)
    // happens here, on the message thread.
    int collect() noexcept;

    // RT-safe.
    const PatchRuntime* activeForAudio() const noexcept;

    int retiredCount() const;
    int pendingCount() const;

private:
    struct Entry
    {
        std::shared_ptr<PatchRuntime> runtime;
        double retiredAtMs = 0.0;
        bool retired = false;
    };

    std::atomic<PatchRuntime*> active { nullptr };
    std::atomic<std::uint64_t> oldestAsset { 0 };
    mutable std::mutex pendingMutex;
    std::vector<Entry> pending;
    std::uint64_t nextId = 1;
    double graceMs = kAssetRetireGraceMs;
};

static_assert (std::atomic<PatchRuntime*>::is_always_lock_free,
               "audio-thread runtime pointer swap must be lock-free");
static_assert (std::atomic<std::uint64_t>::is_always_lock_free,
               "audio-thread oldestAssetInUse update must be lock-free");
