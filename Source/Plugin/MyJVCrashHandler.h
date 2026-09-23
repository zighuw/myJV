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

// Idempotent and safe to call from any thread. Windows: installs a process-wide
// unhandled-exception filter that chains to the previously installed filter and is
// best-effort restored when the module unloads. Other platforms: no-op.
void install (const juce::String& version);
}
