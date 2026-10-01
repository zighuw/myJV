#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"
#include "Params/ParameterIDs.h"
#include "Params/ParamSnapshotCache.h"
#include "Plugin/MyJVProcessor.h"

#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

static_assert (PatchSnapshot::kToneCount == kNumTones,
               "the snapshot tone count must match the runtime tone count");
static_assert (std::is_trivially_copyable_v<ToneSnapshot>, "ToneSnapshot must stay trivially copyable");
static_assert (std::is_trivially_copyable_v<PatchSnapshot>, "PatchSnapshot must stay trivially copyable");

namespace
{
using namespace juce;

constexpr int kExpectedTotalParameters = 518;
constexpr int kExpectedPatchCommonParameters = 26;
constexpr int kExpectedToneParameters = 123;
constexpr int kExpectedChoiceParameters = 106;

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
    {
        auto* ranged = dynamic_cast<RangedAudioParameter*> (parameter);
        REQUIRE (ranged != nullptr);
        parameters.push_back (ranged);
    }

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
        const auto storesNormalisedValueDirectly = dynamic_cast<const AudioParameterBool*> (parameter) != nullptr;

        for (const auto target : { 0.0f, 0.5f, 1.0f })
        {
            parameter->setValueNotifyingHost (target);

            const auto expected = storesNormalisedValueDirectly
                                      ? target
                                      : parameter->convertTo0to1 (parameter->convertFrom0to1 (target));

            INFO ("parameter: " << parameter->getParameterID().toStdString() << " target: " << target);
            REQUIRE (parameter->getValue() == Catch::Approx (expected).margin (1.0e-4f));
        }
    }
}

TEST_CASE ("all choice parameters are well-formed")
{
    Fixture fixture;
    int choiceCount = 0;

    for (auto* parameter : collectParameters (fixture))
    {
        auto* choice = dynamic_cast<const AudioParameterChoice*> (parameter);

        if (choice == nullptr)
            continue;

        ++choiceCount;
        INFO (choice->getParameterID().toStdString());

        REQUIRE (choice->choices.size() >= 2);

        for (const auto& option : choice->choices)
            REQUIRE (option.isNotEmpty());
    }

    REQUIRE (choiceCount == kExpectedChoiceParameters);
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

// ---------------------------------------------------------------------------
// M2-01: parameter snapshot + per-block refresh (activity/freeze contract).
// The field tables below are the independent oracle: they are transcribed from
// REVIEWS/M0-03/parameter-registry.md (generated from the live APVTS), not from
// the production binding tables.
// ---------------------------------------------------------------------------

namespace
{
using namespace juce;

void setParameter (Fixture& fixture, const String& id, float value)
{
    auto* parameter = fixture.apvts.getParameter (id);
    REQUIRE (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

struct ToneFloatCase
{
    const char* suffix;
    float defaultValue;
    float probeValue;
    float (*read) (const ToneSnapshot&);
};

const ToneFloatCase kToneFloatCases[]
{
    { "wg.fxm.depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.fxmDepth; } },
    { "wg.toneDelay.time", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.toneDelayTime; } },
    { "wg.velXfade", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.velXfade; } },
    { "wg.randomPitch", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.randomPitch; } },
    { "wg.pitchKeyfollow", 100.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.pitchKeyfollow; } },
    { "wg.pitchLfo1Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.pitchLfo1Depth; } },
    { "wg.pitchLfo2Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.wg.pitchLfo2Depth; } },

    { "tvf.cutoff", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.cutoff; } },
    { "tvf.cutoffKeyfollow", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.cutoffKeyfollow; } },
    { "tvf.resonance", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.resonance; } },
    { "tvf.fEnv.time1", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.time[0]; } },
    { "tvf.fEnv.time2", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.time[1]; } },
    { "tvf.fEnv.time3", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.time[2]; } },
    { "tvf.fEnv.time4", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.time[3]; } },
    { "tvf.fEnv.level1", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.level[0]; } },
    { "tvf.fEnv.level2", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.level[1]; } },
    { "tvf.fEnv.level3", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.level[2]; } },
    { "tvf.fEnv.level4", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.fEnv.level[3]; } },
    { "tvf.lfo1Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.lfo1Depth; } },
    { "tvf.lfo2Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tvf.lfo2Depth; } },

    { "tva.level", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.level; } },
    { "tva.aEnv.time1", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.time[0]; } },
    { "tva.aEnv.time2", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.time[1]; } },
    { "tva.aEnv.time3", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.time[2]; } },
    { "tva.aEnv.time4", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.time[3]; } },
    { "tva.aEnv.level1", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.level[0]; } },
    { "tva.aEnv.level2", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.level[1]; } },
    { "tva.aEnv.level3", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.aEnv.level[2]; } },
    { "tva.lfo1Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.lfo1Depth; } },
    { "tva.lfo2Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.tva.lfo2Depth; } },

    { "pEnv.time1", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.time[0]; } },
    { "pEnv.time2", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.time[1]; } },
    { "pEnv.time3", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.time[2]; } },
    { "pEnv.time4", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.time[3]; } },
    { "pEnv.level1", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.level[0]; } },
    { "pEnv.level2", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.level[1]; } },
    { "pEnv.level3", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.level[2]; } },
    { "pEnv.level4", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pEnv.level[3]; } },

    { "pan.position", 64.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pan.position; } },
    { "pan.random", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pan.random; } },
    { "pan.alt", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pan.alt; } },
    { "pan.lfo1Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pan.lfo1Depth; } },
    { "pan.lfo2Depth", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.pan.lfo2Depth; } },

    { "output.level", 127.0f, 21.5f, [] (const ToneSnapshot& s) { return s.output.level; } },

    { "lfo1.rate", 64.0f, 21.5f, [] (const ToneSnapshot& s) { return s.lfo[0].rate; } },
    { "lfo1.delayTime", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.lfo[0].delayTime; } },
    { "lfo1.fadeTime", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.lfo[0].fadeTime; } },
    { "lfo2.rate", 64.0f, 21.5f, [] (const ToneSnapshot& s) { return s.lfo[1].rate; } },
    { "lfo2.delayTime", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.lfo[1].delayTime; } },
    { "lfo2.fadeTime", 0.0f, 21.5f, [] (const ToneSnapshot& s) { return s.lfo[1].fadeTime; } },
};

