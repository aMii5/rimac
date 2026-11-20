#include "SpectralSuppressor.h"

namespace rimac
{
//==============================================================================
void SpectralSuppressor::Fifo::init (int capacity)
{
    buf.assign ((size_t) juce::jmax (1, capacity), 0.0f);
    clear();
}

void SpectralSuppressor::Fifo::clear()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    head = 0;
    count = 0;
}

void SpectralSuppressor::Fifo::push (const float* d, int n)
{
    const int size = (int) buf.size();
    for (int i = 0; i < n; ++i)
        buf[(size_t) ((head + count + i) % size)] = d[i];
    count += n;
    jassert (count <= size);
}

void SpectralSuppressor::Fifo::pop (float* d, int n)
{
    const int size = (int) buf.size();
    for (int i = 0; i < n; ++i)
        d[i] = buf[(size_t) ((head + i) % size)];
    head = (head + n) % size;
    count -= n;
}

void SpectralSuppressor::Fifo::popDiscard (int n)
{
    const int size = (int) buf.size();
    head = (head + n) % size;
    count -= n;
}

//==============================================================================
void SpectralSuppressor::Frame::clear()
{
    fftData.fill (0.0f);
    fifo.fill (0.0f);
    overlap.fill (0.0f);
    magDb.fill (-120.0f);
    freqSmooth.fill (-120.0f);
    gainDb.fill (0.0f);
    smoothedGainDb.fill (0.0f);
}

//==============================================================================
void SpectralSuppressor::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    juce::ignoreUnused (numChannels);
    currentSampleRate = juce::jmax (8000.0, sampleRate);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                    * (float) i / (float) fftSize);

    for (int i = 0; i < numBins; ++i)
    {
        binFreq[(size_t) i] = (float) ((double) i * currentSampleRate / (double) fftSize);
        binWeight[(size_t) i] = 1.0f;
    }

    const int cap = juce::jmax (32768, maximumBlockSize * 4 + 4 * hopSize);
    inMid.init (cap);
    inSide.init (cap);
    outMid.init (cap);
    outSide.init (cap);

    cachedEqFreq = -1.0f;
    scCoeffFreq = -1.0f;
    scCoeffQ = -1.0f;

    updateWeights();
    updateSidechainCoefficients();
    reset();
}

void SpectralSuppressor::reset()
{
    midFrame.clear();
    sideFrame.clear();
    inMid.clear();
    inSide.clear();
    outMid.clear();
    outSide.clear();

    // The overlap-add emits its newest hop, so the engine itself is
    // zero-latency.  These zeros are the declared latency: they keep the output
    // aligned with what the host compensates for.
    std::vector<float> zeros ((size_t) latency, 0.0f);
    outMid.push (zeros.data(), latency);
    outSide.push (zeros.data(), latency);

    scEnvDb = -100.0f;
    scFilterL.reset();
    scFilterR.reset();
    uiDecimator = 0;
    reductionDb.store (0.0f, std::memory_order_relaxed);
    spectrumDirty.store (false, std::memory_order_release);
}

//==============================================================================
void SpectralSuppressor::updateWeights()
{
    if (std::abs (eqFreqHz - cachedEqFreq) < 0.5f
        && std::abs (eqGainDb - cachedEqGain) < 0.01f
        && std::abs (eqWidthOct - cachedEqWidth) < 0.01f
        && eqEnabled == cachedEqOn)
        return;

    cachedEqFreq  = eqFreqHz;
    cachedEqGain  = eqGainDb;
    cachedEqWidth = eqWidthOct;
    cachedEqOn    = eqEnabled;

    const auto focusHz = juce::jlimit (20.0f, 20000.0f, eqFreqHz);
    const auto width   = juce::jlimit (0.35f, 4.0f, eqWidthOct);

    // A negative Gain deepens the bell, a positive one opens it up.
    const auto boost = juce::jlimit (0.15f, 1.9f, 1.0f + (-eqGainDb) / 12.0f);

    for (int k = 0; k < numBins; ++k)
    {
        if (! eqEnabled)
        {
            binWeight[(size_t) k] = 1.0f;
            continue;
        }

        const auto hz  = juce::jmax (1.0f, binFreq[(size_t) k]);
        const auto oct = std::log2 (hz / focusHz);
        const auto focus = std::exp (-0.5f * (oct / width) * (oct / width));

        binWeight[(size_t) k] = juce::jlimit (0.0f, 1.0f, 0.30f + 0.70f * focus * boost);
    }
}

void SpectralSuppressor::updateSidechainCoefficients()
{
    const auto freq = juce::jlimit (40.0f, (float) currentSampleRate * 0.45f, sideFreqHz);
    const auto q = juce::jlimit (0.3f, 8.0f, sideWidth);

    if (std::abs (freq - scCoeffFreq) < 0.5f
        && std::abs (q - scCoeffQ) < 0.01f
        && scFilterL.coefficients != nullptr)
        return;

    scCoeffFreq = freq;
    scCoeffQ = q;

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (currentSampleRate,
                                                                     (double) freq, (double) q);
    scFilterL.coefficients = coeffs;
    scFilterR.coefficients = coeffs;
}

