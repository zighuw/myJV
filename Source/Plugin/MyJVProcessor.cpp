#include "MyJVProcessor.h"

#include "Params/ParameterIDs.h"
#include "Plugin/MyJVCrashHandler.h"
#include "Plugin/MyJVEditor.h"
#include "Plugin/MyJVLog.h"

MyJVProcessor::MyJVProcessor()
    : juce::AudioProcessor (createBuses()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    MyJVLog::initialise (MyJVLog::defaultLogDirectory());
    MyJVCrashHandler::install (JucePlugin_VersionString);
    MyJVLog::logInfo ("processor created: " + juce::String (JucePlugin_VersionString));

    if (juce::MessageManager::existsAndIsCurrentThread())
        startTimerHz (10);
}

MyJVProcessor::~MyJVProcessor()
{
    stopTimer();

    MyJVLog::logInfo ("processor destroyed");
    MyJVLog::shutdown();
}

void MyJVProcessor::timerCallback()
{
    audition.purgeRetired();
}

void MyJVProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    audition.prepare (sampleRate);

    // The timer drives audition retire cleanup on the message thread; start it
    // here too in case the processor was constructed off the message thread.
    if (! isTimerRunning() && juce::MessageManager::existsAndIsCurrentThread())
        startTimerHz (10);
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

    const auto buses = buildBusBuffers (*this, buffer);
    engine.process (buses, midiMessages, buffer.getNumSamples());
    audition.render (buses.l[0], buses.r[0], buffer.getNumSamples());
}

juce::AudioProcessorEditor* MyJVProcessor::createEditor()
{
    return new MyJVEditor (*this);
}

bool MyJVProcessor::hasEditor() const
{
    return true;
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
