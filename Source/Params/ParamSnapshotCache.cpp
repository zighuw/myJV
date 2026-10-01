#include "ParamSnapshotCache.h"

#include <cmath>
#include <iterator>

namespace
{
using juce::AudioProcessorValueTreeState;

// One binding per registered parameter, in parameterIds() order. The suffix is
// appended to "patch.common." (common) or "toneN." (tones). Types follow the
// registry: Float -> float, Int/Choice -> int, Bool -> bool (>= 0.5 is on).
using CommonWriter = void (*) (PatchCommonSnapshot&, float) noexcept;
using ToneWriter = void (*) (ToneSnapshot&, float) noexcept;

struct CommonBinding
{
    const char* suffix;
    CommonWriter apply;
};

struct ToneBinding
{
    const char* suffix;
    ToneWriter apply;
};

const CommonBinding kCommonBindings[]
{
    { "level", [] (PatchCommonSnapshot& s, float v) noexcept { s.level = v; } },
    { "pan", [] (PatchCommonSnapshot& s, float v) noexcept { s.pan = v; } },
    { "analogFeel", [] (PatchCommonSnapshot& s, float v) noexcept { s.analogFeel = v; } },
    { "bendUp", [] (PatchCommonSnapshot& s, float v) noexcept { s.bendUp = (int) std::lround (v); } },
    { "bendDown", [] (PatchCommonSnapshot& s, float v) noexcept { s.bendDown = (int) std::lround (v); } },
    { "octaveShift", [] (PatchCommonSnapshot& s, float v) noexcept { s.octaveShift = (int) std::lround (v); } },
    { "stretchTune", [] (PatchCommonSnapshot& s, float v) noexcept { s.stretchTune = (int) std::lround (v); } },
    { "keyAssign", [] (PatchCommonSnapshot& s, float v) noexcept { s.keyAssign = (int) std::lround (v); } },
    { "legato", [] (PatchCommonSnapshot& s, float v) noexcept { s.legato = v >= 0.5f; } },
    { "portamento.switch", [] (PatchCommonSnapshot& s, float v) noexcept { s.portamentoSwitch = v >= 0.5f; } },
    { "portamento.mode", [] (PatchCommonSnapshot& s, float v) noexcept { s.portamentoMode = (int) std::lround (v); } },
    { "portamento.type", [] (PatchCommonSnapshot& s, float v) noexcept { s.portamentoType = (int) std::lround (v); } },
    { "portamento.start", [] (PatchCommonSnapshot& s, float v) noexcept { s.portamentoStart = (int) std::lround (v); } },
    { "portamento.time", [] (PatchCommonSnapshot& s, float v) noexcept { s.portamentoTime = v; } },
    { "voicePriority", [] (PatchCommonSnapshot& s, float v) noexcept { s.voicePriority = (int) std::lround (v); } },
    { "structure12", [] (PatchCommonSnapshot& s, float v) noexcept { s.structure12 = (int) std::lround (v); } },
    { "booster12", [] (PatchCommonSnapshot& s, float v) noexcept { s.booster12 = v; } },
    { "structure34", [] (PatchCommonSnapshot& s, float v) noexcept { s.structure34 = (int) std::lround (v); } },
    { "booster34", [] (PatchCommonSnapshot& s, float v) noexcept { s.booster34 = v; } },
    { "ctrlSource2", [] (PatchCommonSnapshot& s, float v) noexcept { s.ctrlSource2 = (int) std::lround (v); } },
    { "ctrlSource3", [] (PatchCommonSnapshot& s, float v) noexcept { s.ctrlSource3 = (int) std::lround (v); } },
    { "controlHoldPeak", [] (PatchCommonSnapshot& s, float v) noexcept { s.controlHoldPeak = (int) std::lround (v); } },
    { "ctrl1HoldPeak", [] (PatchCommonSnapshot& s, float v) noexcept { s.ctrl1HoldPeak = (int) std::lround (v); } },
    { "ctrl2HoldPeak", [] (PatchCommonSnapshot& s, float v) noexcept { s.ctrl2HoldPeak = (int) std::lround (v); } },
    { "ctrl3HoldPeak", [] (PatchCommonSnapshot& s, float v) noexcept { s.ctrl3HoldPeak = (int) std::lround (v); } },
    { "defaultTempo", [] (PatchCommonSnapshot& s, float v) noexcept { s.defaultTempo = (int) std::lround (v); } },
};

const ToneBinding kToneBindings[]
{
    // WG
    { "wg.toneSwitch", [] (ToneSnapshot& s, float v) noexcept { s.wg.toneSwitch = v >= 0.5f; } },
    { "wg.waveGain", [] (ToneSnapshot& s, float v) noexcept { s.wg.waveGain = (int) std::lround (v); } },
    { "wg.fxm.switch", [] (ToneSnapshot& s, float v) noexcept { s.wg.fxmOn = v >= 0.5f; } },
    { "wg.fxm.color", [] (ToneSnapshot& s, float v) noexcept { s.wg.fxmColor = (int) std::lround (v); } },
    { "wg.fxm.depth", [] (ToneSnapshot& s, float v) noexcept { s.wg.fxmDepth = v; } },
    { "wg.toneDelay.mode", [] (ToneSnapshot& s, float v) noexcept { s.wg.toneDelayMode = (int) std::lround (v); } },
    { "wg.toneDelay.time", [] (ToneSnapshot& s, float v) noexcept { s.wg.toneDelayTime = v; } },
    { "wg.velXfade", [] (ToneSnapshot& s, float v) noexcept { s.wg.velXfade = v; } },
    { "wg.velLow", [] (ToneSnapshot& s, float v) noexcept { s.wg.velLow = (int) std::lround (v); } },
    { "wg.velHigh", [] (ToneSnapshot& s, float v) noexcept { s.wg.velHigh = (int) std::lround (v); } },
    { "wg.keyLow", [] (ToneSnapshot& s, float v) noexcept { s.wg.keyLow = (int) std::lround (v); } },
    { "wg.keyHigh", [] (ToneSnapshot& s, float v) noexcept { s.wg.keyHigh = (int) std::lround (v); } },
    { "wg.redamper", [] (ToneSnapshot& s, float v) noexcept { s.wg.redamper = v >= 0.5f; } },
    { "wg.volCtrl", [] (ToneSnapshot& s, float v) noexcept { s.wg.volCtrl = v >= 0.5f; } },
    { "wg.holdCtrl", [] (ToneSnapshot& s, float v) noexcept { s.wg.holdCtrl = v >= 0.5f; } },
    { "wg.bendCtrl", [] (ToneSnapshot& s, float v) noexcept { s.wg.bendCtrl = v >= 0.5f; } },
    { "wg.panCtrl", [] (ToneSnapshot& s, float v) noexcept { s.wg.panCtrl = v >= 0.5f; } },
    { "wg.coarseTune", [] (ToneSnapshot& s, float v) noexcept { s.wg.coarseTune = (int) std::lround (v); } },
    { "wg.fineTune", [] (ToneSnapshot& s, float v) noexcept { s.wg.fineTune = (int) std::lround (v); } },
    { "wg.randomPitch", [] (ToneSnapshot& s, float v) noexcept { s.wg.randomPitch = v; } },
    { "wg.pitchKeyfollow", [] (ToneSnapshot& s, float v) noexcept { s.wg.pitchKeyfollow = v; } },
    { "wg.pitchLfo1Depth", [] (ToneSnapshot& s, float v) noexcept { s.wg.pitchLfo1Depth = v; } },
    { "wg.pitchLfo2Depth", [] (ToneSnapshot& s, float v) noexcept { s.wg.pitchLfo2Depth = v; } },

    // TVF
    { "tvf.type", [] (ToneSnapshot& s, float v) noexcept { s.tvf.type = (int) std::lround (v); } },
    { "tvf.cutoff", [] (ToneSnapshot& s, float v) noexcept { s.tvf.cutoff = v; } },
    { "tvf.cutoffKeyfollow", [] (ToneSnapshot& s, float v) noexcept { s.tvf.cutoffKeyfollow = v; } },
    { "tvf.resonance", [] (ToneSnapshot& s, float v) noexcept { s.tvf.resonance = v; } },
    { "tvf.resVelSens", [] (ToneSnapshot& s, float v) noexcept { s.tvf.resVelSens = (int) std::lround (v); } },
    { "tvf.fEnv.depth", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.depth = (int) std::lround (v); } },
    { "tvf.fEnv.velCurve", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.velCurve = (int) std::lround (v); } },
    { "tvf.fEnv.velSens", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.velSens = (int) std::lround (v); } },
    { "tvf.fEnv.timeKeyfollow", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.timeKeyfollow = (int) std::lround (v); } },
    { "tvf.fEnv.time1", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.time[0] = v; } },
    { "tvf.fEnv.time2", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.time[1] = v; } },
    { "tvf.fEnv.time3", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.time[2] = v; } },
    { "tvf.fEnv.time4", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.time[3] = v; } },
    { "tvf.fEnv.level1", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.level[0] = v; } },
    { "tvf.fEnv.level2", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.level[1] = v; } },
    { "tvf.fEnv.level3", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.level[2] = v; } },
    { "tvf.fEnv.level4", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.level[3] = v; } },
    { "tvf.fEnv.velTime1Sens", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.velTime1Sens = (int) std::lround (v); } },
    { "tvf.fEnv.velTime4Sens", [] (ToneSnapshot& s, float v) noexcept { s.tvf.fEnv.velTime4Sens = (int) std::lround (v); } },
    { "tvf.lfo1Depth", [] (ToneSnapshot& s, float v) noexcept { s.tvf.lfo1Depth = v; } },
    { "tvf.lfo2Depth", [] (ToneSnapshot& s, float v) noexcept { s.tvf.lfo2Depth = v; } },

    // TVA
    { "tva.level", [] (ToneSnapshot& s, float v) noexcept { s.tva.level = v; } },
    { "tva.bias.direction", [] (ToneSnapshot& s, float v) noexcept { s.tva.biasDirection = (int) std::lround (v); } },
    { "tva.bias.point", [] (ToneSnapshot& s, float v) noexcept { s.tva.biasPoint = (int) std::lround (v); } },
    { "tva.bias.level", [] (ToneSnapshot& s, float v) noexcept { s.tva.biasLevel = (int) std::lround (v); } },
    { "tva.aEnv.velCurve", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.velCurve = (int) std::lround (v); } },
    { "tva.aEnv.velSens", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.velSens = (int) std::lround (v); } },
    { "tva.aEnv.time1", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.time[0] = v; } },
    { "tva.aEnv.time2", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.time[1] = v; } },
    { "tva.aEnv.time3", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.time[2] = v; } },
    { "tva.aEnv.time4", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.time[3] = v; } },
    { "tva.aEnv.level1", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.level[0] = v; } },
    { "tva.aEnv.level2", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.level[1] = v; } },
    { "tva.aEnv.level3", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.level[2] = v; } },
    { "tva.aEnv.timeKeyfollow", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.timeKeyfollow = (int) std::lround (v); } },
    { "tva.aEnv.velTime1Sens", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.velTime1Sens = (int) std::lround (v); } },
    { "tva.aEnv.velTime4Sens", [] (ToneSnapshot& s, float v) noexcept { s.tva.aEnv.velTime4Sens = (int) std::lround (v); } },
    { "tva.lfo1Depth", [] (ToneSnapshot& s, float v) noexcept { s.tva.lfo1Depth = v; } },
    { "tva.lfo2Depth", [] (ToneSnapshot& s, float v) noexcept { s.tva.lfo2Depth = v; } },

    // P-ENV
    { "pEnv.depth", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.depth = (int) std::lround (v); } },
    { "pEnv.velSens", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.velSens = (int) std::lround (v); } },
    { "pEnv.timeKeyfollow", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.timeKeyfollow = (int) std::lround (v); } },
    { "pEnv.time1", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.time[0] = v; } },
    { "pEnv.time2", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.time[1] = v; } },
    { "pEnv.time3", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.time[2] = v; } },
    { "pEnv.time4", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.time[3] = v; } },
    { "pEnv.level1", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.level[0] = v; } },
    { "pEnv.level2", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.level[1] = v; } },
    { "pEnv.level3", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.level[2] = v; } },
    { "pEnv.level4", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.level[3] = v; } },
    { "pEnv.velTime1Sens", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.velTime1Sens = (int) std::lround (v); } },
    { "pEnv.velTime4Sens", [] (ToneSnapshot& s, float v) noexcept { s.pEnv.velTime4Sens = (int) std::lround (v); } },

    // PAN
    { "pan.position", [] (ToneSnapshot& s, float v) noexcept { s.pan.position = v; } },
    { "pan.keyfollow", [] (ToneSnapshot& s, float v) noexcept { s.pan.keyfollow = (int) std::lround (v); } },
    { "pan.random", [] (ToneSnapshot& s, float v) noexcept { s.pan.random = v; } },
    { "pan.alt", [] (ToneSnapshot& s, float v) noexcept { s.pan.alt = v; } },
    { "pan.lfo1Depth", [] (ToneSnapshot& s, float v) noexcept { s.pan.lfo1Depth = v; } },
    { "pan.lfo2Depth", [] (ToneSnapshot& s, float v) noexcept { s.pan.lfo2Depth = v; } },

    // OUTPUT
    { "output.assign", [] (ToneSnapshot& s, float v) noexcept { s.output.assign = (int) std::lround (v); } },
    { "output.level", [] (ToneSnapshot& s, float v) noexcept { s.output.level = v; } },

    // LFO1
    { "lfo1.wave", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].wave = (int) std::lround (v); } },
    { "lfo1.keyTrig", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].keyTrig = v >= 0.5f; } },
    { "lfo1.rate", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].rate = v; } },
    { "lfo1.levelOffset", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].levelOffset = (int) std::lround (v); } },
    { "lfo1.delayTime", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].delayTime = v; } },
    { "lfo1.fadeMode", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].fadeMode = (int) std::lround (v); } },
    { "lfo1.fadeTime", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].fadeTime = v; } },
    { "lfo1.sync", [] (ToneSnapshot& s, float v) noexcept { s.lfo[0].sync = v >= 0.5f; } },

    // LFO2
    { "lfo2.wave", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].wave = (int) std::lround (v); } },
    { "lfo2.keyTrig", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].keyTrig = v >= 0.5f; } },
    { "lfo2.rate", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].rate = v; } },
    { "lfo2.levelOffset", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].levelOffset = (int) std::lround (v); } },
    { "lfo2.delayTime", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].delayTime = v; } },
    { "lfo2.fadeMode", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].fadeMode = (int) std::lround (v); } },
    { "lfo2.fadeTime", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].fadeTime = v; } },
    { "lfo2.sync", [] (ToneSnapshot& s, float v) noexcept { s.lfo[1].sync = v >= 0.5f; } },

    // CTRL1..3
    { "ctrl1.dest1", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].dest[0] = (int) std::lround (v); } },
    { "ctrl1.dest2", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].dest[1] = (int) std::lround (v); } },
    { "ctrl1.dest3", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].dest[2] = (int) std::lround (v); } },
    { "ctrl1.dest4", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].dest[3] = (int) std::lround (v); } },
    { "ctrl1.depth1", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].depth[0] = (int) std::lround (v); } },
    { "ctrl1.depth2", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].depth[1] = (int) std::lround (v); } },
    { "ctrl1.depth3", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].depth[2] = (int) std::lround (v); } },
    { "ctrl1.depth4", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[0].depth[3] = (int) std::lround (v); } },
    { "ctrl2.dest1", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].dest[0] = (int) std::lround (v); } },
    { "ctrl2.dest2", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].dest[1] = (int) std::lround (v); } },
    { "ctrl2.dest3", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].dest[2] = (int) std::lround (v); } },
    { "ctrl2.dest4", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].dest[3] = (int) std::lround (v); } },
    { "ctrl2.depth1", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].depth[0] = (int) std::lround (v); } },
    { "ctrl2.depth2", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].depth[1] = (int) std::lround (v); } },
    { "ctrl2.depth3", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].depth[2] = (int) std::lround (v); } },
    { "ctrl2.depth4", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[1].depth[3] = (int) std::lround (v); } },
    { "ctrl3.dest1", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].dest[0] = (int) std::lround (v); } },
    { "ctrl3.dest2", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].dest[1] = (int) std::lround (v); } },
    { "ctrl3.dest3", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].dest[2] = (int) std::lround (v); } },
    { "ctrl3.dest4", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].dest[3] = (int) std::lround (v); } },
    { "ctrl3.depth1", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].depth[0] = (int) std::lround (v); } },
    { "ctrl3.depth2", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].depth[1] = (int) std::lround (v); } },
    { "ctrl3.depth3", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].depth[2] = (int) std::lround (v); } },
    { "ctrl3.depth4", [] (ToneSnapshot& s, float v) noexcept { s.ctrl[2].depth[3] = (int) std::lround (v); } },
};

