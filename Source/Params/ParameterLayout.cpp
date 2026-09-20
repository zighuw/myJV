#include "ParameterIDs.h"

namespace
{
using namespace juce;

constexpr int kNumTones = 4;
constexpr int kParameterVersionHint = 1;

const char* const kStretchTuneChoices[] { "OFF", "1", "2", "3" };
const char* const kKeyAssignChoices[] { "POLY", "MONO" };
const char* const kPortamentoModeChoices[] { "NORMAL", "LEGATO" };
const char* const kPortamentoTypeChoices[] { "RATE", "TIME" };
const char* const kPortamentoStartChoices[] { "PITCH", "NOTE" };
const char* const kVoicePriorityChoices[] { "LAST", "LOUDEST" };
const char* const kStructureChoices[] { "1", "2", "3", "4" };
const char* const kHoldPeakChoices[] { "HOLD", "PEAK" };

const char* const kWaveGainChoices[] { "-6 dB", "0 dB", "+6 dB", "+12 dB" };
const char* const kToneDelayModeChoices[] { "NORMAL", "HOLD", "KEY INTERVAL" };
const char* const kTvfTypeChoices[] { "OFF", "LPF", "BPF", "HPF", "PKG" };
const char* const kVelocityCurveChoices[] { "LINEAR", "EXP1", "EXP2", "EXP3", "LOG1", "LOG2", "LOG3" };
const char* const kBiasDirectionChoices[] { "LOWER", "UPPER" };
const char* const kLfoWaveChoices[] { "SINE", "SAWTOOTH", "SQUARE", "TRIANGLE", "TRAPEZOID", "SAMPLE&HOLD", "RANDOM", "CHAOTIC" };
const char* const kLfoFadeModeChoices[] { "LINEAR", "EXPONENTIAL" };
const char* const kOutputAssignChoices[] { "MAIN", "OUT1", "OUT2" };
const char* const kControlDestChoices[] { "OFF", "PITCH", "CUTOFF", "RESONANCE", "LEVEL", "PAN",
                                          "LFO1_RATE", "LFO1_PITCH", "LFO1_CUTOFF", "LFO1_AMP", "LFO1_PAN",
                                          "LFO2_RATE", "LFO2_PITCH", "LFO2_CUTOFF", "LFO2_AMP", "LFO2_PAN",
                                          "P_ENV_TIME", "P_ENV_LEVEL", "F_ENV_TIME", "F_ENV_LEVEL",
                                          "A_ENV_TIME", "A_ENV_LEVEL", "TONE_DELAY_TIME" };

constexpr int kControlDestCount = 23;

StringArray makeControlSourceChoices()
{
    StringArray choices;

    for (int i = 1; i <= 95; ++i)
        choices.add ("CC" + String (i).paddedLeft ('0', 2));

    choices.add ("PITCH BEND");
    choices.add ("CH AFTERTOUCH");

    return choices;
}

using Layout = AudioProcessorValueTreeState::ParameterLayout;

void addFloat (Layout& layout, const String& id, float min, float max, float defaultValue)
{
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, kParameterVersionHint }, id,
                                                       NormalisableRange<float> (min, max), defaultValue));
}

void addInt (Layout& layout, const String& id, int min, int max, int defaultValue)
{
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, kParameterVersionHint }, id,
                                                     min, max, defaultValue));
}

void addChoice (Layout& layout, const String& id, const char* const* choices, int numChoices, int defaultValue)
{
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, kParameterVersionHint }, id,
                                                        StringArray (choices, numChoices), defaultValue));
}

void addChoice (Layout& layout, const String& id, const StringArray& choices, int defaultValue)
{
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, kParameterVersionHint }, id,
                                                        choices, defaultValue));
}

void addBool (Layout& layout, const String& id, bool defaultValue)
{
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, kParameterVersionHint }, id, defaultValue));
}

