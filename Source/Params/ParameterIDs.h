#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParameterIDs
{
inline juce::String patch (const char* group, const char* name)
{
    return juce::String ("patch.") + group + "." + name;
}

inline juce::String tone (int toneNumber, const char* group, const char* name)
{
    return "tone" + juce::String (toneNumber) + "." + group + "." + name;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
