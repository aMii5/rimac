#include "PluginEditor.h"

namespace rimac
{
namespace
{
    constexpr int defaultWidth  = 1040;
    constexpr int defaultHeight = 700;
}

//==============================================================================
RimacAudioProcessorEditor::RimacAudioProcessorEditor (RimacAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), apvts (p.apvts),
      eqDisplay (p)
{
    setResizable (true, true);
    setResizeLimits (900, 606, 1600, 1077);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (1040.0 / 700.0);

    // ---- knobs ------------------------------------------------------------
    setupKnob (depthKnob,   "Depth",   ids::depth);
    setupKnob (attackKnob,  "Attack",  ids::attack);
    setupKnob (releaseKnob, "Release", ids::release);

    // ---- mode pill --------------------------------------------------------
    addAndMakeVisible (modePill);
    modePill.onChange = [this] (int idx)
    {
        if (auto* p = apvts.getParameter (ids::mode))
            p->setValueNotifyingHost (idx == 1 ? 1.0f : 0.0f);
    };

    // ---- spectrum display -------------------------------------------------
    addAndMakeVisible (eqDisplay);
    eqDisplay.onNodeMoved = [this] (float freq, float gain)
    {
        eqFreqStepper.setValue (freq, juce::dontSendNotification);
        eqGainStepper.setValue (gain, juce::dontSendNotification);
    };

    // ---- switches ---------------------------------------------------------
    addAndMakeVisible (eqSwitch);
    addAndMakeVisible (scSwitch);
    eqOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ids::eqOn, eqSwitch);
    scOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ids::scOn, scSwitch);

    // ---- steppers ---------------------------------------------------------
    addAndMakeVisible (eqFreqStepper);
    addAndMakeVisible (eqGainStepper);
    addAndMakeVisible (scFreqStepper);
    addAndMakeVisible (scSensStepper);

    eqFreqStepper.formatter = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz"
                                                               : juce::String (juce::roundToInt (v)) + " Hz"; };
    eqGainStepper.formatter = [] (double v) { return juce::String (v, 1) + " dB"; };
    scFreqStepper.formatter = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz"
                                                               : juce::String (juce::roundToInt (v)) + " Hz"; };
    scSensStepper.formatter = [] (double v) { return juce::String (v, 1) + " dB"; };

    eqFreqStepper.onChange = [this] (double v) { setParam (ids::eqFreq, v); };
    eqGainStepper.onChange = [this] (double v) { setParam (ids::eqGain, v); };
    scFreqStepper.onChange = [this] (double v) { setParam (ids::scFreq, v); };
    scSensStepper.onChange = [this] (double v) { setParam (ids::scSens, v); };

    // ---- processing pill --------------------------------------------------
    addAndMakeVisible (processingPill);
    processingPill.onChange = [this] (int idx)
    {
        if (auto* p = apvts.getParameter (ids::processing))
            p->setValueNotifyingHost (idx == 1 ? 1.0f : 0.0f);
    };

    // ---- portrait ---------------------------------------------------------
    addAndMakeVisible (portrait);

    // ---- header -----------------------------------------------------------
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass");
    settingsButton.setTooltip (juce::String ("Rimac v") + RIMAC_VERSION);
    settingsButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon, "Rimac",
            juce::String ("Dynamic Resonance Suppressor  v") + RIMAC_VERSION
                + "\n\nLatency: " + juce::String (SpectralSuppressor::latency) + " samples"
                + "\n\nArrastra los nodos del grafico para mover el foco de la supresion.");
    };

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (presetBox);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ids::bypass, bypassButton);

    presetBox.addItemList ({ "Default Preset", "Vocal Tamer", "Aggressive" }, 1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    presetBox.setColour (juce::ComboBox::outlineColourId, theme::stroke);
    presetBox.setColour (juce::ComboBox::textColourId, theme::text);
    presetBox.setColour (juce::ComboBox::arrowColourId, theme::accentGlow);
    presetBox.onChange = [this]
    {
        processor.setCurrentProgram (presetBox.getSelectedId() - 1);
        syncFromParams();
    };

    syncFromParams();
    setSize (defaultWidth, defaultHeight);
    startTimerHz (30);
}

