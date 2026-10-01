#pragma once

#include "Engine/AssetReclaimer.h"
#include "Params/ParamSnapshot.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <string>
#include <vector>

// Caches the raw APVTS value pointers for every registered parameter once, on a
// non-RT thread, so the audio thread can refresh the active runtime's snapshot
// with plain relaxed atomic loads: no ID lookup, no allocation, no locks.
//
// Every registered parameter (patch.common.*, tone1..4.* - 518 in total) is
// bound exactly once to a snapshot field by the tables in the .cpp. The
// parameterIds() helper exposes the binding order for coverage tests; the
// fromPointers() factory uses the same order (common first, then tone-major)
// and exists so tests can build a cache without a real APVTS.
class ParamSnapshotCache
{
public:
    ParamSnapshotCache() = default;

    // Non-RT. Resolves every parameter ID through apvts.getRawParameterValue;
    // unresolved IDs stay null (asserted in debug builds, reported by
    // resolvedCount() in release builds).
    static ParamSnapshotCache fromApvts (juce::AudioProcessorValueTreeState& apvts);

    // Non-RT. Takes exactly totalParameterCount() pointers in parameterIds()
    // order; a different size yields an empty cache.
    static ParamSnapshotCache fromPointers (const std::vector<std::atomic<float>*>& pointers);

    static int totalParameterCount() noexcept;
    static std::vector<std::string> parameterIds();

    int resolvedCount() const noexcept;

    // RT-safe. Writes the cached values into runtime.snapshot (mutable) and
    // mirrors the owning runtime's id/zone pointers. Null slots are skipped.
    void refresh (const PatchRuntime& runtime) const noexcept;

private:
    static constexpr int kPatchCommonParameterCount = 26;
    static constexpr int kToneParameterCount = 123;
    static constexpr int kToneCount = PatchSnapshot::kToneCount;

    std::atomic<float>* commonPointers[kPatchCommonParameterCount] = {};
    std::atomic<float>* tonePointers[kToneCount][kToneParameterCount] = {};
};
