#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Params/ParameterIDs.h"

#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace
{
using namespace juce;

constexpr int kExpectedTotalParameters = 510;
constexpr int kExpectedPatchCommonParameters = 26;
constexpr int kExpectedToneParameters = 121;

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

std::vector<RangedAudioParameter*> collectParameters (Fixture& fixture)
{
    std::vector<RangedAudioParameter*> parameters;

    for (auto* parameter : fixture.processor.getParameters())
        parameters.push_back (dynamic_cast<RangedAudioParameter*> (parameter));

    return parameters;
}

std::map<String, RangedAudioParameter*> indexById (Fixture& fixture)
{
    std::map<String, RangedAudioParameter*> byId;

    for (auto* parameter : collectParameters (fixture))
        byId.emplace (parameter->getParameterID(), parameter);

    return byId;
}
}

TEST_CASE ("parameter count matches the registered specification")
{
    Fixture fixture;
    const auto parameters = collectParameters (fixture);

    REQUIRE (parameters.size() == kExpectedTotalParameters);
}

TEST_CASE ("parameter IDs are unique")
{
    Fixture fixture;
    const auto parameters = collectParameters (fixture);
    std::set<String> ids;

    for (auto* parameter : parameters)
        ids.insert (parameter->getParameterID());

    REQUIRE (ids.size() == parameters.size());
}

TEST_CASE ("parameter IDs follow the dot-path scheme")
{
    Fixture fixture;

    for (auto* parameter : collectParameters (fixture))
    {
        const auto id = parameter->getParameterID();
        INFO (id.toStdString());

        REQUIRE ((id.startsWith ("patch.common.") || id.startsWith ("tone1.")
                  || id.startsWith ("tone2.") || id.startsWith ("tone3.") || id.startsWith ("tone4.")));
        REQUIRE (id.containsChar ('.'));
        REQUIRE_FALSE (id.containsChar (' '));
    }
}

TEST_CASE ("parameter group sizes match the architecture appendix")
{
    Fixture fixture;

    int patchCommonCount = 0;
    std::map<String, int> toneCounts;

    for (auto* parameter : collectParameters (fixture))
    {
        const auto id = parameter->getParameterID();

        if (id.startsWith ("patch.common."))
        {
            ++patchCommonCount;
        }
        else
        {
            for (int tone = 1; tone <= 4; ++tone)
                if (id.startsWith ("tone" + String (tone) + "."))
                    ++toneCounts["tone" + String (tone)];
        }
    }

    REQUIRE (patchCommonCount == kExpectedPatchCommonParameters);

    for (int tone = 1; tone <= 4; ++tone)
        REQUIRE (toneCounts["tone" + String (tone)] == kExpectedToneParameters);
}

TEST_CASE ("all four tones expose the same parameter set")
{
    Fixture fixture;

    std::vector<String> suffixesByTone[4];

    for (auto* parameter : collectParameters (fixture))
    {
        const auto id = parameter->getParameterID();

        for (int tone = 0; tone < 4; ++tone)
        {
            const auto prefix = "tone" + String (tone + 1) + ".";
            if (id.startsWith (prefix))
                suffixesByTone[tone].push_back (id.substring (prefix.length()));
        }
    }

    REQUIRE (suffixesByTone[0].size() == kExpectedToneParameters);
    REQUIRE (suffixesByTone[1] == suffixesByTone[0]);
    REQUIRE (suffixesByTone[2] == suffixesByTone[0]);
    REQUIRE (suffixesByTone[3] == suffixesByTone[0]);
}

