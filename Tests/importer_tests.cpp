#include <catch2/catch_test_macros.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_graphics/juce_graphics.h>

#include "IO/SampleImporter.h"
#include "Model/SampleLibrary.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace juce;

constexpr double kSampleRate = 48000.0;
constexpr int kSignalSamples = 200;

File makeImportDirectory()
{
    auto directory = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVImporterTests");
    directory.deleteRecursively (false);
    directory.createDirectory();
    return directory;
}

AudioBuffer<float> makeTestSignal (int channels, int samples)
{
    AudioBuffer<float> buffer (channels, samples);

    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
            buffer.setSample (channel, i,
                              0.25f * std::sin (2.0f * MathConstants<float>::pi * 220.0f
                                                * (float) i / (float) kSampleRate));

    return buffer;
}

void writeAudioFile (const File& file, const AudioBuffer<float>& buffer,
                     double sampleRate, const StringPairArray& metadata)
{
    WavAudioFormat wav;
    AiffAudioFormat aiff;
    FlacAudioFormat flac;

    AudioFormat* format = &wav;

    if (file.hasFileExtension ("aif") || file.hasFileExtension ("aiff"))
        format = &aiff;
    else if (file.hasFileExtension ("flac"))
        format = &flac;

    std::unique_ptr<OutputStream> stream = file.createOutputStream();
    REQUIRE (stream != nullptr);

    auto options = AudioFormatWriterOptions{}
                       .withSampleRate (sampleRate)
                       .withNumChannels (buffer.getNumChannels())
                       .withBitsPerSample (16)
                       .withQualityOptionIndex (0);

    for (int i = 0; i < metadata.size(); ++i)
        options = options.withMetadata (metadata.getAllKeys()[i], metadata.getAllValues()[i]);

    auto writer = format->createWriterFor (stream, options);
    REQUIRE (writer != nullptr);

    REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()));
}

StringPairArray loopMetadata (int rootKey, int loopStart, int loopEnd)
{
    StringPairArray metadata;
    metadata.set ("MidiUnityNote", String (rootKey));
    metadata.set ("NumSampleLoops", "1");
    metadata.set ("Loop0Start", String (loopStart));
    metadata.set ("Loop0End", String (loopEnd));
    return metadata;
}

float maximumDifference (const AudioBuffer<float>& a, const AudioBuffer<float>& b)
{
    REQUIRE (a.getNumChannels() == b.getNumChannels());
    REQUIRE (a.getNumSamples() == b.getNumSamples());

    float maximum = 0.0f;

    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int i = 0; i < a.getNumSamples(); ++i)
            maximum = jmax (maximum, std::abs (a.getSample (channel, i) - b.getSample (channel, i)));

    return maximum;
}

Image loadPng (const File& file)
{
    FileInputStream stream (file);
    REQUIRE (stream.openedOk());

    PNGImageFormat format;
    return format.decodeImage (stream);
}

bool hasInk (const Image& image)
{
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (image.getPixelAt (x, y) != Colours::black)
                return true;

    return false;
}

File writePngBytes (const File& file)
{
    file.getParentDirectory().createDirectory();
    REQUIRE (file.replaceWithText ("placeholder-png-bytes"));
    return file;
}
}

TEST_CASE ("import decodes wav audio, root key and loop region")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Tone.wav");
    const auto signal = makeTestSignal (1, kSignalSamples);
    writeAudioFile (file, signal, kSampleRate, loopMetadata (36, 10, 50));

    const auto result = SampleImporter::importFile (file, directory);

    REQUIRE (result.succeeded);
    CHECK (result.errorMessage.empty());
    REQUIRE (result.sample != nullptr);

    CHECK (result.sample->name == "Tone.wav");
    CHECK (result.sample->sourceSampleRate == 48000.0);
    CHECK (result.sample->rootKey == 36);
    CHECK (result.sample->embeddedLoop.start == 10);
    CHECK (result.sample->embeddedLoop.end == 50);
    CHECK (result.sample->embeddedLoop.crossfadeSamples == 0);
    CHECK (result.sample->fileHash == SHA256 (file).toHexString().toStdString());
    REQUIRE (result.sample->data.getNumChannels() == 1);
    REQUIRE (result.sample->data.getNumSamples() == kSignalSamples);
    CHECK (maximumDifference (signal, result.sample->data) < 1.0e-3f);

    CHECK (result.entry.relativePath == "Tone.wav");
    CHECK (result.entry.fileHash == result.sample->fileHash);
    CHECK (result.entry.rootKey == 36);
    CHECK (result.entry.loop.start == 10);
    CHECK (result.entry.loop.end == 50);
    CHECK (result.entry.sourceSampleRate == 48000.0);
    CHECK (result.entry.lengthSamples == kSignalSamples);
    CHECK (result.entry.thumbnailPath == "thumbnails/" + result.sample->fileHash + ".png");
}

