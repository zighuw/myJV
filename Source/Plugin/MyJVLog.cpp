#include "MyJVLog.h"

#include <atomic>

namespace
{
std::atomic<int> initialisationCount { 0 };
std::unique_ptr<juce::FileLogger> fileLogger;
juce::Logger* previousLogger = nullptr;

constexpr juce::int64 kMaxLogFileSizeBytes = 1024 * 1024;
}

namespace MyJVLog
{
juce::File defaultLogDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("myJV")
        .getChildFile ("logs");
}

void initialise (const juce::File& directory)
{
    if (initialisationCount.fetch_add (1, std::memory_order_acq_rel) != 0)
        return;

    previousLogger = juce::Logger::getCurrentLogger();
    directory.createDirectory();

    fileLogger = std::make_unique<juce::FileLogger> (directory.getChildFile ("myJV.log"),
                                                      "myJV log",
                                                      kMaxLogFileSizeBytes);

    juce::Logger::setCurrentLogger (fileLogger.get());
}

void initialise()
{
    initialise (defaultLogDirectory());
}

void shutdown()
{
    const auto count = initialisationCount.load (std::memory_order_acquire);

    if (count <= 0)
        return;

    if (initialisationCount.fetch_sub (1, std::memory_order_acq_rel) == 1)
    {
        juce::Logger::setCurrentLogger (previousLogger);
        previousLogger = nullptr;
        fileLogger.reset();
    }
}

void logInfo (const juce::String& message)
{
    juce::Logger::writeToLog ("[INFO] " + message);
}

void logWarning (const juce::String& message)
{
    juce::Logger::writeToLog ("[WARN] " + message);
}

void logError (const juce::String& message)
{
    juce::Logger::writeToLog ("[ERROR] " + message);
}
}
