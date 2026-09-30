#pragma once

#include "Model/ZoneSet.h"

#include <juce_graphics/juce_graphics.h>

#include <cstdint>
#include <cstdlib>
#include <limits>

// Pure loop-editing geometry for the waveform view: pixel <-> sample mapping
// and clamped handle drags. Loop semantics follow ADR-014 (half-open
// [start, end); invalid start >= end means no loop).
namespace LoopEditGeometry
{
inline std::int64_t sampleAtX (int x, int width, std::int64_t lengthSamples)
{
    if (width <= 0 || lengthSamples <= 0)
        return 0;

    const auto clamped = juce::jlimit (0, width - 1, x);
    return (std::int64_t) clamped * lengthSamples / width;
}

// End-handle mapping: reaches exactly lengthSamples at the last pixel, so a
// loop end can be dragged to the sample end.
inline std::int64_t endSampleAtX (int x, int width, std::int64_t lengthSamples)
{
    if (width <= 0 || lengthSamples <= 0)
        return lengthSamples;

    const auto clamped = juce::jlimit (0, width - 1, x);
    return (std::int64_t) (clamped + 1) * lengthSamples / width;
}

inline int xForSample (std::int64_t sample, int width, std::int64_t lengthSamples)
{
    if (width <= 0 || lengthSamples <= 0)
        return 0;

    return (int) ((std::int64_t) juce::jlimit ((std::int64_t) 0, lengthSamples, sample) * width / lengthSamples);
}

// Valid loop (end > start) stays as-is; invalid/out-of-range data collapses to
// "no loop" when the sample is too short to loop.
inline LoopInfo sanitized (LoopInfo loop, std::int64_t lengthSamples)
{
    lengthSamples = juce::jmin (lengthSamples, (std::int64_t) std::numeric_limits<int>::max());

    if (lengthSamples < 2 || loop.end <= loop.start)
        return {};

    loop.start = (int) juce::jlimit ((std::int64_t) 0, lengthSamples - 1, (std::int64_t) loop.start);
    loop.end = (int) juce::jlimit ((std::int64_t) loop.start + 1, lengthSamples, (std::int64_t) loop.end);
    loop.crossfadeSamples = juce::jlimit (0, (loop.end - loop.start) / 2, loop.crossfadeSamples);
    return loop;
}

// Drags the start handle: clamped to [0, end-1]; when the loop is empty the
// whole sample is used as the end basis.
inline LoopInfo withStart (const LoopInfo& loop, std::int64_t sample, std::int64_t lengthSamples)
{
    const auto hasLoop = loop.end > loop.start && lengthSamples >= 2;
    const auto end = hasLoop ? juce::jmin ((std::int64_t) loop.end, lengthSamples) : lengthSamples;
    const auto start = juce::jlimit ((std::int64_t) 0, juce::jmax ((std::int64_t) 0, end - 1), sample);

    LoopInfo result = loop;
    result.start = (int) start;
    result.end = (int) end;
    result.crossfadeSamples = juce::jlimit (0, (result.end - result.start) / 2, loop.crossfadeSamples);
    return result;
}

// Drags the end handle: clamped to [start+1, length]; when the loop is empty
// zero is used as the start basis.
inline LoopInfo withEnd (const LoopInfo& loop, std::int64_t sample, std::int64_t lengthSamples)
{
    if (lengthSamples < 2)
        return {};

    const auto hasLoop = loop.end > loop.start && loop.start < lengthSamples;
    const auto start = hasLoop ? (std::int64_t) loop.start : (std::int64_t) 0;
    const auto end = juce::jlimit (start + 1, lengthSamples, sample);

    LoopInfo result = loop;
    result.start = (int) start;
    result.end = (int) end;
    result.crossfadeSamples = juce::jlimit (0, (result.end - result.start) / 2, loop.crossfadeSamples);
    return result;
}

// Drags the crossfade handle: 0 .. half of the loop span.
inline LoopInfo withCrossfade (const LoopInfo& loop, std::int64_t samples)
{
    if (loop.end <= loop.start)
        return loop;

    LoopInfo result = loop;
    result.crossfadeSamples = juce::jlimit (0, (loop.end - loop.start) / 2, (int) samples);
    return result;
}

enum class LoopHandle
{
    none,
    loopStart,
    loopEnd,
    crossfade
};

// Height of the bottom lane that creates / adjusts the crossfade. Shared by the
// waveform view's painter and the hit test so they cannot drift apart
// (code-review N-13).
inline constexpr int kCrossfadeLaneHeight = 16;

// Pure handle selection shared by the waveform view. The bottom lane creates /
// adjusts the crossfade. An invalid (empty) loop splits the view into a start
// handle on the left half and an end handle on the right half, so a loop can be
// created by dragging; with a valid loop the end handle wins when it would
// overlap the crossfade handle.
inline LoopHandle handleAt (const LoopInfo& loop, std::int64_t lengthSamples,
                            juce::Point<int> position, int width, int height, int grabPixels = 5)
{
    if (width <= 1 || height <= 0 || lengthSamples < 2)
        return LoopHandle::none;

    const auto inCrossfadeLane = position.y >= height - kCrossfadeLaneHeight;
    const auto hasLoop = loop.end > loop.start;

    if (! hasLoop)
        return inCrossfadeLane ? LoopHandle::none
                               : (position.x <= width / 2 ? LoopHandle::loopStart : LoopHandle::loopEnd);

    if (inCrossfadeLane)
        return LoopHandle::crossfade;

    const auto endX = xForSample (loop.end, width, lengthSamples);

    if (loop.crossfadeSamples > 0)
    {
        const auto crossfadeX = xForSample (loop.end - loop.crossfadeSamples, width, lengthSamples);

        if (crossfadeX < endX - grabPixels && std::abs (position.x - crossfadeX) <= grabPixels)
            return LoopHandle::crossfade;
    }

    if (std::abs (position.x - endX) <= grabPixels)
        return LoopHandle::loopEnd;

    const auto startX = xForSample (loop.start, width, lengthSamples);

    if (std::abs (position.x - startX) <= grabPixels)
        return LoopHandle::loopStart;

    return LoopHandle::none;
}
}