struct ToneIntCase
{
    const char* suffix;
    int defaultValue;
    int probeValue;
    int (*read) (const ToneSnapshot&);
};

const ToneIntCase kToneIntCases[]
{
    { "wg.waveGain", 1, 2, [] (const ToneSnapshot& s) { return s.wg.waveGain; } },
    { "wg.fxm.color", 1, 4, [] (const ToneSnapshot& s) { return s.wg.fxmColor; } },
    { "wg.toneDelay.mode", 0, 2, [] (const ToneSnapshot& s) { return s.wg.toneDelayMode; } },
    { "wg.velLow", 1, 100, [] (const ToneSnapshot& s) { return s.wg.velLow; } },
    { "wg.velHigh", 127, 42, [] (const ToneSnapshot& s) { return s.wg.velHigh; } },
    { "wg.keyLow", 0, 12, [] (const ToneSnapshot& s) { return s.wg.keyLow; } },
    { "wg.keyHigh", 127, 110, [] (const ToneSnapshot& s) { return s.wg.keyHigh; } },
    { "wg.coarseTune", 0, 7, [] (const ToneSnapshot& s) { return s.wg.coarseTune; } },
    { "wg.fineTune", 0, -13, [] (const ToneSnapshot& s) { return s.wg.fineTune; } },

    { "tvf.type", 1, 3, [] (const ToneSnapshot& s) { return s.tvf.type; } },
    { "tvf.resVelSens", 0, 20, [] (const ToneSnapshot& s) { return s.tvf.resVelSens; } },
    { "tvf.fEnv.depth", 0, -20, [] (const ToneSnapshot& s) { return s.tvf.fEnv.depth; } },
    { "tvf.fEnv.velCurve", 0, 5, [] (const ToneSnapshot& s) { return s.tvf.fEnv.velCurve; } },
    { "tvf.fEnv.velSens", 0, 20, [] (const ToneSnapshot& s) { return s.tvf.fEnv.velSens; } },
    { "tvf.fEnv.timeKeyfollow", 0, 20, [] (const ToneSnapshot& s) { return s.tvf.fEnv.timeKeyfollow; } },
    { "tvf.fEnv.velTime1Sens", 0, 20, [] (const ToneSnapshot& s) { return s.tvf.fEnv.velTime1Sens; } },
    { "tvf.fEnv.velTime4Sens", 0, 20, [] (const ToneSnapshot& s) { return s.tvf.fEnv.velTime4Sens; } },

    { "tva.bias.direction", 0, 1, [] (const ToneSnapshot& s) { return s.tva.biasDirection; } },
    { "tva.bias.point", 64, 100, [] (const ToneSnapshot& s) { return s.tva.biasPoint; } },
    { "tva.bias.level", 0, -20, [] (const ToneSnapshot& s) { return s.tva.biasLevel; } },
    { "tva.aEnv.velCurve", 0, 5, [] (const ToneSnapshot& s) { return s.tva.aEnv.velCurve; } },
    { "tva.aEnv.velSens", 0, 20, [] (const ToneSnapshot& s) { return s.tva.aEnv.velSens; } },
    { "tva.aEnv.timeKeyfollow", 0, 20, [] (const ToneSnapshot& s) { return s.tva.aEnv.timeKeyfollow; } },
    { "tva.aEnv.velTime1Sens", 0, 20, [] (const ToneSnapshot& s) { return s.tva.aEnv.velTime1Sens; } },
    { "tva.aEnv.velTime4Sens", 0, 20, [] (const ToneSnapshot& s) { return s.tva.aEnv.velTime4Sens; } },

    { "pEnv.depth", 0, -20, [] (const ToneSnapshot& s) { return s.pEnv.depth; } },
    { "pEnv.velSens", 0, 20, [] (const ToneSnapshot& s) { return s.pEnv.velSens; } },
    { "pEnv.timeKeyfollow", 0, 20, [] (const ToneSnapshot& s) { return s.pEnv.timeKeyfollow; } },
    { "pEnv.velTime1Sens", 0, 20, [] (const ToneSnapshot& s) { return s.pEnv.velTime1Sens; } },
    { "pEnv.velTime4Sens", 0, 20, [] (const ToneSnapshot& s) { return s.pEnv.velTime4Sens; } },

    { "pan.keyfollow", 0, 20, [] (const ToneSnapshot& s) { return s.pan.keyfollow; } },
    { "output.assign", 0, 2, [] (const ToneSnapshot& s) { return s.output.assign; } },

    { "lfo1.wave", 0, 5, [] (const ToneSnapshot& s) { return s.lfo[0].wave; } },
    { "lfo1.levelOffset", 0, -20, [] (const ToneSnapshot& s) { return s.lfo[0].levelOffset; } },
    { "lfo1.fadeMode", 0, 1, [] (const ToneSnapshot& s) { return s.lfo[0].fadeMode; } },
    { "lfo2.wave", 0, 5, [] (const ToneSnapshot& s) { return s.lfo[1].wave; } },
    { "lfo2.levelOffset", 0, -20, [] (const ToneSnapshot& s) { return s.lfo[1].levelOffset; } },
    { "lfo2.fadeMode", 0, 1, [] (const ToneSnapshot& s) { return s.lfo[1].fadeMode; } },

    { "ctrl1.dest1", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[0].dest[0]; } },
    { "ctrl1.dest2", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[0].dest[1]; } },
    { "ctrl1.dest3", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[0].dest[2]; } },
    { "ctrl1.dest4", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[0].dest[3]; } },
    { "ctrl1.depth1", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[0].depth[0]; } },
    { "ctrl1.depth2", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[0].depth[1]; } },
    { "ctrl1.depth3", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[0].depth[2]; } },
    { "ctrl1.depth4", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[0].depth[3]; } },
    { "ctrl2.dest1", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[1].dest[0]; } },
    { "ctrl2.dest2", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[1].dest[1]; } },
    { "ctrl2.dest3", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[1].dest[2]; } },
    { "ctrl2.dest4", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[1].dest[3]; } },
    { "ctrl2.depth1", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[1].depth[0]; } },
    { "ctrl2.depth2", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[1].depth[1]; } },
    { "ctrl2.depth3", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[1].depth[2]; } },
    { "ctrl2.depth4", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[1].depth[3]; } },
    { "ctrl3.dest1", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[2].dest[0]; } },
    { "ctrl3.dest2", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[2].dest[1]; } },
    { "ctrl3.dest3", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[2].dest[2]; } },
    { "ctrl3.dest4", 0, 5, [] (const ToneSnapshot& s) { return s.ctrl[2].dest[3]; } },
    { "ctrl3.depth1", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[2].depth[0]; } },
    { "ctrl3.depth2", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[2].depth[1]; } },
    { "ctrl3.depth3", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[2].depth[2]; } },
    { "ctrl3.depth4", 0, -20, [] (const ToneSnapshot& s) { return s.ctrl[2].depth[3]; } },
};

