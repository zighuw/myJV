#include "RenderRegression.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

namespace RenderRegression
{
namespace
{
constexpr double kSilencePower = 1.0e-18;   // -180 dB

double powerToDb (double power)
{
    return 10.0 * std::log10 (juce::jmax (power, kSilencePower));
}

std::vector<float> makeHannWindow()
{
    std::vector<float> window ((std::size_t) kFftSize);

    for (int i = 0; i < kFftSize; ++i)
        window[(std::size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                          * (float) i / (float) (kFftSize - 1));

    return window;
}
}

Metrics measure (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    Metrics metrics;
    metrics.bandsDb.assign ((std::size_t) kNumBands, -180.0);

    const auto numChannels = buffer.getNumChannels();
    const auto numFrames = buffer.getNumSamples();

    if (numChannels <= 0 || numFrames <= 0)
        return metrics;

    double sumSquares = 0.0;
    double peak = 0.0;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < numFrames; ++i)
        {
            const auto value = (double) data[i];
            sumSquares += value * value;
            peak = juce::jmax (peak, std::abs (value));
        }
    }

    metrics.rmsDb = (double) juce::Decibels::gainToDecibels (
        (float) std::sqrt (sumSquares / (double) (numChannels * numFrames)), -180.0f);
    metrics.peakDb = (double) juce::Decibels::gainToDecibels ((float) peak, -180.0f);
    metrics.sha256 = sha256OfBuffer (buffer);

    // Windowed FFT: per-bin power averaged over frames and channels, then summed
    // into octave bands. The one-sided normalisation keeps band levels on a
    // mean-square (dBFS) scale.
    const auto window = makeHannWindow();
    double windowSumSq = 0.0;

    for (auto value : window)
        windowSumSq += (double) value * (double) value;

    juce::dsp::FFT fft (kFftOrder);
    std::vector<float> fftData ((std::size_t) (2 * kFftSize), 0.0f);
    std::vector<double> binPower ((std::size_t) (kFftSize / 2 + 1), 0.0);
    int numFftFrames = 0;

    for (int start = 0; start + kFftSize <= numFrames; start += kFftHop)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);
            std::fill (fftData.begin(), fftData.end(), 0.0f);

            for (int i = 0; i < kFftSize; ++i)
                fftData[(std::size_t) i] = data[start + i] * window[(std::size_t) i];

            fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

            for (std::size_t bin = 0; bin < binPower.size(); ++bin)
                binPower[bin] += (double) fftData[bin] * (double) fftData[bin];
        }

        ++numFftFrames;
    }

    if (numFftFrames > 0)
    {
        const auto normalisation = 2.0 / ((double) kFftSize * windowSumSq
                                          * (double) numChannels * (double) numFftFrames);
        std::vector<double> bandPower ((std::size_t) kNumBands, 0.0);

        for (std::size_t bin = 0; bin < binPower.size(); ++bin)
        {
            const auto frequency = (double) bin * sampleRate / (double) kFftSize;

            for (int band = 0; band < kNumBands; ++band)
            {
                if (frequency >= kBandEdgesHz[band] && frequency < kBandEdgesHz[band + 1])
                {
                    bandPower[(std::size_t) band] += binPower[bin] * normalisation;
                    break;
                }
            }
        }

        for (int band = 0; band < kNumBands; ++band)
            metrics.bandsDb[(std::size_t) band] = powerToDb (bandPower[(std::size_t) band]);
    }

    return metrics;
}

Comparison compare (const Metrics& reference, const Metrics& rendered, const Tolerance& tolerance)
{
    Comparison result;
    result.rmsDeltaDb = rendered.rmsDb - reference.rmsDb;
    result.rmsPassed = std::abs (result.rmsDeltaDb) <= tolerance.rmsDb;
    result.referenceBandsDb = reference.bandsDb;
    result.renderedBandsDb = rendered.bandsDb;
    result.bandsPassed = true;

    const auto count = juce::jmin (reference.bandsDb.size(), rendered.bandsDb.size());

    for (std::size_t band = 0; band < count; ++band)
    {
        const auto referenceDb = reference.bandsDb[band];
        const auto renderedDb = rendered.bandsDb[band];

        // Bands that are silent in the reference only gate new audible content.
        if (referenceDb < kBandFloorDb)
        {
            if (renderedDb >= kBandFloorDb)
                result.bandsPassed = false;

            continue;
        }

        const auto delta = std::abs (renderedDb - referenceDb);

        if (delta > result.maxBandDeltaDb)
        {
            result.maxBandDeltaDb = delta;
            result.worstBand = (int) band;
        }

        if (delta > tolerance.bandDb)
            result.bandsPassed = false;
    }

    return result;
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer,
               double sampleRate, juce::String& error)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        error = "refusing to write an empty WAV: " + file.getFullPathName();
        return false;
    }

    file.deleteFile();

    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);

    if (auto* fileStream = dynamic_cast<juce::FileOutputStream*> (stream.get()))
    {
        if (! fileStream->openedOk())
        {
            error = "cannot open " + file.getFullPathName() + " for writing";
            return false;
        }
    }

    juce::WavAudioFormat format;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withNumChannels (buffer.getNumChannels())
                             .withBitsPerSample (16);
    auto writer = format.createWriterFor (stream, options);

    if (writer == nullptr)
    {
        error = "cannot create the WAV writer for " + file.getFullPathName();
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
    {
        error = "WAV write failed for " + file.getFullPathName();
        return false;
    }

    writer.reset();   // flush and close
    return true;
}

