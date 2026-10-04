// M2-11 tuning kit and measurements. Hidden `[.tuning]` case: renders the
// listening kit (parameter sweeps, one WAV per item) into REVIEWS/M2-11/kit/
// and writes REVIEWS/M2-11/measurements.txt for the objectively measurable
// constants. Never runs in CI; invoke explicitly:
//   myJV_render_tests.exe "[.tuning]"

#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Model/Sample.h"
#include "Model/ZoneSet.h"
#include "Params/Calibration.h"
#include "Params/ParamSnapshot.h"
#include "RenderRegression.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
using namespace juce;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

struct MidiEventSpec
{
    int samplePosition = 0;
    MidiMessage message;
};

ToneSnapshot baseTone()
{
    ToneSnapshot tone;
    tone.wg.toneSwitch = true;
    tone.wg.keyLow = 0;
    tone.wg.keyHigh = 127;
    tone.wg.velLow = 1;
    tone.wg.velHigh = 127;
    tone.wg.pitchKeyfollow = 100.0f;
    tone.tvf.type = 1;
    tone.tvf.cutoff = 127.0f;
    tone.tva.level = 127.0f;

    for (int i = 0; i < 4; ++i)
    {
        tone.pEnv.level[i] = 127.0f;
        tone.tvf.fEnv.level[i] = 127.0f;
    }

    for (int i = 0; i < 3; ++i)
        tone.tva.aEnv.level[i] = 127.0f;

    tone.pan.position = 64.0f;
    tone.output.level = 127.0f;
    tone.lfo[0].rate = 64.0f;
    tone.lfo[1].rate = 64.0f;
    return tone;
}

std::shared_ptr<Sample> makeSample (bool noise)
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, (int) kSampleRate);
    auto* data = sample->data.getWritePointer (0);

    if (noise)
    {
        Random random (0x5eed);
        for (int i = 0; i < sample->data.getNumSamples(); ++i)
            data[i] = 0.5f * (random.nextFloat() * 2.0f - 1.0f);
    }
    else
    {
        for (int i = 0; i < sample->data.getNumSamples(); ++i)
            data[i] = (float) (0.5 * std::sin (MathConstants<double>::twoPi * 100.0 * (double) i / kSampleRate));
    }

    return sample;
}

AudioBuffer<float> renderAudio (const ToneSnapshot& tone, int frames,
                                const std::vector<MidiEventSpec>& midi, bool noise)
{
    auto zoneSet = std::make_shared<ZoneSet>();
    Zone zone;
    zone.sample = makeSample (noise);
    zone.loopMode = LoopMode::Sustain;
    zone.loop.start = 0;
    zone.loop.end = zone.sample->data.getNumSamples();
    zoneSet->zones.push_back (zone);

    auto runtime = std::make_shared<PatchRuntime>();
    runtime->snapshot.tones[0] = tone;
    runtime->zoneSets[0] = zoneSet;
    runtime->rawZoneSets[0] = zoneSet.get();

    AssetReclaimer reclaimer;
    reclaimer.publish (runtime);

    SynthEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setParamSnapshotSource (nullptr, &reclaimer);

    AudioBuffer<float> output (2, frames);
    output.clear();

    for (int start = 0; start < frames; start += kBlockSize)
    {
        const auto numSamples = jmin (kBlockSize, frames - start);

        MidiBuffer midiBuffer;

        for (const auto& event : midi)
            if (event.samplePosition >= start && event.samplePosition < start + numSamples)
                midiBuffer.addEvent (event.message, event.samplePosition - start);

        BusBuffers buses;
        buses.l[0] = output.getWritePointer (0) + start;
        buses.r[0] = output.getWritePointer (1) + start;

        engine.process (buses, midiBuffer, numSamples);
    }

    return output;
}

AudioBuffer<float> renderNote (const ToneSnapshot& tone, int frames, bool noise = false,
                               int note = 60, int velocity = 100, int noteOffSample = -1)
{
    std::vector<MidiEventSpec> midi { { 0, MidiMessage::noteOn (1, note, (uint8) velocity) } };

    if (noteOffSample >= 0)
        midi.push_back ({ noteOffSample, MidiMessage::noteOff (1, note) });

    return renderAudio (tone, frames, midi, noise);
}

AudioBuffer<float> appendSegments (const std::vector<AudioBuffer<float>>& segments, int gapSamples)
{
    int total = 0;

    for (const auto& segment : segments)
        total += segment.getNumSamples() + gapSamples;

    AudioBuffer<float> output (2, total);
    output.clear();
    int write = 0;

    for (const auto& segment : segments)
    {
        for (int channel = 0; channel < 2; ++channel)
            output.copyFrom (channel, write, segment, channel, 0, segment.getNumSamples());

        write += segment.getNumSamples() + gapSamples;
    }

    return output;
}

File tuningDirectory()
{
    return File (String (MYJV_REPO_DIR)).getChildFile ("REVIEWS").getChildFile ("M2-11");
}

// --- signal analysis helpers ------------------------------------------------

std::vector<double> rmsEnvelope (const AudioBuffer<float>& buffer, int windowSamples)
{
    std::vector<double> envelope;
    const auto numSamples = buffer.getNumSamples();

    for (int start = 0; start + windowSamples <= numSamples; start += windowSamples)
    {
        double sum = 0.0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int i = 0; i < windowSamples; ++i)
                sum += (double) data[start + i] * (double) data[start + i];
        }

        envelope.push_back (std::sqrt (sum / (double) (buffer.getNumChannels() * windowSamples)));
    }

    return envelope;
}