RimacAudioProcessorEditor::~RimacAudioProcessorEditor() { stopTimer(); }

//==============================================================================
void RimacAudioProcessorEditor::setupKnob (ArcKnob& knob, const juce::String& caption,
                                           const char* paramId)
{
    knob.setCaption (caption);
    addAndMakeVisible (knob);

    if (auto* p = apvts.getParameter (paramId))
        knob.setValue (p->getValue(), juce::dontSendNotification);

    knob.onValueChange = [this, &knob, paramId]
    {
        if (auto* p = apvts.getParameter (paramId))
            p->setValueNotifyingHost ((float) knob.getValue());
    };
}

void RimacAudioProcessorEditor::setParam (const char* id, double realValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) realValue));
}

void RimacAudioProcessorEditor::syncFromParams()
{
    auto pull = [this] (juce::Slider& knob, const char* id, auto formatter)
    {
        if (auto* p = apvts.getParameter (id))
        {
            knob.setValue (p->getValue(), juce::dontSendNotification);
            knob.setValueText (formatter (p->convertFrom0to1 (p->getValue())));
        }
    };

    pull (depthKnob, ids::depth,
          [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    pull (attackKnob, ids::attack,
          [] (float v) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; });
    pull (releaseKnob, ids::release,
          [] (float v) { return juce::String (v, v < 100.0f ? 1 : 0) + " ms"; });

    if (auto* p = apvts.getParameter (ids::mode))
        modePill.setSelectedIndex (p->getValue() > 0.5f ? 1 : 0, juce::dontSendNotification);

    if (auto* p = apvts.getParameter (ids::processing))
        processingPill.setSelectedIndex (p->getValue() > 0.5f ? 1 : 0, juce::dontSendNotification);

    auto pullStepper = [this] (ValueStepper& s, const char* id)
    {
        if (auto* p = apvts.getParameter (id))
            s.setValue (p->convertFrom0to1 (p->getValue()), juce::dontSendNotification);
    };

    pullStepper (eqFreqStepper, ids::eqFreq);
    pullStepper (eqGainStepper, ids::eqGain);
    pullStepper (scFreqStepper, ids::scFreq);
    pullStepper (scSensStepper, ids::scSens);

    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
void RimacAudioProcessorEditor::timerCallback()
{
    auto pull = [this] (juce::Slider& knob, const char* id, auto formatter)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const auto v = p->getValue();
            if (std::abs (v - knob.getValue()) > 1.0e-3)
                knob.setValue (v, juce::dontSendNotification);
            knob.setValueText (formatter (p->convertFrom0to1 (v)));
        }
    };

    pull (depthKnob, ids::depth,
          [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });
    pull (attackKnob, ids::attack,
          [] (float v) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; });
    pull (releaseKnob, ids::release,
          [] (float v) { return juce::String (v, v < 100.0f ? 1 : 0) + " ms"; });

    if (auto* p = apvts.getParameter (ids::mode))
    {
        const int idx = p->getValue() > 0.5f ? 1 : 0;
        if (modePill.getSelectedIndex() != idx)
            modePill.setSelectedIndex (idx, juce::dontSendNotification);
    }

    if (auto* p = apvts.getParameter (ids::processing))
    {
        const int idx = p->getValue() > 0.5f ? 1 : 0;
        if (processingPill.getSelectedIndex() != idx)
            processingPill.setSelectedIndex (idx, juce::dontSendNotification);
    }

    portrait.setLevel (processor.getOutputLevelDb());
    repaint (displayArea.withHeight (20.0f).translated (0.0f, -22.0f).toNearestInt());
}

