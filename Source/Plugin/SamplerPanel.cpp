#include "Plugin/SamplerPanel.h"

#include "Engine/AuditionVoice.h"
#include "IO/LibraryIndex.h"
#include "Model/ZoneMapping.h"
#include "Plugin/LoopEditGeometry.h"
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

namespace ImportDispatch
{
void postToMessageThread (std::function<bool()> isAlive,
                          std::function<void (ImportResult)> consumer,
                          ImportResult result)
{
    juce::MessageManager::callAsync ([isAlive = std::move (isAlive),
                                      consumer = std::move (consumer),
                                      result = std::move (result)]() mutable
    {
        // Both checks run here, on the message thread: a destroyed owner must
        // never receive a callback.
        if (isAlive != nullptr && isAlive() && consumer != nullptr)
            consumer (std::move (result));

        // Whether it was consumed or not, whatever the result still owns is
        // released on the message thread (ADR-020, code-review N-20).
        result.sample.reset();
    });
}
}

class SamplerPanel::ImportJob final : public juce::ThreadPoolJob
{
public:
    ImportJob (juce::WeakReference<SamplerPanel> owner, juce::File file, juce::File root,
               bool auditionAfter, int auditionToken)
        : juce::ThreadPoolJob ("myJV sample import"),
          weakPanel (std::move (owner)),
          source (std::move (file)),
          libraryRoot (std::move (root)),
          auditionAfterImport (auditionAfter),
          token (auditionToken)
    {
    }

    JobStatus runJob() override
    {
        auto result = SampleImporter::importFile (source, libraryRoot);

        const auto weak = weakPanel;
        const auto apply = ! shouldExit();
        const auto auditionAfter = auditionAfterImport;
        const auto auditionToken = token;

        // One path for both outcomes: the result is always released on the
        // message thread, and a cancelled job (pool shutdown) simply does not
        // reach applyImport (ADR-020, code-review N-20). Only the destructor's
        // removeAllJobs(true, -1) can make shouldExit() true, so importsInFlight
        // needs no adjustment here (the panel is going away).
        ImportDispatch::postToMessageThread (
            [weak] { return weak.get() != nullptr; },
            [weak, apply, auditionAfter, auditionToken] (ImportResult posted) mutable
            {
                if (auto* panel = weak.get())
                {
                    if (apply)
                        panel->applyImport (std::move (posted), auditionAfter, auditionToken);
                }
            },
            std::move (result));

        return jobHasFinished;
    }

private:
    juce::WeakReference<SamplerPanel> weakPanel;
    juce::File source;
    juce::File libraryRoot;
    bool auditionAfterImport;
    int token;
};

SamplerPanel::SamplerPanel (MyJVProcessor& ownerProcessor)
    : processor (ownerProcessor),
      library (ownerProcessor.getSampleLibrary()),
      audition (ownerProcessor.getAuditionVoice())
{
    addAndMakeVisible (list);
    list.setRowHeight (22);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha (0.85f));

    addAndMakeVisible (scanButton);
    scanButton.onClick = [this] { scanLibrary(); };

    addAndMakeVisible (importButton);
    importButton.onClick = [this] { chooseImportFiles(); };

    addAndMakeVisible (auditionButton);
    auditionButton.onClick = [this] { toggleAudition(); };

    addAndMakeVisible (autoMapButton);
    autoMapButton.onClick = [this] { autoMapZones(); };

    addAndMakeVisible (useFileLoopButton);
    useFileLoopButton.onClick = [this] { useFileLoop(); };

    addAndMakeVisible (statusLabel);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    addAndMakeVisible (zoneMap);
    zoneMap.onStatusMessage = [this] (juce::String message)
    {
        showStatusMessage (std::move (message));
    };

    addAndMakeVisible (zoneProperties);
    addAndMakeVisible (waveform);
    waveform.onStatusMessage = [this] (juce::String message)
    {
        showStatusMessage (std::move (message));
    };

    library.addChangeListener (this);
    zoneDraft.addChangeListener (this);
    library.onScanComplete = [weak = juce::WeakReference<SamplerPanel> (this)] (const ScanResult& result)
    {
        if (auto* panel = weak.get())
            panel->handleScanResult (result);
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

SamplerPanel::~SamplerPanel()
{
    stopTimer();
    zoneDraft.removeChangeListener (this);
    library.removeChangeListener (this);
    library.onScanComplete = nullptr;
    chooser.reset();

    // Wait indefinitely: a long decode must not outlive the pool (UB). Imports
    // are user-initiated and the close simply blocks until the decode finishes.
    importPool.removeAllJobs (true, -1);
}

void SamplerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262b));
}

