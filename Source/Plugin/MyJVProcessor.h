#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/AssetReclaimer.h"
#include "Engine/AuditionVoice.h"
#include "Engine/SynthEngine.h"
#include "Model/SampleLibrary.h"
#include "Params/ParamSnapshotCache.h"

#include <cstdint>
#include <vector>

class MyJVProcessor final : public juce::AudioProcessor,
                            private juce::Timer
{
public:
    MyJVProcessor();
    ~MyJVProcessor() override;

    static BusesProperties createBuses()
    {
        return BusesProperties()
                   .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                   .withOutput ("Out1", juce::AudioChannelSet::stereo(), true)
                   .withOutput ("Out2", juce::AudioChannelSet::stereo(), true);
    }

    static bool isLayoutSupported (const BusesLayout& layouts)
    {
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;

        for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
        {
            const auto& channelSet = layouts.outputBuses.getReference (bus);

            if (! channelSet.isDisabled() && channelSet != juce::AudioChannelSet::stereo())
                return false;
        }

        return true;
    }

    static BusBuffers buildBusBuffers (const juce::AudioProcessor& processor, juce::AudioBuffer<float>& buffer)
    {
        BusBuffers buses;

        for (int bus = 0; bus < juce::jmin (kNumOutputBuses, processor.getBusCount (false)); ++bus)
        {
            auto* outputBus = processor.getBus (false, bus);

            if (outputBus == nullptr || ! outputBus->isEnabled() || outputBus->getNumberOfChannels() < 2)
                continue;

            auto busBuffer = processor.getBusBuffer (buffer, false, bus);

            if (busBuffer.getNumChannels() < 2)
                continue;

            buses.l[bus] = busBuffer.getWritePointer (0);
            buses.r[bus] = busBuffer.getWritePointer (1);
        }

        return buses;
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) noexcept override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Message thread. The library and audition voice are owned here so the
    // editor and tests share one instance (M1-07b).
    SampleLibrary& getSampleLibrary() noexcept
    {
        jassert (juce::MessageManager::existsAndIsCurrentThread());
        return library;
    }

    // Message thread. Exposed so tests (and later patch tooling) can verify the
    // snapshot cache against the live parameter set (M2-01).
    juce::AudioProcessorValueTreeState& getApvts() noexcept
    {
        jassert (juce::MessageManager::existsAndIsCurrentThread());
        return apvts;
    }

    AuditionVoice& getAuditionVoice() noexcept
    {
        jassert (juce::MessageManager::existsAndIsCurrentThread());
        return audition;
    }

private:
    void timerCallback() override;

    // Message thread. Rebuilds a PatchRuntime from the current library (auto
    // mapped zones + refreshed snapshot) and publishes it exactly once. Until
    // the patch pipeline lands (M3-03 / M5-04a) this bootstrap is what makes an
    // imported sample audible from host MIDI (closes M2-R4 C1).
    void publishRuntime();

    static std::uint64_t libraryFingerprint (const std::vector<LibraryEntry>& entries);

    juce::AudioProcessorValueTreeState apvts;
    ParamSnapshotCache paramSnapshotCache;
    AssetReclaimer reclaimer;
    SynthEngine engine;
    SampleLibrary library;
    AuditionVoice audition;

    bool initialRuntimePublished = false;
    std::uint64_t lastLibraryFingerprint = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyJVProcessor)
};
