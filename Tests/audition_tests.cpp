#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Engine/AuditionVoice.h"
#include "Model/ZoneSet.h"
#include "Plugin/MyJVProcessor.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace
{
using namespace juce;

constexpr double kAuditionSampleRate = 48000.0;

Sample makeSample (int frames, bool withLoop = false, float value = 0.25f)
{
    Sample sample;
    sample.data.setSize (1, frames);

    for (int i = 0; i < frames; ++i)
        sample.data.setSample (0, i, value);

    sample.sourceSampleRate = kAuditionSampleRate;
    sample.rootKey = 60;

    if (withLoop)
    {
        sample.embeddedLoop.start = 0;
        sample.embeddedLoop.end = frames;
    }

    return sample;
}

// Keeps a background scan in flight for the duration of a render loop.
void writeBigFile (const File& file, int sizeBytes)
{
    file.deleteFile();

    std::unique_ptr<FileOutputStream> stream (file.createOutputStream());
    REQUIRE (stream != nullptr);

    const std::vector<char> chunk (64 * 1024, 0);
    int written = 0;

    while (written < sizeBytes)
    {
        const auto numBytes = jmin ((int) chunk.size(), sizeBytes - written);
        REQUIRE (stream->write (chunk.data(), (std::size_t) numBytes));
        written += numBytes;
    }

    stream->flush();
}

void writeWavFile (const File& file, int frames = 64)
{
    const auto signal = makeSample (frames);

    WavAudioFormat format;
    std::unique_ptr<OutputStream> stream = file.createOutputStream();
    REQUIRE (stream != nullptr);

    auto writer = format.createWriterFor (stream, AudioFormatWriterOptions()
                                                      .withSampleRate (kAuditionSampleRate)
                                                      .withNumChannels (1)
                                                      .withBitsPerSample (16));
    REQUIRE (writer != nullptr);
    REQUIRE (writer->writeFromAudioSampleBuffer (signal.data, 0, frames));
}

float renderMaxMagnitude (AuditionVoice& audition, int numSamples = 256)
{
    AudioBuffer<float> left (1, numSamples);
    AudioBuffer<float> right (1, numSamples);
    left.clear();
    right.clear();

    audition.render (left.getWritePointer (0), right.getWritePointer (0), numSamples);

    return jmax (left.getMagnitude (0, 0, numSamples), right.getMagnitude (0, 0, numSamples));
}
}

TEST_CASE ("audition renders the sample on request")
{
    auto sample = std::make_shared<Sample> (makeSample (512));

    AuditionVoice audition;
    audition.prepare (kAuditionSampleRate);
    audition.play (sample, 60);

    CHECK (renderMaxMagnitude (audition) > 0.0f);
    CHECK (audition.isPlaying());
}

TEST_CASE ("audition is silent when idle and after stop")
{
    AuditionVoice idle;
    idle.prepare (kAuditionSampleRate);
    CHECK (renderMaxMagnitude (idle) == 0.0f);
    CHECK_FALSE (idle.isPlaying());

    auto sample = std::make_shared<Sample> (makeSample (512));
    idle.play (sample, 60);
    CHECK (renderMaxMagnitude (idle) > 0.0f);

    idle.stop();
    CHECK (renderMaxMagnitude (idle) == 0.0f);
    CHECK_FALSE (idle.isPlaying());
}

TEST_CASE ("audition mixes into both channels")
{
    auto sample = std::make_shared<Sample> (makeSample (512));

    AuditionVoice audition;
    audition.prepare (kAuditionSampleRate);
    audition.play (sample, 60);

    AudioBuffer<float> left (1, 256);
    AudioBuffer<float> right (1, 256);
    left.clear();
    right.clear();

    audition.render (left.getWritePointer (0), right.getWritePointer (0), 256);

    CHECK (left.getMagnitude (0, 0, 256) > 0.0f);
    CHECK (right.getMagnitude (0, 0, 256) == Catch::Approx (left.getMagnitude (0, 0, 256)).margin (1.0e-6f));
}