void SamplerPanel::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto bottom = area.removeFromBottom (26);
    const auto buttonWidth = juce::jmax (1, bottom.getWidth() / 6);

    scanButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));
    importButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));
    auditionButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));
    autoMapButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));
    useFileLoopButton.setBounds (bottom.removeFromLeft (buttonWidth).reduced (2));

    statusLabel.setBounds (area.removeFromBottom (24).reduced (2));

    auto left = area.removeFromLeft (360);
    list.setBounds (left.reduced (0, 2));

    zoneMap.setBounds (area.removeFromTop (juce::jmin (240, area.getHeight() / 3)).reduced (2));
    waveform.setBounds (area.removeFromTop (juce::jmin (170, area.getHeight() / 2)).reduced (2));
    zoneProperties.setBounds (area.reduced (2));
}

int SamplerPanel::getNumRows()
{
    return (int) library.getEntries().size();
}

void SamplerPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
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

void SamplerPanel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
{
    toggleAudition();
}

void SamplerPanel::changeListenerCallback (juce::ChangeBroadcaster* broadcaster)
{
    if (broadcaster == &zoneDraft)
    {
        updateStatus();
        return;
    }

    list.updateContent();
    updateStatus();
}

void SamplerPanel::selectedRowsChanged (int)
{
    const auto row = list.getSelectedRow();
    const auto& entries = library.getEntries();

    if (row < 0 || row >= (int) entries.size())
    {
        zoneMap.setPinnedSample (nullptr);
        waveform.clearEntry();
        pendingPinHash.clear();
        return;
    }

    const auto entry = entries[(std::size_t) row];
    auto sample = library.findSample (entry.fileHash);
    zoneMap.setPinnedSample (sample);
    waveform.setEntry (entry);

    if (sample == nullptr && ! library.isScanning())
    {
        if (pendingPinHash != entry.fileHash)   // don't enqueue duplicate decodes
        {
            pendingPinHash = entry.fileHash;
            beginImport (resolveEntryFile (entry), false);
        }
    }
    else if (sample != nullptr)
    {
        pendingPinHash.clear();
    }
}

void SamplerPanel::timerCallback()
{
    auditionButton.setButtonText (audition.isPlaying() ? "Stop" : "Audition");
    updateStatus();
}

void SamplerPanel::scanLibrary()
{
    if (library.isScanning())
    {
        showStatusMessage (SamplerUi::scanAlreadyRunningMessage());
        return;
    }

    if (importsInFlight > 0)
    {
        showStatusMessage (SamplerUi::importInProgressMessage());
        return;
    }

    missingPaths.clear();
    library.startScan();
    updateStatus();
}

void SamplerPanel::chooseImportFiles()
{
    if (library.isScanning())
    {
        showStatusMessage (SamplerUi::scanInProgressMessage());
        return;
    }

    if (importsInFlight > 0)
    {
        showStatusMessage (SamplerUi::importInProgressMessage());
        return;
    }

    chooser = std::make_unique<juce::FileChooser> ("Import samples", juce::File{},
                                                   "*.wav;*.aif;*.aiff;*.flac");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [weak = juce::WeakReference<SamplerPanel> (this)] (const juce::FileChooser& fc)
    {
        if (auto* panel = weak.get())
            for (const auto& file : fc.getResults())
                panel->beginImport (file, false);
    });
}

void SamplerPanel::beginImport (const juce::File& file, bool auditionAfterImport)
{
    if (library.isScanning())
    {
        showStatusMessage (SamplerUi::scanInProgressMessage());
        return;
    }

    if (! file.existsAsFile())
    {
        // The lazy decode cannot proceed, so drop the pin: otherwise selecting
        // the row again would be suppressed as a duplicate request (code-review
        // N-16).
        pendingPinHash.clear();
        showStatusMessage (SamplerUi::fileNotFoundMessage (file.getFullPathName()));
        return;
    }

    ++importsInFlight;
    updateStatus();

    importPool.addJob (new ImportJob (juce::WeakReference<SamplerPanel> (this),
                                      file, library.getRootDirectory(),
                                      auditionAfterImport, auditionRequestToken),
                       true);
}

void SamplerPanel::applyImport (ImportResult result, bool auditionAfterImport, int auditionToken)
{
    --importsInFlight;

    if (! result.succeeded)
    {
        if (! pendingPinHash.empty() && result.entry.fileHash == pendingPinHash)
            pendingPinHash.clear();

        showStatusMessage (SamplerUi::importFailureMessage (result.errorMessage));
        return;
    }

    if (library.isScanning())
    {
        // Defensive: the UI gates this, but the library contract forbids index
        // mutations during a scan. Drop the import (the decoded sample is
        // released on the message thread).
        showStatusMessage (SamplerUi::scanInProgressMessage());
        return;
    }

    const auto pinRequested = ! pendingPinHash.empty() && result.entry.fileHash == pendingPinHash;
    const auto wantsSample = auditionAfterImport || pinRequested;

    auto entries = library.getEntries();
    const auto outcome = SamplerUi::mergeImportedEntry (entries, result.entry, wantsSample);

    if (outcome.skippedDuplicate)
    {
        showStatusMessage (SamplerUi::duplicateSkipMessage (result.entry.path));
        return;
    }

    library.addSample (result.entry.fileHash, result.sample);
    library.setEntries (std::move (entries));
    saveIndex();

    if (pinRequested)
    {
        zoneMap.setPinnedSample (result.sample);
        pendingPinHash.clear();
    }

    if (waveform.isShowing (result.entry.fileHash))
        waveform.setEntry (result.entry);

    if (auditionAfterImport && auditionToken == auditionRequestToken)
        audition.play (result.sample, result.entry.rootKey);

    list.updateContent();
    showStatusMessage (SamplerUi::importSuccessMessage (result.entry.path, result.entry.fileHash));
}

