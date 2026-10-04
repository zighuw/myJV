#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Model/Sample.h"
#include "Model/ZoneSet.h"
#include "Params/ParamSnapshot.h"
#include "RenderRegression.h"

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

struct RenderCase
{
    String name;
    int frames = 0;
    std::function<void (ToneSnapshot&)> configure;
    std::vector<MidiEventSpec> midi;
    RenderRegression::Tolerance tolerance;
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
    tone.tvf.type = 1;   // LPF
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

std::shared_ptr<Sample> makeFixtureSample()
{
    auto sample = std::make_shared<Sample>();
    sample->sourceSampleRate = kSampleRate;
    sample->rootKey = 60;
    sample->data.setSize (1, (int) kSampleRate);
    auto* data = sample->data.getWritePointer (0);

    for (int i = 0; i < sample->data.getNumSamples(); ++i)
        data[i] = (float) (0.5 * std::sin (MathConstants<double>::twoPi * 100.0 * (double) i / kSampleRate));

    return sample;
}

const std::vector<RenderCase>& renderCases()
{
    static const std::vector<RenderCase> cases = []
    {
        std::vector<RenderCase> list;

        {
            RenderCase testCase;
            testCase.name = "sustained";
            testCase.frames = 96000;   // 2.0 s
            testCase.midi = { { 0, MidiMessage::noteOn (1, 60, (uint8) 100) },
                              { 72000, MidiMessage::noteOff (1, 60) } };
            list.push_back (std::move (testCase));
        }

        {
            RenderCase testCase;
            testCase.name = "modulated";
            testCase.frames = 120000;   // 2.5 s
            testCase.configure = [] (ToneSnapshot& tone)
            {
                tone.wg.pitchLfo1Depth = 40.0f;
                tone.pan.lfo2Depth = 30.0f;
                tone.lfo[1].wave = 3;   // TRIANGLE
                tone.lfo[1].rate = 55.0f;
                tone.tvf.cutoff = 80.0f;
                tone.tvf.cutoffKeyfollow = 50.0f;
                tone.tvf.fEnv.depth = 40;
                tone.tvf.fEnv.time[0] = 20.0f;
                tone.tvf.fEnv.time[1] = 40.0f;
                tone.tvf.fEnv.time[2] = 40.0f;
                tone.tvf.fEnv.time[3] = 60.0f;
                tone.tvf.fEnv.level[0] = 127.0f;
                tone.tvf.fEnv.level[1] = 100.0f;
                tone.tvf.fEnv.level[2] = 80.0f;
                tone.tvf.fEnv.level[3] = 0.0f;
                tone.ctrl[0].dest[0] = 2;   // CUTOFF
                tone.ctrl[0].depth[0] = 40;
            };
            testCase.midi = { { 0, MidiMessage::noteOn (1, 64, (uint8) 100) },
                              { 4800, MidiMessage::controllerEvent (1, 1, 96) },
                              { 96000, MidiMessage::noteOff (1, 64) } };
            list.push_back (std::move (testCase));
        }

        {
            RenderCase testCase;
            testCase.name = "wg-fxm-delay";
            testCase.frames = 72000;   // 1.5 s
            testCase.configure = [] (ToneSnapshot& tone)
            {
                tone.wg.waveGain = 2;   // +6 dB
                tone.wg.fxmOn = true;
                tone.wg.fxmColor = 3;
                tone.wg.fxmDepth = 60.0f;
                tone.wg.toneDelayMode = 0;   // NORMAL
                tone.wg.toneDelayTime = 30.0f;
            };
            testCase.midi = { { 0, MidiMessage::noteOn (1, 60, (uint8) 110) },
                              { 24000, MidiMessage::noteOff (1, 60) } };
            list.push_back (std::move (testCase));
        }

        return list;
    }();

    return cases;
}

// Renders the Main bus exactly like the engine tests' fixture (M2-05), with a
// fresh engine and reclaimer per call so repeated runs are deterministic.
AudioBuffer<float> renderCase (const RenderCase& testCase)
{
    auto zoneSet = std::make_shared<ZoneSet>();
    Zone zone;
    zone.sample = makeFixtureSample();
    zone.loopMode = LoopMode::Sustain;
    zone.loop.start = 0;
    zone.loop.end = zone.sample->data.getNumSamples();
    zoneSet->zones.push_back (zone);

    auto runtime = std::make_shared<PatchRuntime>();
    runtime->snapshot.tones[0] = baseTone();

    if (testCase.configure)
        testCase.configure (runtime->snapshot.tones[0]);

    runtime->zoneSets[0] = zoneSet;
    runtime->rawZoneSets[0] = zoneSet.get();

    AssetReclaimer reclaimer;
    reclaimer.publish (runtime);

    SynthEngine engine;
    engine.prepare (kSampleRate, kBlockSize);
    engine.setParamSnapshotSource (nullptr, &reclaimer);

    AudioBuffer<float> output (2, testCase.frames);
    output.clear();

    for (int start = 0; start < testCase.frames; start += kBlockSize)
    {
        const auto numSamples = jmin (kBlockSize, testCase.frames - start);

        MidiBuffer midi;

        for (const auto& event : testCase.midi)
            if (event.samplePosition >= start && event.samplePosition < start + numSamples)
                midi.addEvent (event.message, event.samplePosition - start);

        BusBuffers buses;
        buses.l[0] = output.getWritePointer (0) + start;
        buses.r[0] = output.getWritePointer (1) + start;

        engine.process (buses, midi, numSamples);
    }

    return output;
}

File baselineDirectory()
{
    return File (String (MYJV_REPO_DIR)).getChildFile ("Tests").getChildFile ("Baselines");
}

int updateBaselines()
{
    const auto directory = baselineDirectory();

    if (! directory.createDirectory())
    {
        std::cout << "[update] cannot create " << directory.getFullPathName() << std::endl;
        return 1;
    }

    RenderRegression::Manifest previous;
    String error;
    RenderRegression::readManifest (directory.getChildFile ("manifest.json"), previous, error);

    RenderRegression::Manifest manifest;
    manifest.generator = "myJV " + String (JucePlugin_VersionString);
    manifest.sampleRate = kSampleRate;
    manifest.blockSize = kBlockSize;

    for (const auto& testCase : renderCases())
    {
        const auto rendered = renderCase (testCase);
        const auto metrics = RenderRegression::measure (rendered, kSampleRate);
        const auto file = directory.getChildFile (testCase.name + ".wav");

        if (! RenderRegression::writeWav (file, rendered, kSampleRate, error))
        {
            std::cout << "[update] " << error << std::endl;
            return 1;
        }

        RenderRegression::CaseMeta meta;
        meta.name = testCase.name;
        meta.file = testCase.name + ".wav";
        meta.frames = rendered.getNumSamples();
        meta.channels = rendered.getNumChannels();
        meta.renderSha256 = metrics.sha256;
        meta.fileSha256 = RenderRegression::sha256OfFile (file);
        meta.rmsDb = metrics.rmsDb;
        meta.peakDb = metrics.peakDb;
        meta.bandsDb = metrics.bandsDb;
        meta.tolerance = testCase.tolerance;

        if (const auto* old = previous.find (testCase.name))
        {
            double maxBandDelta = 0.0;

            for (size_t band = 0; band < jmin (old->bandsDb.size(), meta.bandsDb.size()); ++band)
                maxBandDelta = jmax (maxBandDelta, std::abs (meta.bandsDb[band] - old->bandsDb[band]));

            std::cout << "[update] " << testCase.name
                      << ": rms " << String (old->rmsDb, 3) << " -> " << String (meta.rmsDb, 3) << " dB"
                      << ", peak " << String (old->peakDb, 3) << " -> " << String (meta.peakDb, 3) << " dB"
                      << ", max band delta " << String (maxBandDelta, 3) << " dB"
                      << std::endl;
        }
        else
        {
            std::cout << "[update] " << testCase.name
                      << ": new baseline, rms " << String (meta.rmsDb, 3)
                      << " dB, peak " << String (meta.peakDb, 3) << " dB" << std::endl;
        }

        manifest.cases.push_back (std::move (meta));
    }

    if (! RenderRegression::writeManifest (directory.getChildFile ("manifest.json"), manifest, error))
    {
        std::cout << "[update] " << error << std::endl;
        return 1;
    }

    std::cout << "[update] wrote " << manifest.cases.size() << " cases to " << directory.getFullPathName() << std::endl;
    return 0;
}
}

