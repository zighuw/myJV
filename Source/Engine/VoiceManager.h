#pragma once

#include "Engine/Note.h"
#include "Engine/ToneVoice.h"

#include <cstdint>

class AssetReclaimer;
struct BusBuffers;
struct PatchRuntime;

// Fixed voice pool (architecture 5.2): voices are counted per Tone, capped at
// 64, with a matching 64-note pool. prepare() owns all setup; after it the audio
// path only reads and writes these arrays (no allocation, locks or refcounts).
inline constexpr int kMaxVoices = 64;
inline constexpr int kMaxNotes = 64;

// Owns the Tone voice and Note pools and implements the allocation / stealing
// policy from architecture 5.2. The engine drives it: setModulationInput +
// beginBlock once per block, startNote/releaseNote from the MIDI sub-block loop,
// render per sub-block, then reports oldestRuntimeIdInUse to the AssetReclaimer.
class VoiceManager
{
public:
    VoiceManager() = default;

    // Non-RT: prepares and resets the whole pool.
    void prepare (double engineSampleRate) noexcept;
    void reset() noexcept;

    // RT-safe. Starts one voice per enabled tone (architecture 5.2); returns the
    // number of voices actually started (0 when every tone is switched off or no
    // zone matched - the note is then not kept, code-review R4-M5).
    int startNote (const PatchRuntime& runtime, int midiNote, int velocity, int channel,
                   std::uint64_t rngSeed, float keyIntervalScale = 1.0f) noexcept;

    // RT-safe. Releases every held note numbered midiNote on channel. Repeated
    // note-offs are idempotent and never cancel a HOLD countdown (R4-M1 / D6).
    void releaseNote (int midiNote, int channel) noexcept;

    // RT-safe. Broadcasts the captured controller state to the live voices.
    void setModulationInput (const ModulationInput& input, int blockSamples) noexcept;

    // RT-safe. Per-block parameter decisions for every live voice, plus the
    // per-note level used by the Loudest stealing policy.
    void beginBlock() noexcept;

    // RT-safe. Renders [startSample, startSample + numSamples) into the buses.
    void render (const BusBuffers& buses, int startSample, int numSamples) noexcept;

    // RT-safe. Smallest runtime id referenced by a live note (0 = none); the
    // engine reports it to AssetReclaimer::updateOldestAssetInUse, or the active
    // runtime id when idle (ADR-018).
    std::uint64_t oldestRuntimeIdInUse() const noexcept;

    int activeNoteCount() const noexcept;
    int activeVoiceCount() const noexcept;

    // Diagnostics (M2-05 contract): one updateModulators() per rendered sample
    // per sounding voice, summed across the pool.
    std::uint64_t modulationUpdateCount() const noexcept;

    // Diagnostics: number of still-fading voices reused by hard take-over.
    int hardTakeoverCount() const noexcept { return hardTakeovers; }

    const Note& note (int index) const noexcept { return notes[index]; }

private:
    bool noteHasLiveVoice (const Note& note) const noexcept;
    int findFreeVoice() const noexcept;
    int oldestKillFadingVoice() const noexcept;
    int findFreeNote() const noexcept;
    int oldestActiveNote() const noexcept;
    int countFreeVoices() const noexcept;
    int countKillFadingVoices() const noexcept;
    int selectVictim (int excludeNote, int policy) const noexcept;
    void killNote (int noteIndex) noexcept;
    void refreshFinishedVoices() noexcept;

    ToneVoice voices[kMaxVoices];
    Note notes[kMaxNotes];

    int voiceNote[kMaxVoices] {};               // owning note index, -1 = unowned
    std::uint64_t voiceKillSeq[kMaxVoices] {};  // kill order (oldest KillFading first)
    std::uint64_t orderCounter = 0;
    std::uint64_t killCounter = 0;
    int hardTakeovers = 0;

    ModulationInput lastInput;
    int lastBlockSamples = 512;
};
