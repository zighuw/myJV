#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Model/Sample.h"
#include "Plugin/MyJVProcessor.h"
#include "Plugin/SamplerUiHelpers.h"

#include <cmath>
#include <memory>
#include <string>

namespace
{
using namespace juce;

constexpr double kSampleRate = 48000.0;

std::shared_ptr<Sample> makeSample (int frames = 48000)
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, frames);
    auto* data = sample->data.getWritePointer (0);

    for (int i = 0; i < frames; ++i)
        data[i] = (float) (0.5 * std::sin (6.283185307179586 * 100.0 * (double) i / kSampleRate));

    return sample;
}

LibraryEntry makeEntry (const std::string& path, const std::string& fileHash)
{
    LibraryEntry entry;
    entry.path = path;
    entry.fileHash = fileHash;
    entry.rootKey = 60;
    entry.sourceSampleRate = kSampleRate;
    entry.lengthSamples = 48000;
    return entry;
}

void pumpMessages (int milliseconds)
{
    if (auto* messageManager = MessageManager::getInstanceWithoutCreating())
        messageManager->runDispatchLoopUntil (milliseconds);
}

float renderNoteOn (MyJVProcessor& processor, int midiNote)
{
    AudioBuffer<float> buffer (6, 512);
    buffer.clear();
    MidiBuffer midi;
    midi.addEvent (MidiMessage::noteOn (1, midiNote, 1.0f), 0);
    processor.processBlock (buffer, midi);
    return buffer.getMagnitude (0, 0, 512);
}
}

// C1 end-to-end: an imported sample must become audible from host MIDI without
// any manual wiring, i.e. the processor's library-change republish must feed the
// engine a runtime whose tone 1 zone set resolves the imported sample.
TEST_CASE ("processor publishes an imported sample and renders host midi (C1)")
{
    MyJVProcessor processor;
    processor.prepareToPlay (kSampleRate, 512);

    auto& library = processor.getSampleLibrary();
    const auto root = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVProcessorTests");
    root.deleteRecursively (false);
    root.createDirectory();
    library.setRootDirectory (root);

    const auto entry = makeEntry ("imported.wav",
                                  "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

    library.addSample (entry.fileHash, makeSample());
    library.setEntries ({ entry });

    pumpMessages (400);   // let the 10 Hz timer republish the runtime

    REQUIRE (renderNoteOn (processor, 60) > 0.01f);
}

// The startup scan indexes files without decoding them, so importing (or lazily
// decoding) a file whose hash is already in the index must still cache the sample
// AND trigger a runtime rebuild - otherwise the auto-mapped zone resolves to
// nullptr and the tone stays silent (M3-01 C1 regression).
TEST_CASE ("processor republishes when a sample is decoded for an existing index entry (C1)")
{
    MyJVProcessor processor;
    processor.prepareToPlay (kSampleRate, 512);

    auto& library = processor.getSampleLibrary();
    const auto root = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVProcessorTests2");
    root.deleteRecursively (false);
    root.createDirectory();
    library.setRootDirectory (root);

    const auto entry = makeEntry ("Scanned_C4.wav",
                                  "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");

    // Index already holds the entry (as after the startup scan) but no decoded sample.
    library.setEntries ({ entry });
    REQUIRE (library.findSample (entry.fileHash) == nullptr);

    pumpMessages (300);   // publishes a runtime whose zone resolves to nullptr
    REQUIRE (renderNoteOn (processor, 60) == 0.0f);   // nothing decoded yet

    // The import path caches the decoded sample even though the index entry exists
    // (duplicate import / lazy decode).
    SamplerUi::publishImportedSample (library, entry, makeSample(), false);
    REQUIRE (library.findSample (entry.fileHash) != nullptr);

    pumpMessages (300);   // the sample appearing must rebuild the runtime

    REQUIRE (renderNoteOn (processor, 60) > 0.01f);
}