TEST_CASE ("audition follows the sample's embedded loop")
{
    auto sample = std::make_shared<Sample> (makeSample (64, true));

    AuditionVoice audition;
    audition.prepare (kAuditionSampleRate);
    audition.play (sample, 60);

    CHECK (renderMaxMagnitude (audition, 512) > 0.0f);
    CHECK (audition.isPlaying());   // still looping after the sample end

    AuditionVoice oneShot;
    oneShot.prepare (kAuditionSampleRate);
    oneShot.play (std::make_shared<Sample> (makeSample (64, false)), 60);

    renderMaxMagnitude (oneShot, 512);
    CHECK_FALSE (oneShot.isPlaying());
}

TEST_CASE ("replaced samples stay alive until the retire grace elapses")
{
    AuditionVoice audition;
    audition.prepare (kAuditionSampleRate);

    auto first = std::make_shared<Sample> (makeSample (256));
    std::weak_ptr<const Sample> weakFirst = first;

    audition.play (first, 60);
    renderMaxMagnitude (audition);
    first.reset();

    auto second = std::make_shared<Sample> (makeSample (256));
    std::weak_ptr<const Sample> weakSecond = second;
    audition.play (second, 60);
    second.reset();

    CHECK_FALSE (weakFirst.expired());
    CHECK_FALSE (weakSecond.expired());

    audition.purgeRetired();          // default grace: nothing is old enough yet
    CHECK_FALSE (weakFirst.expired());

    audition.purgeRetired (0.0);      // force purge
    CHECK (weakFirst.expired());
    CHECK_FALSE (weakSecond.expired());   // current owner stays alive

    audition.stop();
    CHECK_FALSE (weakSecond.expired());
}

TEST_CASE ("audition survives concurrent rendering and sample swaps")
{
    AuditionVoice audition;
    audition.prepare (kAuditionSampleRate);

    // Only weak references are held here: a premature release would free the
    // memory for ASan (CI) and make the lock() below fail (local).
    std::weak_ptr<const Sample> weakFirst;
    std::weak_ptr<const Sample> weakSecond;

    {
        auto first = std::make_shared<Sample> (makeSample (2048));
        weakFirst = first;
        audition.play (first, 60);
    }

    {
        auto second = std::make_shared<Sample> (makeSample (2048));
        weakSecond = second;
        audition.play (second, 60);
    }

    std::atomic<bool> stop { false };

    std::thread renderer ([&]
    {
        AudioBuffer<float> left (1, 256);
        AudioBuffer<float> right (1, 256);

        while (! stop.load (std::memory_order_relaxed))
        {
            left.clear();
            right.clear();
            audition.render (left.getWritePointer (0), right.getWritePointer (0), 256);
        }
    });

    bool allSwapsResolved = true;

    for (int i = 0; i < 2000; ++i)
    {
        auto next = (i % 2 == 0) ? weakFirst.lock() : weakSecond.lock();

        if (next == nullptr)
        {
            allSwapsResolved = false;
            break;
        }

        audition.play (std::move (next), 60);
        audition.purgeRetired();   // default grace keeps recent swaps alive
    }

    stop.store (true);
    renderer.join();

    CHECK (allSwapsResolved);
    CHECK_FALSE (weakFirst.expired());
    CHECK_FALSE (weakSecond.expired());

    // The renderer may have exited before it observed the last play(), so
    // isPlaying() is only meaningful after this thread deterministically
    // consumes the pending start request (code-review N-12).
    {
        AudioBuffer<float> left (1, 256);
        AudioBuffer<float> right (1, 256);
        left.clear();
        right.clear();
        audition.render (left.getWritePointer (0), right.getWritePointer (0), 256);
    }

    CHECK (audition.isPlaying());

    audition.stop();
    audition.purgeRetired (0.0);
}

