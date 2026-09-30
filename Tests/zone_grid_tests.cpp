#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Model/ZoneSet.h"
#include "Plugin/ZoneDraft.h"
#include "Plugin/ZoneMapGeometry.h"

#include <memory>

namespace
{
Zone makeZone (int keyLow, int keyHigh, int velLow, int velHigh)
{
    Zone zone;
    zone.keyLow = keyLow;
    zone.keyHigh = keyHigh;
    zone.velLow = velLow;
    zone.velHigh = velHigh;
    zone.sample = std::make_shared<Sample>();
    zone.rootKeyOverride = 60;
    return zone;
}
}

TEST_CASE ("grid coordinates map to keys and velocities")
{
    CHECK (ZoneMapGeometry::keyAtX (0, 128) == 0);
    CHECK (ZoneMapGeometry::keyAtX (60, 128) == 60);
    CHECK (ZoneMapGeometry::keyAtX (127, 128) == 127);
    CHECK (ZoneMapGeometry::keyAtX (-5, 128) == 0);
    CHECK (ZoneMapGeometry::keyAtX (999, 128) == 127);
    CHECK (ZoneMapGeometry::velocityAtY (0, 127) == 127);
    CHECK (ZoneMapGeometry::velocityAtY (63, 127) == 64);
    CHECK (ZoneMapGeometry::velocityAtY (126, 127) == 1);
    CHECK (ZoneMapGeometry::velocityAtY (-1, 127) == 127);
    CHECK (ZoneMapGeometry::velocityAtY (999, 127) == 1);
}

TEST_CASE ("grid uses the named MIDI key count")
{
    // 128 is the number of keys, not a note number, so it gets a name instead of
    // a bare literal (code-review N-14).
    CHECK (ZoneMapGeometry::kMidiKeyCount == kMidiNoteMax - kMidiNoteMin + 1);
    CHECK (ZoneMapGeometry::kMidiKeyCount == 128);

    const auto width = ZoneMapGeometry::kMidiKeyCount;
    CHECK (ZoneMapGeometry::keyAtX (0, width) == kMidiNoteMin);
    CHECK (ZoneMapGeometry::keyAtX (width - 1, width) == kMidiNoteMax);
}

TEST_CASE ("zone rectangles cover the key and velocity ranges")
{
    const auto zone = makeZone (60, 72, 1, 127);
    const auto rect = ZoneMapGeometry::rectForZone (zone, 128, 127);

    CHECK (rect.getX() == 60);
    CHECK (rect.getWidth() == 13);
    CHECK (rect.getY() == 0);
    CHECK (rect.getHeight() == 127);

    const auto midZone = makeZone (0, 127, 64, 127);
    const auto midRect = ZoneMapGeometry::rectForZone (midZone, 128, 127);

    CHECK (midRect.getX() == 0);
    CHECK (midRect.getWidth() == 128);
    CHECK (midRect.getY() == 0);
    CHECK (midRect.getHeight() == 64);
}

TEST_CASE ("hit test prefers the last drawn zone")
{
    ZoneSet zoneSet;
    zoneSet.zones.push_back (makeZone (0, 127, 1, 127));
    zoneSet.zones.push_back (makeZone (60, 72, 64, 127));

    const auto inside = ZoneMapGeometry::rectForZone (zoneSet.zones[1], 128, 127).getCentre();

    CHECK (ZoneMapGeometry::hitTest (zoneSet, inside, 128, 127) == 1);

    const auto lowerOnly = juce::Point<int> { 0, 100 };
    CHECK (ZoneMapGeometry::hitTest (zoneSet, lowerOnly, 128, 127) == 0);

    CHECK (ZoneMapGeometry::hitTest (zoneSet, { 200, 200 }, 128, 127) == -1);
}

TEST_CASE ("drag creates a normalized zone")
{
    auto base = makeZone (0, 127, 1, 127);
    base.rootKeyOverride = 48;

    const auto dragged = ZoneMapGeometry::makeZoneFromDrag (base, 72, 30, 60, 90);

    CHECK (dragged.keyLow == 60);
    CHECK (dragged.keyHigh == 72);
    CHECK (dragged.velLow == 30);
    CHECK (dragged.velHigh == 90);
    CHECK (dragged.rootKeyOverride == 48);
    CHECK (dragged.sample == base.sample);

    const auto clamped = ZoneMapGeometry::makeZoneFromDrag (base, -10, 0, 500, 500);
    CHECK (clamped.keyLow == 0);
    CHECK (clamped.keyHigh == 127);
    CHECK (clamped.velLow == 1);
    CHECK (clamped.velHigh == 127);
}

TEST_CASE ("moving a zone clamps to the grid and keeps its size")
{
    const auto zone = makeZone (60, 72, 20, 40);

    const auto moved = ZoneMapGeometry::movedZone (zone, 5, 10);
    CHECK (moved.keyLow == 65);
    CHECK (moved.keyHigh == 77);
    CHECK (moved.velLow == 30);
    CHECK (moved.velHigh == 50);

    const auto clampedHigh = ZoneMapGeometry::movedZone (zone, 1000, 1000);
    CHECK (clampedHigh.keyLow == 115);
    CHECK (clampedHigh.keyHigh == 127);
    CHECK (clampedHigh.velLow == 107);
    CHECK (clampedHigh.velHigh == 127);

    const auto clampedLow = ZoneMapGeometry::movedZone (zone, -1000, -1000);
    CHECK (clampedLow.keyLow == 0);
    CHECK (clampedLow.keyHigh == 12);
    CHECK (clampedLow.velLow == 1);
    CHECK (clampedLow.velHigh == 21);
}

