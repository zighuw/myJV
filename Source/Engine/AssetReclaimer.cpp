#include "Engine/AssetReclaimer.h"

#include <juce_core/juce_core.h>

namespace
{
double nowMs() noexcept
{
    return juce::Time::getMillisecondCounterHiRes();
}
}

AssetReclaimer::AssetReclaimer (double graceMsToUse) noexcept
    : graceMs (juce::jmax (0.0, graceMsToUse))
{
}

const PatchRuntime* AssetReclaimer::publish (std::shared_ptr<PatchRuntime> next)
{
    if (next == nullptr)
        return active.load (std::memory_order_acquire);

    const std::lock_guard<std::mutex> lock (pendingMutex);

    const auto timestamp = nowMs();

    // Reserve and build the entry before touching the published state so a
    // failed allocation cannot break the one-active-entry invariant.
    Entry entry { std::move (next), timestamp, false };
    pending.reserve (pending.size() + 1);

        for (auto& existing : pending)
        {
            if (! existing.retired)
            {
                existing.retired = true;
                existing.retiredAtMs = timestamp;
            }
        }

        entry.runtime->id = nextId++;
    pending.push_back (std::move (entry));
    active.store (pending.back().runtime.get(), std::memory_order_release);

    return pending.back().runtime.get();
}

void AssetReclaimer::updateOldestAssetInUse (std::uint64_t id) noexcept
{
    auto current = oldestAsset.load (std::memory_order_relaxed);

    while (id > current
           && ! oldestAsset.compare_exchange_weak (current, id,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed))
    {
    }
}

std::uint64_t AssetReclaimer::oldestAssetInUse() const noexcept
{
    return oldestAsset.load (std::memory_order_acquire);
}

int AssetReclaimer::collect() noexcept
{
    const std::lock_guard<std::mutex> lock (pendingMutex);

    const auto oldest = oldestAsset.load (std::memory_order_acquire);
    const auto current = nowMs();
    int reclaimed = 0;

    for (auto it = pending.begin(); it != pending.end(); )
    {
        if (it->retired
            && it->runtime->id < oldest
            && (current - it->retiredAtMs) >= graceMs)
        {
            it = pending.erase (it);
            ++reclaimed;
        }
        else
        {
            ++it;
        }
    }

    return reclaimed;
}

const PatchRuntime* AssetReclaimer::activeForAudio() const noexcept
{
    return active.load (std::memory_order_acquire);
}

int AssetReclaimer::retiredCount() const
{
    const std::lock_guard<std::mutex> lock (pendingMutex);

    int count = 0;

    for (const auto& entry : pending)
        if (entry.retired)
            ++count;

    return count;
}

int AssetReclaimer::pendingCount() const
{
    const std::lock_guard<std::mutex> lock (pendingMutex);

    return (int) pending.size();
}