//==============================================================================
void RimacAudioProcessorEditor::resized()
{
    auto b = getLocalBounds().toFloat().reduced (10.0f);

    headerArea = b.removeFromTop (52.0f);
    b.removeFromTop (6.0f);

    portraitArea = b.removeFromRight (juce::jlimit (200.0f, 320.0f, b.getWidth() * 0.28f));
    b.removeFromRight (10.0f);

    topRowArea = b.removeFromTop (112.0f);
    b.removeFromTop (8.0f);

    bottomArea = b.removeFromBottom (juce::jlimit (140.0f, 190.0f, b.getHeight() * 0.31f));
    b.removeFromBottom (8.0f);

    displayArea = b;

    // ---- header -----------------------------------------------------------
    auto header = headerArea.toNearestInt();
    auto headerRight = header.removeFromRight (180);
    settingsButton.setBounds (headerRight.removeFromRight (32).reduced (2));
    headerRight.removeFromRight (8);
    bypassButton.setBounds (headerRight.removeFromRight (32).reduced (2));
    headerRight.removeFromRight (14);
    presetBox.setBounds (headerRight.reduced (0, 12));

    // ---- top row ----------------------------------------------------------
    auto top = topRowArea.toNearestInt();
    top.removeFromTop (14);   // panel title strip
    modeArea = top.removeFromLeft (juce::roundToInt (top.getWidth() * 0.26f)).toFloat();
    top.removeFromLeft (6);
    const auto third = (float) top.getWidth() / 3.0f;
    depthArea   = top.removeFromLeft ((int) third).toFloat();
    attackArea  = top.removeFromLeft ((int) third).toFloat();
    releaseArea = top.toFloat();

    depthKnob.setBounds (depthArea.reduced (4.0f, 0.0f).toNearestInt());
    attackKnob.setBounds (attackArea.reduced (4.0f, 0.0f).toNearestInt());
    releaseKnob.setBounds (releaseArea.reduced (4.0f, 0.0f).toNearestInt());

    auto modeRect = modeArea.reduced (8.0f, 0.0f).toNearestInt();
    modeRect.removeFromTop (14);
    modePill.setBounds (modeRect.removeFromTop (36));

    // ---- display ----------------------------------------------------------
    eqDisplay.setBounds (displayArea.toNearestInt());
    portrait.setBounds (portraitArea.toNearestInt());

    // ---- bottom -----------------------------------------------------------
    const auto gap = 10.0f;
    const auto w = (bottomArea.getWidth() - gap * 2.0f) / 3.0f;
    auto bottom = bottomArea;

    eqArea = bottom.removeFromLeft (w);
    bottom.removeFromLeft (gap);
    scArea = bottom.removeFromLeft (w);
    bottom.removeFromLeft (gap);
    procArea = bottom;

    auto eqR = eqArea.toNearestInt().reduced (12, 8);
    eqR.removeFromTop (24);
    eqSwitch.setBounds (eqR.removeFromTop (22).removeFromRight (40));
    auto eqRow = eqR.removeFromBottom (34);
    eqFreqStepper.setBounds (eqRow.removeFromLeft (eqRow.getWidth() / 2).reduced (3, 0));
    eqGainStepper.setBounds (eqRow.reduced (3, 0));

    auto scR = scArea.toNearestInt().reduced (12, 8);
    scR.removeFromTop (24);
    scSwitch.setBounds (scR.removeFromTop (22).removeFromRight (40));
    auto scRow = scR.removeFromBottom (34);
    // leave the little level meter on the left
    scRow.removeFromLeft (juce::jmin (scRow.getWidth() / 3, 52));
    scFreqStepper.setBounds (scRow.removeFromLeft (scRow.getWidth() / 2).reduced (3, 0));
    scSensStepper.setBounds (scRow.reduced (3, 0));

    auto procR = procArea.toNearestInt().reduced (14, 8);
    procR.removeFromTop (24);
    processingPill.setBounds (procR.removeFromTop (32).withTrimmedRight (14));
}