double timeToFractionSeconds (const std::vector<double>& envelope, double hopSeconds, double fraction)
{
    if (envelope.empty())
        return 0.0;

    const auto peak = *std::max_element (envelope.begin(), envelope.end());

    for (std::size_t i = 0; i < envelope.size(); ++i)
        if (envelope[i] >= fraction * peak)
            return (double) i * hopSeconds;

    return (double) envelope.size() * hopSeconds;
}

double dominantFrequencyHz (const std::vector<double>& samples, double sampleRate)
{
    const auto order = 16;
    const auto fftSize = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> data ((std::size_t) fftSize * 2, 0.0f);

    double mean = 0.0;

    for (auto value : samples)
        mean += value;

    mean /= jmax ((double) samples.size(), 1.0);
    const auto count = jmin ((std::size_t) fftSize, samples.size());

    for (std::size_t i = 0; i < count; ++i)
    {
        const auto hann = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * (double) i / (double) (count - 1));
        data[i] = (float) ((samples[i] - mean) * hann);
    }

    fft.performFrequencyOnlyForwardTransform (data.data(), true);

    // Ignore DC and very low bins; find the strongest peak.
    const auto minBin = (int) std::ceil (0.02 / (sampleRate / (double) fftSize));
    int bestBin = minBin;
    float best = 0.0f;

    for (int bin = minBin; bin < fftSize / 2; ++bin)
        if (data[(std::size_t) bin] > best)
        {
            best = data[(std::size_t) bin];
            bestBin = bin;
        }

    return (double) bestBin * sampleRate / (double) fftSize;
}

double zeroCrossingHz (const AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const auto* data = buffer.getReadPointer (0);
    int crossings = 0;

    for (int i = startSample + 1; i < startSample + numSamples; ++i)
        if ((data[i - 1] <= 0.0f && data[i] > 0.0f) || (data[i - 1] > 0.0f && data[i] <= 0.0f))
            ++crossings;

    return (double) crossings * kSampleRate / (2.0 * (double) numSamples);
}

std::vector<double> spectrumDb (const AudioBuffer<float>& buffer, int fftOrder)
{
    const auto fftSize = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> window ((std::size_t) fftSize);
    double windowSumSq = 0.0;

    for (int i = 0; i < fftSize; ++i)
    {
        window[(std::size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
        windowSumSq += (double) window[(std::size_t) i] * (double) window[(std::size_t) i];
    }

    std::vector<double> power ((std::size_t) fftSize / 2 + 1, 0.0);
    std::vector<float> data ((std::size_t) fftSize * 2, 0.0f);
    int frames = 0;

    for (int start = 0; start + fftSize <= buffer.getNumSamples(); start += fftSize / 2)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* source = buffer.getReadPointer (channel);
            std::fill (data.begin(), data.end(), 0.0f);

            for (int i = 0; i < fftSize; ++i)
                data[(std::size_t) i] = source[start + i] * window[(std::size_t) i];

            fft.performFrequencyOnlyForwardTransform (data.data(), true);

            for (std::size_t bin = 0; bin < power.size(); ++bin)
                power[bin] += (double) data[bin] * (double) data[bin];
        }

        ++frames;
    }

    std::vector<double> db (power.size(), -300.0);

    if (frames > 0)
        for (std::size_t bin = 0; bin < power.size(); ++bin)
            db[bin] = 10.0 * std::log10 (jmax (power[bin] / (double) (frames * buffer.getNumChannels() * (int) fftSize * windowSumSq), 1.0e-30));

    return db;
}

double minusDbHz (const std::vector<double>& referenceDb, const std::vector<double>& filteredDb,
                  double sampleRate, int fftSize, double thresholdDb)
{
    const auto binHz = sampleRate / (double) fftSize;
    const auto count = jmin (referenceDb.size(), filteredDb.size());

    // Passband reference: response well below the lowest tunable cutoff.
    double passband = -300.0;

    for (std::size_t bin = 0; bin < count; ++bin)
        if ((double) bin * binHz >= 40.0 && (double) bin * binHz <= 100.0)
            passband = jmax (passband, filteredDb[bin] - referenceDb[bin]);

    // First bin above the passband where the response drops below the threshold.
    for (std::size_t bin = (std::size_t) (100.0 / binHz); bin < count; ++bin)
        if (filteredDb[bin] - referenceDb[bin] < passband - thresholdDb)
            return (double) bin * binHz;

    return 0.0;
}

// --- measurements -----------------------------------------------------------
String measureEnvTime (double& maxErrorPercent)
{
    String report;
    maxErrorPercent = 0.0;

    for (int param : { 0, 64, 96, 127 })
    {
        auto tone = baseTone();
        tone.tva.aEnv.time[0] = (float) param;
        const auto expectedMs = Calibration::kEnvTimeMinMs
                                * std::pow (Calibration::kEnvTimeMaxMs / Calibration::kEnvTimeMinMs, param / 127.0);
        const auto frames = (int) ((expectedMs / 1000.0 * 1.15 + 0.2) * kSampleRate);
        const auto rendered = renderNote (tone, frames);
        const auto window = expectedMs < 50.0 ? 8 : 32;
        const auto envelope = rmsEnvelope (rendered, window);
        const auto measuredMs = timeToFractionSeconds (envelope, (double) window / kSampleRate, 0.995) * 1000.0;
        const auto error = std::abs (measuredMs - expectedMs) / expectedMs * 100.0;
        maxErrorPercent = jmax (maxErrorPercent, error);
        report << "ENV attack param=" << param
               << " expected=" << String (expectedMs, 2) << " ms"
               << " measured=" << String (measuredMs, 2) << " ms"
               << " error=" << String (error, 2) << " %"
               << (param == 0 ? " (includes the 1.5 ms sample start fade)" : "") << "\n";
    }

    return report;
}

