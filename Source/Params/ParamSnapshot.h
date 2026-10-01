#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

struct ZoneSet;

// Parameter snapshot value types (architecture 4.2). Each snapshot is a plain
// trivially copyable aggregate with no pointers of its own; the audio thread
// refreshes the active runtime's snapshot once per block from APVTS atomics and
// retired runtimes keep their frozen values (Patch Remain). Choice parameters
// are stored as their option index; enums are the DSP consumers' concern.
//
// The concrete fields follow REVIEWS/M0-03/parameter-registry.md exactly. The
// architecture sketch groups envelopes into a single EnvShape; the registry has
// three distinct parameter sets (P-ENV has no velocity curve, A-ENV has no
// depth and only three levels), so the shapes below mirror the registry and the
// field-mapping evidence records the difference.

struct PEnvSnapshot
{
    int depth = 0;
    int velSens = 0;
    int timeKeyfollow = 0;
    float time[4] {};
    float level[4] {};
    int velTime1Sens = 0;
    int velTime4Sens = 0;

    bool operator== (const PEnvSnapshot&) const = default;
};

struct FEnvSnapshot
{
    int depth = 0;
    int velCurve = 0;
    int velSens = 0;
    int timeKeyfollow = 0;
    float time[4] {};
    float level[4] {};
    int velTime1Sens = 0;
    int velTime4Sens = 0;

    bool operator== (const FEnvSnapshot&) const = default;
};

struct AEnvSnapshot
{
    int velCurve = 0;
    int velSens = 0;
    float time[4] {};
    float level[3] {};      // A-ENV exposes level1..level3 only
    int timeKeyfollow = 0;
    int velTime1Sens = 0;
    int velTime4Sens = 0;

    bool operator== (const AEnvSnapshot&) const = default;
};

struct LfoSnapshot
{
    int wave = 0;
    bool keyTrig = false;
    float rate = 0.0f;
    int levelOffset = 0;
    float delayTime = 0.0f;
    int fadeMode = 0;
    float fadeTime = 0.0f;
    bool sync = false;

    bool operator== (const LfoSnapshot&) const = default;
};

struct ToneSnapshot
{
    struct Wg
    {
        bool toneSwitch = false;
        int waveGain = 0;
        bool fxmOn = false;
        int fxmColor = 0;
        float fxmDepth = 0.0f;
        int toneDelayMode = 0;
        float toneDelayTime = 0.0f;
        float velXfade = 0.0f;
        int velLow = 0;
        int velHigh = 0;
        int keyLow = 0;
        int keyHigh = 0;
        bool redamper = false;
        bool volCtrl = false;
        bool holdCtrl = false;
        bool bendCtrl = false;
        bool panCtrl = false;
        int coarseTune = 0;
        int fineTune = 0;
        float randomPitch = 0.0f;
        float pitchKeyfollow = 0.0f;
        float pitchLfo1Depth = 0.0f;
        float pitchLfo2Depth = 0.0f;

        bool operator== (const Wg&) const = default;
    } wg;

    struct Tvf
    {
        int type = 0;
        float cutoff = 0.0f;
        float cutoffKeyfollow = 0.0f;
        float resonance = 0.0f;
        int resVelSens = 0;
        FEnvSnapshot fEnv;
        float lfo1Depth = 0.0f;
        float lfo2Depth = 0.0f;

        bool operator== (const Tvf&) const = default;
    } tvf;

    struct Tva
    {
        float level = 0.0f;
        int biasDirection = 0;
        int biasPoint = 0;
        int biasLevel = 0;
        AEnvSnapshot aEnv;
        float lfo1Depth = 0.0f;
        float lfo2Depth = 0.0f;

        bool operator== (const Tva&) const = default;
    } tva;

    PEnvSnapshot pEnv;

    struct Pan
    {
        float position = 0.0f;
        int keyfollow = 0;
        float random = 0.0f;
        float alt = 0.0f;
        float lfo1Depth = 0.0f;
        float lfo2Depth = 0.0f;

        bool operator== (const Pan&) const = default;
    } pan;

    struct Output
    {
        int assign = 0;
        float level = 0.0f;

        bool operator== (const Output&) const = default;
    } output;

