#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/SpectralSuppressor.h"

namespace rimac
{
class RimacAudioProcessor : public juce::AudioProcessor
{
public:
    RimacAudioProcessor();
    ~RimacAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rimac"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    SpectralSuppressor& getEngine() noexcept { return engine; }

    float getSpectrum (float* dest, int numBins) const;
    float getReductionDb() const { return engine.getReductionDb(); }
    float getOutputLevelDb() const { return outputLevelDb.load (std::memory_order_relaxed); }

    /** Editor polling helpers. */
    float readParam (const char* id) const;

    void resetAllParameters();

private:
    SpectralSuppressor engine;
    std::atomic<float>* depthParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* eqOnParam = nullptr;
    std::atomic<float>* eqFreqParam = nullptr;
    std::atomic<float>* eqGainParam = nullptr;
    std::atomic<float>* scOnParam = nullptr;
    std::atomic<float>* scFreqParam = nullptr;
    std::atomic<float>* scSensParam = nullptr;
    std::atomic<float>* procParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    int currentProgram = 0;
    std::atomic<float> outputLevelDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RimacAudioProcessor)
};

/** Shared parameter identifiers. */
namespace ids
{
    inline constexpr const char* depth       = "depth";
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* release     = "release";
    inline constexpr const char* mode        = "mode";
    inline constexpr const char* eqOn        = "eqOn";
    inline constexpr const char* eqFreq      = "eqFreq";
    inline constexpr const char* eqGain      = "eqGain";
    inline constexpr const char* scOn        = "scOn";
    inline constexpr const char* scFreq      = "scFreq";
    inline constexpr const char* scSens      = "scSens";
    inline constexpr const char* processing  = "processing";
    inline constexpr const char* bypass      = "bypass";
}
} // namespace rimac