String measureCutoff (double& maxErrorPercent)
{
    String report;
    maxErrorPercent = 0.0;

    const auto fftOrder = 13;
    const auto frames = (int) (3.0 * kSampleRate);

    auto referenceTone = baseTone();
    referenceTone.tvf.type = 0;   // OFF
    const auto reference = spectrumDb (renderNote (referenceTone, frames, true), fftOrder);

    for (int param : { 0, 32, 64, 96, 127 })
    {
        if (param == 0)
        {
            report << "CUTOFF param=0 expected=20.0 Hz measured=n/a (below the 100 Hz measurement floor; formula endpoint)\n";
            continue;
        }

        auto tone = baseTone();
        tone.tvf.type = 1;   // LPF
        tone.tvf.cutoff = (float) param;
        const auto filtered = spectrumDb (renderNote (tone, frames, true), fftOrder);
        const auto expectedHz = Calibration::kCutoffMinHz
                                * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz, param / 127.0);
        // With resonance 0 the SVF Q is 0.5, so |H(f0)| = -6.02 dB: the first
        // bin below passband - 6.02 dB is f0 itself.
        const auto measuredHz = minusDbHz (reference, filtered, kSampleRate, 1 << fftOrder, 6.02);
        const auto error = expectedHz > 0.0 ? std::abs (measuredHz - expectedHz) / expectedHz * 100.0 : 0.0;
        maxErrorPercent = jmax (maxErrorPercent, error);
        report << "CUTOFF param=" << param
               << " expected=" << String (expectedHz, 1) << " Hz"
               << " measured(f0 @ Q=0.5)=" << String (measuredHz, 1) << " Hz"
               << " error=" << String (error, 1) << " %\n";
    }

    return report;
}

String measureResonance (double& measuredQ)
{
    const auto fftOrder = 13;
    const auto frames = (int) (3.0 * kSampleRate);

    auto referenceTone = baseTone();
    referenceTone.tvf.type = 0;
    const auto reference = spectrumDb (renderNote (referenceTone, frames, true), fftOrder);

    auto tone = baseTone();
    tone.tvf.type = 1;
    tone.tvf.cutoff = 64.0f;
    tone.tvf.resonance = 127.0f;
    const auto filtered = spectrumDb (renderNote (tone, frames, true), fftOrder);

    const auto centreHz = Calibration::kCutoffMinHz
                          * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz, 64.0 / 127.0);
    const auto binHz = kSampleRate / (double) (1 << fftOrder);
    double peakGainDb = -300.0;

    for (std::size_t bin = 0; bin < reference.size(); ++bin)
        if (std::abs ((double) bin * binHz - centreHz) <= centreHz * 0.15)
            peakGainDb = jmax (peakGainDb, filtered[bin] - reference[bin]);

    measuredQ = std::pow (10.0, peakGainDb / 20.0);
    String report;
    report << "RESONANCE param=127 at cutoff " << String (centreHz, 1) << " Hz"
           << " peak gain=" << String (peakGainDb, 2) << " dB"
           << " => Q~" << String (measuredQ, 2)
           << " (kResonanceMaxQ=" << Calibration::kResonanceMaxQ << ")\n";
    return report;
}

String measurePkg (double& peakGainDb)
{
    const auto fftOrder = 13;
    const auto frames = (int) (3.0 * kSampleRate);
    peakGainDb = -300.0;

    auto referenceTone = baseTone();
    referenceTone.tvf.type = 0;
    const auto reference = spectrumDb (renderNote (referenceTone, frames, true), fftOrder);

    auto tone = baseTone();
    tone.tvf.type = 4;   // PKG; gain is scaled by the resonance parameter
    tone.tvf.cutoff = 64.0f;
    tone.tvf.resonance = 127.0f;
    const auto filtered = spectrumDb (renderNote (tone, frames, true), fftOrder);

    const auto centreHz = Calibration::kCutoffMinHz
                          * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz, 64.0 / 127.0);
    const auto binHz = kSampleRate / (double) (1 << fftOrder);

    for (std::size_t bin = 0; bin < reference.size(); ++bin)
        if (std::abs ((double) bin * binHz - centreHz) <= centreHz * 0.15)
            peakGainDb = jmax (peakGainDb, filtered[bin] - reference[bin]);

    String report;
    report << "PKG at cutoff " << String (centreHz, 1) << " Hz, resonance=127"
           << " peak gain=" << String (peakGainDb, 2) << " dB"
           << " (kPkgGainDb=" << Calibration::kPkgGainDb << ")\n";
    return report;
}

String measureLfoRate (double& maxErrorPercent)
{
    String report;
    maxErrorPercent = 0.0;

    for (int param : { 32, 64, 96, 127 })
    {
        auto tone = baseTone();
        tone.tva.lfo1Depth = 60.0f;
        tone.lfo[0].rate = (float) param;
        const auto frames = (int) (10.0 * kSampleRate);
        const auto rendered = renderNote (tone, frames);
        const auto envelope = rmsEnvelope (rendered, 128);
        const auto measuredHz = dominantFrequencyHz (envelope, kSampleRate / 128.0);
        const auto expectedHz = Calibration::kLfoRateMinHz
                                * std::pow (Calibration::kLfoRateMaxHz / Calibration::kLfoRateMinHz, param / 127.0);
        const auto error = std::abs (measuredHz - expectedHz) / expectedHz * 100.0;
        maxErrorPercent = jmax (maxErrorPercent, error);
        report << "LFO rate param=" << param
               << " expected=" << String (expectedHz, 3) << " Hz"
               << " measured=" << String (measuredHz, 3) << " Hz"
               << " error=" << String (error, 1) << " %\n";
    }

    return report;
}