TEST_CASE ("import uses defaults without root key or loop metadata")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Stereo.aif");
    const auto signal = makeTestSignal (2, 128);
    writeAudioFile (file, signal, 44100.0, {});

    const auto result = SampleImporter::importFile (file, directory);

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->sourceSampleRate == 44100.0);
    CHECK (result.sample->rootKey == 60);
    CHECK (result.sample->embeddedLoop.start == 0);
    CHECK (result.sample->embeddedLoop.end == 0);
    REQUIRE (result.sample->data.getNumChannels() == 2);
    CHECK (result.sample->data.getNumSamples() == 128);
    CHECK (maximumDifference (signal, result.sample->data) < 1.0e-3f);
    CHECK (result.entry.relativePath == "Stereo.aif");
}

TEST_CASE ("import decodes flac files")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Pad.flac");
    const auto signal = makeTestSignal (1, 96);
    writeAudioFile (file, signal, 48000.0, {});

    const auto result = SampleImporter::importFile (file, directory);

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->sourceSampleRate == 48000.0);
    CHECK (result.sample->rootKey == 60);
    CHECK (result.sample->data.getNumSamples() == 96);
    CHECK (maximumDifference (signal, result.sample->data) < 1.0e-3f);
}

TEST_CASE ("import ignores invalid loop metadata")
{
    const auto directory = makeImportDirectory();
    const auto reversed = directory.getChildFile ("Reversed.wav");
    const auto beyondEnd = directory.getChildFile ("Beyond.wav");
    const auto signal = makeTestSignal (1, 100);

    writeAudioFile (reversed, signal, kSampleRate, loopMetadata (60, 60, 50));
    writeAudioFile (beyondEnd, signal, kSampleRate, loopMetadata (60, 10, 99999));

    const auto reversedResult = SampleImporter::importFile (reversed, directory);
    const auto beyondResult = SampleImporter::importFile (beyondEnd, directory);

    REQUIRE (reversedResult.succeeded);
    REQUIRE (beyondResult.succeeded);
    CHECK (reversedResult.sample->embeddedLoop.start == 0);
    CHECK (reversedResult.sample->embeddedLoop.end == 0);
    CHECK (beyondResult.sample->embeddedLoop.start == 0);
    CHECK (beyondResult.sample->embeddedLoop.end == 0);
}

TEST_CASE ("import resamples and scales the loop region")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Down.wav");
    auto signal = makeTestSignal (1, 100);
    signal.clear();
    signal.setSample (0, 50, 0.5f);
    writeAudioFile (file, signal, kSampleRate, loopMetadata (60, 20, 60));

    const auto result = SampleImporter::importFile (file, directory, { 24000.0, 64, 16 });

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->sourceSampleRate == 24000.0);
    REQUIRE (result.sample->data.getNumSamples() == 50);
    CHECK (result.sample->data.getNumSamples() == result.entry.lengthSamples);
    CHECK (result.entry.sourceSampleRate == 24000.0);
    CHECK (result.sample->embeddedLoop.start == 10);
    CHECK (result.sample->embeddedLoop.end == 30);
    CHECK (result.entry.loop.start == 10);
    CHECK (result.entry.loop.end == 30);

    for (int i = 0; i < result.sample->data.getNumSamples(); ++i)
        CHECK (std::isfinite (result.sample->data.getSample (0, i)));
}

TEST_CASE ("import with matching target rate keeps the decoded samples")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Same.wav");
    const auto signal = makeTestSignal (1, 80);
    writeAudioFile (file, signal, kSampleRate, {});

    const auto result = SampleImporter::importFile (file, directory, { kSampleRate, 64, 16 });

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->data.getNumSamples() == signal.getNumSamples());
    CHECK (result.sample->sourceSampleRate == kSampleRate);
    CHECK (maximumDifference (signal, result.sample->data) < 1.0e-3f);
}

