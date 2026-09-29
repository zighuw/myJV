#pragma once

#include "Model/ZoneSet.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <utility>

// Message-thread draft of the ZoneSet being edited: each edit mutates the draft
// and notifies listeners via ChangeBroadcaster. Zone ranges are normalized
// (clamped and ordered) on entry so geometry and selection never see inverted
// data (ADR-017 edit-boundary validation). Publication into a PatchRuntime is
// deferred to M3.
class ZoneDraft : public juce::ChangeBroadcaster
{
public:
    const ZoneSet& getZoneSet() const noexcept { return zoneSet; }

    int getSelectedIndex() const noexcept { return selected; }

    const Zone* getSelectedZone() const noexcept
    {
        if (selected < 0 || selected >= (int) zoneSet.zones.size())
            return nullptr;

        return &zoneSet.zones[(std::size_t) selected];
    }

    void setZoneSet (ZoneSet newZoneSet)
    {
        zoneSet = std::move (newZoneSet);
        selected = zoneSet.zones.empty() ? -1 : 0;
        sendChangeMessage();
    }

    void setSelectedIndex (int index)
    {
        const auto clamped = zoneSet.zones.empty() ? -1 : juce::jlimit (0, (int) zoneSet.zones.size() - 1, index);
        selected = index < 0 ? -1 : clamped;
        sendChangeMessage();
    }

    void addZone (Zone zone)
    {
        zoneSet.zones.push_back (normalised (std::move (zone)));
        selected = (int) zoneSet.zones.size() - 1;
        sendChangeMessage();
    }

    void updateZone (int index, const Zone& zone)
    {
        if (index < 0 || index >= (int) zoneSet.zones.size())
            return;

        zoneSet.zones[(std::size_t) index] = normalised (zone);
        sendChangeMessage();
    }

    void removeZone (int index)
    {
        if (index < 0 || index >= (int) zoneSet.zones.size())
            return;

        zoneSet.zones.erase (zoneSet.zones.begin() + index);

        if (zoneSet.zones.empty())
            selected = -1;
        else
            selected = juce::jlimit (0, (int) zoneSet.zones.size() - 1, selected);

        sendChangeMessage();
    }

private:
    static Zone normalised (Zone zone)
    {
        if (zone.keyLow > zone.keyHigh)
            std::swap (zone.keyLow, zone.keyHigh);

        if (zone.velLow > zone.velHigh)
            std::swap (zone.velLow, zone.velHigh);

        zone.keyLow = juce::jlimit (kMidiNoteMin, kMidiNoteMax, zone.keyLow);
        zone.keyHigh = juce::jlimit (kMidiNoteMin, kMidiNoteMax, zone.keyHigh);
        zone.velLow = juce::jlimit (kMidiVelocityMin, kMidiVelocityMax, zone.velLow);
        zone.velHigh = juce::jlimit (kMidiVelocityMin, kMidiVelocityMax, zone.velHigh);
        zone.rootKeyOverride = juce::jlimit (-1, kMidiNoteMax, zone.rootKeyOverride);
        zone.pan = juce::jlimit (0, 127, zone.pan);
        return zone;
    }

    ZoneSet zoneSet;
    int selected = -1;
};
