#pragma once

#include "Model/ZoneSet.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Pure grid geometry for the Zone map editor: pixel <-> key/velocity mapping,
// zone rectangles, hit testing and clamped drag/edit operations.
namespace ZoneMapGeometry
{
inline int keyAtX (int x, int width)
{
    if (width <= 0)
        return 0;

    const auto clamped = juce::jlimit (0, width - 1, x);
    return clamped * 128 / width;
}

// The velocity axis is inverted: the top of the grid is velocity 127.
inline int velocityAtY (int y, int height)
{
    if (height <= 0)
        return kMidiVelocityMax;

    const auto clamped = juce::jlimit (0, height - 1, y);
    return kMidiVelocityMax - clamped * kMidiVelocityMax / height;
}

inline juce::Rectangle<int> rectForZone (const Zone& zone, int width, int height)
{
    const auto x0 = zone.keyLow * width / 128;
    const auto x1 = (zone.keyHigh + 1) * width / 128;
    const auto y0 = (kMidiVelocityMax - zone.velHigh) * height / kMidiVelocityMax;
    const auto y1 = (kMidiVelocityMax - zone.velLow + 1) * height / kMidiVelocityMax;
    return { x0, y0, juce::jmax (1, x1 - x0), juce::jmax (1, y1 - y0) };
}

// Returns the index of the topmost (last drawn) zone containing the point.
inline int hitTest (const ZoneSet& zoneSet, juce::Point<int> position, int width, int height)
{
    for (int i = (int) zoneSet.zones.size() - 1; i >= 0; --i)
        if (rectForZone (zoneSet.zones[(std::size_t) i], width, height).contains (position))
            return i;

    return -1;
}

inline Zone makeZoneFromDrag (const Zone& base, int keyA, int velocityA, int keyB, int velocityB)
{
    auto zone = base;
    zone.keyLow = juce::jlimit (kMidiNoteMin, kMidiNoteMax, juce::jmin (keyA, keyB));
    zone.keyHigh = juce::jlimit (kMidiNoteMin, kMidiNoteMax, juce::jmax (keyA, keyB));
    zone.velLow = juce::jlimit (kMidiVelocityMin, kMidiVelocityMax, juce::jmin (velocityA, velocityB));
    zone.velHigh = juce::jlimit (kMidiVelocityMin, kMidiVelocityMax, juce::jmax (velocityA, velocityB));
    return zone;
}

inline Zone movedZone (const Zone& zone, int deltaKeys, int deltaVelocities)
{
    const auto keySpan = zone.keyHigh - zone.keyLow;
    const auto velocitySpan = zone.velHigh - zone.velLow;

    auto newKeyLow = juce::jlimit (kMidiNoteMin, kMidiNoteMax - keySpan, zone.keyLow + deltaKeys);
    auto newVelLow = juce::jlimit (kMidiVelocityMin, kMidiVelocityMax - velocitySpan, zone.velLow + deltaVelocities);

    auto moved = zone;
    moved.keyLow = newKeyLow;
    moved.keyHigh = newKeyLow + keySpan;
    moved.velLow = newVelLow;
    moved.velHigh = newVelLow + velocitySpan;
    return moved;
}

enum class DragEdge
{
    none,
    keyLow,
    keyHigh,
    velLow,
    velHigh
};

// Returns the nearest border of the rect within grabPixels. Borders on an axis
// are ignored when the rect is smaller than 2*grabPixels there, so tiny
// (single-key/single-velocity) zones remain movable instead of unresizable.
inline DragEdge edgeAt (juce::Rectangle<int> rect, juce::Point<int> position, int grabPixels = 4)
{
    auto best = DragEdge::none;
    auto bestDistance = grabPixels + 1;

    const auto consider = [&] (DragEdge edge, int distance)
    {
        if (distance >= 0 && distance < bestDistance)
        {
            bestDistance = distance;
            best = edge;
        }
    };

    if (rect.getWidth() >= 2 * grabPixels)
    {
        consider (DragEdge::keyLow, position.x - rect.getX());
        consider (DragEdge::keyHigh, rect.getRight() - position.x);
    }

    if (rect.getHeight() >= 2 * grabPixels)
    {
        consider (DragEdge::velHigh, position.y - rect.getY());   // top edge = high velocity
        consider (DragEdge::velLow, rect.getBottom() - position.y);
    }

    return best;
}

inline Zone resizedZone (const Zone& zone, DragEdge edge, int key, int velocity)
{
    auto resized = zone;
    key = juce::jlimit (kMidiNoteMin, kMidiNoteMax, key);
    velocity = juce::jlimit (kMidiVelocityMin, kMidiVelocityMax, velocity);

    switch (edge)
    {
        case DragEdge::keyLow:
            resized.keyLow = juce::jmin (key, zone.keyHigh);
            break;
        case DragEdge::keyHigh:
            resized.keyHigh = juce::jmax (key, zone.keyLow);
            break;
        case DragEdge::velLow:
            resized.velLow = juce::jmin (velocity, zone.velHigh);
            break;
        case DragEdge::velHigh:
            resized.velHigh = juce::jmax (velocity, zone.velLow);
            break;
        case DragEdge::none:
        default:
            break;
    }

    return resized;
}
}