//==============================================================================
void RimacAudioProcessorEditor::drawHeader (juce::Graphics& g)
{
    auto h = headerArea;

    // ---- logo -------------------------------------------------------------
    auto logoArea = h.removeFromLeft (38.0f).withSizeKeepingCentre (32.0f, 30.0f);
    juce::Path wave;
    const auto cy = logoArea.getCentreY();
    const int points = 25;
    for (int i = 0; i < points; ++i)
    {
        const auto t = (float) i / (float) (points - 1);
        const auto x = logoArea.getX() + t * logoArea.getWidth();
        const auto amps = std::array<float, 5> { 0.10f, 0.55f, 0.18f, 0.85f, 0.12f };
        const float amp = amps[(size_t) juce::jlimit (0, 4, i * 5 / points)];
        const auto y = cy + (i % 2 == 0 ? -1.0f : 1.0f) * amp * logoArea.getHeight() * 0.45f;
        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }
    theme::glow (g, wave, theme::accentBright, 1.6f, 6.0f);
    g.setColour (theme::accentGlow);
    g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    h.removeFromLeft (6.0f);
    auto titleArea = h.removeFromTop (30.0f);
    g.setColour (theme::text);
    g.setFont (theme::displayFont (23.0f));
    g.drawText ("Rimac", titleArea, juce::Justification::bottomLeft, false);

    g.setColour (theme::textFaint);
    g.setFont (theme::font (9.5f));
    g.drawText (juce::String::fromUTF8 ("RESONANCE  SUPPRESSOR"), h.removeFromTop (14.0f),
                juce::Justification::topLeft, false);

    // ---- custom drawn power / settings glyphs ----------------------------
    auto drawPower = [&] (juce::Rectangle<int> r, bool on)
    {
        auto f = r.toFloat().withSizeKeepingCentre (18.0f, 18.0f);
        const auto c = theme::textDim.withAlpha (0.85f);

        juce::Path arc;
        arc.addCentredArc (f.getCentreX(), f.getCentreY(), 6.5f, 6.5f, 0.0f,
                           juce::MathConstants<float>::pi * 0.32f,
                           juce::MathConstants<float>::pi * 1.68f, false);
        g.setColour (c);
        g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.drawLine (f.getCentreX(), f.getY() + 1.0f, f.getCentreX(), f.getCentreY() - 1.0f, 1.8f);
        juce::ignoreUnused (on);
    };

    auto drawGear = [&] (juce::Rectangle<int> r)
    {
        auto f = r.toFloat().withSizeKeepingCentre (16.0f, 16.0f);
        g.setColour (theme::textDim.withAlpha (0.85f));
        g.drawEllipse (f.reduced (2.0f), 1.6f);
        g.drawEllipse (f.reduced (6.0f), 1.4f);
        for (int i = 0; i < 6; ++i)
        {
            const auto a = (float) i * juce::MathConstants<float>::twoPi / 6.0f;
            const auto p1 = f.getCentre() + juce::Point<float> (std::sin (a), -std::cos (a)) * 6.0f;
            const auto p2 = f.getCentre() + juce::Point<float> (std::sin (a), -std::cos (a)) * 8.5f;
            g.drawLine ({ p1, p2 }, 1.6f);
        }
    };

    drawPower (bypassButton.getBounds(), ! bypassButton.getToggleState());
    drawGear (settingsButton.getBounds());
}

