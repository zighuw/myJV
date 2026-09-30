#include "Plugin/WaveformView.h"

#include "Plugin/LoopEditGeometry.h"

#include <algorithm>

WaveformView::WaveformView (ZoneDraft& ownerDraft, SampleLibrary& ownerLibrary)
    : draft (ownerDraft),
      library (ownerLibrary)
{
    draft.addChangeListener (this);
}

WaveformView::~WaveformView()
{
    draft.removeChangeListener (this);
}

void WaveformView::setEntry (const LibraryEntry& newEntry)
{
    entry = newEntry;
    hasEntry = true;
    decoded = library.findSample (entry.fileHash);

    if (decoded != nullptr)
        thumbnail = {};   // peaks are more accurate; skip the PNG decode
    else
        loadThumbnail();

    peaksWidth = -1;      // force a peak rebuild for the new sample
    peaksSample = nullptr;
    rebuildPeaks();
    repaint();
}

void WaveformView::clearEntry()
{
    hasEntry = false;
    decoded = nullptr;
    thumbnail = {};
    minPeaks.clear();
    maxPeaks.clear();
    peaksWidth = -1;
    peaksSample = nullptr;
    dragHandle = LoopEditGeometry::LoopHandle::none;
    repaint();
}

void WaveformView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

std::int64_t WaveformView::sampleLength() const
{
    if (decoded != nullptr)
        return decoded->data.getNumSamples();

    return entry.lengthSamples;
}

const Zone* WaveformView::editableZone() const
{
    const auto* zone = draft.getSelectedZone();

    if (zone == nullptr)
        return nullptr;

    if (zone->sample == nullptr)
        return zone;

    if (hasEntry && zone->sample->fileHash == entry.fileHash)
        return zone;

    return nullptr;   // selected zone references another sample
}

const LoopInfo* WaveformView::currentLoop() const
{
    if (const auto* zone = editableZone())
        return &zone->loop;

    if (hasEntry)
        return &entry.loop;

    return nullptr;
}

void WaveformView::loadThumbnail()
{
    thumbnail = {};

    if (! hasEntry || entry.thumbnailPath.empty())
        return;

    const auto root = library.getRootDirectory();

    if (root.getFullPathName().isEmpty())
        return;

    const auto file = root.getChildFile (juce::String (entry.thumbnailPath));

    if (! file.existsAsFile())
        return;

    juce::FileInputStream stream (file);

    if (stream.openedOk())
    {
        juce::PNGImageFormat png;
        thumbnail = png.decodeImage (stream);
    }
}

// Known limit (code-review N-13): the peak cache is rebuilt on the message
// thread, so the first display of a very long sample costs O(width * samples *
// channels) - for the 50M-frame ceiling that is a perceptible hitch. The
// background-precompute option is deferred to M5-04, which revisits the peak
// strategy together with zoom/scroll. The displayed peaks themselves are exact.
void WaveformView::rebuildPeaks()
{
    const auto width = getWidth();

    if (decoded == nullptr || width <= 0)
    {
        minPeaks.clear();
        maxPeaks.clear();
        peaksWidth = -1;
        peaksSample = nullptr;
        return;
    }

    if (width == peaksWidth && decoded.get() == peaksSample)
        return;

    const auto numSamples = decoded->data.getNumSamples();
    const auto numChannels = decoded->data.getNumChannels();

    minPeaks.assign ((std::size_t) width, 0.0f);
    maxPeaks.assign ((std::size_t) width, 0.0f);

    if (numSamples <= 0 || numChannels <= 0)
        return;

    for (int column = 0; column < width; ++column)
    {
        const auto start = (int) ((std::int64_t) column * numSamples / width);
        const auto end = (int) juce::jmin ((std::int64_t) numSamples,
                                           juce::jmax ((std::int64_t) start + 1,
                                                       (std::int64_t) (column + 1) * numSamples / width));

        float minimum = 0.0f;
        float maximum = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            for (int i = start; i < end; ++i)
            {
                const auto value = decoded->data.getSample (channel, i);
                minimum = juce::jmin (minimum, value);
                maximum = juce::jmax (maximum, value);
            }

        minPeaks[(std::size_t) column] = minimum;
        maxPeaks[(std::size_t) column] = maximum;
    }

    peaksWidth = width;
    peaksSample = decoded.get();
}

int WaveformView::xFor (std::int64_t sample) const
{
    return LoopEditGeometry::xForSample (sample, getWidth(), sampleLength());
}

