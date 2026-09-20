#include "MyJVProcessor.h"
#include "Params/ParameterIDs.h"

MyJVProcessor::MyJVProcessor()
    : juce::AudioProcessor (createBuses()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void MyJVProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

void MyJVProcessor::releaseResources()
{
    engine.releaseResources();
}

bool MyJVProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

// RT-safe
void MyJVProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    BusBuffers buses;

    for (int bus = 0; bus < juce::jmin (3, getBusCount (false)); ++bus)
    {
        auto* outputBus = getBus (false, bus);

        if (outputBus == nullptr || ! outputBus->isEnabled() || outputBus->getNumberOfChannels() < 2)
            continue;

        auto busBuffer = getBusBuffer (buffer, false, bus);
        buses.l[bus] = busBuffer.getWritePointer (0);
        buses.r[bus] = busBuffer.getWritePointer (1);
    }

    engine.process (buses, buffer.getNumSamples());
}

juce::AudioProcessorEditor* MyJVProcessor::createEditor()
{
    return nullptr;
}

bool MyJVProcessor::hasEditor() const
{
    return false;
}

const juce::String MyJVProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MyJVProcessor::acceptsMidi() const
{
    return JucePlugin_WantsMidiInput;
}

bool MyJVProcessor::producesMidi() const
{
    return JucePlugin_ProducesMidiOutput;
}

bool MyJVProcessor::isMidiEffect() const
{
    return JucePlugin_IsMidiEffect;
}

double MyJVProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MyJVProcessor::getNumPrograms()
{
    return 1;
}

int MyJVProcessor::getCurrentProgram()
{
    return 0;
}

void MyJVProcessor::setCurrentProgram (int) {}

const juce::String MyJVProcessor::getProgramName (int)
{
    return {};
}

void MyJVProcessor::changeProgramName (int, const juce::String&) {}

void MyJVProcessor::getStateInformation (juce::MemoryBlock&) {}

void MyJVProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyJVProcessor();
}
