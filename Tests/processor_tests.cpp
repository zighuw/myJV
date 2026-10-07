#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "IO/SampleImporter.h"
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

bool writeWavFile (const File& file, int frames = 4800)
{
    file.deleteFile();

    WavAudioFormat format;
    std::unique_ptr<OutputStream> stream = file.createOutputStream();

    if (stream == nullptr)
        return false;

    auto writer = format.createWriterFor (stream, AudioFormatWriterOptions()
                                                      .withSampleRate (kSampleRate)
                                                      .withNumChannels (1)
                                                      .withBitsPerSample (16));

    if (writer == nullptr)
        return false;

    AudioBuffer<float> buffer (1, frames);
    auto* data = buffer.getWritePointer (0);

    for (int i = 0; i < frames; ++i)
        data[i] = (float) (0.5 * std::sin (6.283185307179586 * 100.0 * (double) i / kSampleRate));

    return writer->writeFromAudioSampleBuffer (buffer, 0, frames);
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

// The full user path with no synthetic shortcuts: a real WAV on disk is indexed
// by the background scan, then imported (duplicate hash) and the tone must sound.
TEST_CASE ("a scanned file imported into the library sounds from host midi (C1 real path)")
{
    const auto root = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVProcessorTests3");
    root.deleteRecursively (false);
    REQUIRE (root.createDirectory().wasOk());
    REQUIRE (writeWavFile (root.getChildFile ("Sine_C4.wav")));

    MyJVProcessor processor;
    processor.prepareToPlay (kSampleRate, 512);

    auto& library = processor.getSampleLibrary();
    library.setRootDirectory (root);
    library.startScan();

    pumpMessages (800);   // let the background scan apply on the message thread

    REQUIRE (! library.isScanning());
    REQUIRE (library.getEntries().size() == 1);

    const auto scannedHash = library.getEntries().front().fileHash;
    REQUIRE (scannedHash.size() == 64);
    REQUIRE (library.findSample (scannedHash) == nullptr);   // scan does not decode

    // The user imports the already-indexed file (duplicate hash).
    const auto result = SampleImporter::importFile (root.getChildFile ("Sine_C4.wav"), root);
    REQUIRE (result.succeeded);
    REQUIRE (result.entry.fileHash == scannedHash);

    SamplerUi::publishImportedSample (library, result.entry, result.sample, false);
    pumpMessages (400);   // the decoded sample must trigger a runtime rebuild

    REQUIRE (library.findSample (scannedHash) != nullptr);
    REQUIRE (renderNoteOn (processor, 60) > 0.01f);   // "Sine_C4.wav" maps to C4 = 60
}