String measureDelay (double& maxErrorPercent)
{
    String report;
    maxErrorPercent = 0.0;

    for (int param : { 64, 96, 127 })
    {
        auto tone = baseTone();
        tone.wg.toneDelayMode = 0;   // NORMAL: the tone start is delayed
        tone.wg.toneDelayTime = (float) param;
        const auto expectedMs = Calibration::kToneDelayMinMs
                                * std::pow (Calibration::kToneDelayMaxMs / Calibration::kToneDelayMinMs, param / 127.0);
        const auto frames = (int) ((expectedMs / 1000.0 + 1.0) * kSampleRate);
        const auto rendered = renderNote (tone, frames);   // held note
        const auto window = 32;
        const auto envelope = rmsEnvelope (rendered, window);
        const auto measuredMs = timeToFractionSeconds (envelope, (double) window / kSampleRate, 0.5) * 1000.0;
        const auto error = expectedMs > 0.0 ? std::abs (measuredMs - expectedMs) / expectedMs * 100.0 : 0.0;
        maxErrorPercent = jmax (maxErrorPercent, error);
        report << "TONE DELAY param=" << param
               << " expected=" << String (expectedMs, 1) << " ms"
               << " measured(onset)=" << String (measuredMs, 1) << " ms"
               << " error=" << String (error, 1) << " %\n";
    }

    return report;
}

String measureWaveGain (double& maxErrorDb)
{
    String report;
    maxErrorDb = 0.0;

    auto tone = baseTone();
    const auto frames = (int) (1.0 * kSampleRate);
    std::vector<AudioBuffer<float>> segments;

    for (int index : { 0, 1, 2, 3 })
    {
        tone.wg.waveGain = index;
        segments.push_back (renderNote (tone, frames));
    }

    const auto combined = appendSegments (segments, 0);
    std::vector<double> measured (4, 0.0);

    for (int index : { 0, 1, 2, 3 })
    {
        const auto start = index * frames;
        measured[(std::size_t) index] = (double) Decibels::gainToDecibels (
            combined.getRMSLevel (0, start, frames), -100.0f);
    }

    for (int index : { 0, 1, 2, 3 })
    {
        const auto measuredStep = measured[(std::size_t) index] - measured[1];
        const auto expectedStep = (double) Calibration::kWaveGainDb[index] - (double) Calibration::kWaveGainDb[1];
        const auto errorDb = std::abs (measuredStep - expectedStep);
        maxErrorDb = juce::jmax (maxErrorDb, errorDb);
        report << "WAVE GAIN index=" << index
               << " expected step=" << String (expectedStep, 1) << " dB"
               << " measured step=" << String (measuredStep, 2) << " dB\n";
    }

    return report;
}

String measurePanDepth (double& minBalance, double& maxBalance)
{
    auto tone = baseTone();
    tone.pan.lfo1Depth = 127.0f;
    tone.lfo[0].rate = 64.0f;
    const auto rendered = renderNote (tone, (int) (4.0 * kSampleRate));
    const auto window = 512;
    minBalance = 1.0;
    maxBalance = -1.0;

    for (int start = 0; start + window <= rendered.getNumSamples(); start += window)
    {
        const auto l = rendered.getRMSLevel (0, start, window);
        const auto r = rendered.getRMSLevel (1, start, window);

        if (l + r < 1.0e-4f)
            continue;

        const auto balance = (double) (l - r) / (double) (l + r);
        minBalance = jmin (minBalance, balance);
        maxBalance = jmax (maxBalance, balance);
    }

    String report;
    report << "PAN LFO depth=127 balance range = [" << String (minBalance, 3) << ", "
           << String (maxBalance, 3) << "] (kRandomPan=" << Calibration::kRandomPan << ")\n";
    return report;
}

String measurePitchDepth (double& measuredSemitones)
{
    auto base = baseTone();

    auto openTone = base;
    openTone.pEnv.depth = 63;

    for (int i = 0; i < 4; ++i)
        openTone.pEnv.level[i] = 127.0f;   // constant full depth

    const auto frames = (int) (1.5 * kSampleRate);
    const auto open = renderNote (openTone, frames);
    const auto flat = renderNote (base, frames);
    const auto openHz = zeroCrossingHz (open, (int) (0.5 * kSampleRate), (int) (0.1 * kSampleRate));
    const auto baseHz = zeroCrossingHz (flat, (int) (0.5 * kSampleRate), (int) (0.1 * kSampleRate));
    measuredSemitones = 12.0 * std::log2 (openHz / jmax (baseHz, 1.0));

    String report;
    report << "P-ENV depth=63 (constant full vs off): " << String (baseHz, 1) << " -> "
           << String (openHz, 1) << " Hz => " << String (measuredSemitones, 2)
           << " semitones (kPEnvDepthSemitones=" << Calibration::kPEnvDepthSemitones << ")\n";
    return report;
}