void addPatchCommon (Layout& layout)
{
    const auto prefix = String ("patch.common.");

    addFloat (layout, prefix + "level", 0.0f, 127.0f, 127.0f);
    addFloat (layout, prefix + "pan", 0.0f, 127.0f, 64.0f);
    addFloat (layout, prefix + "analogFeel", 0.0f, 127.0f, 0.0f);
    addInt (layout, prefix + "bendUp", 0, 24, 2);
    addInt (layout, prefix + "bendDown", 0, 24, 2);
    addInt (layout, prefix + "octaveShift", -3, 3, 0);
    addChoice (layout, prefix + "stretchTune", kStretchTuneChoices, 4, 0);
    addChoice (layout, prefix + "keyAssign", kKeyAssignChoices, 2, 0);
    addBool (layout, prefix + "legato", false);
    addBool (layout, prefix + "portamento.switch", false);
    addChoice (layout, prefix + "portamento.mode", kPortamentoModeChoices, 2, 0);
    addChoice (layout, prefix + "portamento.type", kPortamentoTypeChoices, 2, 1);
    addChoice (layout, prefix + "portamento.start", kPortamentoStartChoices, 2, 0);
    addFloat (layout, prefix + "portamento.time", 0.0f, 127.0f, 64.0f);
    addChoice (layout, prefix + "voicePriority", kVoicePriorityChoices, 2, 0);
    addChoice (layout, prefix + "structure12", kStructureChoices, 4, 0);
    addFloat (layout, prefix + "booster12", 0.0f, 127.0f, 64.0f);
    addChoice (layout, prefix + "structure34", kStructureChoices, 4, 0);
    addFloat (layout, prefix + "booster34", 0.0f, 127.0f, 64.0f);

    const auto controlSources = makeControlSourceChoices();
    addChoice (layout, prefix + "ctrlSource2", controlSources, 10);
    addChoice (layout, prefix + "ctrlSource3", controlSources, 12);

    addChoice (layout, prefix + "controlHoldPeak", kHoldPeakChoices, 2, 0);
    addChoice (layout, prefix + "ctrl1HoldPeak", kHoldPeakChoices, 2, 0);
    addChoice (layout, prefix + "ctrl2HoldPeak", kHoldPeakChoices, 2, 0);
    addChoice (layout, prefix + "ctrl3HoldPeak", kHoldPeakChoices, 2, 0);
    addInt (layout, prefix + "defaultTempo", 20, 250, 120);
}

void addWg (Layout& layout, const String& prefix)
{
    addBool (layout, prefix + "toneSwitch", true);
    addChoice (layout, prefix + "waveGain", kWaveGainChoices, 4, 1);
    addBool (layout, prefix + "fxm.switch", false);
    addInt (layout, prefix + "fxm.color", 1, 7, 1);
    addFloat (layout, prefix + "fxm.depth", 0.0f, 127.0f, 0.0f);
    addChoice (layout, prefix + "toneDelay.mode", kToneDelayModeChoices, 3, 0);
    addFloat (layout, prefix + "toneDelay.time", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "velXfade", 0.0f, 127.0f, 0.0f);
    addInt (layout, prefix + "velLow", 1, 127, 1);
    addInt (layout, prefix + "velHigh", 1, 127, 127);
    addInt (layout, prefix + "keyLow", 0, 127, 0);
    addInt (layout, prefix + "keyHigh", 0, 127, 127);
    addBool (layout, prefix + "redamper", false);
    addBool (layout, prefix + "volCtrl", true);
    addBool (layout, prefix + "holdCtrl", true);
    addBool (layout, prefix + "bendCtrl", true);
    addBool (layout, prefix + "panCtrl", true);
    addInt (layout, prefix + "coarseTune", -24, 24, 0);
    addInt (layout, prefix + "fineTune", -50, 50, 0);
    addFloat (layout, prefix + "randomPitch", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "pitchKeyfollow", -100.0f, 100.0f, 100.0f);
}

void addTvf (Layout& layout, const String& prefix)
{
    addChoice (layout, prefix + "type", kTvfTypeChoices, 5, 1);
    addFloat (layout, prefix + "cutoff", 0.0f, 127.0f, 127.0f);
    addFloat (layout, prefix + "cutoffKeyfollow", -100.0f, 100.0f, 0.0f);
    addFloat (layout, prefix + "resonance", 0.0f, 127.0f, 0.0f);
    addInt (layout, prefix + "resVelSens", -63, 63, 0);
    addInt (layout, prefix + "fEnv.depth", -63, 63, 0);
    addChoice (layout, prefix + "fEnv.velCurve", kVelocityCurveChoices, 7, 0);
    addInt (layout, prefix + "fEnv.velSens", -63, 63, 0);
    addInt (layout, prefix + "fEnv.timeKeyfollow", -63, 63, 0);

    for (int i = 1; i <= 4; ++i)
        addFloat (layout, prefix + "fEnv.time" + String (i), 0.0f, 127.0f, 0.0f);

    for (int i = 1; i <= 4; ++i)
        addFloat (layout, prefix + "fEnv.level" + String (i), 0.0f, 127.0f, 127.0f);

    addInt (layout, prefix + "fEnv.velTime1Sens", -63, 63, 0);
    addInt (layout, prefix + "fEnv.velTime4Sens", -63, 63, 0);
    addFloat (layout, prefix + "lfo1Depth", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "lfo2Depth", 0.0f, 127.0f, 0.0f);
}