bool readWav (const juce::File& file, juce::AudioBuffer<float>& buffer,
              double& sampleRate, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        error = "cannot read " + file.getFullPathName();
        return false;
    }

    sampleRate = reader->sampleRate;
    buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
    return true;
}

juce::String sha256OfBuffer (const juce::AudioBuffer<float>& buffer)
{
    juce::MemoryBlock block;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        block.append (buffer.getReadPointer (channel),
                      sizeof (float) * (std::size_t) buffer.getNumSamples());

    return juce::SHA256 (block.getData(), block.getSize()).toHexString();
}

juce::String sha256OfFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return {};

    return juce::SHA256 (file).toHexString();
}

const CaseMeta* Manifest::find (const juce::String& caseName) const
{
    for (const auto& meta : cases)
        if (meta.name == caseName)
            return &meta;

    return nullptr;
}

bool readManifest (const juce::File& file, Manifest& manifest, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "manifest not found: " + file.getFullPathName();
        return false;
    }

    const auto parsed = juce::JSON::parse (file);

    if (! parsed.isObject())
    {
        error = "manifest is not a JSON object: " + file.getFullPathName();
        return false;
    }

    manifest = {};
    manifest.schemaVersion = (int) parsed.getProperty ("schemaVersion", 0);
    manifest.generator = parsed.getProperty ("generator", {}).toString();
    manifest.sampleRate = (double) parsed.getProperty ("sampleRate", 48000.0);
    manifest.blockSize = (int) parsed.getProperty ("blockSize", 512);

    if (auto* cases = parsed.getProperty ("cases", {}).getArray())
    {
        for (const auto& item : *cases)
        {
            if (! item.isObject())
                continue;

            CaseMeta meta;
            meta.name = item.getProperty ("name", {}).toString();
            meta.file = item.getProperty ("file", {}).toString();
            meta.frames = (int) item.getProperty ("frames", 0);
            meta.channels = (int) item.getProperty ("channels", 0);
            meta.renderSha256 = item.getProperty ("renderSha256", {}).toString();
            meta.fileSha256 = item.getProperty ("fileSha256", {}).toString();
            meta.rmsDb = (double) item.getProperty ("rmsDb", 0.0);
            meta.peakDb = (double) item.getProperty ("peakDb", 0.0);

            if (auto* bands = item.getProperty ("bandsDb", {}).getArray())
                for (const auto& band : *bands)
                    meta.bandsDb.push_back ((double) band);

            if (auto* tolerance = item.getProperty ("tolerance", {}).getDynamicObject())
            {
                meta.tolerance.rmsDb = (double) tolerance->getProperty ("rmsDb");
                meta.tolerance.bandDb = (double) tolerance->getProperty ("bandDb");
            }

            manifest.cases.push_back (std::move (meta));
        }
    }

    return true;
}

bool writeManifest (const juce::File& file, const Manifest& manifest, juce::String& error)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("schemaVersion", manifest.schemaVersion);
    root->setProperty ("generator", manifest.generator);
    root->setProperty ("sampleRate", manifest.sampleRate);
    root->setProperty ("blockSize", manifest.blockSize);

    juce::Array<juce::var> cases;

    for (const auto& meta : manifest.cases)
    {
        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty ("name", meta.name);
        object->setProperty ("file", meta.file);
        object->setProperty ("frames", meta.frames);
        object->setProperty ("channels", meta.channels);
        object->setProperty ("renderSha256", meta.renderSha256);
        object->setProperty ("fileSha256", meta.fileSha256);
        object->setProperty ("rmsDb", meta.rmsDb);
        object->setProperty ("peakDb", meta.peakDb);

        juce::Array<juce::var> bands;

        for (auto band : meta.bandsDb)
            bands.add (band);

        object->setProperty ("bandsDb", bands);

        juce::DynamicObject::Ptr tolerance = new juce::DynamicObject();
        tolerance->setProperty ("rmsDb", meta.tolerance.rmsDb);
        tolerance->setProperty ("bandDb", meta.tolerance.bandDb);
        object->setProperty ("tolerance", juce::var (tolerance.get()));

        cases.add (juce::var (object.get()));
    }

    root->setProperty ("cases", cases);

    const auto text = juce::JSON::toString (juce::var (root.get()), false, 6);

    if (! file.replaceWithText (text))
    {
        error = "cannot write the manifest: " + file.getFullPathName();
        return false;
    }

    return true;
}

juce::String formatMetrics (const Metrics& metrics)
{
    juce::String text;
    text << "rms " << juce::String (metrics.rmsDb, 3) << " dB, peak "
         << juce::String (metrics.peakDb, 3) << " dB";
    return text;
}

juce::String formatComparison (const Comparison& comparison, const char* caseName)
{
    juce::String text;
    text << caseName << ": rms delta " << juce::String (comparison.rmsDeltaDb, 4)
         << " dB, max band delta " << juce::String (comparison.maxBandDeltaDb, 4)
         << " dB @ band " << comparison.worstBand
         << (comparison.passed() ? " (pass)" : " (FAIL)");

    const auto count = juce::jmin (comparison.referenceBandsDb.size(), comparison.renderedBandsDb.size());

    for (std::size_t band = 0; band < count; ++band)
        text << "\n  band " << (int) band << " ["
             << juce::String (kBandEdgesHz[band], 1) << "-" << juce::String (kBandEdgesHz[band + 1], 1) << " Hz]: ref "
             << juce::String (comparison.referenceBandsDb[band], 3) << " dB, rendered "
             << juce::String (comparison.renderedBandsDb[band], 3) << " dB, delta "
             << juce::String (comparison.renderedBandsDb[band] - comparison.referenceBandsDb[band], 3) << " dB";

    return text;
}
}
