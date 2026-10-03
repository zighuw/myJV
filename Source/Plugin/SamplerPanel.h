#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "IO/SampleImporter.h"
#include "Model/SampleLibrary.h"
#include "Plugin/SamplerUiHelpers.h"
#include "Plugin/WaveformView.h"
#include "Plugin/ZoneDraft.h"
#include "Plugin/ZoneMapView.h"
#include "Plugin/ZonePropertiesPanel.h"

#include <functional>
#include <memory>
#include <set>
#include <string>

class MyJVProcessor;
class AuditionVoice;

// Message-thread hand-off for a background import (ADR-020). A free function so
// the thread contract is testable without a live editor (M1-F07 / code-review
// P-3): the result is published on the message thread, the consumer only runs
// while the owner is still alive, and the owned sample is released there too.
namespace ImportDispatch
{
void postToMessageThread (std::function<bool()> isAlive,
                          std::function<void (ImportResult)> consumer,
                          ImportResult result);
}

// Sampler tab: sample browser with import/scan/audition, the Zone map grid, the
// waveform loop editor and the zone properties panel. Moved out of MyJVEditor
// mechanically for the M2-09 tabbed skeleton. A single editor instance is
// assumed: SampleLibrary exposes one onScanComplete callback, so a second panel
// would overwrite it (code-review N-15).
class SamplerPanel final : public juce::Component,
                           private juce::ListBoxModel,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit SamplerPanel (MyJVProcessor&);
    ~SamplerPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class ImportJob;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int rowNumber, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void scanLibrary();
    void chooseImportFiles();
    void beginImport (const juce::File& file, bool auditionAfterImport);
    void applyImport (ImportResult result, bool auditionAfterImport, int auditionToken);
    void handleScanResult (const ScanResult& result);
    void toggleAudition();
    void autoMapZones();
    void useFileLoop();
    void saveIndex();
    void showStatusMessage (juce::String message);
    void updateStatus();

    juce::File resolveEntryFile (const LibraryEntry& entry) const;

    MyJVProcessor& processor;
    SampleLibrary& library;
    AuditionVoice& audition;

    juce::ListBox list { "samples", this };
    juce::TextButton scanButton { "Scan" };
    juce::TextButton importButton { "Import..." };
    juce::TextButton auditionButton { "Audition" };
    juce::TextButton autoMapButton { "Auto-Map" };
    juce::TextButton useFileLoopButton { "Use File Loop" };
    juce::Label statusLabel;

    ZoneDraft zoneDraft;
    ZoneMapView zoneMap { zoneDraft };
    ZonePropertiesPanel zoneProperties { zoneDraft };
    WaveformView waveform { zoneDraft, library };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::ThreadPool importPool { 1 };
    SamplerUi::StatusLineState statusLine;
    std::set<std::string> missingPaths;
    std::string pendingPinHash;
    int importsInFlight = 0;
    int auditionRequestToken = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (SamplerPanel)
};