TEST_CASE ("render regression matches the committed baselines")
{
    RenderRegression::Manifest manifest;
    String error;
    const auto manifestFile = baselineDirectory().getChildFile ("manifest.json");

    REQUIRE (RenderRegression::readManifest (manifestFile, manifest, error));
    CHECK (manifest.schemaVersion == 1);
    REQUIRE (manifest.cases.size() == renderCases().size());

    for (const auto& testCase : renderCases())
    {
        INFO (testCase.name.toStdString());

        const auto* meta = manifest.find (testCase.name);
        REQUIRE (meta != nullptr);

        const auto firstRender = renderCase (testCase);
        const auto secondRender = renderCase (testCase);

        // Determinism: identical inputs must produce bit-identical output.
        CHECK (RenderRegression::sha256OfBuffer (firstRender)
               == RenderRegression::sha256OfBuffer (secondRender));

        // Baseline-file integrity (strict).
        const auto baselineFile = baselineDirectory().getChildFile (meta->file);
        CHECK (RenderRegression::sha256OfFile (baselineFile) == meta->fileSha256);

        AudioBuffer<float> baseline;
        double baselineSampleRate = 0.0;
        REQUIRE (RenderRegression::readWav (baselineFile, baseline, baselineSampleRate, error));
        CHECK (baselineSampleRate == kSampleRate);
        REQUIRE (baseline.getNumSamples() == testCase.frames);
        REQUIRE (baseline.getNumChannels() == 2);

        const auto baselineMetrics = RenderRegression::measure (baseline, baselineSampleRate);
        const auto renderedMetrics = RenderRegression::measure (firstRender, kSampleRate);
        const auto comparison = RenderRegression::compare (baselineMetrics, renderedMetrics, meta->tolerance);

        INFO (RenderRegression::formatComparison (comparison, testCase.name.toRawUTF8()));

        if (renderedMetrics.sha256 != meta->renderSha256)
            WARN ("float render hash differs from the generation platform (cross-platform drift)");

        CHECK (comparison.passed());

        if (! comparison.passed())
        {
            const auto renderedFile = File::getCurrentWorkingDirectory().getChildFile ("rendered-" + testCase.name + ".wav");
            RenderRegression::writeWav (renderedFile, firstRender, kSampleRate, error);
        }
    }
}

int main (int argc, char* argv[])
{
    const ScopedJuceInitialiser_GUI juceInitialiser;

    for (int i = 1; i < argc; ++i)
        if (String (argv[i]) == "--update-baselines")
            return updateBaselines();

    return Catch::Session().run (argc, argv);
}