void addTva (Layout& layout, const String& prefix)
{
    addFloat (layout, prefix + "level", 0.0f, 127.0f, 127.0f);
    addChoice (layout, prefix + "bias.direction", kBiasDirectionChoices, 2, 0);
    addInt (layout, prefix + "bias.point", 0, 127, 64);
    addInt (layout, prefix + "bias.level", -63, 63, 0);
    addChoice (layout, prefix + "aEnv.velCurve", kVelocityCurveChoices, 7, 0);
    addInt (layout, prefix + "aEnv.velSens", -63, 63, 0);

    for (int i = 1; i <= 4; ++i)
        addFloat (layout, prefix + "aEnv.time" + String (i), 0.0f, 127.0f, 0.0f);

    for (int i = 1; i <= 3; ++i)
        addFloat (layout, prefix + "aEnv.level" + String (i), 0.0f, 127.0f, 127.0f);

    addInt (layout, prefix + "aEnv.timeKeyfollow", -63, 63, 0);
    addInt (layout, prefix + "aEnv.velTime1Sens", -63, 63, 0);
    addInt (layout, prefix + "aEnv.velTime4Sens", -63, 63, 0);
    addFloat (layout, prefix + "lfo1Depth", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "lfo2Depth", 0.0f, 127.0f, 0.0f);
}

void addPEnv (Layout& layout, const String& prefix)
{
    addInt (layout, prefix + "depth", -63, 63, 0);
    addInt (layout, prefix + "velSens", -63, 63, 0);
    addInt (layout, prefix + "timeKeyfollow", -63, 63, 0);

    for (int i = 1; i <= 4; ++i)
        addFloat (layout, prefix + "time" + String (i), 0.0f, 127.0f, 0.0f);

    for (int i = 1; i <= 4; ++i)
        addFloat (layout, prefix + "level" + String (i), 0.0f, 127.0f, 127.0f);

    addInt (layout, prefix + "velTime1Sens", -63, 63, 0);
    addInt (layout, prefix + "velTime4Sens", -63, 63, 0);
}

void addPan (Layout& layout, const String& prefix)
{
    addFloat (layout, prefix + "position", 0.0f, 127.0f, 64.0f);
    addInt (layout, prefix + "keyfollow", -63, 63, 0);
    addFloat (layout, prefix + "random", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "alt", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "lfo1Depth", 0.0f, 127.0f, 0.0f);
    addFloat (layout, prefix + "lfo2Depth", 0.0f, 127.0f, 0.0f);
}

void addOutput (Layout& layout, const String& prefix)
{
    addChoice (layout, prefix + "assign", kOutputAssignChoices, 3, 0);
    addFloat (layout, prefix + "level", 0.0f, 127.0f, 127.0f);
}

void addLfo (Layout& layout, const String& prefix)
{
    addChoice (layout, prefix + "wave", kLfoWaveChoices, 8, 0);
    addBool (layout, prefix + "keyTrig", false);
    addFloat (layout, prefix + "rate", 0.0f, 127.0f, 64.0f);
    addInt (layout, prefix + "levelOffset", -63, 63, 0);
    addFloat (layout, prefix + "delayTime", 0.0f, 127.0f, 0.0f);
    addChoice (layout, prefix + "fadeMode", kLfoFadeModeChoices, 2, 0);
    addFloat (layout, prefix + "fadeTime", 0.0f, 127.0f, 0.0f);
    addBool (layout, prefix + "sync", false);
}

void addControl (Layout& layout, const String& prefix)
{
    for (int i = 1; i <= 4; ++i)
        addChoice (layout, prefix + "dest" + String (i), kControlDestChoices, kControlDestCount, 0);

    for (int i = 1; i <= 4; ++i)
        addInt (layout, prefix + "depth" + String (i), -63, 63, 0);
}

void addTone (Layout& layout, int toneNumber)
{
    const auto root = String ("tone") + String (toneNumber) + ".";

    addWg (layout, root + "wg.");
    addTvf (layout, root + "tvf.");
    addTva (layout, root + "tva.");
    addPEnv (layout, root + "pEnv.");
    addPan (layout, root + "pan.");
    addOutput (layout, root + "output.");
    addLfo (layout, root + "lfo1.");
    addLfo (layout, root + "lfo2.");
    addControl (layout, root + "ctrl1.");
    addControl (layout, root + "ctrl2.");
    addControl (layout, root + "ctrl3.");
}
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    addPatchCommon (layout);

    for (int toneNumber = 1; toneNumber <= kNumTones; ++toneNumber)
        addTone (layout, toneNumber);

    return layout;
}
