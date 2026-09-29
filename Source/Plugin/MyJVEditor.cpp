#include "Plugin/MyJVEditor.h"

#include "Engine/AuditionVoice.h"
#include "IO/LibraryIndex.h"
#include "Plugin/MyJVProcessor.h"
#include "Plugin/SamplerUiHelpers.h"

#include <algorithm>
#include <utility>

namespace
{
juce::File defaultLibraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("myJV")
        .getChildFile ("Library");
}
}

class MyJVEditor::ImportJob final : public juce::ThreadPoolJob
{
public:
    ImportJob (juce::WeakReference<MyJVEditor> owner, juce::File file, juce::File root,
               bool auditionAfter, int auditionToken)
        : juce::ThreadPoolJob ("myJV sample import"),
          weakEditor (std::move (owner)),
          source (std::move (file)),
          libraryRoot (std::move (root)),
          auditionAfterImport (auditionAfter),
          token (auditionToken)
    {
    }

    JobStatus runJob() override
    {
        auto result = SampleImporter::importFile (source, libraryRoot);

        if (shouldExit())
            return jobHasFinished;

        juce::MessageManager::callAsync ([weak = weakEditor, auditionAfter = auditionAfterImport,
                                          auditionToken = token,
                                          result = std::move (result)]() mutable
        {
            if (auto* editor = weak.get())
                editor->applyImport (std::move (result), auditionAfter, auditionToken);
        });

        return jobHasFinished;
    }

private:
    juce::WeakReference<MyJVEditor> weakEditor;
    juce::File source;
    juce::File libraryRoot;
    bool auditionAfterImport;
    int token;
};

MyJVEditor::MyJVEditor (MyJVProcessor& ownerProcessor)
    : juce::AudioProcessorEditor (&ownerProcessor),
      processor (ownerProcessor),
      library (ownerProcessor.getSampleLibrary()),
      audition (ownerProcessor.getAuditionVoice())
{
    setSize (560, 380);

    addAndMakeVisible (list);
    list.setRowHeight (22);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha (0.85f));

    addAndMakeVisible (scanButton);
    scanButton.onClick = [this] { scanLibrary(); };

    addAndMakeVisible (importButton);
    importButton.onClick = [this] { chooseImportFiles(); };

    addAndMakeVisible (auditionButton);
    auditionButton.onClick = [this] { toggleAudition(); };

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    library.addChangeListener (this);
    library.onScanComplete = [weak = juce::WeakReference<MyJVEditor> (this)] (const ScanResult& result)
    {
        if (auto* editor = weak.get())
            editor->handleScanResult (result);
    };

    const auto root = defaultLibraryRoot();
    root.createDirectory();
    library.setRootDirectory (root);

    std::vector<LibraryEntry> entries;

    if (LibraryIndex::load (root.getChildFile ("library.json"), entries))
        library.setEntries (std::move (entries));

    missingPaths.clear();
    library.startScan();

    startTimerHz (10);
    updateStatus();
}

MyJVEditor::~MyJVEditor()
{
    stopTimer();
    library.removeChangeListener (this);
    library.onScanComplete = nullptr;
    chooser.reset();

    // Wait indefinitely: a long decode must not outlive the pool (UB). Imports
    // are user-initiated and the close simply blocks until the decode finishes.
    importPool.removeAllJobs (true, -1);
}

void MyJVEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262b));
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawText ("Sampler", getLocalBounds().removeFromTop (26).reduced (8, 0),
                juce::Justification::centredLeft);
}

void MyJVEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (22);   // title

    auto bottom = area.removeFromBottom (26);
    const auto buttonWidth = bottom.getWidth() / 4;

    scanButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));
    importButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));
    auditionButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));

    statusLabel.setBounds (area.removeFromBottom (24).reduced (2));
    list.setBounds (area);
}

int MyJVEditor::getNumRows()
{
    return (int) library.getEntries().size();
}

void MyJVEditor::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    const auto& entries = library.getEntries();

    if (rowNumber < 0 || rowNumber >= (int) entries.size())
        return;

    const auto& entry = entries[(std::size_t) rowNumber];
    const auto isMissing = missingPaths.count (entry.path) > 0;

    if (rowIsSelected)
        g.fillAll (juce::Colours::steelblue.withAlpha (0.5f));
    else if (isMissing)
        g.fillAll (juce::Colours::darkred.withAlpha (0.35f));
    else if (rowNumber % 2 != 0)
        g.fillAll (juce::Colours::white.withAlpha (0.04f));

    g.setColour (isMissing ? juce::Colours::orangered : juce::Colours::lightgrey);
    g.drawText (SamplerUi::entryLabel (entry, isMissing),
                juce::Rectangle<int> (4, 0, width - 8, height), juce::Justification::centredLeft, true);
}

void MyJVEditor::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    toggleAudition();
}

void MyJVEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    list.updateContent();
    updateStatus();
}

void MyJVEditor::timerCallback()
{
    auditionButton.setButtonText (audition.isPlaying() ? "Stop" : "Audition");
    updateStatus();
}

void MyJVEditor::scanLibrary()
{
    if (library.isScanning() || importsInFlight > 0)
    {
        updateStatus();
        return;
    }

    missingPaths.clear();
    library.startScan();
    updateStatus();
}

void MyJVEditor::chooseImportFiles()
{
    if (library.isScanning() || importsInFlight > 0)
    {
        updateStatus();
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Import samples", juce::File{},
                                                   "*.wav;*.aif;*.aiff;*.flac");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [weak = juce::WeakReference<MyJVEditor> (this)] (const juce::FileChooser& fc)
    {
        if (auto* editor = weak.get())
            for (const auto& file : fc.getResults())
                editor->beginImport (file, false);
    });
}

void MyJVEditor::beginImport (const juce::File& file, bool auditionAfterImport)
{
    if (library.isScanning())
    {
        updateStatus();
        return;
    }

    if (! file.existsAsFile())
    {
        statusLabel.setText ("File not found: " + file.getFullPathName(), juce::dontSendNotification);
        return;
    }

    ++importsInFlight;
    updateStatus();

    importPool.addJob (new ImportJob (juce::WeakReference<MyJVEditor> (this),
                                      file, library.getRootDirectory(),
                                      auditionAfterImport, auditionRequestToken),
                       true);
}

void MyJVEditor::applyImport (ImportResult result, bool auditionAfterImport, int auditionToken)
{
    --importsInFlight;

    if (! result.succeeded)
    {
        statusLabel.setText ("Import failed: " + juce::String (result.errorMessage), juce::dontSendNotification);
        updateStatus();
        return;
    }

    if (library.isScanning())
    {
        // Defensive: the UI gates this, but the library contract forbids index
        // mutations during a scan. Drop the import (the decoded sample is
        // released on the message thread).
        statusLabel.setText ("Scan in progress; import skipped", juce::dontSendNotification);
        updateStatus();
        return;
    }

    auto entries = library.getEntries();
    const auto outcome = SamplerUi::mergeImportedEntry (entries, result.entry, auditionAfterImport);

    if (outcome.skippedDuplicate)
    {
        statusLabel.setText ("Already in library: " + juce::String (result.entry.path), juce::dontSendNotification);
        updateStatus();
        return;
    }

    library.addSample (result.entry.fileHash, result.sample);
    library.setEntries (std::move (entries));
    saveIndex();

    if (auditionAfterImport && auditionToken == auditionRequestToken)
        audition.play (result.sample, result.entry.rootKey);

    list.updateContent();
    updateStatus();
}

void MyJVEditor::handleScanResult (const ScanResult& result)
{
    missingPaths.clear();

    for (const auto& path : result.missingPaths)
        missingPaths.insert (path);

    list.updateContent();
    updateStatus();
}

void MyJVEditor::toggleAudition()
{
    if (audition.isPlaying())
    {
        audition.stop();
        ++auditionRequestToken;   // cancel any pending lazy audition
        updateStatus();
        return;
    }

    if (library.isScanning())
    {
        updateStatus();
        return;
    }

    const auto row = list.getSelectedRow();
    const auto& entries = library.getEntries();

    if (row < 0 || row >= (int) entries.size())
    {
        statusLabel.setText ("Select a sample first", juce::dontSendNotification);
        return;
    }

    const auto entry = entries[(std::size_t) row];
    auto sample = library.findSample (entry.fileHash);

    if (sample != nullptr)
    {
        audition.play (sample, entry.rootKey);
        updateStatus();
        return;
    }

    // Scanned (or reloaded) entries have metadata but no decoded sample yet:
    // decode in the background, then audition.
    ++auditionRequestToken;
    beginImport (resolveEntryFile (entry), true);
}

void MyJVEditor::saveIndex()
{
    LibraryIndex::save (library.getRootDirectory().getChildFile ("library.json"), library.getEntries());
}

void MyJVEditor::updateStatus()
{
    juce::String text;

    if (library.isScanning())
        text = "Scanning library...";
    else if (importsInFlight > 0)
        text = "Importing (" + juce::String (importsInFlight) + ")...";
    else
        text = juce::String ((int) library.getEntries().size()) + " samples, "
               + juce::String ((int) missingPaths.size()) + " missing";

    statusLabel.setText (text, juce::dontSendNotification);
    scanButton.setEnabled (! library.isScanning() && importsInFlight == 0);
    importButton.setEnabled (! library.isScanning() && importsInFlight == 0);
}

juce::File MyJVEditor::resolveEntryFile (const LibraryEntry& entry) const
{
    return entry.external ? juce::File (juce::String (entry.path))
                          : library.getRootDirectory().getChildFile (juce::String (entry.path));
}