String measureMatrixCutoff (double& measuredOctaves)
{
    auto tone = baseTone();
    tone.tvf.cutoff = 32.0f;
    tone.ctrl[0].dest[0] = 2;   // CUTOFF
    tone.ctrl[0].depth[0] = 63;

    const auto frames = (int) (3.0 * kSampleRate);
    const auto fftOrder = 13;

    const auto noMatrix = renderNote (tone, frames, true);
    auto drivenTone = tone;
    std::vector<MidiEventSpec> midi { { 0, MidiMessage::noteOn (1, 60, (uint8) 100) } };

    for (int i = 0; i < 64; ++i)
        midi.push_back ({ (int) (i * 0.02 * kSampleRate), MidiMessage::controllerEvent (1, 1, 127) });

    const auto driven = renderAudio (drivenTone, frames, midi, true);

    const auto spectrumA = spectrumDb (noMatrix, fftOrder);
    const auto spectrumB = spectrumDb (driven, fftOrder);
    const auto binHz = kSampleRate / (double) (1 << fftOrder);
    double sumA = 0.0, sumB = 0.0, weightedA = 0.0, weightedB = 0.0;

    for (std::size_t bin = 1; bin < spectrumA.size(); ++bin)
    {
        const auto frequency = (double) bin * binHz;

        if (frequency > 12000.0)
            break;

        const auto powerA = std::pow (10.0, spectrumA[bin] / 10.0);
        const auto powerB = std::pow (10.0, spectrumB[bin] / 10.0);
        sumA += powerA;
        sumB += powerB;
        weightedA += powerA * frequency;
        weightedB += powerB * frequency;
    }

    const auto centroidA = weightedA / jmax (sumA, 1.0e-30);
    const auto centroidB = weightedB / jmax (sumB, 1.0e-30);
    measuredOctaves = std::log2 (centroidB / jmax (centroidA, 1.0));

    String report;
    report << "MATRIX cutoff depth=63 CC1=127: centroid " << String (centroidA, 1) << " -> "
           << String (centroidB, 1) << " Hz => " << String (measuredOctaves, 2)
           << " octaves (kMatrixCutoffOctaves=" << Calibration::kMatrixCutoffOctaves << ")\n";
    return report;
}

double centroidHz (const AudioBuffer<float>& buffer, int startSample, int numSamples, int fftOrder)
{
    AudioBuffer<float> slice (buffer.getNumChannels(), numSamples);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        slice.copyFrom (channel, 0, buffer, channel, startSample, numSamples);

    const auto db = spectrumDb (slice, fftOrder);
    const auto binHz = kSampleRate / (double) (1 << fftOrder);
    double sum = 0.0, weighted = 0.0;

    for (std::size_t bin = 1; bin < db.size(); ++bin)
    {
        const auto frequency = (double) bin * binHz;

        if (frequency > 12000.0)
            break;

        const auto power = std::pow (10.0, db[bin] / 10.0);
        sum += power;
        weighted += power * frequency;
    }

    return weighted / jmax (sum, 1.0e-30);
}

String measureLfoPitchDepth (double& measuredSemitones)
{
    auto tone = baseTone();
    tone.wg.pitchLfo1Depth = 127.0f;
    tone.lfo[0].rate = 32.0f;   // ~0.23 Hz: 300 ms analysis windows stay near-static
    const auto rendered = renderNote (tone, (int) (12.0 * kSampleRate));
    double minHz = 1.0e9, maxHz = 0.0;

    for (int start = 4800; start + 14400 < rendered.getNumSamples(); start += 4800)
    {
        const auto hz = zeroCrossingHz (rendered, start, 14400);   // 300 ms window

        if (hz > 1.0)
        {
            minHz = jmin (minHz, hz);
            maxHz = jmax (maxHz, hz);
        }
    }

    measuredSemitones = 12.0 * std::log2 (maxHz / jmax (minHz, 1.0));

    String report;
    report << "LFO pitch depth=127: " << String (minHz, 1) << "-" << String (maxHz, 1) << " Hz => "
           << String (measuredSemitones, 2) << " semitones peak-to-peak (2 x kLfoPitchSemitones="
           << 2.0 * Calibration::kLfoPitchSemitones << ")\n";
    return report;
}

String measureFEnvDepth (double& measuredOctaves)
{
    auto base = baseTone();
    base.tvf.cutoff = 32.0f;

    auto openTone = base;
    openTone.tvf.fEnv.depth = 63;

    for (int i = 0; i < 4; ++i)
        openTone.tvf.fEnv.level[i] = 127.0f;   // constant full depth

    const auto frames = (int) (1.5 * kSampleRate);
    const auto open = renderNote (openTone, frames, true);
    const auto flat = renderNote (base, frames, true);
    const auto fftOrder = 13;
    const auto window = 1 << fftOrder;
    const auto openHz = centroidHz (open, (int) (0.5 * kSampleRate), window, fftOrder);
    const auto baseHz = centroidHz (flat, (int) (0.5 * kSampleRate), window, fftOrder);
    measuredOctaves = std::log2 (openHz / jmax (baseHz, 1.0));

    String report;
    report << "F-ENV depth=63 (constant full vs off): centroid " << String (baseHz, 1) << " -> "
           << String (openHz, 1) << " Hz => " << String (measuredOctaves, 2)
           << " octaves (kFEnvDepthOctaves=" << Calibration::kFEnvDepthOctaves << ")\n";
    return report;
}

