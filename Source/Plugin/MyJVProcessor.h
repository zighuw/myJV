#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/SynthEngine.h"

class MyJVProcessor final : public juce::AudioProcessor
{
public:
    MyJVProcessor();
    ~MyJVProcessor() override = default;

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

private:
    juce::AudioProcessorValueTreeState apvts;
    SynthEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyJVProcessor)
};
