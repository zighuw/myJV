#include "MyJVProcessor.h"

#include "Model/ZoneMapping.h"
#include "Params/ParameterIDs.h"
#include "Plugin/MyJVCrashHandler.h"
#include "Plugin/MyJVEditor.h"
#include "Plugin/MyJVLog.h"

MyJVProcessor::MyJVProcessor()
    : juce::AudioProcessor (createBuses()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout()),
      paramSnapshotCache (ParamSnapshotCache::fromApvts (apvts))
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

    // Message thread: reclaim retired runtimes and republish when the library
    // changed (import/scan). The 10 Hz cadence debounces bursts of changes.
    reclaimer.collect();

    if (! juce::MessageManager::existsAndIsCurrentThread() || library.isScanning())
        return;

    const auto fingerprint = libraryFingerprint (library);

    if (! initialRuntimePublished || fingerprint != lastLibraryFingerprint)
        publishRuntime();
}

void MyJVProcessor::publishRuntime()
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());

    if (library.isScanning())
        return;   // index mutation in flight: wait for the scan to settle

    auto runtime = std::make_shared<PatchRuntime>();

    // Bootstrap zone mapping (M3-03 / M5-04a replace this with the patch/edit
    // pipeline). Entries without a decoded sample resolve to nullptr and are
    // skipped by selectZone, which keeps tone 1 silent until a sample exists.
    ZoneMapping::AutoMapOptions options;
    options.resolveSample = [this] (const std::string& fileHash)
    {
        return library.findSample (fileHash);
    };

    auto zoneSet = std::make_shared<ZoneSet> (ZoneMapping::buildAutoMappedZoneSet (library.getEntries(), options));
    runtime->zoneSets[0] = zoneSet;
    runtime->rawZoneSets[0] = zoneSet.get();

    // Fill the snapshot before publication so nothing but reading happens once
    // the runtime is visible to the audio thread.
    paramSnapshotCache.refresh (*runtime);

    reclaimer.publish (std::move (runtime));

    initialRuntimePublished = true;
    lastLibraryFingerprint = libraryFingerprint (library);
}

std::uint64_t MyJVProcessor::libraryFingerprint (const SampleLibrary& library)
{
    // FNV-1a over the runtime-relevant library state (count + path/hash/root/
    // length/flags + decoded-sample presence): cheap enough to run at 10 Hz and
    // exact enough to spot an import, a scan or a lazy decode. The decoded-sample
    // bit is essential: the auto-mapped zones resolve through findSample, so a
    // sample appearing (or disappearing) must rebuild the runtime even when the
    // index entries themselves are unchanged (M3-01 C1).
    const auto& entries = library.getEntries();

    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash] (std::uint64_t value)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    };

    mix ((std::uint64_t) entries.size());

    for (const auto& entry : entries)
    {
        mix (entry.external ? 1u : 0u);
        mix ((std::uint64_t) entry.rootKey);
        mix ((std::uint64_t) entry.lengthSamples);
        mix (library.findSample (entry.fileHash) != nullptr ? 1u : 0u);

        for (const auto character : entry.path)
            mix ((std::uint64_t) (unsigned char) character);

        for (const auto character : entry.fileHash)
            mix ((std::uint64_t) (unsigned char) character);
    }

    return hash;
}

void MyJVProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    audition.prepare (sampleRate);

    // M3-01/C1: the processor now owns the reclaimer, so the audio thread can
    // refresh the active runtime's snapshot and start voices from it.
    engine.setParamSnapshotSource (&paramSnapshotCache, &reclaimer);

    // The timer drives audition retire cleanup, runtime collection and the
    // library-change republish; start it here too in case the processor was
    // constructed off the message thread.
    if (! isTimerRunning() && juce::MessageManager::existsAndIsCurrentThread())
        startTimerHz (10);

    // Publish the first runtime now when possible; otherwise the timer does it.
    if (juce::MessageManager::existsAndIsCurrentThread())
        publishRuntime();
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
