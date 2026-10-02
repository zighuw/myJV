#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "DSP/Biquad.h"
#include "DSP/SVF.h"
#include "Params/Calibration.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace
{
constexpr double kPi = 3.14159265358979323846;

template <typename Process>
double gainDbAt (Process&& process, double frequency, double sampleRate,
                 int settleSamples = 20000, int measureSamples = 48000)
{
    double outputEnergy = 0.0;
    double inputEnergy = 0.0;

    for (int i = 0; i < settleSamples + measureSamples; ++i)
    {
        const auto x = (float) std::sin (2.0 * kPi * frequency * (double) i / sampleRate);
        const auto y = process (x);

        if (i >= settleSamples)
        {
            outputEnergy += (double) y * (double) y;
            inputEnergy += (double) x * (double) x;
        }
    }

    return 20.0 * std::log10 (std::sqrt (outputEnergy / inputEnergy));
}

// Bilinear-equivalent analog prototype magnitudes (the TPT SVF is the bilinear
// transform of this prototype): digital f maps to Omega = 2 fs tan(pi f / fs).
// The raw SVF band output has a peak gain of Q at the cutoff.
void prototypeMagnitudeDb (double frequency, double cutoffHz, double q, double sampleRate,
                           double& low, double& band, double& high)
{
    const auto omega = 2.0 * sampleRate * std::tan (kPi * frequency / sampleRate);
    const auto omegaC = 2.0 * sampleRate * std::tan (kPi * cutoffHz / sampleRate);
    const auto re = omegaC * omegaC - omega * omega;
    const auto im = omega * omegaC / q;
    const auto denominator = std::sqrt (re * re + im * im);

    low = 20.0 * std::log10 (omegaC * omegaC / denominator);
    band = 20.0 * std::log10 (omega * omegaC / denominator);
    high = 20.0 * std::log10 (omega * omega / denominator);
}

double peakingMagnitudeDb (double frequency, double cutoffHz, double q, double gainDb, double sampleRate)
{
    const auto A = std::pow (10.0, gainDb / 40.0);
    const auto w0 = 2.0 * kPi * cutoffHz / sampleRate;
    const auto alpha = std::sin (w0) / (2.0 * q);
    const auto w = 2.0 * kPi * frequency / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;

    const std::complex<double> denominator (1.0 + alpha / A, 0.0);
    const std::complex<double> denominatorZ (-2.0 * std::cos (w0), 0.0);
    const std::complex<double> denominatorZ2 (1.0 - alpha / A, 0.0);

    const std::complex<double> numerator (1.0 + alpha * A, 0.0);
    const std::complex<double> numeratorZ (-2.0 * std::cos (w0), 0.0);
    const std::complex<double> numeratorZ2 (1.0 - alpha * A, 0.0);

    const auto h = (numerator + numeratorZ * z1 + numeratorZ2 * z2)
                   / (denominator + denominatorZ * z1 + denominatorZ2 * z2);

    return 20.0 * std::log10 (std::abs (h));
}

float mappedCutoffHz (float cutoffParam)
{
    return (float) (Calibration::kCutoffMinHz
                    * std::pow (Calibration::kCutoffMaxHz / Calibration::kCutoffMinHz,
                                (double) cutoffParam / 127.0));
}

float mappedQ (float resonanceParam)
{
    return (float) (0.5 * std::pow (Calibration::kResonanceMaxQ / 0.5, (double) resonanceParam / 127.0));
}
}

TEST_CASE ("cutoff and resonance map to the specified ranges")
{
    SVF svf;
    svf.prepare (48000.0);

    svf.setParameters (0.0f, 0.0f);
    REQUIRE (svf.cutoffHz() == Catch::Approx ((float) Calibration::kCutoffMinHz));
    REQUIRE (svf.q() == Catch::Approx (mappedQ (0.0f)));

    svf.setParameters (127.0f, 127.0f);
    REQUIRE (svf.cutoffHz() == Catch::Approx ((float) Calibration::kCutoffMaxHz));
    REQUIRE (svf.q() == Catch::Approx ((float) Calibration::kResonanceMaxQ));

    svf.setParameters (64.0f, 64.0f);
    REQUIRE (svf.cutoffHz() == Catch::Approx (mappedCutoffHz (64.0f)).epsilon (1.0e-4));
    REQUIRE (svf.q() == Catch::Approx (mappedQ (64.0f)).epsilon (1.0e-4));

    // Out-of-range parameters are clamped.
    svf.setParameters (-10.0f, 200.0f);
    REQUIRE (svf.cutoffHz() == Catch::Approx ((float) Calibration::kCutoffMinHz));
    REQUIRE (svf.q() == Catch::Approx ((float) Calibration::kResonanceMaxQ));

    // Direct coefficients clamp to the audible range and the Nyquist guard.
    svf.setCoefficients (1.0f, 0.001f);
    REQUIRE (svf.cutoffHz() == Catch::Approx ((float) Calibration::kCutoffMinHz));
    REQUIRE (svf.q() == Catch::Approx (0.05f));

    svf.setCoefficients (40000.0f, 1.0f);
    REQUIRE (svf.cutoffHz() == Catch::Approx (0.49f * 48000.0f).epsilon (1.0e-4));
}

