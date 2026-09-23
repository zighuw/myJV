#pragma once

#include <juce_core/juce_core.h>

namespace MyJVCrashHandler
{
struct CrashInfo
{
    juce::String timestamp;
    juce::String version;
    juce::uint32 exceptionCode = 0;
    juce::uint64 exceptionAddress = 0;
};

juce::File writeCrashReport (const CrashInfo& info, const juce::File& directory);

void install (const juce::String& version);
}
