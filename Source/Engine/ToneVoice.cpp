#include "ToneVoice.h"

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"   // BusBuffers / kNumOutputBuses
#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979323846f;

double cutoffParamToHz (float cutoffParam) noexcept
{
    const auto value = std::clamp (cutoffParam, 0.0f, 127.0f);
    return Calibration::kCutoffMinHz
           * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz, (double) value / 127.0);
}

double resonanceParamToQ (float resonanceParam) noexcept
{
    const auto value = std::clamp (resonanceParam, 0.0f, 127.0f);
    return 0.5 * std::pow (Calibration::kResonanceMaxQ / 0.5, (double) value / 127.0);
}

EnvelopeSettings makePEnvSettings (const PEnvSnapshot& source) noexcept
{
    EnvelopeSettings settings;
    settings.velocitySens = source.velSens;
    settings.timeKeyfollow = source.timeKeyfollow;
    settings.velTime1Sens = source.velTime1Sens;
    settings.velTime4Sens = source.velTime4Sens;

    for (int i = 0; i < 4; ++i)
    {
        settings.time[i] = source.time[i];
        settings.level[i] = source.level[i];
    }

    return settings;
}

EnvelopeSettings makeFEnvSettings (const FEnvSnapshot& source) noexcept
{
    EnvelopeSettings settings;
    settings.velocityCurve = source.velCurve;
    settings.velocitySens = source.velSens;
    settings.timeKeyfollow = source.timeKeyfollow;
    settings.velTime1Sens = source.velTime1Sens;
    settings.velTime4Sens = source.velTime4Sens;

    for (int i = 0; i < 4; ++i)
    {
        settings.time[i] = source.time[i];
        settings.level[i] = source.level[i];
    }

    return settings;
}

EnvelopeSettings makeAEnvSettings (const AEnvSnapshot& source) noexcept
{
    EnvelopeSettings settings;
    settings.velocityCurve = source.velCurve;
    settings.velocitySens = source.velSens;
    settings.timeKeyfollow = source.timeKeyfollow;
    settings.velTime1Sens = source.velTime1Sens;
    settings.velTime4Sens = source.velTime4Sens;

    for (int i = 0; i < 4; ++i)
        settings.time[i] = source.time[i];

    for (int i = 0; i < 3; ++i)
        settings.level[i] = source.level[i];

    settings.level[3] = 0.0f;   // A-ENV level 4 is fixed at zero

    return settings;
}

LfoSettings makeLfoSettings (const LfoSnapshot& source) noexcept
{
    LfoSettings settings;
    settings.wave = source.wave;
    settings.keyTrig = source.keyTrig;
    settings.rate = source.rate;
    settings.levelOffset = source.levelOffset;
    settings.delayTime = source.delayTime;
    settings.fadeMode = source.fadeMode;
    settings.fadeTime = source.fadeTime;
    settings.sync = source.sync;
    return settings;
}
}

void ToneVoice::prepare (double engineSampleRate) noexcept
{
    sampleRate = engineSampleRate > 0.0 ? engineSampleRate : 48000.0;
    filter.prepare (sampleRate);
    peaking.prepare (sampleRate);
    pEnv.prepare (sampleRate);
    fEnv.prepare (sampleRate);
    aEnv.prepare (sampleRate);
    lfo[0].prepare (sampleRate);
    lfo[1].prepare (sampleRate);
    matrix.prepare (sampleRate);
    levelSmoother.prepare (sampleRate, Calibration::kLevelSmoothingMs);
    outputSmoother.prepare (sampleRate, Calibration::kOutputLevelSmoothingMs);
    panSmoother.prepare (sampleRate, Calibration::kPanSmoothingMs);
    cutoffSmoother.prepare (sampleRate, Calibration::kCutoffSmoothingMs);
    resonanceSmoother.prepare (sampleRate, Calibration::kResonanceSmoothingMs);
    reset();
}

