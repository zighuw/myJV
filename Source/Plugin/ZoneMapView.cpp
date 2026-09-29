#include "Plugin/ZoneMapView.h"

#include <utility>

ZoneMapView::ZoneMapView (ZoneDraft& ownerDraft)
    : draft (ownerDraft)
{
    draft.addChangeListener (this);
}

ZoneMapView::~ZoneMapView()
{
    draft.removeChangeListener (this);
}

void ZoneMapView::setPinnedSample (std::shared_ptr<const Sample> sample)
{
    pinnedSample = std::move (sample);
}

void ZoneMapView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void ZoneMapView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff17191d));

    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.08f));

    for (int key = 0; key <= 127; key += 12)
    {
        const auto x = key * bounds.getWidth() / 128;
        g.drawVerticalLine (x, 0.0f, (float) bounds.getHeight());
    }

    for (int velocity = 16; velocity < 127; velocity += 16)
    {
        const auto y = (127 - velocity) * bounds.getHeight() / 127;
        g.drawHorizontalLine (y, 0.0f, (float) bounds.getWidth());
    }

    const auto& zones = draft.getZoneSet().zones;

    for (int i = 0; i < (int) zones.size(); ++i)
    {
        const auto rect = ZoneMapGeometry::rectForZone (zones[(std::size_t) i],
                                                        bounds.getWidth(), bounds.getHeight());
        const auto isSelected = i == draft.getSelectedIndex();

        g.setColour ((isSelected ? juce::Colours::orange : juce::Colours::steelblue).withAlpha (0.45f));
        g.fillRect (rect);
        g.setColour (isSelected ? juce::Colours::orange : juce::Colours::lightblue);
        g.drawRect (rect, isSelected ? 2 : 1);

        if (rect.getWidth() > 40)
            g.drawText (juce::String (zones[(std::size_t) i].rootKeyOverride),
                        rect.reduced (3, 1), juce::Justification::topLeft, false);
    }

    if (zones.empty())
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Drag here to create a zone (select a sample in the browser first)",
                    getLocalBounds().reduced (8), juce::Justification::centred, true);
    }
}

int ZoneMapView::keyAt (juce::Point<int> position) const
{
    return ZoneMapGeometry::keyAtX (position.x, getWidth());
}

int ZoneMapView::velocityAt (juce::Point<int> position) const
{
    return ZoneMapGeometry::velocityAtY (position.y, getHeight());
}

void ZoneMapView::mouseDown (const juce::MouseEvent& event)
{
    const auto position = event.getPosition();
    const auto hit = ZoneMapGeometry::hitTest (draft.getZoneSet(), position, getWidth(), getHeight());

    if (hit >= 0)
    {
        draft.setSelectedIndex (hit);
        dragIndex = hit;
        dragOriginal = draft.getZoneSet().zones[(std::size_t) hit];
        dragStart = position;
        dragEdge = ZoneMapGeometry::edgeAt (ZoneMapGeometry::rectForZone (dragOriginal, getWidth(), getHeight()),
                                            position);
        mode = dragEdge == ZoneMapGeometry::DragEdge::none ? Mode::moving : Mode::resizing;
        return;
    }

    if (pinnedSample == nullptr)
    {
        if (onStatusMessage)
            onStatusMessage ("Select a sample in the browser first");

        return;
    }

    Zone base;
    base.sample = pinnedSample;
    base.keyLow = keyAt (position);
    base.keyHigh = base.keyLow;
    base.velLow = velocityAt (position);
    base.velHigh = base.velLow;
    base.loop = pinnedSample->embeddedLoop;   // effective region seed (ADR-014)

    dragOriginal = base;
    dragStart = position;
    mode = Mode::creating;

    draft.addZone (base);
    dragIndex = draft.getSelectedIndex();
}

void ZoneMapView::mouseDrag (const juce::MouseEvent& event)
{
    const auto position = event.getPosition();

    if (mode == Mode::creating)
    {
        draft.updateZone (dragIndex, ZoneMapGeometry::makeZoneFromDrag (dragOriginal,
                                                                       keyAt (dragStart), velocityAt (dragStart),
                                                                       keyAt (position), velocityAt (position)));
        return;
    }

    if (mode == Mode::moving)
    {
        draft.updateZone (dragIndex, ZoneMapGeometry::movedZone (dragOriginal,
                                                                 keyAt (position) - keyAt (dragStart),
                                                                 velocityAt (position) - velocityAt (dragStart)));
        return;
    }

    if (mode == Mode::resizing)
        draft.updateZone (dragIndex, ZoneMapGeometry::resizedZone (dragOriginal, dragEdge,
                                                                   keyAt (position), velocityAt (position)));
}

void ZoneMapView::mouseUp (const juce::MouseEvent&)
{
    mode = Mode::idle;
    dragIndex = -1;
    dragEdge = ZoneMapGeometry::DragEdge::none;
}
