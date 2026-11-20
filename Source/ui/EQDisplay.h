#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../dsp/SpectralSuppressor.h"

namespace rimac
{
class RimacAudioProcessor;

/** Spectrum analyser with an interactive reduction curve.

    The violet line is the current suppression curve (0 dB flat at the top,
    dipping where resonances are being tamed). Three draggable nodes shift
    the curve shape: node 1 sets the low focus, node 2 the main dip, node 3
    the high shelf.  Nodes are draggable horizontally and vertically.
*/
class EQDisplay : public juce::Component,
                  private juce::Timer
{
public:
    explicit EQDisplay (RimacAudioProcessor& processorToUse);
    ~EQDisplay() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    /** Called when the user drags a node; used by the editor to keep the
        EQ / Gain steppers in sync. */
    std::function<void (float freqHz, float gainDb)> onNodeMoved;

private:
    void timerCallback() override;

    juce::Rectangle<float> plotArea() const;    float freqToX (float hz) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;

    juce::Point<float> nodePosition (int index) const;
    void applyNode (int index);

    juce::AudioProcessorValueTreeState& apvts;
    RimacAudioProcessor& processor;

    std::array<float, SpectralSuppressor::numBins> spectrum {};
    std::array<float, SpectralSuppressor::numBins> smoothedSpectrum {};
    bool haveSpectrum = false;

    // Three control nodes: [freq, gain]
    struct Node { float freq, gain; };
    std::array<Node, 3> nodes { { { 150.0f, -7.0f }, { 1300.0f, -13.0f }, { 9000.0f, -2.0f } } };

    int draggingNode = -1;
    int hoverNode = -1;
    float phase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDisplay)
};
} // namespace rimac