//==============================================================================
void SpectralSuppressor::computeGains (Frame& f, float depthScale, float maxCutDb)
{
    auto& magDb = f.magDb;
    for (int k = 0; k < numBins; ++k)
    {
        const auto re = f.fftData[(size_t) (2 * k)];
        const auto im = f.fftData[(size_t) (2 * k + 1)];
        magDb[(size_t) k] = juce::Decibels::gainToDecibels (std::sqrt (re * re + im * im), -120.0f);
    }

    // Local spectral average (±22 bins ≈ ±500 Hz at 48 kHz).
    constexpr int envRadius = 22;
    const float inv = 1.0f / (float) (envRadius * 2 + 1);

    for (int k = 0; k < numBins; ++k)
    {
        float sum = 0.0f;
        for (int j = -envRadius; j <= envRadius; ++j)
            sum += magDb[(size_t) juce::jlimit (0, numBins - 1, k + j)];
        f.freqSmooth[(size_t) k] = sum * inv;
    }

    // Soft reacts to broad, sustained resonances; Hard is quicker and grabs
    // narrower peaks.
    const float thresholdDb = hardMode ? 1.5f : 5.0f;
    const float slope       = hardMode ? 1.75f : 0.85f;

    float averageCut = 0.0f;
    int   activeBins = 0;

    auto& gains = f.smoothedGainDb;

    for (int k = 1; k < numBins - 1; ++k)
    {
        const auto hz = binFreq[(size_t) k];
        if (hz < 25.0f || hz > juce::jmin (19500.0f, (float) currentSampleRate * 0.46f))
            continue;

        const auto residual = magDb[(size_t) k] - f.freqSmooth[(size_t) k];
        float target = 0.0f;

        if (residual > thresholdDb)
        {
            const auto over = residual - thresholdDb;
            target = -juce::jmin (maxCutDb, over * slope * depthScale);
            target *= binWeight[(size_t) k];
        }

        const auto& s = gains[(size_t) k];
        const auto coeff = (target < s) ? attackCoeff : releaseCoeff;
        gains[(size_t) k] = coeff * s + (1.0f - coeff) * target;

        if (gains[(size_t) k] < -0.5f)
        {
            averageCut += gains[(size_t) k];
            ++activeBins;
        }
    }

    // Widen slightly in frequency: musical dips instead of a comb of notches.
    const std::array<float, numBins> tmp = gains;
    for (int k = 1; k < numBins - 1; ++k)
        gains[(size_t) k] = 0.25f * tmp[(size_t) (k - 1)]
                          + 0.50f * tmp[(size_t) k]
                          + 0.25f * tmp[(size_t) (k + 1)];

    reductionDb.store (activeBins > 0 ? averageCut / (float) activeBins : 0.0f,
                       std::memory_order_relaxed);
}

void SpectralSuppressor::applyGains (Frame& f)
{
    for (int k = 1; k < numBins - 1; ++k)
    {
        const auto db = f.smoothedGainDb[(size_t) k];
        if (db > -0.01f)
            continue;

        const auto g = juce::Decibels::decibelsToGain (db);

        f.fftData[(size_t) (2 * k)]     *= g;
        f.fftData[(size_t) (2 * k + 1)] *= g;

        const int mirror = fftSize - k;
        f.fftData[(size_t) (2 * mirror)]     *= g;
        f.fftData[(size_t) (2 * mirror + 1)] *= g;
    }
}