TEST_CASE ("lowpass magnitude follows the prewarped prototype")
{
    for (const double sampleRate : { 44100.0, 48000.0 })
    {
        for (const double q : { 0.5, 2.0 })
        {
            SVF svf;
            svf.prepare (sampleRate);
            svf.setCoefficients (1000.0f, (float) q);

            for (const double frequency : { 250.0, 500.0, 1000.0, 2000.0, 4000.0 })
            {
                const auto measured = gainDbAt ([&svf] (float x) { return svf.processLowpass (x); },
                                                frequency, sampleRate);
                double low = 0.0, band = 0.0, high = 0.0;
                prototypeMagnitudeDb (frequency, 1000.0, q, sampleRate, low, band, high);

                INFO ("fs " << sampleRate << " q " << q << " f " << frequency);
                REQUIRE (measured == Catch::Approx (low).margin (0.5));
            }
        }
    }
}

TEST_CASE ("highpass magnitude follows the prewarped prototype")
{
    for (const double sampleRate : { 44100.0, 48000.0 })
    {
        for (const double q : { 0.5, 2.0 })
        {
            SVF svf;
            svf.prepare (sampleRate);
            svf.setCoefficients (1000.0f, (float) q);

            for (const double frequency : { 250.0, 1000.0, 4000.0, 8000.0 })
            {
                const auto measured = gainDbAt ([&svf] (float x) { return svf.processHighpass (x); },
                                                frequency, sampleRate);
                double low = 0.0, band = 0.0, high = 0.0;
                prototypeMagnitudeDb (frequency, 1000.0, q, sampleRate, low, band, high);

                INFO ("fs " << sampleRate << " q " << q << " f " << frequency);
                REQUIRE (measured == Catch::Approx (high).margin (0.5));
            }
        }
    }
}

TEST_CASE ("the raw bandpass peaks at the quality factor at the cutoff")
{
    for (const double q : { 0.5, 4.0 })
    {
        SVF svf;
        svf.prepare (48000.0);
        svf.setCoefficients (1000.0f, (float) q);

        const auto centre = gainDbAt ([&svf] (float x) { return svf.processBandpass (x); }, 1000.0, 48000.0);
        REQUIRE (centre == Catch::Approx (20.0 * std::log10 (q)).margin (0.5));

        for (const double frequency : { 500.0, 2000.0 })
        {
            const auto measured = gainDbAt ([&svf] (float x) { return svf.processBandpass (x); },
                                            frequency, 48000.0);
            double low = 0.0, band = 0.0, high = 0.0;
            prototypeMagnitudeDb (frequency, 1000.0, q, 48000.0, low, band, high);
            INFO ("q " << q << " f " << frequency);
            REQUIRE (measured == Catch::Approx (band).margin (0.5));
        }
    }
}

TEST_CASE ("the peaking filter follows the RBJ response")
{
    for (const double q : { 0.5, 1.0, 4.0 })
    {
        for (const double gainDb : { 12.0, -6.0 })
        {
            Biquad biquad;
            biquad.prepare (48000.0);
            biquad.setPeaking (1000.0f, (float) q, (float) gainDb);

            REQUIRE (biquad.centreHz() == Catch::Approx (1000.0f));
            REQUIRE (biquad.gainDb() == Catch::Approx (gainDb));

            for (const double frequency : { 250.0, 1000.0, 4000.0 })
            {
                const auto measured = gainDbAt ([&biquad] (float x) { return biquad.process (x); },
                                                frequency, 48000.0);
                const auto expected = peakingMagnitudeDb (frequency, 1000.0, q, gainDb, 48000.0);

                INFO ("q " << q << " gain " << gainDb << " f " << frequency);
                REQUIRE (measured == Catch::Approx (expected).margin (0.5));
            }
        }
    }
}