String measureMatrixPitch (double& measuredSemitones){
    auto tone = baseTone();
    tone.ctrl[0].dest[0] = 1;   // PITCH
    tone.ctrl[0].depth[0] = 63;
    const auto frames = (int) (2.0 * kSampleRate);
    std::vector<MidiEventSpec> midi { { 0, MidiMessage::noteOn (1, 60, (uint8) 100) },
                                      { (int) (0.2 * kSampleRate), MidiMessage::controllerEvent (1, 1, 127) } };
    const auto rendered = renderAudio (tone, frames, midi, false);
    const auto earlyHz = zeroCrossingHz (rendered, (int) (0.05 * kSampleRate), (int) (0.05 * kSampleRate));
    const auto lateHz = zeroCrossingHz (rendered, (int) (1.5 * kSampleRate), (int) (0.05 * kSampleRate));
    measuredSemitones = 12.0 * std::log2 (lateHz / jmax (earlyHz, 1.0));

    String report;
    report << "MATRIX pitch depth=63 CC1=127: " << String (earlyHz, 1) << " -> " << String (lateHz, 1) << " Hz => "
           << String (measuredSemitones, 2) << " semitones (kMatrixPitchSemitones="
           << Calibration::kMatrixPitchSemitones << ")\n";
    return report;
}

void writeText (const File& file, const String& text)
{
    file.replaceWithText (text, false, false, "\n");
}

