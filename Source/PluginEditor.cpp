#include "PluginEditor.h"

namespace rimac
{
namespace
{
    constexpr int defaultWidth  = 1000;
    constexpr int defaultHeight = 620;
}

//==============================================================================
RimacAudioProcessorEditor::RimacAudioProcessorEditor (RimacAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), apvts (p.apvts),
      eqDisplay (p)
{
    setResizable (true, true);
    setResizeLimits (880, 546, 1600, 992);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (1000.0 / 620.0);

    // ---- knobs ------------------------------------------------------------
    setupKnob (depthKnob,   "Depth",   ids::depth);
    setupKnob (attackKnob,  "Attack",  ids::attack);
    setupKnob (releaseKnob, "Release", ids::release);

    depthAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ids::depth, depthKnob);
    attackAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ids::attack, attackKnob);
    releaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ids::release, releaseKnob);

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

    // ---- processing list --------------------------------------------------
    addAndMakeVisible (processingList);
    processingList.onChange = [this] (int idx)
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

    // Dev helper: when RIMAC_SNAPSHOT points at a file path, render the whole
    // editor to that PNG once and quit. Used to verify the layout headlessly.
    if (auto path = juce::SystemStats::getEnvironmentVariable ("RIMAC_SNAPSHOT", {}); path.isNotEmpty())
    {
        juce::Timer::callAfterDelay (900, [this, path]
        {
            if (auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f); img.isValid())
            {
                if (auto f = juce::File (path).createOutputStream())
                {
                    juce::PNGImageFormat png;
                    png.writeImageToStream (img, *f);
                }
            }
            juce::JUCEApplicationBase::quit();
        });
    }
}

RimacAudioProcessorEditor::~RimacAudioProcessorEditor() { stopTimer(); }

//==============================================================================
void RimacAudioProcessorEditor::setupKnob (ArcKnob& knob, const juce::String& caption,
                                           const char* paramId)
{
    knob.setCaption (caption);
    addAndMakeVisible (knob);

    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramId)))
    {
        const auto& range = rp->getNormalisableRange();
        knob.setRange ((double) range.start, (double) range.end, (double) range.interval);
    }
}

void RimacAudioProcessorEditor::setParam (const char* id, double realValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) realValue));
}

