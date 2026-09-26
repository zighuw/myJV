#include "IO/SampleImporter.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <limits>

namespace
{
std::string relativePathString (const juce::File& root, const juce::File& file)
{
    return file.getRelativePathFrom (root).replaceCharacter ('\\', '/').toStdString();
}

int parseRootKey (const juce::StringPairArray& metadata)
{
    const auto value = metadata.getValue ("MidiUnityNote", {});

    if (value.isEmpty())
        return 60;

    const auto parsed = value.getIntValue();

    // -1 (AIFF INST "no note") and out-of-range values fall back to the default.
    return (parsed >= 0 && parsed <= 127) ? parsed : 60;
}

LoopInfo parseEmbeddedLoop (const juce::StringPairArray& metadata, int numSamples)
{
    if (metadata.getValue ("NumSampleLoops", "0").getIntValue() <= 0)
        return {};

    const auto start = metadata.getValue ("Loop0Start", "0").getIntValue();
    const auto end = metadata.getValue ("Loop0End", "0").getIntValue();

    // WAV smpl: dwEnd is the first sample after the loop, so [start, end) maps
    // directly onto LoopInfo (ADR-014). Out-of-range data is ignored.
    if (start < 0 || end <= start || end > numSamples)
        return {};

    LoopInfo loop;
    loop.start = start;
    loop.end = end;
    return loop;
}

LoopInfo scaleLoop (const LoopInfo& loop, int sourceLength, int targetLength)
{
    if (loop.end <= loop.start || sourceLength <= 0 || targetLength <= 0)
        return {};

    const auto scale = (double) targetLength / (double) sourceLength;

    LoopInfo scaled;
    scaled.start = juce::jlimit (0, targetLength, (int) std::llround (loop.start * scale));
    scaled.end = juce::jlimit (scaled.start, targetLength, (int) std::llround (loop.end * scale));
    return scaled;
}

bool resample (const juce::AudioBuffer<float>& input, double sourceRate, double targetRate,
               juce::AudioBuffer<float>& output)
{
    const auto ratio = sourceRate / targetRate;
    const auto numOutputSamples = (int) std::llround ((double) input.getNumSamples() / ratio);

    if (numOutputSamples <= 0)
        return false;

    output.setSize (input.getNumChannels(), numOutputSamples);

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
    {
        juce::LagrangeInterpolator interpolator;
        output.clear (channel, 0, numOutputSamples);
        interpolator.process (ratio, input.getReadPointer (channel), output.getWritePointer (channel),
                              numOutputSamples, input.getNumSamples(), 0);
    }

    return true;
}

juce::String writeThumbnail (const juce::File& libraryRoot, const std::string& hash,
                             const juce::AudioBuffer<float>& data, int width, int height)
{
    if (width <= 0 || height <= 0 || data.getNumSamples() <= 0)
        return {};

    const auto directory = libraryRoot.getChildFile (kThumbnailsDirectoryName);

    if (! directory.isDirectory() && directory.createDirectory().failed())
        return {};

    // SoftwareImageType: deterministic off-message-thread rendering (the
    // default native/Direct2D type needs a device context and silently no-ops).
    juce::Image image (juce::Image::RGB, width, height, true, juce::SoftwareImageType());
    juce::Graphics graphics (image);
    graphics.fillAll (juce::Colours::black);
    graphics.setColour (juce::Colours::limegreen);

    const auto numSamples = (juce::int64) data.getNumSamples();

    for (int column = 0; column < width; ++column)
    {
        // 64-bit column math: column * numSamples overflows int for long files.
        const auto start = (int) ((juce::int64) column * numSamples / width);
        const auto end = (int) juce::jmin (numSamples,
                                           juce::jmax ((juce::int64) start + 1,
                                                       (juce::int64) (column + 1) * numSamples / width));

        float minimum = 0.0f;
        float maximum = 0.0f;

        for (int channel = 0; channel < data.getNumChannels(); ++channel)
            for (int i = start; i < end; ++i)
            {
                const auto value = data.getSample (channel, i);
                minimum = juce::jmin (minimum, value);
                maximum = juce::jmax (maximum, value);
            }

        // Positive peaks point up (image y grows downwards).
        const auto top = juce::jmap (maximum, -1.0f, 1.0f, (float) height, 0.0f);
        const auto bottom = juce::jmap (minimum, -1.0f, 1.0f, (float) height, 0.0f);
        graphics.drawVerticalLine (column, juce::jmin (top, bottom), juce::jmax (top, bottom));
    }

    const auto file = directory.getChildFile (juce::String (hash) + ".png");
    file.deleteFile();

    auto stream = file.createOutputStream();

    if (stream == nullptr)
        return {};

    juce::PNGImageFormat png;

    if (! png.writeImageToStream (image, *stream))
    {
        stream.reset();
        file.deleteFile();
        return {};
    }

    stream->flush();
    stream.reset();

    if (! file.existsAsFile())
    {
        file.deleteFile();
        return {};
    }

    return file.getRelativePathFrom (libraryRoot).replaceCharacter ('\\', '/');
}
}