    LfoSnapshot lfo[2];

    struct Ctrl
    {
        int dest[4] {};
        int depth[4] {};

        bool operator== (const Ctrl&) const = default;
    } ctrl[3];

    bool operator== (const ToneSnapshot&) const = default;
};

struct PatchCommonSnapshot
{
    float level = 0.0f;
    float pan = 0.0f;
    float analogFeel = 0.0f;
    int bendUp = 0;
    int bendDown = 0;
    int octaveShift = 0;
    int stretchTune = 0;
    int keyAssign = 0;
    bool legato = false;
    bool portamentoSwitch = false;
    int portamentoMode = 0;
    int portamentoType = 0;
    int portamentoStart = 0;
    float portamentoTime = 0.0f;
    int voicePriority = 0;
    int structure12 = 0;
    float booster12 = 0.0f;
    int structure34 = 0;
    float booster34 = 0.0f;
    int ctrlSource2 = 0;
    int ctrlSource3 = 0;
    int controlHoldPeak = 0;
    int ctrl1HoldPeak = 0;
    int ctrl2HoldPeak = 0;
    int ctrl3HoldPeak = 0;
    int defaultTempo = 0;

    bool operator== (const PatchCommonSnapshot&) const = default;
};

struct PatchSnapshot
{
    static constexpr int kToneCount = 4;

    PatchCommonSnapshot common;
    ToneSnapshot tones[kToneCount];
    const ZoneSet* zoneSets[kToneCount] = {};   // audio-thread read-only, mirrors PatchRuntime::rawZoneSets
    std::uint64_t assetId = 0;                  // owning PatchRuntime id

    // Exact aggregate comparison (including bitwise float equality) for tests
    // and diagnostics; not an audio-path operation.
    bool operator== (const PatchSnapshot&) const = default;

    // Non-RT diagnostic summary (not the M3-05 serialization format).
    std::string toDebugString() const;
};

static_assert (std::is_trivially_copyable_v<PEnvSnapshot>);
static_assert (std::is_trivially_copyable_v<FEnvSnapshot>);
static_assert (std::is_trivially_copyable_v<AEnvSnapshot>);
static_assert (std::is_trivially_copyable_v<LfoSnapshot>);
static_assert (std::is_trivially_copyable_v<ToneSnapshot>);
static_assert (std::is_trivially_copyable_v<PatchCommonSnapshot>);
static_assert (std::is_trivially_copyable_v<PatchSnapshot>);

inline std::string PatchSnapshot::toDebugString() const
{
    std::string text = "PatchSnapshot{assetId=" + std::to_string (assetId)
                       + ", zones=" + std::to_string ((zoneSets[0] != nullptr ? 1 : 0)
                                                      + (zoneSets[1] != nullptr ? 1 : 0)
                                                      + (zoneSets[2] != nullptr ? 1 : 0)
                                                      + (zoneSets[3] != nullptr ? 1 : 0))
                       + ", common{level=" + std::to_string (common.level)
                       + ", pan=" + std::to_string (common.pan)
                       + ", structure12=" + std::to_string (common.structure12)
                       + ", structure34=" + std::to_string (common.structure34)
                       + ", ctrlSource2=" + std::to_string (common.ctrlSource2)
                       + ", ctrlSource3=" + std::to_string (common.ctrlSource3) + "}";

    for (int tone = 0; tone < kToneCount; ++tone)
    {
        const auto& value = tones[tone];
        text += ", tone" + std::to_string (tone + 1)
                + "{switch=" + (value.wg.toneSwitch ? "on" : "off")
                + ", waveGain=" + std::to_string (value.wg.waveGain)
                + ", coarse=" + std::to_string (value.wg.coarseTune)
                + ", fine=" + std::to_string (value.wg.fineTune)
                + ", tvfType=" + std::to_string (value.tvf.type)
                + ", cutoff=" + std::to_string (value.tvf.cutoff)
                + ", level=" + std::to_string (value.tva.level)
                + ", pEnvDepth=" + std::to_string (value.pEnv.depth)
                + ", pan=" + std::to_string (value.pan.position)
                + ", output=" + std::to_string (value.output.assign) + "}";
    }

    return text + "}";
}