struct ToneBoolCase
{
    const char* suffix;
    bool defaultValue;
    bool probeValue;
    bool (*read) (const ToneSnapshot&);
};

const ToneBoolCase kToneBoolCases[]
{
    { "wg.toneSwitch", true, false, [] (const ToneSnapshot& s) { return s.wg.toneSwitch; } },
    { "wg.fxm.switch", false, true, [] (const ToneSnapshot& s) { return s.wg.fxmOn; } },
    { "wg.redamper", false, true, [] (const ToneSnapshot& s) { return s.wg.redamper; } },
    { "wg.volCtrl", true, false, [] (const ToneSnapshot& s) { return s.wg.volCtrl; } },
    { "wg.holdCtrl", true, false, [] (const ToneSnapshot& s) { return s.wg.holdCtrl; } },
    { "wg.bendCtrl", true, false, [] (const ToneSnapshot& s) { return s.wg.bendCtrl; } },
    { "wg.panCtrl", true, false, [] (const ToneSnapshot& s) { return s.wg.panCtrl; } },
    { "lfo1.keyTrig", false, true, [] (const ToneSnapshot& s) { return s.lfo[0].keyTrig; } },
    { "lfo1.sync", false, true, [] (const ToneSnapshot& s) { return s.lfo[0].sync; } },
    { "lfo2.keyTrig", false, true, [] (const ToneSnapshot& s) { return s.lfo[1].keyTrig; } },
    { "lfo2.sync", false, true, [] (const ToneSnapshot& s) { return s.lfo[1].sync; } },
};