TEST_CASE ("processor mixes the audition into the main bus only")
{
    auto sample = std::make_shared<Sample> (makeSample (2048));

    MyJVProcessor processor;
    processor.prepareToPlay (kAuditionSampleRate, 512);

    processor.getAuditionVoice().prepare (kAuditionSampleRate);
    processor.getAuditionVoice().play (sample, 60);

    AudioBuffer<float> buffer (6, 512);
    buffer.clear();

    MidiBuffer midi;
    processor.processBlock (buffer, midi);

    CHECK (buffer.getMagnitude (0, 0, 512) > 0.0f);
    CHECK (buffer.getMagnitude (1, 0, 512) > 0.0f);
    CHECK (buffer.getMagnitude (2, 0, 512) == 0.0f);
    CHECK (buffer.getMagnitude (5, 0, 512) == 0.0f);
}

TEST_CASE ("processor stays silent while a library scan runs")
{
    const auto directory = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVUiScanTests");
    directory.deleteRecursively (false);
    REQUIRE (directory.createDirectory().wasOk());
    writeWavFile (directory.getChildFile ("Tone.wav"));

    MyJVProcessor processor;
    processor.prepareToPlay (kAuditionSampleRate, 512);

    auto& library = processor.getSampleLibrary();
    library.setRootDirectory (directory);
    library.startScan();

    AudioBuffer<float> buffer (6, 512);
    MidiBuffer midi;

    for (int block = 0; block < 20; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            CHECK (buffer.getMagnitude (channel, 0, 512) == 0.0f);
    }

    library.waitForScanToFinish();
    CHECK (library.getEntries().size() == 1);

    directory.deleteRecursively (false);
}

TEST_CASE ("audio keeps its contract while a scan is hashing")
{
    // P-2: the audio path shares no state with the library, so rendering must
    // keep working - and keep its established behaviour - while a background
    // scan is busy. Contract assertions only, no wall-clock thresholds.
    const auto directory = File::getSpecialLocation (File::tempDirectory).getChildFile ("myJVUiScanContractTests");
    directory.deleteRecursively (false);
    REQUIRE (directory.createDirectory().wasOk());
    writeWavFile (directory.getChildFile ("Tone.wav"));

    // 16 MiB keeps the scan in flight for the whole render loop below.
    writeBigFile (directory.getChildFile ("Big.wav"), 16 * 1024 * 1024);

    MyJVProcessor processor;
    processor.prepareToPlay (kAuditionSampleRate, 512);

    auto& library = processor.getSampleLibrary();
    library.setRootDirectory (directory);
    REQUIRE (library.scanNow().succeeded);

    const auto entriesBefore = library.getEntries();
    REQUIRE (entriesBefore.size() == 2);

    auto& audition = processor.getAuditionVoice();
    audition.prepare (kAuditionSampleRate);
    audition.play (std::make_shared<Sample> (makeSample ((int) kAuditionSampleRate)), 60);

    library.startScan();
    REQUIRE (library.isScanning());

    AudioBuffer<float> buffer (6, 512);
    MidiBuffer midi;

    for (int block = 0; block < 20; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);   // must return on every block

        // The audition is mixed into Main only and is not silenced by the scan.
        CHECK (buffer.getMagnitude (0, 0, 512) > 0.0f);
        CHECK (buffer.getMagnitude (1, 0, 512) > 0.0f);
        CHECK (buffer.getMagnitude (2, 0, 512) == 0.0f);
        CHECK (buffer.getMagnitude (5, 0, 512) == 0.0f);
    }

    // The scan was still running throughout, and rendering touched no library state.
    CHECK (library.isScanning());

    const auto entriesAfter = library.getEntries();
    REQUIRE (entriesAfter.size() == entriesBefore.size());

    for (std::size_t i = 0; i < entriesBefore.size(); ++i)
    {
        CHECK (entriesAfter[i].path == entriesBefore[i].path);
        CHECK (entriesAfter[i].fileHash == entriesBefore[i].fileHash);
    }

    library.cancelScan();
    library.waitForScanToFinish();
    audition.stop();

    directory.deleteRecursively (false);
}
