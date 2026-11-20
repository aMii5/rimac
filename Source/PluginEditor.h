#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Theme.h"
#include "ui/Knob.h"
#include "ui/EQDisplay.h"
#include "ui/PortraitPanel.h"

namespace rimac
{
class RimacAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit RimacAudioProcessorEditor (RimacAudioProcessor&);
    ~RimacAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (ArcKnob& knob, const juce::String& caption, const char* paramId);
    void syncFromParams();
    void setParam (const char* id, double realValue);
    void drawHeader (juce::Graphics&);

    RimacAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    ArcKnob depthKnob, attackKnob, releaseKnob;
    SegmentedPill modePill { juce::StringArray { "Soft", "Hard" } };
    RadioList processingList { juce::StringArray { "Mid", "Side" } };

    EQDisplay eqDisplay;

    MiniSwitch eqSwitch { "eqOn" };
    MiniSwitch scSwitch { "scOn" };

    ValueStepper eqFreqStepper { "Freq", 20.0, 20000.0, 1.0 };
    ValueStepper eqGainStepper { "Gain", -24.0, 12.0, 0.1 };
    ValueStepper scFreqStepper { "Freq", 20.0, 20000.0, 1.0 };
    ValueStepper scSensStepper { "Sens", -60.0, 0.0, 0.1 };

    IconButton bypassButton { "Power" };
    IconButton settingsButton { "Settings" };
    juce::ComboBox presetBox;

    PortraitPanel portrait;

    juce::Rectangle<float> headerArea, topRowArea, displayArea, bottomArea;
    juce::Rectangle<float> modeArea, depthArea, attackArea, releaseArea;
    juce::Rectangle<float> eqArea, scArea, procArea;
    juce::Rectangle<float> portraitArea;
    int previousProcRightEdge = 0;
    juce::Rectangle<float> scIconArea;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, attackAtt, releaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAtt, scOnAtt, bypassAtt;

    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RimacAudioProcessorEditor)
};
} // namespace rimac