void generateKit (const File& directory)
{
    directory.createDirectory();
    const auto gap = (int) (0.4 * kSampleRate);
    String readme = "# M2-11 listening kit\n\n"
                    "由 `myJV_render_tests.exe \"[.tuning]\"` 生成；逐个文件试听并给出裁决。\n\n";

    auto writeKit = [&] (const String& name, const AudioBuffer<float>& buffer, const String& listenFor)
    {
        String error;

        if (! RenderRegression::writeWav (directory.getChildFile (name), buffer, kSampleRate, error))
        {
            std::cout << "[tuning] WAV failed: " << error << std::endl;
            return;
        }

        readme << "- `" << name << "`: " << listenFor << "\n";
    };

    // ENV attack (A-ENV time1 sweep).
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 32, 64, 96, 127 })
        {
            auto tone = baseTone();
            tone.tva.aEnv.time[0] = (float) param;
            const auto expectedMs = Calibration::kEnvTimeMinMs
                                    * std::pow (Calibration::kEnvTimeMaxMs / Calibration::kEnvTimeMinMs, param / 127.0);
            const auto frames = (int) ((expectedMs / 1000.0 * 1.15 + 0.3) * kSampleRate);
            segments.push_back (renderNote (tone, frames));
        }

        writeKit ("env_attack.wav", appendSegments (segments, gap),
                  "A-ENV time1 = 32/64/96/127（约 12/147/1789/20000 ms）；攻击手感与段长是否符合预期");
    }

    // ENV release (A-ENV time4 sweep).
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 32, 64, 96, 127 })
        {
            auto tone = baseTone();
            tone.tva.aEnv.time[3] = (float) param;
            const auto expectedMs = Calibration::kEnvTimeMinMs
                                    * std::pow (Calibration::kEnvTimeMaxMs / Calibration::kEnvTimeMinMs, param / 127.0);
            const auto frames = (int) ((1.0 + expectedMs / 1000.0 * 1.15 + 0.3) * kSampleRate);
            segments.push_back (renderNote (tone, frames, false, 60, 100, (int) kSampleRate));
        }

        writeKit ("env_release.wav", appendSegments (segments, gap),
                  "A-ENV time4 = 32/64/96/127；释放尾巴长度与自然度");
    }

    // Envelope shape (current kEnvCurve) with mixed segments.
    {
        auto tone = baseTone();
        tone.tva.level = 100.0f;
        tone.tva.aEnv.time[0] = 96.0f;
        tone.tva.aEnv.time[1] = 96.0f;
        tone.tva.aEnv.time[2] = 64.0f;
        tone.tva.aEnv.time[3] = 96.0f;
        tone.tva.aEnv.level[0] = 127.0f;
        tone.tva.aEnv.level[1] = 60.0f;
        tone.tva.aEnv.level[2] = 80.0f;
        const auto frames = (int) (8.0 * kSampleRate);
        writeKit ("env_curve.wav", renderNote (tone, frames, false, 60, 100, (int) (2.0 * kSampleRate)),
                  "当前 kEnvCurve=" + String (Calibration::kEnvCurve, 1) + " 的段形状（A/D/S/R 手感）");
    }

    // Cutoff sweep (noise through LPF).
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 0, 32, 64, 96, 127 })
        {
            auto tone = baseTone();
            tone.tvf.type = 1;
            tone.tvf.cutoff = (float) param;
            segments.push_back (renderNote (tone, (int) (1.2 * kSampleRate), true));
        }

        writeKit ("cutoff_sweep.wav", appendSegments (segments, gap),
                  "白噪声 LPF cutoff = 0/32/64/96/127（20 Hz–20 kHz）；暗度范围是否合理");
    }

    // Resonance sweep.
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 0, 32, 64, 96, 127 })
        {
            auto tone = baseTone();
            tone.tvf.type = 1;
            tone.tvf.cutoff = 64.0f;
            tone.tvf.resonance = (float) param;
            segments.push_back (renderNote (tone, (int) (1.2 * kSampleRate), true));
        }

        writeKit ("resonance_sweep.wav", appendSegments (segments, gap),
                  "cutoff=64 下 resonance = 0/32/64/96/127；共振上限与自激边缘");
    }

    // PKG gain (scaled by the resonance parameter).
    {
        auto tone = baseTone();
        tone.tvf.type = 4;
        tone.tvf.cutoff = 64.0f;
        tone.tvf.resonance = 127.0f;
        writeKit ("pkg_sweep.wav", renderNote (tone, (int) (2.0 * kSampleRate), true),
                  "PKG（峰值修正）resonance=127 → 最大 +" + String (Calibration::kPkgGainDb, 0) + " dB；增益是否合适");
    }

    // LFO rate.
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 32, 64, 96, 127 })
        {
            auto tone = baseTone();
            tone.tva.lfo1Depth = 60.0f;
            tone.lfo[0].rate = (float) param;
            segments.push_back (renderNote (tone, (int) (6.0 * kSampleRate)));
        }

        writeKit ("lfo_rate.wav", appendSegments (segments, gap),
                  "Amp LFO rate = 32/64/96/127（约 0.23/1.1/5.4/20 Hz）；慢速下限与快速上限");
    }

    // LFO delay + fade.
    {
        auto tone = baseTone();
        tone.tva.lfo1Depth = 60.0f;
        tone.lfo[0].rate = 64.0f;
        tone.lfo[0].delayTime = 48.0f;
        tone.lfo[0].fadeTime = 64.0f;
        writeKit ("lfo_delay_fade.wav", renderNote (tone, (int) (8.0 * kSampleRate)),
                  "delayTime=48 + fadeTime=64（当前 kLfoFadeCurve=" + String (Calibration::kLfoFadeCurve, 1) + "）；淡入手感");
    }

    // CHAOTIC waveform.
    {
        auto tone = baseTone();
        tone.lfo[0].wave = 7;   // CHAOTIC
        tone.lfo[0].rate = 64.0f;
        tone.tvf.lfo1Depth = 48.0f;
        writeKit ("chaos.wav", renderNote (tone, (int) (8.0 * kSampleRate), true),
                  "CHAOTIC 波形（kChaosR=" + String (Calibration::kChaosR, 1) + "）驱动 cutoff 的音色/统计特征");
    }

    // Wave gain steps.
    {
        std::vector<AudioBuffer<float>> segments;

        for (int index : { 0, 1, 2, 3 })
        {
            auto tone = baseTone();
            tone.wg.waveGain = index;
            segments.push_back (renderNote (tone, (int) (1.2 * kSampleRate)));
        }

        writeKit ("wg_gain.wav", appendSegments (segments, gap),
                  "Wave Gain = -6/0/+6/+12 dB 四档；档位间距是否合理");
    }

    // FXM depth.
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 0, 32, 64, 127 })
        {
            auto tone = baseTone();
            tone.wg.fxmOn = true;
            tone.wg.fxmColor = 3;
            tone.wg.fxmDepth = (float) param;
            segments.push_back (renderNote (tone, (int) (1.5 * kSampleRate)));
        }

        writeKit ("fxm_depth.wav", appendSegments (segments, gap),
                  "FXM color=3 depth = 0/32/64/127（kFxmDepthSamples=" + String (Calibration::kFxmDepthSamples, 0) + "）；深度手感");
    }

    // Tone delay times.
    {
        std::vector<AudioBuffer<float>> segments;

        for (int param : { 32, 64, 96, 127 })
        {
            auto tone = baseTone();
            tone.wg.toneDelayMode = 0;
            tone.wg.toneDelayTime = (float) param;
            const auto expectedMs = Calibration::kToneDelayMinMs
                                    * std::pow (Calibration::kToneDelayMaxMs / Calibration::kToneDelayMinMs, param / 127.0);
            const auto frames = (int) ((expectedMs / 1000.0 + 1.0) * kSampleRate);
            segments.push_back (renderNote (tone, frames));   // held note: hear the delayed start
        }

        writeKit ("tone_delay.wav", appendSegments (segments, gap),
                  "Tone Delay NORMAL time = 32/64/96/127：起音被整体延迟（非回声）；间距与最长 2 s 手感");
    }

    // Key interval mode.
    {
        auto tone = baseTone();
        tone.wg.toneDelayMode = 2;   // KEY INTERVAL
        tone.wg.toneDelayTime = 64.0f;
        const auto frames = (int) (4.0 * kSampleRate);
        std::vector<MidiEventSpec> midi { { 0, MidiMessage::noteOn (1, 48, (uint8) 100) },
                                          { (int) (0.2 * kSampleRate), MidiMessage::noteOff (1, 48) },
                                          { (int) (1.0 * kSampleRate), MidiMessage::noteOn (1, 72, (uint8) 100) },
                                          { (int) (1.2 * kSampleRate), MidiMessage::noteOff (1, 72) } };
        const auto rendered = renderAudio (tone, frames, midi, false);
        writeKit ("key_interval.wav", rendered,
                  "Tone Delay KEY INTERVAL：低音/高音两次触发的延迟差异（kKeyIntervalReferenceMs="
                      + String (Calibration::kKeyIntervalReferenceMs, 0) + "）");
    }

    // Pitch depth.
    {
        auto tone = baseTone();
        tone.pEnv.depth = 63;
        tone.pEnv.time[0] = 40.0f;
        tone.pEnv.time[1] = 96.0f;
        tone.pEnv.level[0] = 127.0f;
        tone.pEnv.level[1] = 0.0f;
        tone.pEnv.level[2] = 0.0f;
        tone.pEnv.level[3] = 0.0f;
        writeKit ("depth_pitch.wav", renderNote (tone, (int) (3.0 * kSampleRate)),
                  "P-ENV depth=63（kPEnvDepthSemitones=" + String (Calibration::kPEnvDepthSemitones, 0)
                      + " 半音）；上行后回落（note-off 释放未接线，见 record 缺口 G1）");
    }

    // Filter depth.
    {
        auto tone = baseTone();
        tone.tvf.cutoff = 32.0f;
        tone.tvf.fEnv.depth = 63;
        tone.tvf.fEnv.time[0] = 40.0f;
        tone.tvf.fEnv.time[1] = 64.0f;
        tone.tvf.fEnv.level[0] = 127.0f;
        tone.tvf.fEnv.level[1] = 0.0f;
        tone.tvf.fEnv.level[2] = 0.0f;
        tone.tvf.fEnv.level[3] = 0.0f;
        writeKit ("depth_filter.wav", renderNote (tone, (int) (3.0 * kSampleRate), true),
                  "F-ENV depth=63（kFEnvDepthOctaves=" + String (Calibration::kFEnvDepthOctaves, 0) + " 八度）；扫频幅度");
    }

    // Pan depth.
    {
        auto tone = baseTone();
        tone.pan.lfo1Depth = 127.0f;
        tone.lfo[0].rate = 64.0f;
        writeKit ("depth_pan.wav", renderNote (tone, (int) (4.0 * kSampleRate)),
                  "Pan LFO depth=127（满深度）；声像摆动幅度（kRandomPan=" + String (Calibration::kRandomPan, 1) + "）");
    }

    // Fades.
    {
        std::vector<AudioBuffer<float>> segments;
        segments.push_back (renderNote (baseTone(), (int) (0.5 * kSampleRate)));   // short note (start fade)
        auto releaseTone = baseTone();
        releaseTone.tva.aEnv.time[3] = 64.0f;
        segments.push_back (renderNote (releaseTone, (int) (1.5 * kSampleRate), false, 60, 100, (int) (0.5 * kSampleRate)));
        writeKit ("fades.wav", appendSegments (segments, gap),
                  "极短音（kSampleFadeInMs=" + String (Calibration::kSampleFadeInMs, 1) + "）与自然收尾（kNoteEndFadeMs="
                      + String (Calibration::kNoteEndFadeMs, 1) + "）；有无咔哒");
    }

    // Matrix cutoff.
    {
        auto tone = baseTone();
        tone.tvf.cutoff = 32.0f;
        tone.ctrl[0].dest[0] = 2;
        tone.ctrl[0].depth[0] = 63;
        std::vector<MidiEventSpec> midi { { 0, MidiMessage::noteOn (1, 60, (uint8) 100) } };

        for (int i = 0; i < 64; ++i)
            midi.push_back ({ (int) (i * 0.04 * kSampleRate), MidiMessage::controllerEvent (1, 1, (uint8) (i * 2 + 1)) });

        writeKit ("matrix_cutoff.wav", renderAudio (tone, (int) (4.0 * kSampleRate), midi, true),
                  "CC1 0→127 经 Control 矩阵驱动 cutoff（depth=63，kMatrixCutoffOctaves="
                      + String (Calibration::kMatrixCutoffOctaves, 0) + "）；扫频幅度");
    }

    readme << "\n## 非套件项（按说明现场试听）\n\n"
              "- 平滑（kLevelSmoothingMs=10 / kPanSmoothingMs=10 / kOutputLevelSmoothingMs=10 / kCutoffSmoothingMs=5 / "
              "kResonanceSmoothingMs=5 / kModSmoothingMs=10）：用 Standalone 的 Tone 1 页拖动 level/cutoff/pan 扫动，"
              "听 zipper 与跟随迟滞。\n"
              "- kKillFadeMs / 抢占相关：VoiceManager 未接线（M3-01），本卡仅记录，M6-01 复评。\n"
              "- kFilterControlRate / kFilterStateLimit：只观察（控制率与稳定性另属 M6）。\n";

    writeText (directory.getChildFile ("README.md"), readme);
}

