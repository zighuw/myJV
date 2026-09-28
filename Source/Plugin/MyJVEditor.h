#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "IO/SampleImporter.h"
#include "Model/SampleLibrary.h"

#include <memory>
#include <set>
#include <string>

class MyJVProcessor;
class AuditionVoice;

// First editor skeleton: a single Sampler panel (sample browser, scan/import,
// basic audition). Tabbed pages land in M5.
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

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void scanLibrary();
    void chooseImportFiles();
    void beginImport (const juce::File& file, bool auditionAfterImport);
    void applyImport (ImportResult result, bool auditionAfterImport, int auditionToken);
    void handleScanResult (const ScanResult& result);
    void toggleAudition();
    void saveIndex();
    void updateStatus();

    juce::File resolveEntryFile (const LibraryEntry& entry) const;

    MyJVProcessor& processor;
    SampleLibrary& library;
    AuditionVoice& audition;

    juce::ListBox list { "samples", this };
    juce::TextButton scanButton { "Scan" };
    juce::TextButton importButton { "Import..." };
    juce::TextButton auditionButton { "Audition" };
    juce::Label statusLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::ThreadPool importPool { 1 };
    std::set<std::string> missingPaths;
    int importsInFlight = 0;
    int auditionRequestToken = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (MyJVEditor)
};