static_assert (std::size (kCommonBindings) == 26, "patch.common must bind all 26 parameters");
static_assert (std::size (kToneBindings) == 123, "each tone must bind all 123 parameters");
}

ParamSnapshotCache ParamSnapshotCache::fromApvts (AudioProcessorValueTreeState& apvts)
{
    ParamSnapshotCache cache;

    for (int i = 0; i < kPatchCommonParameterCount; ++i)
    {
        const auto id = juce::String ("patch.common.") + kCommonBindings[i].suffix;
        cache.commonPointers[i] = apvts.getRawParameterValue (id);
        jassert (cache.commonPointers[i] != nullptr);
    }

    for (int tone = 0; tone < kToneCount; ++tone)
    {
        for (int i = 0; i < kToneParameterCount; ++i)
        {
            const auto id = "tone" + juce::String (tone + 1) + "." + kToneBindings[i].suffix;
            cache.tonePointers[tone][i] = apvts.getRawParameterValue (id);
            jassert (cache.tonePointers[tone][i] != nullptr);
        }
    }

    return cache;
}

ParamSnapshotCache ParamSnapshotCache::fromPointers (const std::vector<std::atomic<float>*>& pointers)
{
    ParamSnapshotCache cache;

    jassert (pointers.size() == (std::size_t) totalParameterCount());

    if (pointers.size() != (std::size_t) totalParameterCount())
        return cache;

    std::size_t index = 0;

    for (int i = 0; i < kPatchCommonParameterCount; ++i)
        cache.commonPointers[i] = pointers[index++];

    for (int tone = 0; tone < kToneCount; ++tone)
        for (int i = 0; i < kToneParameterCount; ++i)
            cache.tonePointers[tone][i] = pointers[index++];

    return cache;
}