struct CommonFloatCase
{
    const char* suffix;
    float defaultValue;
    float probeValue;
    float (*read) (const PatchCommonSnapshot&);
};

const CommonFloatCase kCommonFloatCases[]
{
    { "level", 127.0f, 21.5f, [] (const PatchCommonSnapshot& s) { return s.level; } },
    { "pan", 64.0f, 21.5f, [] (const PatchCommonSnapshot& s) { return s.pan; } },
    { "analogFeel", 0.0f, 21.5f, [] (const PatchCommonSnapshot& s) { return s.analogFeel; } },
    { "portamento.time", 64.0f, 21.5f, [] (const PatchCommonSnapshot& s) { return s.portamentoTime; } },
    { "booster12", 64.0f, 21.5f, [] (const PatchCommonSnapshot& s) { return s.booster12; } },
    { "booster34", 64.0f, 21.5f, [] (const PatchCommonSnapshot& s) { return s.booster34; } },
};

struct CommonIntCase
{
    const char* suffix;
    int defaultValue;
    int probeValue;
    int (*read) (const PatchCommonSnapshot&);
};

const CommonIntCase kCommonIntCases[]
{
    { "bendUp", 2, 7, [] (const PatchCommonSnapshot& s) { return s.bendUp; } },
    { "bendDown", 2, 7, [] (const PatchCommonSnapshot& s) { return s.bendDown; } },
    { "octaveShift", 0, -2, [] (const PatchCommonSnapshot& s) { return s.octaveShift; } },
    { "stretchTune", 0, 3, [] (const PatchCommonSnapshot& s) { return s.stretchTune; } },
    { "keyAssign", 0, 1, [] (const PatchCommonSnapshot& s) { return s.keyAssign; } },
    { "portamento.mode", 0, 1, [] (const PatchCommonSnapshot& s) { return s.portamentoMode; } },
    { "portamento.type", 1, 0, [] (const PatchCommonSnapshot& s) { return s.portamentoType; } },
    { "portamento.start", 0, 1, [] (const PatchCommonSnapshot& s) { return s.portamentoStart; } },
    { "voicePriority", 0, 1, [] (const PatchCommonSnapshot& s) { return s.voicePriority; } },
    { "structure12", 0, 3, [] (const PatchCommonSnapshot& s) { return s.structure12; } },
    { "structure34", 0, 3, [] (const PatchCommonSnapshot& s) { return s.structure34; } },
    { "ctrlSource2", 10, 77, [] (const PatchCommonSnapshot& s) { return s.ctrlSource2; } },
    { "ctrlSource3", 12, 77, [] (const PatchCommonSnapshot& s) { return s.ctrlSource3; } },
    { "controlHoldPeak", 0, 1, [] (const PatchCommonSnapshot& s) { return s.controlHoldPeak; } },
    { "ctrl1HoldPeak", 0, 1, [] (const PatchCommonSnapshot& s) { return s.ctrl1HoldPeak; } },
    { "ctrl2HoldPeak", 0, 1, [] (const PatchCommonSnapshot& s) { return s.ctrl2HoldPeak; } },
    { "ctrl3HoldPeak", 0, 1, [] (const PatchCommonSnapshot& s) { return s.ctrl3HoldPeak; } },
    { "defaultTempo", 120, 188, [] (const PatchCommonSnapshot& s) { return s.defaultTempo; } },
};