//==============================================================================
void RimacAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    {
        juce::ColourGradient glowGrad (theme::accentDeep.withAlpha (0.20f),
                                       getWidth() * 0.70f, 0.0f,
                                       juce::Colours::transparentBlack,
                                       getWidth() * 0.30f, (float) getHeight(), true);
        g.setGradientFill (glowGrad);
        g.fillAll();
    }

    drawHeader (g);

    // ---- top row container ------------------------------------------------
    auto topPanel = topRowArea;
    theme::drawPanel (g, topPanel, 10.0f);

    g.setColour (theme::text);
    g.setFont (theme::font (13.5f, true));
    g.drawText ("Modo", modeArea.reduced (12.0f, 0.0f).withHeight (16.0f).translated (0.0f, 4.0f),
                juce::Justification::topLeft, false);

    for (auto x : { depthArea.getX() - 4.0f, attackArea.getX() - 4.0f, releaseArea.getX() - 4.0f })
    {
        g.setColour (theme::strokeSoft);
        g.drawVerticalLine ((int) x, topPanel.getY() + 10.0f, topPanel.getBottom() - 10.0f);
    }

    // ---- bottom panels ----------------------------------------------------
    for (auto r : { eqArea, scArea, procArea })
        theme::drawPanel (g, r, 10.0f);

    g.setColour (theme::text);
    g.setFont (theme::font (13.0f, true));
    g.drawText (juce::String::fromUTF8 ("Ecualizaci\xc3\xb3n"),
                eqArea.reduced (13.0f, 8.0f).withHeight (18.0f), juce::Justification::topLeft, false);
    g.drawText ("Sidechain", scArea.reduced (13.0f, 8.0f).withHeight (18.0f),
                juce::Justification::topLeft, false);
    g.drawText ("Procesamiento", procArea.reduced (14.0f, 8.0f).withHeight (18.0f),
                juce::Justification::topLeft, false);

    // ---- mini EQ curve preview -------------------------------------------
    {
        auto r = eqArea.reduced (18.0f, 8.0f);
        r.removeFromTop (44.0f);
        r.removeFromBottom (38.0f);

        if (r.getHeight() > 10.0f)
        {
            juce::Path p;
            p.startNewSubPath (r.getX(), r.getCentreY());
            p.cubicTo (r.getX() + r.getWidth() * 0.28f, r.getCentreY() - r.getHeight() * 0.55f,
                       r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.10f,
                       r.getX() + r.getWidth() * 0.55f, r.getCentreY() - r.getHeight() * 0.18f);
            p.cubicTo (r.getX() + r.getWidth() * 0.72f, r.getY(),
                       r.getX() + r.getWidth() * 0.86f, r.getY(),
                       r.getRight(), r.getCentreY() - r.getHeight() * 0.12f);

            g.setColour (theme::accent.withAlpha (0.9f));
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

            auto dot = [&] (float t)
            {
                const auto pt = p.getPointAlongPath (p.getLength() * t);
                g.setColour (theme::accentBright);
                g.fillEllipse (pt.x - 3.2f, pt.y - 3.2f, 6.4f, 6.4f);
            };
            dot (0.04f); dot (0.24f); dot (0.88f);
        }
    }

    // ---- sidechain level icon ---------------------------------------------
    {
        auto r = scArea.reduced (16.0f, 8.0f);
        r.removeFromTop (50.0f);
        r.removeFromBottom (40.0f);
        r = r.removeFromLeft (juce::jmin (r.getWidth() * 0.30f, 50.0f));

        if (r.getHeight() > 12.0f)
        {
            const int bars = 6;
            const auto bw = r.getWidth() / (float) bars;
            const auto lvl = juce::jlimit (0.0f, 1.0f, (processor.getOutputLevelDb() + 60.0f) / 60.0f);
            const float heights[] = { 0.45f, 0.75f, 1.0f, 0.6f, 0.85f, 0.35f };

            for (int i = 0; i < bars; ++i)
            {
                const auto h = r.getHeight() * heights[i] * (0.35f + 0.65f * lvl);
                g.setColour (theme::accentBright.withAlpha (0.35f + 0.5f * lvl));
                g.fillRoundedRectangle (r.getX() + (float) i * bw + 1.4f,
                                        r.getBottom() - h, bw - 2.8f, h, 1.5f);
            }
        }
    }

    // ---- processing panel: Mid/Side radio dots ----------------------------
    {
        auto r = procArea.reduced (18.0f, 8.0f);
        r.removeFromTop (58.0f);

        const auto selected = processingPill.getSelectedIndex();
        for (int i = 0; i < 2; ++i)
        {
            auto row = r.removeFromTop (26);
            auto dotArea = row.removeFromLeft (22).toFloat().withSizeKeepingCentre (14.0f, 14.0f);
            g.setColour (i == selected ? theme::accent : juce::Colours::transparentBlack);
            g.fillEllipse (dotArea);
            g.setColour (i == selected ? theme::accentGlow : theme::stroke);
            g.drawEllipse (dotArea.reduced (0.5f), i == selected ? 1.6f : 1.2f);
        }
    }

    // ---- footer -----------------------------------------------------------
    g.setColour (theme::textFaint.withAlpha (0.75f));
    g.setFont (theme::font (9.0f));
    g.drawText ("RIMAC  v" RIMAC_VERSION, getLocalBounds().reduced (14, 5).removeFromBottom (12),
                juce::Justification::bottomRight, false);

    // ---- reduction read-out ----------------------------------------------
    {
        auto r = eqDisplay.getBounds().toFloat().removeFromTop (20.0f).translated (-6.0f, -18.0f);
        const auto red = processor.getReductionDb();
        g.setColour (red < -0.4f ? theme::accentGlow : theme::textFaint);
        g.setFont (theme::monoFont (10.0f));
        g.drawText ("GR " + juce::String (red, 1) + " dB", r, juce::Justification::centredRight, false);
    }
}
} // namespace rimac
