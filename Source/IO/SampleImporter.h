#pragma once

#include <juce_core/juce_core.h>

#include "Model/SampleLibrary.h"

#include <memory>
#include <string>

// v1.0 full-preload guards (architecture 6.4: streaming is v1.1+).
inline constexpr int kMaxImportSamples = 50 * 1024 * 1024;
inline constexpr double kMinImportSampleRate = 8000.0;
inline constexpr double kMaxImportSampleRate = 384000.0;

struct ImportOptions
{
    double targetSampleRate = 0.0;   // 0 = keep the source sample rate
    int thumbnailWidth = 512;
    int thumbnailHeight = 64;
};

struct ImportResult
{
    bool succeeded = false;
    std::string errorMessage;
    std::shared_ptr<const Sample> sample;
    LibraryEntry entry;
};

// Non-RT; synchronous decode/decode-time processing. The caller is responsible
// for publishing the sample and entry on the message thread afterwards
// (SampleLibrary contract, M1-02).
//
// The file must live under libraryRoot (the caller copies external files into
// the library first). Resampling uses Lagrange interpolation without an
// anti-alias filter: adequate for upsampling and for downsampling when the
// source has little energy above the target Nyquist; a high-quality offline
// resampler (libsamplerate/r8brain, architecture 2.2) remains a v1.1 option.
class SampleImporter
{
public:
    static ImportResult importFile (const juce::File& file,
                                    const juce::File& libraryRoot,
                                    const ImportOptions& options = {});
};
