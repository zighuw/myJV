#include "MyJVLog.h"

namespace
{
int initialisationCount = 0;
std::unique_ptr<juce::FileLogger> fileLogger;

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
    jassert (initialisationCount >= 0);

    if (initialisationCount++ == 0)
    {
        directory.createDirectory();

        fileLogger = std::make_unique<juce::FileLogger> (directory.getChildFile ("myJV.log"),
                                                          "myJV log",
                                                          kMaxLogFileSizeBytes);

        juce::Logger::setCurrentLogger (fileLogger.get());
    }
}

void initialise()
{
    initialise (defaultLogDirectory());
}

void shutdown()
{
    jassert (initialisationCount > 0);

    if (--initialisationCount == 0)
    {
        juce::Logger::setCurrentLogger (nullptr);
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