TEST_CASE ("import rejects missing, empty and corrupt files")
{
    const auto directory = makeImportDirectory();

    const auto missing = SampleImporter::importFile (directory.getChildFile ("Missing.wav"), directory);
    CHECK_FALSE (missing.succeeded);
    CHECK_FALSE (missing.errorMessage.empty());
    CHECK (missing.sample == nullptr);

    const auto empty = directory.getChildFile ("Empty.wav");
    REQUIRE (empty.create());
    const auto emptyResult = SampleImporter::importFile (empty, directory);
    CHECK_FALSE (emptyResult.succeeded);
    CHECK_FALSE (emptyResult.errorMessage.empty());

    const auto corrupt = directory.getChildFile ("Corrupt.wav");
    REQUIRE (corrupt.replaceWithText ("this is not a wav file at all"));
    const auto corruptResult = SampleImporter::importFile (corrupt, directory);
    CHECK_FALSE (corruptResult.succeeded);
    CHECK_FALSE (corruptResult.errorMessage.empty());
}

TEST_CASE ("import rejects more than two channels")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Surround.wav");
    const auto signal = makeTestSignal (4, 64);
    writeAudioFile (file, signal, kSampleRate, {});

    const auto result = SampleImporter::importFile (file, directory);

    CHECK_FALSE (result.succeeded);
    CHECK_FALSE (result.errorMessage.empty());
}

TEST_CASE ("import writes a peak thumbnail image")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Tone.wav");
    writeAudioFile (file, makeTestSignal (1, kSignalSamples), kSampleRate, {});

    const auto result = SampleImporter::importFile (file, directory, { 0.0, 64, 16 });

    REQUIRE (result.succeeded);
    REQUIRE_FALSE (result.entry.thumbnailPath.empty());
    CHECK (result.entry.thumbnailPath == "thumbnails/" + result.entry.fileHash + ".png");

    const auto thumbnail = directory.getChildFile (result.entry.thumbnailPath);
    REQUIRE (thumbnail.existsAsFile());

    const auto image = loadPng (thumbnail);
    CHECK (image.getWidth() == 64);
    CHECK (image.getHeight() == 16);
    CHECK (hasInk (image));
}

TEST_CASE ("import reads aiff instrument root key metadata")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Inst.aif");

    StringPairArray metadata;
    metadata.set ("MidiUnityNote", "48");
    metadata.set ("LowNote", "0");
    metadata.set ("HighNote", "127");

    writeAudioFile (file, makeTestSignal (1, 64), kSampleRate, metadata);

    const auto result = SampleImporter::importFile (file, directory);

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->rootKey == 48);
    CHECK (result.sample->embeddedLoop.end == 0);
}

TEST_CASE ("import falls back to the default root key for invalid metadata")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Invalid.wav");

    StringPairArray metadata;
    metadata.set ("MidiUnityNote", "200");
    writeAudioFile (file, makeTestSignal (1, 64), kSampleRate, metadata);

    const auto result = SampleImporter::importFile (file, directory);

    REQUIRE (result.succeeded);
    CHECK (result.sample->rootKey == 60);
}

TEST_CASE ("import resamples up and scales the loop region")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Up.wav");
    writeAudioFile (file, makeTestSignal (1, 60), 24000.0, loopMetadata (60, 10, 30));

    const auto result = SampleImporter::importFile (file, directory, { 48000.0, 64, 16 });

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->sourceSampleRate == 48000.0);
    CHECK (result.sample->data.getNumSamples() == 120);
    CHECK (result.sample->embeddedLoop.start == 20);
    CHECK (result.sample->embeddedLoop.end == 60);
}

TEST_CASE ("thumbnail generation handles fewer samples than columns")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Tiny.wav");
    writeAudioFile (file, makeTestSignal (1, 4), kSampleRate, {});

    const auto result = SampleImporter::importFile (file, directory, { 0.0, 64, 16 });

    REQUIRE (result.succeeded);
    REQUIRE_FALSE (result.entry.thumbnailPath.empty());

    const auto image = loadPng (directory.getChildFile (result.entry.thumbnailPath));
    CHECK (image.getWidth() == 64);
    CHECK (image.getHeight() == 16);
}