TEST_CASE ("resizing edges clamps to the grid and keeps at least one unit")
{
    const auto zone = makeZone (60, 72, 20, 40);

    const auto widenedLow = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::keyLow, 50, 0);
    CHECK (widenedLow.keyLow == 50);
    CHECK (widenedLow.keyHigh == 72);

    const auto collapsedLow = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::keyLow, 100, 0);
    CHECK (collapsedLow.keyLow == 72);
    CHECK (collapsedLow.keyHigh == 72);

    const auto collapsedHigh = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::keyHigh, 0, 0);
    CHECK (collapsedHigh.keyLow == 60);
    CHECK (collapsedHigh.keyHigh == 60);

    const auto velLow = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::velLow, 0, 60);
    CHECK (velLow.velLow == 40);    // low edge clamps to the high edge
    CHECK (velLow.velHigh == 40);

    const auto velHigh = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::velHigh, 0, 1);
    CHECK (velHigh.velLow == 20);   // high edge clamps to the low edge
    CHECK (velHigh.velHigh == 20);

    const auto clamped = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::keyHigh, 500, 0);
    CHECK (clamped.keyHigh == 127);

    const auto untouched = ZoneMapGeometry::resizedZone (zone, ZoneMapGeometry::DragEdge::none, 0, 0);
    CHECK (untouched.keyLow == zone.keyLow);
    CHECK (untouched.velHigh == zone.velHigh);
}

TEST_CASE ("edge detection picks the nearest border and keeps tiny zones movable")
{
    const auto zone = makeZone (60, 72, 20, 40);
    const auto rect = ZoneMapGeometry::rectForZone (zone, 128, 127);

    CHECK (ZoneMapGeometry::edgeAt (rect, rect.getCentre()) == ZoneMapGeometry::DragEdge::none);
    CHECK (ZoneMapGeometry::edgeAt (rect, { rect.getX() + 1, rect.getCentreY() }) == ZoneMapGeometry::DragEdge::keyLow);
    CHECK (ZoneMapGeometry::edgeAt (rect, { rect.getRight() - 1, rect.getCentreY() }) == ZoneMapGeometry::DragEdge::keyHigh);
    CHECK (ZoneMapGeometry::edgeAt (rect, { rect.getCentreX(), rect.getY() + 1 }) == ZoneMapGeometry::DragEdge::velHigh);
    CHECK (ZoneMapGeometry::edgeAt (rect, { rect.getCentreX(), rect.getBottom() - 1 }) == ZoneMapGeometry::DragEdge::velLow);

    // A single-key zone is narrower than 2*grab; its vertical edges must not
    // swallow the whole zone (otherwise it could never be moved).
    const auto singleKey = makeZone (60, 60, 20, 40);
    const auto singleRect = ZoneMapGeometry::rectForZone (singleKey, 128, 127);
    CHECK (ZoneMapGeometry::edgeAt (singleRect, singleRect.getCentre()) == ZoneMapGeometry::DragEdge::none);
}

TEST_CASE ("zone draft normalizes malformed zones")
{
    ZoneDraft draft;

    draft.addZone (makeZone (72, 60, 40, 20));
    REQUIRE (draft.getZoneSet().zones.size() == 1);
    CHECK (draft.getZoneSet().zones[0].keyLow == 60);
    CHECK (draft.getZoneSet().zones[0].keyHigh == 72);
    CHECK (draft.getZoneSet().zones[0].velLow == 20);
    CHECK (draft.getZoneSet().zones[0].velHigh == 40);

    auto outOfRange = makeZone (0, 0, 1, 1);
    outOfRange.keyLow = -10;
    outOfRange.keyHigh = 200;
    outOfRange.velLow = -5;
    outOfRange.velHigh = 500;
    outOfRange.rootKeyOverride = -50;
    outOfRange.pan = 900;

    draft.updateZone (0, outOfRange);
    CHECK (draft.getZoneSet().zones[0].keyLow == 0);
    CHECK (draft.getZoneSet().zones[0].keyHigh == 127);
    CHECK (draft.getZoneSet().zones[0].velLow == 1);
    CHECK (draft.getZoneSet().zones[0].velHigh == 127);
    CHECK (draft.getZoneSet().zones[0].rootKeyOverride == -1);
    CHECK (draft.getZoneSet().zones[0].pan == 127);
}

TEST_CASE ("zone draft edits are copy-on-write and keep the selection valid")
{
    ZoneDraft draft;
    CHECK (draft.getSelectedIndex() == -1);
    CHECK (draft.getSelectedZone() == nullptr);

    draft.setZoneSet ({ "draft", { makeZone (60, 60, 1, 127), makeZone (62, 62, 1, 127) } });
    CHECK (draft.getSelectedIndex() == 0);
    REQUIRE (draft.getSelectedZone() != nullptr);
    CHECK (draft.getSelectedZone()->keyLow == 60);

    draft.setSelectedIndex (1);
    CHECK (draft.getSelectedZone()->keyLow == 62);

    auto added = makeZone (64, 64, 1, 127);
    draft.addZone (added);
    CHECK (draft.getZoneSet().zones.size() == 3);
    CHECK (draft.getSelectedIndex() == 2);

    auto edited = *draft.getSelectedZone();
    edited.gainDb = -6.0f;
    draft.updateZone (2, edited);
    CHECK (draft.getZoneSet().zones[2].gainDb == -6.0f);

    draft.removeZone (2);
    CHECK (draft.getZoneSet().zones.size() == 2);
    CHECK (draft.getSelectedIndex() == 1);

    draft.removeZone (1);
    draft.removeZone (0);
    CHECK (draft.getSelectedIndex() == -1);
    CHECK (draft.getSelectedZone() == nullptr);

    draft.setSelectedIndex (5);   // ignored on an empty draft
    CHECK (draft.getSelectedIndex() == -1);
}