TEST_CASE ("the peaking parameters map resonance to the gain")
{
    Biquad biquad;
    biquad.prepare (48000.0);

    biquad.setParameters (64.0f, 0.0f);
    REQUIRE (biquad.centreHz() == Catch::Approx (mappedCutoffHz (64.0f)).epsilon (1.0e-4));
    REQUIRE (biquad.gainDb() == Catch::Approx (0.0f));

    biquad.setParameters (64.0f, 127.0f);
    REQUIRE (biquad.gainDb() == Catch::Approx ((float) Calibration::kPkgGainDb));

    biquad.setParameters (64.0f, 63.5f);
    REQUIRE (biquad.gainDb() == Catch::Approx ((float) Calibration::kPkgGainDb * 0.5f).epsilon (1.0e-4f));
}

TEST_CASE ("the filters stay bounded across the parameter range")
{
    SVF svf;
    svf.prepare (48000.0);

    for (int cutoff = 0; cutoff <= 127; cutoff += 7)
    {
        for (const int resonance : { 0, 63, 127 })
        {
            svf.reset();
            svf.setParameters ((float) cutoff, (float) resonance);

            float peak = 0.0f;
            bool finite = true;

            for (int i = 0; i < 20000; ++i)
            {
                if (i % Calibration::kFilterControlRate == 0)
                    svf.setParameters ((float) cutoff, (float) resonance);

                const auto y = svf.processLowpass ((i % 2 == 0) ? 1.0f : -1.0f);
                finite = finite && std::isfinite (y);
                peak = std::max (peak, std::abs (y));
            }

            INFO ("cutoff " << cutoff << " resonance " << resonance);
            REQUIRE (finite);
            REQUIRE (peak < 50.0f);
        }
    }

    // Extreme coefficient jumps at the control rate stay bounded.
    svf.reset();
    float jumpPeak = 0.0f;
    bool jumpFinite = true;

    for (int i = 0; i < 40000; ++i)
    {
        if (i % Calibration::kFilterControlRate == 0)
            svf.setParameters ((i % 32 == 0) ? 0.0f : 127.0f, (i % 32 == 0) ? 127.0f : 0.0f);

        const auto y = svf.processLowpass ((i % 2 == 0) ? 1.0f : -1.0f);
        jumpFinite = jumpFinite && std::isfinite (y);
        jumpPeak = std::max (jumpPeak, std::abs (y));
    }

    REQUIRE (jumpFinite);
    REQUIRE (jumpPeak < 50.0f);

    Biquad biquad;
    biquad.prepare (48000.0);

    for (int cutoff = 0; cutoff <= 127; cutoff += 15)
    {
        for (const int resonance : { 0, 127 })
        {
            biquad.reset();
            biquad.setParameters ((float) cutoff, (float) resonance);

            float peak = 0.0f;
            bool finite = true;

            for (int i = 0; i < 20000; ++i)
            {
                const auto y = biquad.process ((i % 2 == 0) ? 1.0f : -1.0f);
                finite = finite && std::isfinite (y);
                peak = std::max (peak, std::abs (y));
            }

            INFO ("pkg cutoff " << cutoff << " resonance " << resonance);
            REQUIRE (finite);
            REQUIRE (peak < 50.0f);
        }
    }
}

TEST_CASE ("reset clears the filter state")
{
    SVF svf;
    svf.prepare (48000.0);
    svf.setCoefficients (1000.0f, 8.0f);

    for (int i = 0; i < 100; ++i)
        svf.processLowpass (1.0f);

    svf.reset();
    REQUIRE (svf.processLowpass (0.0f) == 0.0f);
    REQUIRE (svf.processBandpass (0.0f) == 0.0f);
    REQUIRE (svf.processHighpass (0.0f) == 0.0f);

    Biquad biquad;
    biquad.prepare (48000.0);
    biquad.setPeaking (1000.0f, 1.0f, 12.0f);

    for (int i = 0; i < 100; ++i)
        biquad.process (1.0f);

    biquad.reset();
    REQUIRE (biquad.process (0.0f) == 0.0f);
}