void RimacAudioProcessorEditor::syncFromParams()
{
    auto pull = [this] (ArcKnob& knob, const char* id, auto formatter)
    {
        if (auto* p = apvts.getParameter (id))
        {
            const auto real = (double) p->convertFrom0to1 (p->getValue());
            if (std::abs (real - knob.getValue()) > 1.0e-3)
                knob.setValue (real, juce::dontSendNotification);
            knob.setValueText (formatter ((float) real));
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
        processingList.setSelectedIndex (p->getValue() > 0.5f ? 1 : 0, juce::dontSendNotification);

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
    auto pull = [this] (ArcKnob& knob, const char* id, auto formatter)
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
        if (processingList.getSelectedIndex() != idx)
            processingList.setSelectedIndex (idx, juce::dontSendNotification);
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

    // The bottom row spans the full width, so reserve it before the portrait
    // column: in the reference layout the artwork stops above that row.
    bottomArea = b.removeFromBottom (juce::jlimit (146.0f, 200.0f, b.getHeight() * 0.36f));
    b.removeFromBottom (8.0f);

    portraitArea = b.removeFromRight (juce::jlimit (180.0f, 300.0f, b.getWidth() * 0.27f));
    b.removeFromRight (10.0f);

    topRowArea = b.removeFromTop (juce::jmin (106.0f, b.getHeight() * 0.26f));
    b.removeFromTop (8.0f);

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
    top.removeFromTop (22);   // panel title strip
    modeArea = top.removeFromLeft (juce::roundToInt (top.getWidth() * 0.26f)).toFloat();
    top.removeFromLeft (6);
    const auto third = (float) top.getWidth() / 3.0f;
    depthArea   = top.removeFromLeft ((int) third).toFloat();
    attackArea  = top.removeFromLeft ((int) third).toFloat();
    releaseArea = top.toFloat();

    depthKnob.setBounds (depthArea.reduced (4.0f, 0.0f).toNearestInt());
    attackKnob.setBounds (attackArea.reduced (4.0f, 0.0f).toNearestInt());
    releaseKnob.setBounds (releaseArea.reduced (4.0f, 0.0f).toNearestInt());

    auto modeRect = modeArea.reduced (14.0f, 0.0f).toNearestInt();
    modeRect.removeFromTop (20);
    modePill.setBounds (modeRect.removeFromTop (34));

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
    scR.removeFromTop (4);

    // Icon on the left spans the free height; the two steppers sit centred
    // beside it, which keeps the panel balanced at any size.
    const auto meterW = juce::jlimit (30, 54, scR.getWidth() / 4);
    scIconArea = scR.removeFromLeft (meterW).toFloat();
    scIconArea = scIconArea.withSizeKeepingCentre (scIconArea.getWidth(),
                                                   juce::jmin (scIconArea.getHeight(), 74.0f));
    scR.removeFromLeft (8);

    auto scRow = scR.withSizeKeepingCentre (scR.getWidth(),
                                            juce::jmin (68, scR.getHeight())).toNearestInt();
    scFreqStepper.setBounds (scRow.removeFromTop (33).reduced (0, 1));
    scRow.removeFromTop (2);
    scSensStepper.setBounds (scRow.removeFromTop (33).reduced (0, 1));

    auto procR = procArea.toNearestInt().reduced (14, 8);
    procR.removeFromTop (24);
    previousProcRightEdge = procR.getRight();
    processingList.setBounds (procR.removeFromTop (52).withTrimmedRight (16));

    // Children moved to new bounds leave stale pixels behind unless the whole
    // canvas is repainted.
    repaint();
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
    g.drawText ("Modo", topRowArea.reduced (14.0f, 0.0f).withHeight (17.0f).translated (0.0f, 4.0f),
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
        r.removeFromTop (46.0f);
        r.removeFromBottom (36.0f);

        if (r.getHeight() > 8.0f && r.getWidth() > 20.0f)
        {
            juce::Path p;
            const auto mid = r.getCentreY();
            const auto amp = r.getHeight() * 0.36f;

            // Mirrors the mock-up: rises from the left, holds a broad plateau
            // and falls away at the far right.
            p.startNewSubPath (r.getX(), mid + amp * 0.85f);
            p.cubicTo (r.getX() + r.getWidth() * 0.10f, mid + amp * 0.35f,
                       r.getX() + r.getWidth() * 0.16f, mid - amp * 0.55f,
                       r.getX() + r.getWidth() * 0.30f, mid - amp * 0.72f);
            p.cubicTo (r.getX() + r.getWidth() * 0.52f, mid - amp * 0.95f,
                       r.getX() + r.getWidth() * 0.72f, mid - amp * 0.92f,
                       r.getX() + r.getWidth() * 0.86f, mid - amp * 0.55f);
            p.quadraticTo (r.getRight() - 2.0f, mid - amp * 0.05f, r.getRight(), mid + amp * 0.30f);

            g.setColour (theme::accent.withAlpha (0.95f));
            g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

            auto dot = [&] (float t)
            {
                const auto pt = p.getPointAlongPath (p.getLength() * t);
                g.setColour (theme::accentBright);
                g.fillEllipse (pt.x - 3.0f, pt.y - 3.0f, 6.0f, 6.0f);
            };
            dot (0.16f); dot (0.50f); dot (0.86f);
        }
    }

    // ---- sidechain level icon ---------------------------------------------
    {
        auto r = scIconArea.reduced (0.0f, 6.0f);

        if (r.getHeight() > 12.0f && r.getWidth() > 10.0f)
        {
            theme::drawPanel (g, r, 8.0f);

            auto bars = r.reduced (8.0f, 9.0f);
            const int n = 6;
            const auto bw = bars.getWidth() / (float) n;
            const auto lvl = juce::jlimit (0.0f, 1.0f, (processor.getOutputLevelDb() + 60.0f) / 60.0f);
            const float heights[] = { 0.45f, 0.75f, 1.0f, 0.6f, 0.85f, 0.35f };

            for (int i = 0; i < n; ++i)
            {
                const auto h = bars.getHeight() * heights[i] * (0.35f + 0.65f * lvl);
                g.setColour (theme::accentBright.withAlpha (0.40f + 0.55f * lvl));
                g.fillRoundedRectangle (bars.getX() + (float) i * bw + 1.2f,
                                        bars.getBottom() - h, bw - 2.4f, h, 1.5f);
            }
        }
    }

    // ---- processing panel: L/R meter on the right -------------------------
    {
        auto meter = procArea.reduced (18.0f, 12.0f);
        meter.removeFromTop (26.0f);
        meter = meter.removeFromRight (juce::jmin (28.0f, meter.getWidth() * 0.24f));

        auto labels = meter.removeFromBottom (13.0f);

        if (meter.getHeight() > 16.0f && meter.getWidth() > 8.0f)
        {
            const int bars = 12;
            const auto bh = meter.getHeight() / (float) bars;
            const auto barW = meter.getWidth() * 0.42f;

            for (int i = 0; i < bars; ++i)
            {
                const auto t = (float) i / (float) (bars - 1);
                const auto y = meter.getY() + meter.getHeight() * t;

                g.setColour (theme::accentBright.withAlpha (0.88f - 0.42f * t));
                g.fillRoundedRectangle (meter.getX(), y, barW, bh * 0.68f, 1.0f);
                g.setColour (theme::accentBright.withAlpha (0.72f - 0.38f * t));
                g.fillRoundedRectangle (meter.getX() + meter.getWidth() - barW, y,
                                        barW, bh * 0.68f, 1.0f);
            }
        }

        g.setColour (theme::textDim);
        g.setFont (theme::font (9.0f));
        g.drawText ("L", labels, juce::Justification::centredLeft, false);
        g.drawText ("R", labels, juce::Justification::centredRight, false);
    }

    // ---- footer -----------------------------------------------------------
    g.setColour (theme::textFaint.withAlpha (0.75f));
    g.setFont (theme::font (9.0f));
    g.drawText ("RIMAC  v" RIMAC_VERSION, getLocalBounds().reduced (14, 5).removeFromBottom (12),
                juce::Justification::bottomRight, false);

    // ---- reduction read-out (drawn inside EQDisplay) ----------------------
}
} // namespace rimac