struct CommonBoolCase
{
    const char* suffix;
    bool defaultValue;
    bool probeValue;
    bool (*read) (const PatchCommonSnapshot&);
};

const CommonBoolCase kCommonBoolCases[]
{
    { "legato", false, true, [] (const PatchCommonSnapshot& s) { return s.legato; } },
    { "portamento.switch", false, true, [] (const PatchCommonSnapshot& s) { return s.portamentoSwitch; } },
};

void makeTestBuses (AudioBuffer<float> (&buffers)[kNumOutputBuses], BusBuffers& buses)
{
    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        buffers[bus].setSize (2, 512);
        buffers[bus].clear();
        buses.l[bus] = buffers[bus].getWritePointer (0);
        buses.r[bus] = buffers[bus].getWritePointer (1);
    }
}
}

TEST_CASE ("snapshot cache covers every registered parameter exactly once")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    const auto ids = ParamSnapshotCache::parameterIds();

    REQUIRE (ids.size() == (std::size_t) ParamSnapshotCache::totalParameterCount());
    REQUIRE (cache.resolvedCount() == ParamSnapshotCache::totalParameterCount());

    std::set<String> cachedIds;

    for (const auto& id : ids)
        cachedIds.insert (id);

    REQUIRE (cachedIds.size() == ids.size());

    std::set<String> registeredIds;

    for (auto* parameter : collectParameters (fixture))
        registeredIds.insert (parameter->getParameterID());

    REQUIRE (cachedIds == registeredIds);

    int commonCount = 0;

    for (const auto& id : cachedIds)
        if (id.startsWith ("patch.common."))
            ++commonCount;

    CHECK (commonCount == 26);
    CHECK (ParamSnapshotCache::totalParameterCount() == 26 + 4 * 123);
}