// RT-safe
void ToneVoice::reset() noexcept
{
    voiceState = State::Free;
    runtime = nullptr;
    player.stop();
    filter.reset();
    peaking.reset();
    fxm.reset();
    matrix.reset();
    levelSmoother.reset();
    outputSmoother.reset();
    panSmoother.reset();
    cutoffSmoother.reset();
    resonanceSmoother.reset();
    killGain = 1.0f;
    killStep = 0.0f;
    pitchOffset = 0.0f;
    activeGain = 0.0f;
    delayStarted = true;
    startOnNoteOff = false;
    delayRemaining = 0;
    holdDelaySamples = 0;
    endFadeRemaining = 0;
    endFadeTotal = 0;
    ending = false;
    lastWgSample = 0.0f;
}

// RT-safe
void ToneVoice::startNote (const PatchRuntime* newRuntime, int newToneIndex, int midiNote,
                           float newVelocity, std::uint64_t rngSeed, float keyIntervalScale) noexcept
{
    voiceState = State::Free;
    player.stop();

    runtime = newRuntime;
    toneIndex = std::clamp (newToneIndex, 0, PatchSnapshot::kToneCount - 1);
    note = std::clamp (midiNote, 0, 127);
    velocity = std::clamp (newVelocity, 0.0f, 1.0f);

    if (runtime == nullptr)
        return;

    const auto& tone = runtime->snapshot.tones[toneIndex];
    const auto* zoneSet = runtime->rawZoneSets[toneIndex];

    if (! tone.wg.toneSwitch || zoneSet == nullptr)
        return;

    const auto midiVelocity = std::clamp ((int) std::lround (velocity * 127.0f), 1, 127);
    const auto* zone = selectZone (*zoneSet, note, midiVelocity,
                                   tone.wg.keyLow, tone.wg.keyHigh, tone.wg.velLow, tone.wg.velHigh);

    if (zone == nullptr || zone->sample == nullptr)
        return;

    player.start (*zone->sample, *zone, note, sampleRate);
    player.setPitchOffsetSemitones (0.0f);
    rootKey = zone->rootKeyOverride >= 0 ? zone->rootKeyOverride : zone->sample->rootKey;

    DeterministicRandom noteRandom (rngSeed);
    randomPitchSemitones = (float) Calibration::kRandomPitchSemitones
                           * (tone.wg.randomPitch / 127.0f) * noteRandom.nextBipolar();
    randomPan = Calibration::kRandomPan * (tone.pan.random / 127.0f) * noteRandom.nextBipolar();

    const auto tempo = runtime->snapshot.common.defaultTempo > 0
                           ? (double) runtime->snapshot.common.defaultTempo : 120.0;

    pEnv.start (makePEnvSettings (tone.pEnv), note, velocity);
    fEnv.start (makeFEnvSettings (tone.tvf.fEnv), note, velocity);
    aEnv.start (makeAEnvSettings (tone.tva.aEnv), note, velocity);
    lfo[0].start (makeLfoSettings (tone.lfo[0]), rngSeed, tempo);
    lfo[1].start (makeLfoSettings (tone.lfo[1]), rngSeed ^ 0x9E3779B97F4A7C15ull, tempo);

    killGain = 1.0f;
    killStep = 0.0f;
    pitchOffset = 0.0f;
    filterControlCounter = 0;
    activeGain = 1.0f;
    fxm.reset();
    lastWgSample = 0.0f;
    ending = false;
    endFadeRemaining = 0;
    endFadeTotal = 0;

    // Tone Delay (architecture 5.11, basic semantics; M4-06 refines). Time 0
    // means "no delay" (the 1 ms floor only applies once a delay is set).
    auto baseDelaySamples = 0;

    if (tone.wg.toneDelayTime > 0.0f)
    {
        const auto delayMs = Calibration::kToneDelayMinMs
                             * std::pow (Calibration::kToneDelayMaxMs / Calibration::kToneDelayMinMs,
                                         (double) std::clamp (tone.wg.toneDelayTime, 0.0f, 127.0f) / 127.0);
        baseDelaySamples = (int) std::lround (delayMs * sampleRate / 1000.0);
    }

    holdDelaySamples = baseDelaySamples;

    if (tone.wg.toneDelayMode == 1 && baseDelaySamples > 0)   // HOLD: start on note-off
    {
        delayStarted = false;
        startOnNoteOff = true;
        delayRemaining = 0;
    }
    else
    {
        startOnNoteOff = false;
        delayStarted = true;
        const auto scale = tone.wg.toneDelayMode == 2
                               ? std::clamp (keyIntervalScale, Calibration::kKeyIntervalScaleMin,
                                             Calibration::kKeyIntervalScaleMax)
                               : 1.0f;
        delayRemaining = (int) std::lround ((double) baseDelaySamples * (double) scale);
    }

    voiceState = State::Active;
    beginBlock();
    snapSmoothers();
}