int ParamSnapshotCache::totalParameterCount() noexcept
{
    return kPatchCommonParameterCount + kToneCount * kToneParameterCount;
}

std::vector<std::string> ParamSnapshotCache::parameterIds()
{
    std::vector<std::string> ids;
    ids.reserve ((std::size_t) totalParameterCount());

    for (const auto& binding : kCommonBindings)
        ids.emplace_back (std::string ("patch.common.") + binding.suffix);

    for (int tone = 0; tone < kToneCount; ++tone)
        for (const auto& binding : kToneBindings)
            ids.emplace_back ("tone" + std::to_string (tone + 1) + "." + binding.suffix);

    return ids;
}

int ParamSnapshotCache::resolvedCount() const noexcept
{
    int count = 0;

    for (const auto* pointer : commonPointers)
        count += pointer != nullptr ? 1 : 0;

    for (const auto& tone : tonePointers)
        for (const auto* pointer : tone)
            count += pointer != nullptr ? 1 : 0;

    return count;
}

// RT-safe
void ParamSnapshotCache::refresh (const PatchRuntime& runtime) const noexcept
{
    auto& snapshot = runtime.snapshot;

    for (int i = 0; i < kPatchCommonParameterCount; ++i)
        if (const auto* value = commonPointers[i])
            kCommonBindings[i].apply (snapshot.common, value->load (std::memory_order_relaxed));

    for (int tone = 0; tone < kToneCount; ++tone)
    {
        for (int i = 0; i < kToneParameterCount; ++i)
            if (const auto* value = tonePointers[tone][i])
                kToneBindings[i].apply (snapshot.tones[tone], value->load (std::memory_order_relaxed));

        snapshot.zoneSets[tone] = runtime.rawZoneSets[tone];
    }

    snapshot.assetId = runtime.id;
}
