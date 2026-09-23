#include "MyJVProcessor.h"

#include "Params/ParameterIDs.h"
#include "Plugin/MyJVCrashHandler.h"
#include "Plugin/MyJVLog.h"

MyJVProcessor::MyJVProcessor()
    : juce::AudioProcessor (createBuses()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    MyJVLog::initialise (MyJVLog::defaultLogDirectory());
    MyJVCrashHandler::install (JucePlugin_VersionString);
    MyJVLog::logInfo ("processor created: " + juce::String (JucePlugin_VersionString));
}

MyJVProcessor::~MyJVProcessor()
{
    MyJVLog::logInfo ("processor destroyed");
    MyJVLog::shutdown();
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
    return isLayoutSupported (layouts);
}

// RT-safe
void MyJVProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    engine.process (buildBusBuffers (*this, buffer), midiMessages, buffer.getNumSamples());
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