//==============================================================================
void SpectralSuppressor::processHop (const float* midIn, const float* sideIn,
                                    float* midOut, float* sideOut,
                                    float depthScale, float maxCutDb)
{
    Frame* frames[2] = { &midFrame, &sideFrame };
    const float* ins[2] = { midIn, sideIn };
    float* outs[2] = { midOut, sideOut };

    for (int c = 0; c < 2; ++c)
    {
        auto& f = *frames[c];

        std::memmove (f.fifo.data(), f.fifo.data() + hopSize,
                      sizeof (float) * (size_t) (fftSize - hopSize));
        std::memcpy (f.fifo.data() + (fftSize - hopSize), ins[c],
                     sizeof (float) * (size_t) hopSize);

        for (int i = 0; i < fftSize; ++i)
            f.fftData[(size_t) i] = f.fifo[(size_t) i] * window[(size_t) i];
        std::fill (f.fftData.begin() + fftSize, f.fftData.end(), 0.0f);

        fft.performRealOnlyForwardTransform (f.fftData.data(), false);

        const bool selected = processSide ? (c == 1) : (c == 0);
        if (selected)
        {
            computeGains (f, depthScale, maxCutDb);
            applyGains (f);
        }

        fft.performRealOnlyInverseTransform (f.fftData.data());

        // Overlap-add with the analysis window only: the Hann windows of four
        // consecutive frames sum to exactly 2.0 at 75% overlap.
        constexpr float olaScale = 0.5f;
        for (int i = 0; i < fftSize; ++i)
            f.overlap[(size_t) i] += f.fftData[(size_t) i] * olaScale;

        // Emit the newest hop so the engine has no hidden latency of its own.
        std::memcpy (outs[c], f.overlap.data() + (fftSize - hopSize),
                     sizeof (float) * (size_t) hopSize);

        std::memmove (f.overlap.data(), f.overlap.data() + hopSize,
                      sizeof (float) * (size_t) (fftSize - hopSize));
        std::fill (f.overlap.begin() + (fftSize - hopSize), f.overlap.end(), 0.0f);
    }

    if (++uiDecimator >= 3)
    {
        uiDecimator = 0;
        const juce::SpinLock::ScopedLockType sl (spectrumLock);
        uiSpectrum = midFrame.magDb;
        spectrumDirty.store (true, std::memory_order_release);
    }
}

//==============================================================================
void SpectralSuppressor::process (float* const* channels, int numChannels, int numSamples,
                                  const float* sidechainL, const float* sidechainR)
{
    if (numChannels <= 0 || numSamples <= 0)
        return;

    updateWeights();
    updateSidechainCoefficients();

    // ---- detector ---------------------------------------------------------
    float detectorDb = -100.0f;
    if (sidechainEnabled && sidechainL != nullptr)
    {
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = std::abs (scFilterL.processSample (sidechainL[i]));
            const auto r = (sidechainR != nullptr) ? std::abs (scFilterR.processSample (sidechainR[i]))
                                                   : l;
            peak = juce::jmax (peak, l, r);
        }
        detectorDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    }

    const auto hopMs = 1000.0 * (double) hopSize / juce::jmax (1.0, currentSampleRate);
    const auto decayPerHop = (float) (hopMs / 100.0) * 9.0f;

    if (detectorDb > scEnvDb) scEnvDb = detectorDb;
    else                      scEnvDb = juce::jmax (detectorDb, scEnvDb - decayPerHop);

    // ---- ballistics (coefficients for one hop) ----------------------------
    const auto aMs = juce::jlimit (0.1, 500.0, (double) attackMs);
    const auto rMs = juce::jlimit (1.0, 3000.0, (double) releaseMs);
    attackCoeff  = (float) std::exp (-hopMs / aMs);
    releaseCoeff = (float) std::exp (-hopMs / rMs);

    // ---- depth / sensitivity ---------------------------------------------
    const auto depthScale = bypassed ? 0.0f : juce::jlimit (0.0f, 1.0f, depth);

    float sensitivity = 1.0f;
    if (sidechainEnabled && ! bypassed)
        sensitivity = juce::jlimit (0.2f, 1.9f, 0.2f + (scEnvDb - sideSensDb) / 18.0f);

    const auto maxCutDb = hardMode ? 32.0f : 20.0f;
    const auto depthAmt = juce::jlimit (0.0f, 1.2f, depthScale * sensitivity);

    // ---- push input -------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = channels[0][i];
        const auto r = numChannels > 1 ? channels[1][i] : l;
        const float m = 0.5f * (l + r);
        const float s = 0.5f * (l - r);
        inMid.push (&m, 1);
        inSide.push (&s, 1);
    }

    float hopMidIn[hopSize], hopSideIn[hopSize];
    float hopMidOut[hopSize], hopSideOut[hopSize];

    while (inMid.count >= hopSize)
    {
        inMid.pop (hopMidIn, hopSize);
        inSide.pop (hopSideIn, hopSize);

        processHop (hopMidIn, hopSideIn, hopMidOut, hopSideOut, depthAmt, maxCutDb);

        outMid.push (hopMidOut, hopSize);
        outSide.push (hopSideOut, hopSize);
    }

    // ---- emit -------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        float m = 0.0f, s = 0.0f;
        outMid.pop (&m, 1);
        outSide.pop (&s, 1);

        if (numChannels > 1)
        {
            channels[0][i] = m + s;
            channels[1][i] = m - s;
        }
        else
        {
            channels[0][i] = m;
        }
    }
}

//==============================================================================
bool SpectralSuppressor::readSpectrum (float* dest, int numBinsOut) const
{
    if (! spectrumDirty.load (std::memory_order_acquire))
        return false;

    const juce::SpinLock::ScopedLockType sl (spectrumLock);
    const int n = juce::jmin (numBinsOut, numBins);
    std::memcpy (dest, uiSpectrum.data(), sizeof (float) * (size_t) n);
    spectrumDirty.store (false, std::memory_order_release);
    return true;
}
} // namespace rimac
