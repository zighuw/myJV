#include "MyJVProcessor.h"

MyJVProcessor::MyJVProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true))
{
}

void MyJVProcessor::prepareToPlay (double, int) {}

void MyJVProcessor::releaseResources() {}

bool MyJVProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MyJVProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
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
