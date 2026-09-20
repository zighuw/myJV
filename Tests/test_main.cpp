#include <catch2/catch_session.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    return Catch::Session().run (argc, argv);
}
