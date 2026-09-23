#pragma once

#include <juce_core/juce_core.h>

namespace MyJVLog
{
juce::File defaultLogDirectory();

void initialise (const juce::File& directory);
void initialise();
void shutdown();

void logInfo (const juce::String& message);
void logWarning (const juce::String& message);
void logError (const juce::String& message);
}
