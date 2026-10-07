#include "Engine/VoiceManager.h"

#include "Engine/AssetReclaimer.h"
#include "Engine/SynthEngine.h"   // BusBuffers / kNumOutputBuses

#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

static_assert (Note::kMaxVoicesPerNote == kNumTones,
               "the per-note voice slots must match the Tone count");
static_assert (kMaxVoices >= kMaxNotes, "the voice pool must cover the note pool");

namespace
{
constexpr int kPolicyLast = 0;      // "LAST"
constexpr int kPolicyLoudest = 1;   // "LOUDEST"
}

void VoiceManager::prepare (double engineSampleRate) noexcept
{
    for (auto& voice : voices)
        voice.prepare (engineSampleRate > 0.0 ? engineSampleRate : 48000.0);

    reset();
}

// RT-safe
void VoiceManager::reset() noexcept
{
    for (auto& voice : voices)
        voice.reset();

    for (auto& note : notes)
        note = Note {};

    for (int i = 0; i < kMaxVoices; ++i)
    {
        voiceNote[i] = -1;
        voiceKillSeq[i] = 0;
    }

    orderCounter = 0;
    killCounter = 0;
    hardTakeovers = 0;
    lastInput = ModulationInput {};
    lastBlockSamples = 512;
}

// RT-safe
bool VoiceManager::noteHasLiveVoice (const Note& note) const noexcept
{
    for (int i = 0; i < note.voiceCount; ++i)
        if (note.voiceIndex[i] >= 0 && ! voices[note.voiceIndex[i]].finished())
            return true;

    return false;
}

// RT-safe
void VoiceManager::refreshFinishedVoices() noexcept
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (voiceNote[i] >= 0 && voices[i].finished())
            voiceNote[i] = -1;

    for (auto& note : notes)
        if (note.active && ! noteHasLiveVoice (note))
            note = Note {};
}

// RT-safe
int VoiceManager::findFreeVoice() const noexcept
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (voices[i].finished())
            return i;

    return -1;
}

// RT-safe
int VoiceManager::oldestKillFadingVoice() const noexcept
{
    int best = -1;
    std::uint64_t bestSeq = 0;

    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[i].state() != ToneVoice::State::KillFading)
            continue;

        if (best < 0 || voiceKillSeq[i] < bestSeq)
        {
            best = i;
            bestSeq = voiceKillSeq[i];
        }
    }

    return best;
}

// RT-safe
int VoiceManager::findFreeNote() const noexcept
{
    for (int i = 0; i < kMaxNotes; ++i)
        if (! notes[i].active)
            return i;

    return -1;
}

// RT-safe
int VoiceManager::countFreeVoices() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        count += voice.finished() ? 1 : 0;

    return count;
}

// RT-safe
int VoiceManager::countKillFadingVoices() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        count += voice.state() == ToneVoice::State::KillFading ? 1 : 0;

    return count;
}

// RT-safe
int VoiceManager::oldestActiveNote() const noexcept
{
    int best = -1;
    std::uint64_t bestOrder = 0;

    for (int i = 0; i < kMaxNotes; ++i)
    {
        if (! notes[i].active || ! noteHasLiveVoice (notes[i]))
            continue;

        if (best < 0 || notes[i].order < bestOrder)
        {
            best = i;
            bestOrder = notes[i].order;
        }
    }

    return best;
}

// RT-safe
int VoiceManager::selectVictim (int excludeNote, int policy) const noexcept
{
    int best = -1;
    float bestLevel = 0.0f;
    std::uint64_t bestOrder = 0;

    for (int i = 0; i < kMaxNotes; ++i)
    {
        const auto& note = notes[i];

        if (i == excludeNote || ! note.active || ! noteHasLiveVoice (note))
            continue;

        if (policy == kPolicyLoudest)
        {
            if (best < 0 || note.level < bestLevel
                || (note.level == bestLevel && note.order < bestOrder))
            {
                best = i;
                bestLevel = note.level;
                bestOrder = note.order;
            }
        }
        else if (best < 0 || note.order < bestOrder)
        {
            best = i;
            bestOrder = note.order;
        }
    }

    return best;
}

// RT-safe
void VoiceManager::killNote (int noteIndex) noexcept
{
    auto& note = notes[noteIndex];

    for (int i = 0; i < note.voiceCount; ++i)
    {
        const auto slot = note.voiceIndex[i];

        if (slot < 0)
            continue;

        // A live voice is moved to KillFading (fast fade, never restarted); a
        // voice that already finished is simply released from the pool.
        if (! voices[slot].finished())
        {
            voices[slot].kill();
            voiceKillSeq[slot] = ++killCounter;
        }

        voiceNote[slot] = -1;
    }

    note = Note {};
}