TEST_CASE ("tone snapshot fields mirror the tone parameters")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    PatchRuntime runtime;

    cache.refresh (runtime);

    for (const auto& c : kToneFloatCases)
    {
        INFO ("tone1." << c.suffix << " (default)");
        REQUIRE (c.read (runtime.snapshot.tones[0]) == Catch::Approx (c.defaultValue));
    }

    for (const auto& c : kToneIntCases)
    {
        INFO ("tone1." << c.suffix << " (default)");
        REQUIRE (c.read (runtime.snapshot.tones[0]) == c.defaultValue);
    }

    for (const auto& c : kToneBoolCases)
    {
        INFO ("tone1." << c.suffix << " (default)");
        REQUIRE (c.read (runtime.snapshot.tones[0]) == c.defaultValue);
    }

    for (const auto& c : kToneFloatCases)
    {
        setParameter (fixture, "tone1." + String (c.suffix), c.probeValue);
        cache.refresh (runtime);
        INFO ("tone1." << c.suffix << " (probe)");
        REQUIRE (c.read (runtime.snapshot.tones[0]) == Catch::Approx (c.probeValue));
    }

    for (const auto& c : kToneIntCases)
    {
        setParameter (fixture, "tone1." + String (c.suffix), (float) c.probeValue);
        cache.refresh (runtime);
        INFO ("tone1." << c.suffix << " (probe)");
        REQUIRE (c.read (runtime.snapshot.tones[0]) == c.probeValue);
    }

    for (const auto& c : kToneBoolCases)
    {
        setParameter (fixture, "tone1." + String (c.suffix), c.probeValue ? 1.0f : 0.0f);
        cache.refresh (runtime);
        INFO ("tone1." << c.suffix << " (probe)");
        REQUIRE (c.read (runtime.snapshot.tones[0]) == c.probeValue);
    }
}

TEST_CASE ("common snapshot fields mirror the patch common parameters")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    PatchRuntime runtime;

    cache.refresh (runtime);

    for (const auto& c : kCommonFloatCases)
    {
        INFO ("patch.common." << c.suffix << " (default)");
        REQUIRE (c.read (runtime.snapshot.common) == Catch::Approx (c.defaultValue));
    }

    for (const auto& c : kCommonIntCases)
    {
        INFO ("patch.common." << c.suffix << " (default)");
        REQUIRE (c.read (runtime.snapshot.common) == c.defaultValue);
    }

    for (const auto& c : kCommonBoolCases)
    {
        INFO ("patch.common." << c.suffix << " (default)");
        REQUIRE (c.read (runtime.snapshot.common) == c.defaultValue);
    }

    for (const auto& c : kCommonFloatCases)
    {
        setParameter (fixture, "patch.common." + String (c.suffix), c.probeValue);
        cache.refresh (runtime);
        INFO ("patch.common." << c.suffix << " (probe)");
        REQUIRE (c.read (runtime.snapshot.common) == Catch::Approx (c.probeValue));
    }

    for (const auto& c : kCommonIntCases)
    {
        setParameter (fixture, "patch.common." + String (c.suffix), (float) c.probeValue);
        cache.refresh (runtime);
        INFO ("patch.common." << c.suffix << " (probe)");
        REQUIRE (c.read (runtime.snapshot.common) == c.probeValue);
    }

    for (const auto& c : kCommonBoolCases)
    {
        setParameter (fixture, "patch.common." + String (c.suffix), c.probeValue ? 1.0f : 0.0f);
        cache.refresh (runtime);
        INFO ("patch.common." << c.suffix << " (probe)");
        REQUIRE (c.read (runtime.snapshot.common) == c.probeValue);
    }
}

TEST_CASE ("tone snapshots stay independent across the four tones")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    PatchRuntime runtime;

    for (int tone = 1; tone <= 4; ++tone)
    {
        setParameter (fixture, "tone" + String (tone) + ".wg.coarseTune", (float) (tone * 5));
        setParameter (fixture, "tone" + String (tone) + ".tvf.cutoff", (float) (30 + tone));
    }

    cache.refresh (runtime);

    for (int tone = 1; tone <= 4; ++tone)
    {
        INFO ("tone" << tone);
        REQUIRE (runtime.snapshot.tones[tone - 1].wg.coarseTune == tone * 5);
        REQUIRE (runtime.snapshot.tones[tone - 1].tvf.cutoff == Catch::Approx (30.0f + (float) tone));
    }
}