// RT-safe
void ToneVoice::release() noexcept
{
    if (voiceState == State::Free || voiceState == State::KillFading)
        return;

    if (startOnNoteOff && ! delayStarted)   // HOLD: the note-off starts the countdown
    {
        delayStarted = true;
        delayRemaining = holdDelaySamples;
        return;
    }

    if (delayRemaining > 0)   // NORMAL / KEY INTERVAL cancel before sounding
    {
        player.stop();
        voiceState = State::Free;
        activeGain = 0.0f;
        return;
    }

    if (voiceState == State::Active)
    {
        aEnv.release();
        pEnv.release();   // G1 fix (M2-F01): P/F envelopes follow T4 -> L4 on note-off
        fEnv.release();
        voiceState = State::Releasing;
    }
}

// RT-safe
void ToneVoice::kill() noexcept
{
    if (voiceState == State::Free)
        return;

    voiceState = State::KillFading;
    killGain = 1.0f;

    const auto fadeSamples = std::max (1.0, Calibration::kKillFadeMs * sampleRate / 1000.0);
    killStep = (float) (1.0 / fadeSamples);
}

// RT-safe
void ToneVoice::setModulationInput (const ModulationInput& input, int blockSamples) noexcept
{
    modulationInput = input;
    modulationBlockSamples = std::max (0, blockSamples);
}

// RT-safe
void ToneVoice::beginBlock() noexcept
{
    if (voiceState == State::Free || runtime == nullptr)
        return;

    const auto& tone = runtime->snapshot.tones[toneIndex];

    matrix.update (modulationInput, modulationBlockSamples);
    block.mod = matrix.evaluate (tone.ctrl);

    block.filterType = tone.tvf.type;
    block.baseCutoffHz = (float) cutoffParamToHz (tone.tvf.cutoff);
    block.baseQ = (float) resonanceParamToQ (tone.tvf.resonance);
    block.pkgGainDb = (tone.tvf.resonance / 127.0f) * (float) Calibration::kPkgGainDb;
    block.keyfollowOctaves = (tone.tvf.cutoffKeyfollow / 100.0f) * (float) (note - 60) / 12.0f;
    block.fEnvDepth = (tone.tvf.fEnv.depth / 63.0f) * (float) Calibration::kFEnvDepthOctaves;
    block.lfoCutoffDepth[0] = (tone.tvf.lfo1Depth / 127.0f) * (float) Calibration::kLfoFilterDepthOctaves;
    block.lfoCutoffDepth[1] = (tone.tvf.lfo2Depth / 127.0f) * (float) Calibration::kLfoFilterDepthOctaves;

    block.toneLevel = tone.tva.level * Calibration::kToneOutputLevelScale;
    block.panBase = tone.pan.position / 64.0f - 1.0f;
    block.panKey = (tone.pan.keyfollow / 63.0f) * (float) (note - 60) / 12.0f;
    block.panLfoDepth[0] = tone.pan.lfo1Depth / 127.0f;
    block.panLfoDepth[1] = tone.pan.lfo2Depth / 127.0f;
    block.outputAssign = tone.output.assign;
    block.outputLevel = tone.output.level * Calibration::kToneOutputLevelScale;

    block.pitchKeyfollowScale = (tone.wg.pitchKeyfollow / 100.0f - 1.0f) * (float) (note - rootKey);
    block.pEnvDepthSemis = (tone.pEnv.depth / 63.0f) * (float) Calibration::kPEnvDepthSemitones;
    block.pitchLfoDepth[0] = (tone.wg.pitchLfo1Depth / 127.0f) * (float) Calibration::kLfoPitchSemitones;
    block.pitchLfoDepth[1] = (tone.wg.pitchLfo2Depth / 127.0f) * (float) Calibration::kLfoPitchSemitones;

    block.waveGain = std::pow (10.0f, Calibration::kWaveGainDb[std::clamp (tone.wg.waveGain, 0, 3)] / 20.0f);
    block.fxmOn = tone.wg.fxmOn;
    block.fxmColor = tone.wg.fxmColor;
    block.fxmDepth = tone.wg.fxmDepth;
    fxm.setParameters (block.fxmOn, block.fxmColor, block.fxmDepth);

    for (int i = 0; i < 2; ++i)
    {
        block.pitchLfoDepth[i] *= block.mod.lfoPitchDepthScale[i];
        block.lfoCutoffDepth[i] *= block.mod.lfoCutoffDepthScale[i];
        block.panLfoDepth[i] *= block.mod.lfoPanDepthScale[i];
        block.tvaLfoDepth[i] = (i == 0 ? tone.tva.lfo1Depth : tone.tva.lfo2Depth)
                               / 127.0f * (float) Calibration::kLfoAmpDepth;
        lfo[i].setRateMultiplier (std::exp2 (block.mod.lfoRateOctaves[i]));
    }

    // Block-rate component targets for the parameter smoothers (architecture
    // 4.3). LFO/ENV modulation stays outside the smoothers.
    levelSmoother.setTarget (block.toneLevel
                             * std::pow (10.0f, block.mod.levelDb / 20.0f)
                             * block.mod.envLevelScale[2]);
    outputSmoother.setTarget (block.outputLevel);
    panSmoother.setTarget (std::clamp (block.panBase + block.panKey + randomPan + block.mod.pan,
                                       -1.0f, 1.0f));
    cutoffSmoother.setTarget (block.baseCutoffHz
                              * std::exp2 (block.keyfollowOctaves + block.mod.cutoffOctaves));
    resonanceSmoother.setTarget (block.baseQ * std::exp2 (block.mod.resonanceOctaves));
}

