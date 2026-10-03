#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Params/ParameterIDs.h"
#include "Plugin/TonePage.h"
#include "Plugin/TonePageLayout.h"

#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace juce;

constexpr int kExpectedTone1Parameters = 123;

class TestProcessor final : public AudioProcessor
{
public:
    TestProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Main", AudioChannelSet::stereo(), true))
    {
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override {}
    AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const String getName() const override { return "TestProcessor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

struct Fixture
{
    TestProcessor processor;
    AudioProcessorValueTreeState apvts { processor, nullptr, "PARAMETERS", createParameterLayout() };
};

const TonePage::Entry* findEntry (const std::string& suffix)
{
    for (const auto& entry : TonePage::kTone1Layout)
        if (suffix == entry.suffix)
            return &entry;

    return nullptr;
}

int countSubPage (TonePage::SubPage page)
{
    int count = 0;

    for (const auto& entry : TonePage::kTone1Layout)
        count += entry.subPage == page ? 1 : 0;

    return count;
}
}

TEST_CASE ("tone page layout covers every tone1 parameter exactly once")
{
    Fixture fixture;
    std::vector<RangedAudioParameter*> tone1;

    for (auto* parameter : fixture.processor.getParameters())
        if (auto* ranged = dynamic_cast<RangedAudioParameter*> (parameter);
            ranged != nullptr && ranged->getParameterID().startsWith ("tone1."))
            tone1.push_back (ranged);

    REQUIRE ((int) tone1.size() == kExpectedTone1Parameters);

    std::set<std::string> suffixes;

    for (auto* parameter : tone1)
    {
        const auto suffix = parameter->getParameterID().substring (6).toStdString();
        INFO (parameter->getParameterID().toStdString());
        REQUIRE (suffixes.insert (suffix).second);
        REQUIRE (findEntry (suffix) != nullptr);
    }

    for (const auto& entry : TonePage::kTone1Layout)
    {
        INFO (entry.suffix);
        REQUIRE (fixture.apvts.getParameter ("tone1." + String (entry.suffix)) != nullptr);
    }
}

TEST_CASE ("tone page layout suffixes are unique and well formed")
{
    std::set<std::string> seen;

    for (const auto& entry : TonePage::kTone1Layout)
    {
        REQUIRE (entry.suffix != nullptr);
        REQUIRE (String (entry.suffix).isNotEmpty());
        CHECK (seen.insert (entry.suffix).second);
    }

    CHECK (seen.size() == std::size (TonePage::kTone1Layout));
}

TEST_CASE ("tone page control kinds match the registered parameter types")
{
    Fixture fixture;

    for (const auto& entry : TonePage::kTone1Layout)
    {
        auto* parameter = fixture.apvts.getParameter ("tone1." + String (entry.suffix));
        INFO (entry.suffix);
        REQUIRE (parameter != nullptr);

        switch (entry.kind)
        {
            case TonePage::ControlKind::Slider:
            {
                const auto isFloatOrInt = dynamic_cast<AudioParameterFloat*> (parameter) != nullptr
                                          || dynamic_cast<AudioParameterInt*> (parameter) != nullptr;
                CHECK (isFloatOrInt);
                CHECK (dynamic_cast<AudioParameterChoice*> (parameter) == nullptr);
                CHECK (dynamic_cast<AudioParameterBool*> (parameter) == nullptr);
                break;
            }

            case TonePage::ControlKind::Combo:
                CHECK (dynamic_cast<AudioParameterChoice*> (parameter) != nullptr);
                break;

            case TonePage::ControlKind::Toggle:
                CHECK (dynamic_cast<AudioParameterBool*> (parameter) != nullptr);
                break;
        }
    }
}

TEST_CASE ("tone page combo controls expose the registered choices")
{
    Fixture fixture;

    for (const auto& entry : TonePage::kTone1Layout)
    {
        if (entry.kind != TonePage::ControlKind::Combo)
            continue;

        auto* parameter = fixture.apvts.getParameter ("tone1." + String (entry.suffix));
        INFO (entry.suffix);
        REQUIRE (parameter != nullptr);

        const auto items = TonePageUi::comboChoices (*parameter);
        CHECK (items.size() >= 2);

        if (auto* choice = dynamic_cast<AudioParameterChoice*> (parameter))
            CHECK (items == choice->choices);
    }
}

TEST_CASE ("tone page subpages partition the layout with the planned sizes")
{
    CHECK (countSubPage (TonePage::SubPage::Wg) == 23);
    CHECK (countSubPage (TonePage::SubPage::Tvf) == 7);
    CHECK (countSubPage (TonePage::SubPage::Tva) == 14);
    CHECK (countSubPage (TonePage::SubPage::Lfo) == 16);
    CHECK (countSubPage (TonePage::SubPage::Env) == 39);
    CHECK (countSubPage (TonePage::SubPage::Ctrl) == 24);
    CHECK (std::size (TonePage::kTone1Layout) == (std::size_t) kExpectedTone1Parameters);
}
