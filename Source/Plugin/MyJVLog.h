#pragma once

#include <juce_core/juce_core.h>

namespace MyJVLog
{
juce::File defaultLogDirectory();

// Message thread only. Paired initialise/shutdown; reference counted across plugin instances.
void initialise (const juce::File& directory);
void initialise();
void shutdown();

// Non-RT threads only. Never call from audio-thread code (see plan 7.4).
void logInfo (const juce::String& message);
void logWarning (const juce::String& message);
void logError (const juce::String& message);
}