void WaveformView::resized()
{
    rebuildPeaks();
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll (juce::Colour (0xff101216));

    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (bounds.getCentreY(), 0.0f, (float) bounds.getWidth());

    // Crossfade drag lane.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (bounds.withTop (bounds.getBottom() - LoopEditGeometry::kCrossfadeLaneHeight));

    if (! hasEntry)
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Select a sample to view its waveform", bounds, juce::Justification::centred, true);
        return;
    }

    const auto centreY = (float) bounds.getCentreY();
    const auto halfHeight = (float) bounds.getHeight() * 0.48f;

    if (! minPeaks.empty())
    {
        g.setColour (juce::Colours::lightsteelblue.withAlpha (0.8f));

        for (int column = 0; column < (int) minPeaks.size() && column < bounds.getWidth(); ++column)
        {
            const auto top = centreY - maxPeaks[(std::size_t) column] * halfHeight;
            const auto bottom = centreY - minPeaks[(std::size_t) column] * halfHeight;
            g.drawVerticalLine (column, juce::jmin (top, bottom), juce::jmax (top, bottom) + 1.0f);
        }
    }
    else if (thumbnail.isValid())
    {
        g.setOpacity (0.75f);
        g.drawImage (thumbnail, bounds.toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
    }
    else
    {
        g.setColour (juce::Colours::grey.withAlpha (0.8f));
        g.drawText ("Waveform unavailable (import or audition the sample to decode it)",
                    bounds, juce::Justification::centred, true);
    }

    const auto* loop = currentLoop();
    const auto length = sampleLength();
    const auto editable = editableZone() != nullptr;
    const auto hasZoneSelection = draft.getSelectedZone() != nullptr;

    if (loop == nullptr || length < 2 || loop->end <= loop->start)
    {
        g.setColour (juce::Colours::grey);
        auto hint = juce::String ("No loop");

        if (editable)
            hint << " (drag left half for start, right half for end)";
        else if (hasZoneSelection)
            hint << " (selected zone uses a different sample)";

        g.drawText (hint, bounds.removeFromBottom (16), juce::Justification::centred, false);
        return;
    }

    const auto loopX0 = xFor (loop->start);
    const auto loopX1 = xFor (loop->end);

    g.setColour (juce::Colours::orange.withAlpha (0.15f));
    g.fillRect (juce::Rectangle<int> (loopX0, 0, juce::jmax (1, loopX1 - loopX0), bounds.getHeight()));
    g.setColour (juce::Colours::orange.withAlpha (0.9f));
    g.drawVerticalLine (loopX0, 0.0f, (float) bounds.getHeight());
    g.drawVerticalLine (loopX1, 0.0f, (float) bounds.getHeight());

    if (loop->crossfadeSamples > 0)
    {
        const auto crossfadeX = xFor (loop->end - loop->crossfadeSamples);
        const auto headX = xFor (loop->start + loop->crossfadeSamples);

        g.setColour (juce::Colours::red.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<int> (crossfadeX, 0, juce::jmax (1, loopX1 - crossfadeX), bounds.getHeight()));
        g.fillRect (juce::Rectangle<int> (loopX0, 0, juce::jmax (1, headX - loopX0), bounds.getHeight()));
        g.setColour (juce::Colours::red.withAlpha (0.9f));
        g.drawVerticalLine (crossfadeX, 0.0f, (float) bounds.getHeight());
    }

    const auto status = editable ? "Editable loop"
                                 : (hasZoneSelection ? "Read-only (zone uses a different sample)" : "File loop");

    const auto label = juce::String (status)
                       + "  " + juce::String (loop->start) + ".." + juce::String (loop->end)
                       + "  xf " + juce::String (loop->crossfadeSamples);

    g.setColour (juce::Colours::lightgrey);
    g.drawText (label, bounds.removeFromBottom (16), juce::Justification::centred, false);
}

void WaveformView::mouseDown (const juce::MouseEvent& event)
{
    dragHandle = LoopEditGeometry::LoopHandle::none;

    if (editableZone() == nullptr)
    {
        if (onStatusMessage)
            onStatusMessage (draft.getSelectedZone() != nullptr
                                 ? "Selected zone uses a different sample"
                                 : "Select a zone to edit its loop");

        return;
    }

    const auto* loop = currentLoop();

    if (loop == nullptr)
        return;

    dragHandle = LoopEditGeometry::handleAt (*loop, sampleLength(), event.getPosition(),
                                             getWidth(), getHeight());

    if (dragHandle == LoopEditGeometry::LoopHandle::none && onStatusMessage)
        onStatusMessage ("Drag the loop start/end or the crossfade lane");
}

void WaveformView::mouseDrag (const juce::MouseEvent& event)
{
    if (dragHandle == LoopEditGeometry::LoopHandle::none)
        return;

    const auto index = draft.getSelectedIndex();
    const auto* selected = editableZone();

    if (selected == nullptr || index < 0)
        return;

    const auto length = sampleLength();

    if (length < 2)
        return;

    const auto position = event.getPosition();
    const auto sample = LoopEditGeometry::sampleAtX (position.x, getWidth(), length);
    auto zone = *selected;

    switch (dragHandle)
    {
        case LoopEditGeometry::LoopHandle::loopStart:
            zone.loop = LoopEditGeometry::withStart (zone.loop, sample, length);
            break;
        case LoopEditGeometry::LoopHandle::loopEnd:
            zone.loop = LoopEditGeometry::withEnd (zone.loop,
                                                   LoopEditGeometry::endSampleAtX (position.x, getWidth(), length),
                                                   length);
            break;
        case LoopEditGeometry::LoopHandle::crossfade:
            zone.loop = LoopEditGeometry::withCrossfade (zone.loop, (std::int64_t) zone.loop.end - sample);
            break;
        case LoopEditGeometry::LoopHandle::none:
        default:
            return;
    }

    zone.loop = LoopEditGeometry::sanitized (zone.loop, length);
    draft.updateZone (index, zone);
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    dragHandle = LoopEditGeometry::LoopHandle::none;
}
