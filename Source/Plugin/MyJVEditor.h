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

#include <memory>
#include <set>
#include <string>

class MyJVProcessor;
class AuditionVoice;

// Sampler editor: sample browser with import/scan/audition, the Zone map grid,
// the waveform loop editor and the zone properties panel. Tabbed pages land in
// M5. A single editor instance is assumed: SampleLibrary exposes one
// onScanComplete callback, so a second editor would overwrite it (code-review
// N-15).
class MyJVEditor final : public juce::AudioProcessorEditor,
                         private juce::ListBoxModel,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    explicit MyJVEditor (MyJVProcessor&);
    ~MyJVEditor() override;

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

    JUCE_DECLARE_WEAK_REFERENCEABLE (MyJVEditor)
};