TEST_CASE ("import generates thumbnails for long files without overflow")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Long.wav");
    constexpr int kLongSamples = 5000000;
    writeAudioFile (file, makeTestSignal (1, kLongSamples), kSampleRate, {});

    const auto result = SampleImporter::importFile (file, directory);

    REQUIRE (result.succeeded);
    REQUIRE (result.sample != nullptr);
    CHECK (result.sample->data.getNumSamples() == kLongSamples);
    REQUIRE_FALSE (result.entry.thumbnailPath.empty());

    const auto image = loadPng (directory.getChildFile (result.entry.thumbnailPath));
    CHECK (image.getWidth() == 512);
    CHECK (hasInk (image));
}

TEST_CASE ("import rejects files outside the library root")
{
    const auto directory = makeImportDirectory();
    const auto outside = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVOutside.wav");
    writeAudioFile (outside, makeTestSignal (1, 32), kSampleRate, {});

    const auto result = SampleImporter::importFile (outside, directory);

    CHECK_FALSE (result.succeeded);
    CHECK_FALSE (result.errorMessage.empty());
    outside.deleteFile();
}

TEST_CASE ("import rejects extensions outside the whitelist")
{
    const auto directory = makeImportDirectory();
    const auto source = directory.getChildFile ("Source.wav");
    writeAudioFile (source, makeTestSignal (1, 32), kSampleRate, {});

    const auto foreign = directory.getChildFile ("Foreign.ogg");
    REQUIRE (source.copyFileTo (foreign));

    const auto result = SampleImporter::importFile (foreign, directory);

    CHECK_FALSE (result.succeeded);
    CHECK_FALSE (result.errorMessage.empty());
}

TEST_CASE ("import rejects an empty library root and out-of-range target rates")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("A.wav");
    writeAudioFile (file, makeTestSignal (1, 32), kSampleRate, {});

    CHECK_FALSE (SampleImporter::importFile (file, {}).succeeded);
    CHECK_FALSE (SampleImporter::importFile (file, directory, { 1.0e9, 64, 16 }).succeeded);
    CHECK_FALSE (SampleImporter::importFile (file, directory, { 100.0, 64, 16 }).succeeded);
}

TEST_CASE ("thumbnail write failure does not fail the import")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("Tone.wav");
    writeAudioFile (file, makeTestSignal (1, 64), kSampleRate, {});

    const auto blocker = directory.getChildFile ("thumbnails");
    REQUIRE (blocker.replaceWithText ("blocker"));

    const auto result = SampleImporter::importFile (file, directory);

    CHECK (result.succeeded);
    CHECK (result.entry.thumbnailPath.empty());
}

TEST_CASE ("scan removes old orphaned thumbnails and keeps recent ones")
{
    const auto directory = makeImportDirectory();
    const auto file = directory.getChildFile ("A.wav");
    writeAudioFile (file, makeTestSignal (1, 64), kSampleRate, {});

    const auto imported = SampleImporter::importFile (file, directory);
    REQUIRE (imported.succeeded);

    SampleLibrary library;
    library.setRootDirectory (directory);
    library.setEntries ({ imported.entry });

    const auto referenced = directory.getChildFile (imported.entry.thumbnailPath);
    const auto oldOrphan = writePngBytes (directory.getChildFile ("thumbnails/orphan-old.png"));
    const auto freshOrphan = writePngBytes (directory.getChildFile ("thumbnails/orphan-fresh.png"));
    const auto notes = directory.getChildFile ("thumbnails/notes.txt");
    REQUIRE (notes.replaceWithText ("keep me"));

    REQUIRE (oldOrphan.setLastModificationTime (Time::getCurrentTime() - RelativeTime::hours (1)));

    const auto result = library.scanNow();

    REQUIRE (result.succeeded);
    CHECK (result.thumbnailsRemoved == 1);
    CHECK_FALSE (oldOrphan.existsAsFile());
    CHECK (freshOrphan.existsAsFile());
    CHECK (referenced.existsAsFile());
    CHECK (notes.existsAsFile());
}

TEST_CASE ("manual cleanup removes fresh orphaned thumbnails")
{
    const auto directory = makeImportDirectory();

    SampleLibrary library;
    library.setRootDirectory (directory);

    const auto freshOrphan = writePngBytes (directory.getChildFile ("thumbnails/orphan-fresh.png"));

    CHECK (library.cleanupOrphanedThumbnails (0.0) == 1);
    CHECK_FALSE (freshOrphan.existsAsFile());
}


