#include <catch2/catch_test_macros.hpp>

#include "Model/ZoneSet.h"
#include "Plugin/LoopEditGeometry.h"
#include "Plugin/ZoneDraft.h"

#include <memory>

namespace
{
LoopInfo makeLoop (int start, int end, int crossfade = 0)
{
    LoopInfo loop;
    loop.start = start;
    loop.end = end;
    loop.crossfadeSamples = crossfade;
    return loop;
}
}

TEST_CASE ("waveform x coordinates map to sample positions")
{
    CHECK (LoopEditGeometry::sampleAtX (0, 100, 1000) == 0);
    CHECK (LoopEditGeometry::sampleAtX (50, 100, 1000) == 500);
    CHECK (LoopEditGeometry::sampleAtX (99, 100, 1000) == 990);
    CHECK (LoopEditGeometry::sampleAtX (-5, 100, 1000) == 0);
    CHECK (LoopEditGeometry::sampleAtX (500, 100, 1000) == 990);
    CHECK (LoopEditGeometry::sampleAtX (0, 0, 1000) == 0);
    CHECK (LoopEditGeometry::sampleAtX (0, 100, 0) == 0);

    CHECK (LoopEditGeometry::xForSample (0, 100, 1000) == 0);
    CHECK (LoopEditGeometry::xForSample (500, 100, 1000) == 50);
    CHECK (LoopEditGeometry::xForSample (1000, 100, 1000) == 100);
    CHECK (LoopEditGeometry::xForSample (500, 0, 1000) == 0);
    CHECK (LoopEditGeometry::xForSample (0, 100, 0) == 0);
}

TEST_CASE ("dragging the loop start clamps inside the loop")
{
    const auto loop = makeLoop (100, 200, 10);

    CHECK (LoopEditGeometry::withStart (loop, 120, 1000).start == 120);
    CHECK (LoopEditGeometry::withStart (loop, 120, 1000).end == 200);

    // Cannot cross the end.
    CHECK (LoopEditGeometry::withStart (loop, 500, 1000).start == 199);
    CHECK (LoopEditGeometry::withStart (loop, -50, 1000).start == 0);

    // Dragging from an empty loop uses the whole sample as the end basis.
    const auto empty = makeLoop (0, 0);
    const auto started = LoopEditGeometry::withStart (empty, 300, 1000);
    CHECK (started.start == 300);
    CHECK (started.end == 1000);

    // Crossfade shrinks when the loop gets too short.
    const auto shortened = LoopEditGeometry::withStart (loop, 195, 1000);
    CHECK (shortened.start == 195);
    CHECK (shortened.crossfadeSamples == 2);   // (200-195)/2
}

TEST_CASE ("dragging the loop end clamps inside the sample")
{
    const auto loop = makeLoop (100, 200, 10);

    CHECK (LoopEditGeometry::withEnd (loop, 300, 1000).end == 300);
    CHECK (LoopEditGeometry::withEnd (loop, 300, 1000).start == 100);

    // Cannot cross the start; cannot exceed the sample length.
    CHECK (LoopEditGeometry::withEnd (loop, 50, 1000).end == 101);
    CHECK (LoopEditGeometry::withEnd (loop, 5000, 1000).end == 1000);

    // Dragging from an empty loop uses zero as the start basis.
    const auto empty = makeLoop (0, 0);
    const auto ended = LoopEditGeometry::withEnd (empty, 400, 1000);
    CHECK (ended.start == 0);
    CHECK (ended.end == 400);
}

TEST_CASE ("crossfade dragging is capped at half the loop")
{
    const auto loop = makeLoop (100, 200, 0);

    CHECK (LoopEditGeometry::withCrossfade (loop, 30).crossfadeSamples == 30);
    CHECK (LoopEditGeometry::withCrossfade (loop, 500).crossfadeSamples == 50);
    CHECK (LoopEditGeometry::withCrossfade (loop, -5).crossfadeSamples == 0);
    CHECK (LoopEditGeometry::withCrossfade (makeLoop (10, 11), 5).crossfadeSamples == 0);
}