TEST_CASE ("key parameter defaults match the specification")
{
    Fixture fixture;
    const auto byId = indexById (fixture);

    const auto defaultValue = [&byId] (const String& id)
    {
        auto* parameter = byId.at (id);
        return parameter->convertFrom0to1 (parameter->getDefaultValue());
    };

    REQUIRE (defaultValue ("patch.common.level") == Catch::Approx (127.0f));
    REQUIRE (defaultValue ("patch.common.pan") == Catch::Approx (64.0f));
    REQUIRE (defaultValue ("patch.common.keyAssign") == Catch::Approx (0.0f));
    REQUIRE (defaultValue ("patch.common.defaultTempo") == Catch::Approx (120.0f));
    REQUIRE (defaultValue ("tone1.wg.toneSwitch") == Catch::Approx (1.0f));
    REQUIRE (defaultValue ("tone1.wg.pitchKeyfollow") == Catch::Approx (100.0f));
    REQUIRE (defaultValue ("tone1.wg.coarseTune") == Catch::Approx (0.0f));
    REQUIRE (defaultValue ("tone1.tvf.type") == Catch::Approx (1.0f));
    REQUIRE (defaultValue ("tone1.pan.position") == Catch::Approx (64.0f));
    REQUIRE (defaultValue ("tone4.ctrl3.dest4") == Catch::Approx (0.0f));
}

TEST_CASE ("choice parameters expose the specified option counts")
{
    Fixture fixture;

    std::map<String, const AudioParameterChoice*> choicesById;

    for (auto* parameter : collectParameters (fixture))
        if (auto* choice = dynamic_cast<const AudioParameterChoice*> (parameter))
            choicesById.emplace (choice->getParameterID(), choice);

    REQUIRE (choicesById.at ("patch.common.structure12")->choices.size() == 4);
    REQUIRE (choicesById.at ("patch.common.ctrlSource2")->choices.size() == 97);
    REQUIRE (choicesById.at ("tone1.wg.waveGain")->choices.size() == 4);
    REQUIRE (choicesById.at ("tone1.tvf.type")->choices.size() == 5);
    REQUIRE (choicesById.at ("tone1.lfo1.wave")->choices.size() == 8);
    REQUIRE (choicesById.at ("tone1.ctrl1.dest1")->choices.size() == 23);
}

TEST_CASE ("every parameter round-trips host automation values")
{
    Fixture fixture;

    for (auto* parameter : collectParameters (fixture))
    {
        const auto range = parameter->getNormalisableRange();
        const auto steps = range.interval > 0.0f ? (range.end - range.start) / range.interval + 1.0f : 0.0f;
        const auto tolerance = steps > 1.0f ? 1.0f / (steps - 1.0f) + 1.0e-4f : 1.0e-4f;

        for (const auto target : { 0.0f, 0.5f, 1.0f })
        {
            parameter->setValueNotifyingHost (target);

            INFO ("parameter: " << parameter->getParameterID().toStdString() << " target: " << target);
            REQUIRE (parameter->getValue() == Catch::Approx (target).margin (tolerance));
        }
    }
}

TEST_CASE ("registry dump", "[.registry]")
{
    Fixture fixture;
    const auto parameters = collectParameters (fixture);

    std::ofstream out (std::string (MYJV_REPO_DIR) + "/REVIEWS/M0-03/parameter-registry.md");
    REQUIRE (out.is_open());

    out << "# myJV 参数注册表（由 myJV_tests \"[.registry]\" 自动生成，请勿手改）\n\n";
    out << "| ID | 名称 | 类型 | 范围 / 选项数 | 默认 |\n";
    out << "| --- | --- | --- | --- | --- |\n";

    for (auto* parameter : parameters)
    {
        const auto range = parameter->getNormalisableRange();

        String type = "Float";
        String rangeText = String (range.start, 1) + " - " + String (range.end, 1);

        if (auto* choice = dynamic_cast<const AudioParameterChoice*> (parameter))
        {
            type = "Choice";
            rangeText = String (choice->choices.size()) + " options";
        }
        else if (dynamic_cast<const AudioParameterInt*> (parameter) != nullptr)
        {
            type = "Int";
            rangeText = String (roundToInt (range.start)) + " - " + String (roundToInt (range.end));
        }
        else if (dynamic_cast<const AudioParameterBool*> (parameter) != nullptr)
        {
            type = "Bool";
            rangeText = "Off / On";
        }

        out << "| `" << parameter->getParameterID().toStdString() << "` | "
            << parameter->getName (128).toStdString() << " | "
            << type.toStdString() << " | "
            << rangeText.toStdString() << " | "
            << parameter->getText (parameter->getDefaultValue(), 64).toStdString() << " |\n";
    }

    out << "\n";
}