void SamplerPanel::handleScanResult (const ScanResult& result)
{
    missingPaths.clear();

    for (const auto& path : result.missingPaths)
        missingPaths.insert (path);

    list.updateContent();

    // Entries may have changed (loop/thumbnail/length) or disappeared; re-running
    // the selection path also retries a pin whose decode was refused while the
    // scan was in flight (code-review N-16).
    selectedRowsChanged (list.getSelectedRow());

    // A scan can drop already-indexed entries through deduplication; the files
    // stay on disk, so the shrink must be visible (code-review I-6).
    if (result.entriesDropped > 0)
        showStatusMessage (SamplerUi::entriesDroppedMessage (result.entriesDropped));
    else
        updateStatus();
}

void SamplerPanel::toggleAudition()
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
        showStatusMessage ("Select a sample first");
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

void SamplerPanel::autoMapZones()
{
    std::vector<LibraryEntry> source;
    const auto row = list.getSelectedRow();
    const auto& entries = library.getEntries();

    if (row >= 0 && row < (int) entries.size())
        source.push_back (entries[(std::size_t) row]);
    else
        source = entries;

    if (source.empty())
    {
        showStatusMessage ("No library entries to map");
        return;
    }

    ZoneMapping::AutoMapOptions options;
    options.resolveSample = [this] (const std::string& fileHash) { return library.findSample (fileHash); };

    zoneDraft.setZoneSet (ZoneMapping::buildAutoMappedZoneSet (source, options));
    showStatusMessage ("Auto-mapped " + juce::String ((int) zoneDraft.getZoneSet().zones.size()) + " zones");
}

void SamplerPanel::useFileLoop()
{
    const auto row = list.getSelectedRow();
    const auto& entries = library.getEntries();
    const auto index = zoneDraft.getSelectedIndex();
    const auto* selected = zoneDraft.getSelectedZone();

    if (row < 0 || row >= (int) entries.size() || selected == nullptr || index < 0)
    {
        showStatusMessage ("Select a sample and a zone first");
        return;
    }

    auto zone = *selected;

    if (zone.sample != nullptr && zone.sample->fileHash != entries[(std::size_t) row].fileHash)
    {
        showStatusMessage ("Selected zone uses a different sample");
        return;
    }

    const auto length = zone.sample != nullptr ? (std::int64_t) zone.sample->data.getNumSamples()
                                               : entries[(std::size_t) row].lengthSamples;
    const auto loop = LoopEditGeometry::sanitized (entries[(std::size_t) row].loop, length);

    if (loop.end <= loop.start)
    {
        showStatusMessage ("File has no valid loop");
        return;
    }

    zone.loop = loop;
    zoneDraft.updateZone (index, zone);

    showStatusMessage ("File loop applied to the selected zone");
}

void SamplerPanel::saveIndex()
{
    LibraryIndex::save (library.getRootDirectory().getChildFile ("library.json"), library.getEntries());
}

void SamplerPanel::showStatusMessage (juce::String message)
{
    statusLine.setMessage (std::move (message), juce::Time::currentTimeMillis());
    updateStatus();
}

void SamplerPanel::updateStatus()
{
    juce::String stats;

    if (library.isScanning())
        stats = "Scanning library...";
    else if (importsInFlight > 0)
        stats = "Importing (" + juce::String (importsInFlight) + ")...";
    else
        stats = juce::String ((int) library.getEntries().size()) + " samples, "
               + juce::String ((int) missingPaths.size()) + " missing, "
               + juce::String ((int) zoneDraft.getZoneSet().zones.size()) + " zones";

    // A transient import/scan message outranks the periodic statistics until it
    // expires (code-review I-1), so a result is never swallowed by the 10 Hz
    // refresh or by the change listeners.
    statusLabel.setText (statusLine.resolve (stats, juce::Time::currentTimeMillis()),
                         juce::dontSendNotification);
    scanButton.setEnabled (! library.isScanning() && importsInFlight == 0);
    importButton.setEnabled (! library.isScanning() && importsInFlight == 0);
}

juce::File SamplerPanel::resolveEntryFile (const LibraryEntry& entry) const
{
    return entry.external ? juce::File (juce::String (entry.path))
                          : library.getRootDirectory().getChildFile (juce::String (entry.path));
}
