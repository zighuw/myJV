#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <vector>

// Baseline audio regression helpers (M2-10): metrics, comparison, WAV and
// manifest IO. Test-only, GUI-free: the render harness in render_tests.cpp
// feeds it stereo Main-bus buffers.
namespace RenderRegression
{
inline constexpr int kNumBands = 10;

// Octave-band edges in Hz; band i covers [edges[i], edges[i + 1]).
inline constexpr double kBandEdgesHz[kNumBands + 1]
{
    31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0, 24000.0
};

// Bands quieter than this in the reference are reported but never gated
// (quantisation and denormal noise floor).
inline constexpr double kBandFloorDb = -90.0;

inline constexpr int kFftOrder = 12;              // 4096
inline constexpr int kFftSize = 1 << kFftOrder;
inline constexpr int kFftHop = kFftSize / 4;      // 75% overlap

struct Tolerance
{
    double rmsDb = 0.1;
    double bandDb = 0.5;
};

struct Metrics
{
    double rmsDb = 0.0;
    double peakDb = 0.0;
    std::vector<double> bandsDb;   // size kNumBands
    juce::String sha256;           // hash of the raw float samples
};

Metrics measure (const juce::AudioBuffer<float>& buffer, double sampleRate);

struct Comparison
{
    double rmsDeltaDb = 0.0;
    double maxBandDeltaDb = 0.0;
    int worstBand = -1;
    std::vector<double> referenceBandsDb;
    std::vector<double> renderedBandsDb;
    bool rmsPassed = false;
    bool bandsPassed = false;

    bool passed() const noexcept { return rmsPassed && bandsPassed; }
};

Comparison compare (const Metrics& reference, const Metrics& rendered, const Tolerance& tolerance);

// 16-bit PCM WAV IO.
bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer,
               double sampleRate, juce::String& error);
bool readWav (const juce::File& file, juce::AudioBuffer<float>& buffer,
              double& sampleRate, juce::String& error);

juce::String sha256OfBuffer (const juce::AudioBuffer<float>& buffer);
juce::String sha256OfFile (const juce::File& file);

struct CaseMeta
{
    juce::String name;
    juce::String file;
    int frames = 0;
    int channels = 0;
    juce::String renderSha256;   // float render at generation time (informational across platforms)
    juce::String fileSha256;     // baseline WAV bytes (strict integrity)
    double rmsDb = 0.0;
    double peakDb = 0.0;
    std::vector<double> bandsDb;
    Tolerance tolerance;
};

struct Manifest
{
    int schemaVersion = 1;
    juce::String generator;
    double sampleRate = 48000.0;
    int blockSize = 512;
    std::vector<CaseMeta> cases;

    const CaseMeta* find (const juce::String& caseName) const;
};

bool readManifest (const juce::File& file, Manifest& manifest, juce::String& error);
bool writeManifest (const juce::File& file, const Manifest& manifest, juce::String& error);

juce::String formatMetrics (const Metrics& metrics);
juce::String formatComparison (const Comparison& comparison, const char* caseName);
}
