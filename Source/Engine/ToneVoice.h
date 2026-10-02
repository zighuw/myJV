#pragma once

#include "DSP/Biquad.h"
#include "DSP/Envelope.h"
#include "DSP/LFO.h"
#include "DSP/SamplePlayer.h"
#include "DSP/SVF.h"

#include <cstdint>

struct PatchRuntime;
struct BusBuffers;

// One Tone instance (architecture 5.4/5.10): consumes the active runtime's
// ToneSnapshot and renders the staged WG -> TVF -> TVA chain. RT-safe: no
// allocation, locks, IO, refcounts or exceptions on the audio path. The caller
// owns the runtime lifetime (AssetReclaimer protocol) and calls beginBlock()
// once per process block, updateModulators() exactly once per sample, then the
// process* stages and addToBus().
class ToneVoice
{
public:
    enum class State { Free, Active, Releasing, KillFading };

    ToneVoice() = default;

    // Non-RT: sample rate and reset.
    void prepare (double engineSampleRate) noexcept;
    void reset() noexcept;

    // RT-safe. Reads the runtime's ToneSnapshot and ZoneSet; tone switch off,
    // no zone or a null-sample zone leaves the voice Free (silent).
    void startNote (const PatchRuntime* runtime, int toneIndex, int midiNote,
                    float velocity, std::uint64_t rngSeed) noexcept;

    // RT-safe.
    void release() noexcept;
    void kill() noexcept;

    // RT-safe. Caches the per-block parameter decisions (architecture 5.5:
    // filter type and base values are read once per block).
    void beginBlock() noexcept;

    // RT-safe. LFO1/2 -> P-ENV -> F-ENV -> A-ENV, pitch and modulation sums;
    // exactly once per sample.
    void updateModulators() noexcept;

    // RT-safe staged processing. addToBus writes at index 0 of the supplied
    // (already offset) bus pointers.
    float processWG() noexcept;
    float processTVF (float in) noexcept;
    float processTVA (float in) noexcept;
    void addToBus (float sample, BusBuffers& buses) noexcept;

    bool finished() const noexcept { return voiceState == State::Free; }
    State state() const noexcept { return voiceState; }

    // Diagnostics (tests / M3 voice manager).
    float currentPitchOffsetSemitones() const noexcept { return pitchOffset; }
    float currentCutoffHz() const noexcept { return activeCutoffHz; }
    float currentGain() const noexcept { return activeGain; }

    // Number of updateModulators() invocations (cadence contract).
    std::uint64_t modulationUpdateCount() const noexcept { return modulatorCalls; }

private:
    struct BlockParams
    {
        int filterType = 0;
        float baseCutoffHz = 1000.0f;
        float baseQ = 0.5f;
        float pkgGainDb = 0.0f;
        float keyfollowOctaves = 0.0f;
        float fEnvDepth = 0.0f;
        float lfoCutoffDepth[2] {};
        float toneLevel = 1.0f;
        float panBase = 0.0f;
        float panKey = 0.0f;
        float panLfoDepth[2] {};
        int outputAssign = 0;
        float outputLevel = 1.0f;
        float pitchKeyfollowScale = 0.0f;
        float pEnvDepthSemis = 0.0f;
        float pitchLfoDepth[2] {};
    };

    void updateFilterCoefficients() noexcept;

    State voiceState = State::Free;
    const PatchRuntime* runtime = nullptr;
    int toneIndex = 0;
    int note = 60;
    int rootKey = 60;
    float velocity = 1.0f;
    double sampleRate = 48000.0;
    int filterControlCounter = 0;
    float killGain = 1.0f;
    float killStep = 0.0f;
    float randomPitchSemitones = 0.0f;
    float randomPan = 0.0f;
    float pitchOffset = 0.0f;
    float activeCutoffHz = 1000.0f;
    float activeGain = 1.0f;
    std::uint64_t modulatorCalls = 0;

    BlockParams block;

    SamplePlayer player;
    Envelope pEnv;
    Envelope fEnv;
    Envelope aEnv;
    LFO lfo[2];
    SVF filter;
    Biquad peaking;
};
