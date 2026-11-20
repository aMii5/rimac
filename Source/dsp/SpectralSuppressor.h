#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace rimac
{
/** Real-time STFT resonance suppressor.

    The signal is split into Mid/Side. Both components run through the same
    windowed 4x-overlap STFT so they stay perfectly time aligned, but only the
    selected component (Mid or Side) is attenuated.  Bins that stick out above
    their local spectral average are reduced by an amount driven by Depth, then
    smoothed in frequency (to stay musical) and in time with asymmetric
    attack/release ballistics.
*/
class SpectralSuppressor
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;      // 2048
    static constexpr int hopSize  = fftSize / 4;        // 512
    static constexpr int numBins  = fftSize / 2 + 1;    // 1025
    static constexpr int latency  = fftSize - hopSize;  // 1536

    SpectralSuppressor() = default;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /** Processes in place. Sidechain pointers may be nullptr. */
    void process (float* const* channels, int numChannels, int numSamples,
                  const float* sidechainL, const float* sidechainR);

    // --- parameters -------------------------------------------------------
    float depth      = 0.70f;    // 0..1
    float attackMs   = 5.0f;
    float releaseMs  = 100.0f;
    bool  hardMode   = true;
    bool  eqEnabled  = true;
    float eqFreqHz   = 1200.0f;
    float eqGainDb   = -6.0f;
    float eqWidthOct = 1.4f;
    bool  sidechainEnabled = true;
    float sideFreqHz = 2000.0f;
    float sideSensDb = -24.0f;
    float sideWidth  = 1.0f;     // detector Q
    bool  processSide = false;
    bool  bypassed    = false;

    int getLatencySamples() const noexcept { return latency; }

    /** Latest analysis curve in dB, for the editor. */
    bool readSpectrum (float* dest, int numBinsOut) const;

    /** Current overall attenuation in dB (<= 0). */
    float getReductionDb() const noexcept { return reductionDb.load (std::memory_order_relaxed); }

private:
    struct Fifo
    {
        std::vector<float> buf;
        int head = 0, count = 0;

        void init (int capacity);
        void clear();
        void push (const float* d, int n);
        void pop (float* d, int n);
        void popDiscard (int n);
    };

    struct Frame
    {
        std::array<float, fftSize * 2> fftData {};
        std::array<float, fftSize>     fifo {};
        std::array<float, fftSize>     overlap {};
        std::array<float, numBins>     magDb {};
        std::array<float, numBins>     freqSmooth {};
        std::array<float, numBins>     gainDb {};
        std::array<float, numBins>     smoothedGainDb {};

        void clear();
    };

    void processHop (const float* midIn, const float* sideIn,
                     float* midOut, float* sideOut,
                     float depthScale, float maxCutDb);
    void computeGains (Frame& f, float depthScale, float maxCutDb);
    static void applyGains (Frame& f);
    void updateSidechainCoefficients();
    void updateWeights();

    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize> window {};

    Frame midFrame, sideFrame;

    std::array<float, numBins> binWeight {};
    std::array<float, numBins> binFreq {};

    Fifo inMid, inSide, outMid, outSide;

    double currentSampleRate = 48000.0;

    float attackCoeff = 0.0f, releaseCoeff = 0.0f;

    juce::dsp::IIR::Filter<float> scFilterL, scFilterR;
    float scEnvDb = -100.0f;
    float scCoeffFreq = -1.0f, scCoeffQ = -1.0f;
    float cachedEqFreq = -1.0f, cachedEqGain = 0.0f;
    float cachedEqWidth = -1.0f;
    bool  cachedEqOn = false;

    mutable juce::SpinLock spectrumLock;
    std::array<float, numBins> uiSpectrum {};
    std::atomic<bool> spectrumDirty { false };
    std::atomic<float> reductionDb { 0.0f };
    int uiDecimator = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralSuppressor)
};
} // namespace rimac
