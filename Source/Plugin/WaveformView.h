#pragma once

#include "Model/SampleLibrary.h"
#include "Plugin/LoopEditGeometry.h"
#include "Plugin/ZoneDraft.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

// Waveform display for the selected library entry (architecture 6.2): draws
// decoded-sample peaks when available, falling back to the imported thumbnail
// PNG, and overlays the effective loop region. Loop start/end and the crossfade
// (bottom lane) can be dragged when the selected draft zone references the same
// sample; otherwise the entry's file loop is shown read-only. Clamps live in
// LoopEditGeometry (ADR-014 half-open semantics).
class WaveformView final : public juce::Component,
                           private juce::ChangeListener
{
public:
    WaveformView (ZoneDraft&, SampleLibrary&);
    ~WaveformView() override;

    void setEntry (const LibraryEntry& entry);
    void clearEntry();

    bool isShowing (const std::string& fileHash) const noexcept
    {
        return hasEntry && entry.fileHash == fileHash;
    }

    std::function<void (juce::String)> onStatusMessage;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void rebuildPeaks();
    void loadThumbnail();
    std::int64_t sampleLength() const;
    const Zone* editableZone() const;          // nullptr when not editable for this entry
    const LoopInfo* currentLoop() const;       // editable zone loop, else entry file loop
    int xFor (std::int64_t sample) const;

    ZoneDraft& draft;
    SampleLibrary& library;

    LibraryEntry entry;
    bool hasEntry = false;

    std::shared_ptr<const Sample> decoded;
    std::vector<float> minPeaks;
    std::vector<float> maxPeaks;
    int peaksWidth = -1;
    const Sample* peaksSample = nullptr;
    juce::Image thumbnail;

    LoopEditGeometry::LoopHandle dragHandle = LoopEditGeometry::LoopHandle::none;
};
