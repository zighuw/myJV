#pragma once

#include "DSP/SamplePlayer.h"
#include "Model/ZoneSet.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

// Retired audition samples are released only after this grace period (message
// thread timer), so the audio thread never releases or refcounts.
inline constexpr double kAuditionReleaseGraceMs = 200.0;

// Independent preview voice (architecture 5.2): renders into the Main bus
// without touching the 64-voice pool or the Tone chain. The message-thread API
// holds the owning shared_ptr; the audio thread only dereferences an atomic raw
// pointer published by play().
class AuditionVoice
{
public:
    AuditionVoice() = default;

    // Message thread (called before rendering starts / while stopped).
    void prepare (double newSampleRate) noexcept;

    // Message thread only.
    void play (std::shared_ptr<const Sample> sample, int note) noexcept;
    void stop() noexcept;
    void purgeRetired (double minimumAgeMs = kAuditionReleaseGraceMs) noexcept;

    // RT-safe; adds the voice into the given channel pointers.
    void render (float* left, float* right, int numSamples) noexcept;

    bool isPlaying() const noexcept;

private:
    struct RetiredSample
    {
        std::shared_ptr<const Sample> sample;
        double retiredAtMs = 0.0;
    };

    std::atomic<const Sample*> pendingSample { nullptr };
    std::atomic<int> pendingNote { 60 };
    std::atomic<double> sampleRate { 48000.0 };
    std::atomic<bool> startRequested { false };
    std::atomic<bool> stopRequested { false };
    std::atomic<bool> playing { false };

    // Message-thread ownership; the audio thread never touches these. Playable
    // samples are normally also pinned by SampleLibrary's cache (custody), so
    // the retired queue is a second, best-effort protection layer for callers
    // that drop their reference immediately.
    std::shared_ptr<const Sample> owner;
    std::vector<RetiredSample> retired;

    // Audio-thread state.
    SamplePlayer player;
};

static_assert (std::atomic<double>::is_always_lock_free,
               "audition sample rate must be lock-free");
