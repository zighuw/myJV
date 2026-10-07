#pragma once

#include <cstdint>

struct PatchRuntime;

// One MIDI note (architecture 5.2). The voice indices are resolved once at
// Note-On from the Tone Switch states and never change afterwards. The captured
// runtime is the Patch Remain anchor: a note keeps rendering the runtime it
// started with even after a newer one is published. Plain value semantics:
// VoiceManager owns a fixed array of these, so the audio path never allocates.
//
// `held` makes repeated note-offs idempotent (R4-M1 / D6): the first note-off
// releases the voices, later ones for the same note+channel are ignored.
struct Note
{
    static constexpr int kMaxVoicesPerNote = 4;   // = kNumTones (checked in VoiceManager.cpp)

    bool active = false;
    bool held = false;
    int midiNote = -1;                                          // 0..127
    int channel = 0;                                            // 0..15
    std::uint64_t runtimeId = 0;                                // captured PatchRuntime::id
    const PatchRuntime* runtime = nullptr;                      // captured runtime (Patch Remain)
    int voiceIndex[kMaxVoicesPerNote] = { -1, -1, -1, -1 };     // one per enabled tone
    int voiceCount = 0;
    std::uint64_t order = 0;                                    // trigger order (Last policy)
    float level = 0.0f;                                         // per-block level (Loudest policy)
};
