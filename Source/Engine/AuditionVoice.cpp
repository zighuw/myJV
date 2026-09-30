#include "Engine/AuditionVoice.h"

#include <juce_core/juce_core.h>

namespace
{
double nowMs() noexcept
{
    return juce::Time::getMillisecondCounterHiRes();
}
}

void AuditionVoice::prepare (double newSampleRate) noexcept
{
    sampleRate.store (newSampleRate > 0.0 ? newSampleRate : 48000.0, std::memory_order_relaxed);
}

void AuditionVoice::play (std::shared_ptr<const Sample> sample, int note)
{
    if (sample == nullptr)
    {
        stop();
        return;
    }

    if (owner != nullptr)
        retired.push_back ({ std::move (owner), nowMs() });

    owner = std::move (sample);
    pendingNote.store (juce::jlimit (kMidiNoteMin, kMidiNoteMax, note), std::memory_order_relaxed);
    pendingSample.store (owner.get(), std::memory_order_release);
    stopRequested.store (false, std::memory_order_release);
    startRequested.store (true, std::memory_order_release);
}

void AuditionVoice::stop() noexcept
{
    startRequested.store (false, std::memory_order_release);
    stopRequested.store (true, std::memory_order_release);
}

void AuditionVoice::purgeRetired (double minimumAgeMs) noexcept
{
    const auto now = nowMs();
    const auto minimumAge = juce::jmax (0.0, minimumAgeMs);

    for (auto it = retired.begin(); it != retired.end(); )
    {
        if (now - it->retiredAtMs >= minimumAge)
            it = retired.erase (it);
        else
            ++it;
    }
}

// RT-safe
void AuditionVoice::render (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    if (startRequested.exchange (false, std::memory_order_acq_rel))
    {
        const auto* sample = pendingSample.load (std::memory_order_acquire);

        if (sample != nullptr && sample->data.getNumChannels() > 0 && sample->data.getNumSamples() > 0)
        {
            // Local Zone on the audio thread: the shared_ptr member stays null
            // (no refcount operations) and only the loop data is consumed.
            Zone zone;
            zone.loop = sample->embeddedLoop;
            zone.loopMode = zone.loop.end > zone.loop.start ? LoopMode::Forward : LoopMode::Off;

            player.start (*sample, zone, pendingNote.load (std::memory_order_relaxed),
                          sampleRate.load (std::memory_order_relaxed));
            playing.store (true, std::memory_order_release);
        }
        else
        {
            player.stop();
            playing.store (false, std::memory_order_release);
        }
    }

    if (stopRequested.exchange (false, std::memory_order_acq_rel))
    {
        player.stop();
        playing.store (false, std::memory_order_release);
    }

    if (! player.isActive())
    {
        playing.store (false, std::memory_order_release);
        return;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto value = player.getNextSample();
        left[i] += value;
        right[i] += value;
    }

    if (! player.isActive())
        playing.store (false, std::memory_order_release);
}

bool AuditionVoice::isPlaying() const noexcept
{
    return playing.load (std::memory_order_acquire);
}