void writeMeasurements (const File& file)
{
    String report;
    double error = 0.0;
    report << measureEnvTime (error);
    report << measureCutoff (error);
    double q = 0.0;
    report << measureResonance (q);
    double pkg = 0.0;
    report << measurePkg (pkg);
    report << measureLfoRate (error);
    report << measureDelay (error);
    report << measureWaveGain (error);
    double minBalance = 0.0, maxBalance = 0.0;
    report << measurePanDepth (minBalance, maxBalance);
    double semitones = 0.0;
    report << measurePitchDepth (semitones);
    double octaves = 0.0;
    report << measureMatrixCutoff (octaves);
    double matrixSemitones = 0.0;
    report << measureMatrixPitch (matrixSemitones);
    double lfoPitchSemitones = 0.0;
    report << measureLfoPitchDepth (lfoPitchSemitones);
    double fEnvOctaves = 0.0;
    report << measureFEnvDepth (fEnvOctaves);
    report << "\nFormula-only checks (documented mapping):\n"
           << "  kLfoSyncBeats[0]=4 beats (1/1) .. [17]=1/12 beat (1/32T)\n"
           << "  kEnvLevelScale/kToneOutputLevelScale/kLfoLevelOffsetScale = 1/127, 1/127, 1/63\n"
           << "  LFO depth scales kLfoPitchSemitones=" << Calibration::kLfoPitchSemitones
           << ", kLfoFilterDepthOctaves=" << Calibration::kLfoFilterDepthOctaves << "\n";

    writeText (file, report);
    std::cout << report.toStdString() << std::endl;
}
}

TEST_CASE ("generate the M2-11 listening kit and measurements", "[.tuning]")
{
    const auto directory = tuningDirectory();
    directory.createDirectory();
    generateKit (directory.getChildFile ("kit"));
    writeMeasurements (directory.getChildFile ("measurements.txt"));
    std::cout << "[tuning] kit and measurements written to " << directory.getFullPathName() << std::endl;
}