TEST_CASE ("sanitized loops stay valid for any sample length")
{
    const auto valid = makeLoop (100, 200, 10);
    const auto sanitized = LoopEditGeometry::sanitized (valid, 1000);
    CHECK (sanitized.start == 100);
    CHECK (sanitized.end == 200);
    CHECK (sanitized.crossfadeSamples == 10);

    const auto clamped = LoopEditGeometry::sanitized (makeLoop (-10, 5000, 999), 1000);
    CHECK (clamped.start == 0);
    CHECK (clamped.end == 1000);
    CHECK (clamped.crossfadeSamples == 500);

    const auto invalid = LoopEditGeometry::sanitized (makeLoop (200, 100, 5), 1000);
    CHECK (invalid.start == 0);
    CHECK (invalid.end == 0);
    CHECK (invalid.crossfadeSamples == 0);

    const auto tiny = LoopEditGeometry::sanitized (makeLoop (0, 1, 0), 1);
    CHECK (tiny.start == 0);
    CHECK (tiny.end == 0);
}

TEST_CASE ("end mapping reaches the sample end")
{
    CHECK (LoopEditGeometry::endSampleAtX (0, 100, 1000) == 10);
    CHECK (LoopEditGeometry::endSampleAtX (99, 100, 1000) == 1000);
    CHECK (LoopEditGeometry::endSampleAtX (500, 100, 1000) == 1000);
    CHECK (LoopEditGeometry::endSampleAtX (-5, 100, 1000) == 10);
    CHECK (LoopEditGeometry::endSampleAtX (0, 100, 0) == 0);
}

TEST_CASE ("loop handles are reachable for empty and zero-crossfade loops")
{
    // Empty loop: left half creates a start handle, right half an end handle.
    CHECK (LoopEditGeometry::handleAt (makeLoop (0, 0), 1000, { 10, 10 }, 100, 60)
           == LoopEditGeometry::LoopHandle::loopStart);
    CHECK (LoopEditGeometry::handleAt (makeLoop (0, 0), 1000, { 80, 10 }, 100, 60)
           == LoopEditGeometry::LoopHandle::loopEnd);

    // Zero crossfade: the bottom lane creates the crossfade.
    const auto loop = makeLoop (100, 600, 0);
    CHECK (LoopEditGeometry::handleAt (loop, 1000, { 50, 55 }, 100, 60)
           == LoopEditGeometry::LoopHandle::crossfade);
    CHECK (LoopEditGeometry::handleAt (loop, 1000, { 50, 20 }, 100, 60)
           == LoopEditGeometry::LoopHandle::none);

    // An existing crossfade forms a distinct handle; when it would overlap the
    // end edge the end handle wins.
    CHECK (LoopEditGeometry::handleAt (makeLoop (100, 600, 100), 1000, { 50, 20 }, 100, 60)
           == LoopEditGeometry::LoopHandle::crossfade);
    CHECK (LoopEditGeometry::handleAt (makeLoop (100, 600, 5), 1000, { 60, 20 }, 100, 60)
           == LoopEditGeometry::LoopHandle::loopEnd);

    // Start/end handles still match near the borders.
    CHECK (LoopEditGeometry::handleAt (loop, 1000, { 10, 20 }, 100, 60)
           == LoopEditGeometry::LoopHandle::loopStart);
    CHECK (LoopEditGeometry::handleAt (loop, 1000, { 60, 20 }, 100, 60)
           == LoopEditGeometry::LoopHandle::loopEnd);
}

TEST_CASE ("sanitized handles two-sample loops and clamp helpers saturate")
{
    const auto two = LoopEditGeometry::sanitized (makeLoop (0, 1, 5), 2);
    CHECK (two.start == 0);
    CHECK (two.end == 1);
    CHECK (two.crossfadeSamples == 0);

    CHECK (LoopEditGeometry::xForSample (-100, 100, 1000) == 0);
    CHECK (LoopEditGeometry::xForSample (5000, 100, 1000) == 100);
}

TEST_CASE ("loop drags flow through the draft immediately")
{
    ZoneDraft draft;

    Zone zone;
    zone.sample = std::make_shared<Sample>();
    zone.loop = makeLoop (100, 200, 10);
    draft.addZone (zone);

    const auto* selected = draft.getSelectedZone();
    REQUIRE (selected != nullptr);

    auto edited = *selected;
    edited.loop = LoopEditGeometry::withEnd (edited.loop, 400, 1000);
    draft.updateZone (draft.getSelectedIndex(), edited);

    CHECK (draft.getZoneSet().zones[0].loop.end == 400);

    edited.loop = LoopEditGeometry::withCrossfade (edited.loop, 500);
    draft.updateZone (draft.getSelectedIndex(), edited);

    CHECK (draft.getZoneSet().zones[0].loop.crossfadeSamples == 150);
}