TEST_CASE ("choice parameters round-trip every option index")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    PatchRuntime runtime;

    cache.refresh (runtime);
    CHECK (runtime.snapshot.common.ctrlSource2 == 10);   // registry default CC11
    CHECK (runtime.snapshot.common.ctrlSource3 == 12);   // registry default CC13
    CHECK (runtime.snapshot.tones[0].ctrl[0].dest[0] == 0);

    // The registry lists 97 control sources (CC1..CC95, PITCH BEND, CH AFTERTOUCH).
    for (int index = 0; index <= 96; ++index)
    {
        setParameter (fixture, "patch.common.ctrlSource2", (float) index);
        setParameter (fixture, "patch.common.ctrlSource3", (float) index);
        cache.refresh (runtime);
        INFO ("control source index " << index);
        REQUIRE (runtime.snapshot.common.ctrlSource2 == index);
        REQUIRE (runtime.snapshot.common.ctrlSource3 == index);
    }

    // The registry lists 23 control destinations.
    for (int index = 0; index <= 22; ++index)
    {
        setParameter (fixture, "tone1.ctrl1.dest1", (float) index);
        cache.refresh (runtime);
        INFO ("control destination index " << index);
        REQUIRE (runtime.snapshot.tones[0].ctrl[0].dest[0] == index);
    }
}

TEST_CASE ("the engine refreshes the active runtime snapshot every block")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    AssetReclaimer reclaimer;
    SynthEngine engine;
    engine.prepare (48000.0, 512);

    AudioBuffer<float> buffers[kNumOutputBuses];
    BusBuffers buses;
    makeTestBuses (buffers, buses);
    MidiBuffer midi;

    engine.process (buses, midi, 512);              // no source at all: silent skip

    engine.setParamSnapshotSource (&cache, nullptr);
    engine.process (buses, midi, 512);              // cache but no reclaimer: skip

    engine.setParamSnapshotSource (&cache, &reclaimer);
    engine.process (buses, midi, 512);              // wired, no active runtime: skip

    auto runtime = std::make_shared<PatchRuntime>();
    reclaimer.publish (runtime);

    setParameter (fixture, "patch.common.level", 100.0f);
    setParameter (fixture, "tone1.wg.coarseTune", 12.0f);
    engine.process (buses, midi, 512);
    CHECK (runtime->snapshot.common.level == Catch::Approx (100.0f));
    CHECK (runtime->snapshot.tones[0].wg.coarseTune == 12);

    setParameter (fixture, "patch.common.level", 33.0f);
    setParameter (fixture, "tone1.wg.coarseTune", -7.0f);
    engine.process (buses, midi, 512);
    CHECK (runtime->snapshot.common.level == Catch::Approx (33.0f));
    CHECK (runtime->snapshot.tones[0].wg.coarseTune == -7);
}

TEST_CASE ("retired runtime snapshots are frozen")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    AssetReclaimer reclaimer (0.0);
    SynthEngine engine;
    engine.prepare (48000.0, 512);
    engine.setParamSnapshotSource (&cache, &reclaimer);

    AudioBuffer<float> buffers[kNumOutputBuses];
    BusBuffers buses;
    makeTestBuses (buffers, buses);
    MidiBuffer midi;

    auto first = std::make_shared<PatchRuntime>();
    reclaimer.publish (first);

    setParameter (fixture, "patch.common.level", 100.0f);
    engine.process (buses, midi, 512);
    REQUIRE (first->snapshot.common.level == Catch::Approx (100.0f));

    setParameter (fixture, "patch.common.level", 44.0f);
    auto second = std::make_shared<PatchRuntime>();
    reclaimer.publish (second);
    engine.process (buses, midi, 512);
    CHECK (second->snapshot.common.level == Catch::Approx (44.0f));
    CHECK (first->snapshot.common.level == Catch::Approx (100.0f));

    setParameter (fixture, "patch.common.level", 11.0f);
    engine.process (buses, midi, 512);
    CHECK (second->snapshot.common.level == Catch::Approx (11.0f));
    CHECK (first->snapshot.common.level == Catch::Approx (100.0f));
    CHECK (first->id == 1);
    CHECK (second->id == 2);
}

