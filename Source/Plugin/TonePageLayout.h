#pragma once

// Pure layout table for the single-tone edit page (M2-09). Data only: TonePage
// builds its controls from this table, and the unit tests check that it covers
// every registered tone1.* parameter exactly once with a matching control kind.
// Deliberately free of JUCE includes so the table is testable without a GUI
// (CI has no display environment).

namespace TonePage
{
enum class ControlKind { Slider, Combo, Toggle };
enum class SubPage { Wg, Tvf, Tva, Lfo, Env, Ctrl };

struct Entry
{
    const char* suffix;   // parameter ID after the "tone1." prefix
    SubPage subPage;
    ControlKind kind;
};

inline constexpr Entry kTone1Layout[] =
{
    // WG: 23
    { "wg.toneSwitch", SubPage::Wg, ControlKind::Toggle },
    { "wg.waveGain", SubPage::Wg, ControlKind::Combo },
    { "wg.fxm.switch", SubPage::Wg, ControlKind::Toggle },
    { "wg.fxm.color", SubPage::Wg, ControlKind::Slider },
    { "wg.fxm.depth", SubPage::Wg, ControlKind::Slider },
    { "wg.toneDelay.mode", SubPage::Wg, ControlKind::Combo },
    { "wg.toneDelay.time", SubPage::Wg, ControlKind::Slider },
    { "wg.velXfade", SubPage::Wg, ControlKind::Slider },
    { "wg.velLow", SubPage::Wg, ControlKind::Slider },
    { "wg.velHigh", SubPage::Wg, ControlKind::Slider },
    { "wg.keyLow", SubPage::Wg, ControlKind::Slider },
    { "wg.keyHigh", SubPage::Wg, ControlKind::Slider },
    { "wg.redamper", SubPage::Wg, ControlKind::Toggle },
    { "wg.volCtrl", SubPage::Wg, ControlKind::Toggle },
    { "wg.holdCtrl", SubPage::Wg, ControlKind::Toggle },
    { "wg.bendCtrl", SubPage::Wg, ControlKind::Toggle },
    { "wg.panCtrl", SubPage::Wg, ControlKind::Toggle },
    { "wg.coarseTune", SubPage::Wg, ControlKind::Slider },
    { "wg.fineTune", SubPage::Wg, ControlKind::Slider },
    { "wg.randomPitch", SubPage::Wg, ControlKind::Slider },
    { "wg.pitchKeyfollow", SubPage::Wg, ControlKind::Slider },
    { "wg.pitchLfo1Depth", SubPage::Wg, ControlKind::Slider },
    { "wg.pitchLfo2Depth", SubPage::Wg, ControlKind::Slider },

    // TVF: 7 (the fEnv group lives on the ENV subpage)
    { "tvf.type", SubPage::Tvf, ControlKind::Combo },
    { "tvf.cutoff", SubPage::Tvf, ControlKind::Slider },
    { "tvf.cutoffKeyfollow", SubPage::Tvf, ControlKind::Slider },
    { "tvf.resonance", SubPage::Tvf, ControlKind::Slider },
    { "tvf.resVelSens", SubPage::Tvf, ControlKind::Slider },
    { "tvf.lfo1Depth", SubPage::Tvf, ControlKind::Slider },
    { "tvf.lfo2Depth", SubPage::Tvf, ControlKind::Slider },

    // TVA: 14 (tva without aEnv, plus pan and output)
    { "tva.level", SubPage::Tva, ControlKind::Slider },
    { "tva.bias.direction", SubPage::Tva, ControlKind::Combo },
    { "tva.bias.point", SubPage::Tva, ControlKind::Slider },
    { "tva.bias.level", SubPage::Tva, ControlKind::Slider },
    { "tva.lfo1Depth", SubPage::Tva, ControlKind::Slider },
    { "tva.lfo2Depth", SubPage::Tva, ControlKind::Slider },
    { "pan.position", SubPage::Tva, ControlKind::Slider },
    { "pan.keyfollow", SubPage::Tva, ControlKind::Slider },
    { "pan.random", SubPage::Tva, ControlKind::Slider },
    { "pan.alt", SubPage::Tva, ControlKind::Slider },
    { "pan.lfo1Depth", SubPage::Tva, ControlKind::Slider },
    { "pan.lfo2Depth", SubPage::Tva, ControlKind::Slider },
    { "output.assign", SubPage::Tva, ControlKind::Combo },
    { "output.level", SubPage::Tva, ControlKind::Slider },

    // LFO: 16
    { "lfo1.wave", SubPage::Lfo, ControlKind::Combo },
    { "lfo1.keyTrig", SubPage::Lfo, ControlKind::Toggle },
    { "lfo1.rate", SubPage::Lfo, ControlKind::Slider },
    { "lfo1.levelOffset", SubPage::Lfo, ControlKind::Slider },
    { "lfo1.delayTime", SubPage::Lfo, ControlKind::Slider },
    { "lfo1.fadeMode", SubPage::Lfo, ControlKind::Combo },
    { "lfo1.fadeTime", SubPage::Lfo, ControlKind::Slider },
    { "lfo1.sync", SubPage::Lfo, ControlKind::Toggle },
    { "lfo2.wave", SubPage::Lfo, ControlKind::Combo },
    { "lfo2.keyTrig", SubPage::Lfo, ControlKind::Toggle },
    { "lfo2.rate", SubPage::Lfo, ControlKind::Slider },
    { "lfo2.levelOffset", SubPage::Lfo, ControlKind::Slider },
    { "lfo2.delayTime", SubPage::Lfo, ControlKind::Slider },
    { "lfo2.fadeMode", SubPage::Lfo, ControlKind::Combo },
    { "lfo2.fadeTime", SubPage::Lfo, ControlKind::Slider },
    { "lfo2.sync", SubPage::Lfo, ControlKind::Toggle },

    // ENV: 39 (pEnv 13 + tvf.fEnv 14 + tva.aEnv 12)
    { "pEnv.depth", SubPage::Env, ControlKind::Slider },
    { "pEnv.velSens", SubPage::Env, ControlKind::Slider },
    { "pEnv.timeKeyfollow", SubPage::Env, ControlKind::Slider },
    { "pEnv.time1", SubPage::Env, ControlKind::Slider },
    { "pEnv.time2", SubPage::Env, ControlKind::Slider },
    { "pEnv.time3", SubPage::Env, ControlKind::Slider },
    { "pEnv.time4", SubPage::Env, ControlKind::Slider },
    { "pEnv.level1", SubPage::Env, ControlKind::Slider },
    { "pEnv.level2", SubPage::Env, ControlKind::Slider },
    { "pEnv.level3", SubPage::Env, ControlKind::Slider },
    { "pEnv.level4", SubPage::Env, ControlKind::Slider },
    { "pEnv.velTime1Sens", SubPage::Env, ControlKind::Slider },
    { "pEnv.velTime4Sens", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.depth", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.velCurve", SubPage::Env, ControlKind::Combo },
    { "tvf.fEnv.velSens", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.timeKeyfollow", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.time1", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.time2", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.time3", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.time4", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.level1", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.level2", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.level3", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.level4", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.velTime1Sens", SubPage::Env, ControlKind::Slider },
    { "tvf.fEnv.velTime4Sens", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.velCurve", SubPage::Env, ControlKind::Combo },
    { "tva.aEnv.velSens", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.time1", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.time2", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.time3", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.time4", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.level1", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.level2", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.level3", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.timeKeyfollow", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.velTime1Sens", SubPage::Env, ControlKind::Slider },
    { "tva.aEnv.velTime4Sens", SubPage::Env, ControlKind::Slider },

    // CTRL: 24
    { "ctrl1.dest1", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl1.dest2", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl1.dest3", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl1.dest4", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl1.depth1", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl1.depth2", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl1.depth3", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl1.depth4", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl2.dest1", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl2.dest2", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl2.dest3", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl2.dest4", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl2.depth1", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl2.depth2", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl2.depth3", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl2.depth4", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl3.dest1", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl3.dest2", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl3.dest3", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl3.dest4", SubPage::Ctrl, ControlKind::Combo },
    { "ctrl3.depth1", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl3.depth2", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl3.depth3", SubPage::Ctrl, ControlKind::Slider },
    { "ctrl3.depth4", SubPage::Ctrl, ControlKind::Slider },
};
}
