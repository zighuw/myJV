#pragma once

#include "Plugin/ZoneDraft.h"
#include "Plugin/ZoneMapGeometry.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

// Key/velocity grid editor for the draft ZoneSet (architecture 6.2): drag on
// empty space to create a zone from the pinned browser sample, click to select,
// drag inside to move, drag an edge to resize.
class ZoneMapView final : public juce::Component,
                          private juce::ChangeListener
{
public:
    explicit ZoneMapView (ZoneDraft&);
    ~ZoneMapView() override;

    void setPinnedSample (std::shared_ptr<const Sample> sample);

    std::function<void (juce::String)> onStatusMessage;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    int keyAt (juce::Point<int> position) const;
    int velocityAt (juce::Point<int> position) const;

    ZoneDraft& draft;
    std::shared_ptr<const Sample> pinnedSample;

    enum class Mode { idle, creating, moving, resizing };

    Mode mode = Mode::idle;
    int dragIndex = -1;
    ZoneMapGeometry::DragEdge dragEdge = ZoneMapGeometry::DragEdge::none;
    juce::Point<int> dragStart;
    Zone dragOriginal;
};