TEST_CASE ("the snapshot mirrors the runtime asset id and zone pointers")
{
    Fixture fixture;
    auto cache = ParamSnapshotCache::fromApvts (fixture.apvts);
    AssetReclaimer reclaimer;
    SynthEngine engine;
    engine.prepare (48000.0, 512);
    engine.setParamSnapshotSource (&cache, &reclaimer);

    AudioBuffer<float> buffers[kNumOutputBuses];
    BusBuffers buses;
    makeTestBuses (buffers, buses);
    MidiBuffer midi;

    auto zoneSet = std::make_shared<ZoneSet>();
    auto runtime = std::make_shared<PatchRuntime>();
    runtime->zoneSets[2] = zoneSet;
    runtime->rawZoneSets[2] = zoneSet.get();
    reclaimer.publish (runtime);

    engine.process (buses, midi, 512);

    CHECK (runtime->snapshot.assetId == runtime->id);
    CHECK (runtime->snapshot.zoneSets[2] == zoneSet.get());
    CHECK (runtime->snapshot.zoneSets[0] == nullptr);
    CHECK_FALSE (runtime->snapshot.toDebugString().empty());
}

TEST_CASE ("an empty snapshot cache is a safe no-op")
{
    ParamSnapshotCache cache;
    PatchRuntime runtime;

    cache.refresh (runtime);

    CHECK (cache.resolvedCount() == 0);
    CHECK (runtime.snapshot == PatchSnapshot {});
}

TEST_CASE ("fromPointers follows the parameterIds order")
{
    std::vector<std::unique_ptr<std::atomic<float>>> values;

    for (int i = 0; i < ParamSnapshotCache::totalParameterCount(); ++i)
        values.push_back (std::make_unique<std::atomic<float>> (0.0f));

    std::vector<std::atomic<float>*> pointers;

    for (const auto& value : values)
        pointers.push_back (value.get());

    auto cache = ParamSnapshotCache::fromPointers (pointers);
    REQUIRE (cache.resolvedCount() == ParamSnapshotCache::totalParameterCount());

    values[0]->store (5.0f);            // patch.common.level: first binding
    values[26]->store (9.0f);           // tone1.wg.toneSwitch: first tone binding
    values[27]->store (3.0f);           // tone1.wg.waveGain
    values[26 + 123 + 1]->store (4.0f); // tone2.wg.waveGain (tone-major stride)
    values[517]->store (-9.0f);         // tone4.ctrl3.depth4: last binding

    PatchRuntime runtime;
    cache.refresh (runtime);

    CHECK (runtime.snapshot.common.level == Catch::Approx (5.0f));
    CHECK (runtime.snapshot.tones[0].wg.toneSwitch);
    CHECK (runtime.snapshot.tones[0].wg.waveGain == 3);
    CHECK (runtime.snapshot.tones[0].tvf.cutoff == Catch::Approx (0.0f));
    CHECK (runtime.snapshot.tones[1].wg.waveGain == 4);
    CHECK (runtime.snapshot.tones[0].wg.waveGain == 3);
    CHECK (runtime.snapshot.tones[3].ctrl[2].depth[3] == -9);

    // A differently sized list yields an empty cache instead of reading past it.
    auto empty = ParamSnapshotCache::fromPointers ({});
    CHECK (empty.resolvedCount() == 0);
}

TEST_CASE ("the processor exposes its APVTS and resolves the full snapshot")
{
    MyJVProcessor processor;
    auto& apvts = processor.getApvts();
    auto cache = ParamSnapshotCache::fromApvts (apvts);

    CHECK (cache.resolvedCount() == ParamSnapshotCache::totalParameterCount());
}