ImportResult SampleImporter::importFile (const juce::File& file, const juce::File& libraryRoot,
                                         const ImportOptions& options)
{
    ImportResult result;

    if (libraryRoot.getFullPathName().isEmpty() || ! libraryRoot.isDirectory())
    {
        result.errorMessage = "invalid library root";
        return result;
    }

    if (! file.existsAsFile())
    {
        result.errorMessage = "file does not exist";
        return result;
    }

    if (! isSupportedAudioFile (file.getFileName()))
    {
        result.errorMessage = "unsupported file extension";
        return result;
    }

    if (! file.isAChildOf (libraryRoot))
    {
        result.errorMessage = "file is outside the library root";
        return result;
    }

    if (options.targetSampleRate != 0.0
        && (options.targetSampleRate < kMinImportSampleRate || options.targetSampleRate > kMaxImportSampleRate))
    {
        result.errorMessage = "invalid target sample rate";
        return result;
    }

    // WAV/AIFF/FLAC only: registerBasicFormats would also accept Ogg/mp3 on
    // some platforms, diverging from the scan whitelist. The manager takes
    // ownership of the registered formats.
    juce::AudioFormatManager formats;
    formats.registerFormat (new juce::WavAudioFormat(), false);
    formats.registerFormat (new juce::AiffAudioFormat(), false);
    formats.registerFormat (new juce::FlacAudioFormat(), false);

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        result.errorMessage = "unsupported or corrupt audio file";
        return result;
    }

    if (reader->lengthInSamples <= 0)
    {
        result.errorMessage = "audio file contains no samples";
        return result;
    }

    if (reader->numChannels < 1 || reader->numChannels > 2)
    {
        result.errorMessage = "unsupported channel count";
        return result;
    }

    if (reader->lengthInSamples > (juce::int64) kMaxImportSamples)
    {
        result.errorMessage = "audio file is too long";
        return result;
    }

    if (! (reader->sampleRate > 0.0))
    {
        result.errorMessage = "invalid sample rate";
        return result;
    }

    const auto sourceSampleRate = reader->sampleRate;
    const auto numChannels = (int) reader->numChannels;
    const auto numSamples = (int) reader->lengthInSamples;

    juce::AudioBuffer<float> decoded (numChannels, numSamples);

    if (! reader->read (&decoded, 0, numSamples, 0, true, true))
    {
        result.errorMessage = "failed to decode audio data";
        return result;
    }

    const auto metadata = reader->metadataValues;
    const auto rootKey = parseRootKey (metadata);
    auto loop = parseEmbeddedLoop (metadata, numSamples);

    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = sourceSampleRate;
    sample->rootKey = rootKey;

    if (options.targetSampleRate > 0.0
        && std::abs (options.targetSampleRate - sourceSampleRate) > 1.0e-9)
    {
        juce::AudioBuffer<float> resampled;

        if (! resample (decoded, sourceSampleRate, options.targetSampleRate, resampled))
        {
            result.errorMessage = "failed to resample audio data";
            return result;
        }

        loop = scaleLoop (loop, numSamples, resampled.getNumSamples());
        sample->data = std::move (resampled);
        sample->sourceSampleRate = options.targetSampleRate;
    }
    else
    {
        sample->data = std::move (decoded);
    }

    sample->embeddedLoop = loop;
    sample->name = relativePathString (libraryRoot, file);
    sample->fileHash = juce::SHA256 (file).toHexString().toStdString();

    result.entry.relativePath = sample->name;
    result.entry.fileHash = sample->fileHash;
    result.entry.rootKey = rootKey;
    result.entry.loop = loop;
    result.entry.sourceSampleRate = sample->sourceSampleRate;
    result.entry.lengthSamples = sample->data.getNumSamples();
    result.entry.thumbnailPath = writeThumbnail (libraryRoot, sample->fileHash, sample->data,
                                                 options.thumbnailWidth, options.thumbnailHeight).toStdString();

    result.sample = std::move (sample);
    result.succeeded = true;
    return result;
}