// RT-safe
int VoiceManager::startNote (const PatchRuntime& runtime, int midiNote, int velocity, int channel,
                             std::uint64_t rngSeed, float keyIntervalScale) noexcept
{
    refreshFinishedVoices();

    // 1. The voice count is fixed at Note-On from the enabled tones.
    int enabledTones[kNumTones];
    int numTones = 0;

    for (int tone = 0; tone < kNumTones; ++tone)
        if (runtime.snapshot.tones[tone].wg.toneSwitch)
            enabledTones[numTones++] = tone;

    if (numTones == 0)
        return 0;

    const auto policy = runtime.snapshot.common.voicePriority;

    // 2. Take a note slot (steal the oldest note if the pool is full).
    auto noteIndex = findFreeNote();

    if (noteIndex < 0)
    {
        auto victim = selectVictim (-1, policy);

        if (victim < 0)
            victim = oldestActiveNote();

        if (victim < 0)
            return 0;

        killNote (victim);
        noteIndex = victim;
    }

    // 3. Steal notes until enough voices are free or fading.
    while (countFreeVoices() + countKillFadingVoices() < numTones)
    {
        const auto victim = selectVictim (noteIndex, policy);

        if (victim < 0)
            break;

        killNote (victim);
    }

    // 4. Bind one voice per enabled tone.
    auto& note = notes[noteIndex];
    note = Note {};

    const auto velocityFloat = (float) std::clamp (velocity, 0, 127) / 127.0f;
    int assigned = 0;

    for (int i = 0; i < numTones; ++i)
    {
        auto slot = findFreeVoice();

        if (slot < 0)
        {
            slot = oldestKillFadingVoice();

            if (slot < 0)
                break;

            ++hardTakeovers;
        }

        if (voiceNote[slot] >= 0)
            voiceNote[slot] = -1;

        auto& voice = voices[slot];
        voice.reset();
        voice.setModulationInput (lastInput, lastBlockSamples);
        voice.startNote (&runtime, enabledTones[i], midiNote, velocityFloat, rngSeed, keyIntervalScale);

        if (voice.finished())
        {
            // Tone switch off, no zone or no sample: the slot was not really used
            // (R4-M5), so it is not kept and the note does not hold a silent voice.
            voiceNote[slot] = -1;
            continue;
        }

        voiceNote[slot] = noteIndex;
        note.voiceIndex[assigned++] = slot;
    }

    if (assigned == 0)
    {
        note = Note {};
        return 0;
    }

    note.active = true;
    note.held = true;
    note.midiNote = std::clamp (midiNote, 0, 127);
    note.channel = channel;
    note.runtimeId = runtime.id;
    note.runtime = &runtime;
    note.voiceCount = assigned;
    note.order = ++orderCounter;
    note.level = 0.0f;

    return assigned;
}

// RT-safe
void VoiceManager::releaseNote (int midiNote, int channel) noexcept
{
    refreshFinishedVoices();

    for (auto& note : notes)
    {
        if (! note.active || ! note.held)
            continue;

        if (note.midiNote != midiNote || note.channel != channel)
            continue;

        note.held = false;

        for (int i = 0; i < note.voiceCount; ++i)
            if (note.voiceIndex[i] >= 0)
                voices[note.voiceIndex[i]].release();
    }
}

// RT-safe
void VoiceManager::setModulationInput (const ModulationInput& input, int blockSamples) noexcept
{
    lastInput = input;
    lastBlockSamples = std::max (0, blockSamples);
}

// RT-safe
void VoiceManager::beginBlock() noexcept
{
    for (auto& voice : voices)
    {
        if (voice.finished())
            continue;

        voice.setModulationInput (lastInput, lastBlockSamples);
        voice.beginBlock();
    }

    for (auto& note : notes)
    {
        if (! note.active)
            continue;

        auto level = 0.0f;

        for (int i = 0; i < note.voiceCount; ++i)
            if (note.voiceIndex[i] >= 0)
                level = std::max (level, voices[note.voiceIndex[i]].currentGain());

        note.level = level;
    }
}

// RT-safe
void VoiceManager::render (const BusBuffers& buses, int startSample, int numSamples) noexcept
{
    for (int bus = 0; bus < kNumOutputBuses; ++bus)
    {
        if (buses.l[bus] != nullptr)
            juce::FloatVectorOperations::clear (buses.l[bus] + startSample, numSamples);

        if (buses.r[bus] != nullptr)
            juce::FloatVectorOperations::clear (buses.r[bus] + startSample, numSamples);
    }

    if (numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        BusBuffers cursor;

        for (int bus = 0; bus < kNumOutputBuses; ++bus)
        {
            cursor.l[bus] = buses.l[bus] != nullptr ? buses.l[bus] + startSample + i : nullptr;
            cursor.r[bus] = buses.r[bus] != nullptr ? buses.r[bus] + startSample + i : nullptr;
        }

        for (auto& voice : voices)
        {
            if (voice.finished())
                continue;

            voice.updateModulators();
            const auto sample = voice.processTVA (voice.processTVF (voice.processWG()));
            voice.addToBus (sample, cursor);
        }
    }
}

// RT-safe
std::uint64_t VoiceManager::oldestRuntimeIdInUse() const noexcept
{
    std::uint64_t oldest = 0;

    for (const auto& note : notes)
    {
        if (! note.active || note.runtimeId == 0 || ! noteHasLiveVoice (note))
            continue;

        if (oldest == 0 || note.runtimeId < oldest)
            oldest = note.runtimeId;
    }

    return oldest;
}

int VoiceManager::activeNoteCount() const noexcept
{
    int count = 0;

    for (const auto& note : notes)
        count += note.active && noteHasLiveVoice (note) ? 1 : 0;

    return count;
}

int VoiceManager::activeVoiceCount() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        count += voice.finished() ? 0 : 1;

    return count;
}

std::uint64_t VoiceManager::modulationUpdateCount() const noexcept
{
    std::uint64_t count = 0;

    for (const auto& voice : voices)
        count += voice.modulationUpdateCount();

    return count;
}