// RT-safe
void ToneVoice::snapSmoothers() noexcept
{
    levelSmoother.snapToTarget();
    outputSmoother.snapToTarget();
    panSmoother.snapToTarget();
    cutoffSmoother.snapToTarget();
    resonanceSmoother.snapToTarget();
}

// RT-safe
void ToneVoice::updateModulators() noexcept
{
    ++modulatorCalls;

    if (voiceState == State::Free)
        return;

    if (startOnNoteOff && ! delayStarted)   // HOLD waiting for note-off
    {
        activeGain = 0.0f;
        return;
    }

    if (delayRemaining > 0)
    {
        --delayRemaining;
        activeGain = 0.0f;

        if (voiceState == State::KillFading)
        {
            killGain = std::max (0.0f, killGain - killStep);

            if (killGain <= 0.0f)
            {
                player.stop();
                voiceState = State::Free;
            }
        }

        return;
    }

    lfo[0].process();
    lfo[1].process();
    pEnv.process();
    fEnv.process();
    aEnv.process();

    if (voiceState == State::Releasing && aEnv.isFinished())
    {
        player.stop();
        voiceState = State::Free;
        activeGain = 0.0f;
        return;
    }

    if (ending)
    {
        if (endFadeRemaining > 0)
            --endFadeRemaining;

        activeGain = aEnv.level() * block.toneLevel * killGain
                     * ((float) endFadeRemaining / (float) endFadeTotal);

        if (endFadeRemaining <= 0)
        {
            player.stop();
            voiceState = State::Free;
            activeGain = 0.0f;
        }

        return;
    }

    if (voiceState == State::Active && player.hasFinished())
    {
        // Natural one-shot end: hold the last sample and fade it out before
        // freeing the voice (architecture 5.4, no end click).
        ending = true;
        endFadeTotal = std::max (1, (int) std::lround (Calibration::kNoteEndFadeMs * sampleRate / 1000.0));
        endFadeRemaining = endFadeTotal;
        return;
    }

    if (voiceState == State::KillFading)
    {
        killGain = std::max (0.0f, killGain - killStep);

        if (killGain <= 0.0f)
        {
            player.stop();
            voiceState = State::Free;
            activeGain = 0.0f;
            return;
        }
    }

    levelSmoother.process();
    outputSmoother.process();
    panSmoother.process();
    cutoffSmoother.process();
    resonanceSmoother.process();

    pitchOffset = block.pitchKeyfollowScale
                  + randomPitchSemitones
                  + block.mod.pitchSemitones
                  + block.pEnvDepthSemis * pEnv.level() * block.mod.envLevelScale[0]
                  + block.pitchLfoDepth[0] * lfo[0].value()
                  + block.pitchLfoDepth[1] * lfo[1].value();

    player.setPitchOffsetSemitones (pitchOffset);

    if (filterControlCounter <= 0)
    {
        updateFilterCoefficients();
        filterControlCounter = Calibration::kFilterControlRate;
    }

    --filterControlCounter;

    auto gain = aEnv.level() * levelSmoother.current() * killGain;

    for (int i = 0; i < 2; ++i)
        gain *= std::clamp (1.0f + block.tvaLfoDepth[i] * lfo[i].value() * block.mod.lfoAmpDepthScale[i],
                            0.0f, 2.0f);

    activeGain = gain;
}

// RT-safe
float ToneVoice::processWG() noexcept
{
    if (voiceState == State::Free)
        return 0.0f;

    if (startOnNoteOff && ! delayStarted)
        return 0.0f;

    if (delayRemaining > 0)
        return 0.0f;

    if (player.hasFinished())
        return ending ? lastWgSample : 0.0f;

    player.setPhaseModulation (fxm.process (lastWgSample));

    const auto sample = player.getNextSample() * block.waveGain;
    lastWgSample = sample;
    return sample;
}

// RT-safe
float ToneVoice::processTVF (float in) noexcept
{
    if (voiceState == State::Free)
        return 0.0f;

    switch (block.filterType)
    {
        case 1:
            return filter.processLowpass (in);
        case 2:
            return filter.processBandpass (in);
        case 3:
            return filter.processHighpass (in);
        case 4:
            return peaking.process (in);
        default:
            return in;   // OFF
    }
}

// RT-safe
float ToneVoice::processTVA (float in) noexcept
{
    if (voiceState == State::Free)
        return 0.0f;

    return in * activeGain;
}

// RT-safe
void ToneVoice::addToBus (float sample, BusBuffers& buses) noexcept
{
    if (voiceState == State::Free)
        return;

    const auto pan = std::clamp (panSmoother.current()
                                     + block.panLfoDepth[0] * lfo[0].value()
                                     + block.panLfoDepth[1] * lfo[1].value(),
                                 -1.0f, 1.0f);
    const auto theta = (pan + 1.0f) * 0.25f * kPi;
    const auto level = outputSmoother.current();
    const auto left = sample * std::cos (theta) * level;
    const auto right = sample * std::sin (theta) * level;

    const auto bus = block.outputAssign >= 0 && block.outputAssign < kNumOutputBuses
                         ? block.outputAssign : 0;

    if (buses.l[bus] != nullptr)
        buses.l[bus][0] += left;

    if (buses.r[bus] != nullptr)
        buses.r[bus][0] += right;
}

void ToneVoice::updateFilterCoefficients() noexcept
{
    // Keyfollow/matrix are already smoothed into the base target; the F-ENV
    // and LFO modulation stays per-sample (architecture 4.3).
    const auto octaves = block.fEnvDepth * fEnv.level() * block.mod.envLevelScale[1]
                         + block.lfoCutoffDepth[0] * lfo[0].value()
                         + block.lfoCutoffDepth[1] * lfo[1].value();

    activeCutoffHz = std::clamp (cutoffSmoother.current() * std::exp2 (octaves),
                                 (float) Calibration::kCutoffMinHz,
                                 (float) Calibration::kCutoffMaxHz);

    if (block.filterType == 4)
        peaking.setPeaking (activeCutoffHz, 1.0f, block.pkgGainDb);
    else if (block.filterType >= 1 && block.filterType <= 3)
        filter.setCoefficients (activeCutoffHz, resonanceSmoother.current());
}
