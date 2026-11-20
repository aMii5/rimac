#include "EQDisplay.h"
#include "../PluginProcessor.h"

namespace rimac
{
namespace
{
    constexpr float minDb = -54.0f;
    constexpr float maxDb = 6.0f;
}

EQDisplay::EQDisplay (RimacAudioProcessor& p) : apvts (p.apvts), processor (p)
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::NormalCursor);
    startTimerHz (30);

    if (auto* p = apvts.getRawParameterValue (ids::eqFreq))
        nodes[1].freq = p->load();
    if (auto* p = apvts.getRawParameterValue (ids::eqGain))
        nodes[1].gain = p->load();
}

EQDisplay::~EQDisplay() { stopTimer(); }

void EQDisplay::resized() {}

juce::Rectangle<float> EQDisplay::plotArea() const
{
    auto b = getLocalBounds().toFloat();
    b.removeFromLeft (34.0f);    // dB scale
    b.removeFromBottom (24.0f);  // Hz scale
    b.removeFromTop (8.0f);
    b.removeFromRight (10.0f);
    return b;
}

float EQDisplay::freqToX (float hz) const
{
    auto a = plotArea();
    const auto lo = std::log10 (20.0f), hi = std::log10 (20000.0f);
    const auto t = (std::log10 (juce::jlimit (20.0f, 20000.0f, hz)) - lo) / (hi - lo);
    return a.getX() + t * a.getWidth();
}

float EQDisplay::xToFreq (float x) const
{
    auto a = plotArea();
    const auto t = juce::jlimit (0.0f, 1.0f, (x - a.getX()) / juce::jmax (1.0f, a.getWidth()));
    const auto lo = std::log10 (20.0f), hi = std::log10 (20000.0f);
    return std::pow (10.0f, (float) (lo + t * (hi - lo)));
}

float EQDisplay::dbToY (float db) const
{
    auto a = plotArea();
    const auto t = (juce::jlimit (minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return a.getBottom() - t * a.getHeight();
}

float EQDisplay::yToDb (float y) const
{
    auto a = plotArea();
    const auto t = juce::jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
    return minDb + t * (maxDb - minDb);
}

juce::Point<float> EQDisplay::nodePosition (int index) const
{
    const auto& n = nodes[(size_t) index];
    return { freqToX (n.freq), dbToY (n.gain) };
}

void EQDisplay::applyNode (int index)
{
    auto& n = nodes[(size_t) index];
    n.freq = juce::jlimit (20.0f, 20000.0f, n.freq);
    n.gain = juce::jlimit (minDb + 4.0f, 3.0f, n.gain);

    if (index == 1)
    {
        if (auto* p = apvts.getParameter (ids::eqFreq))
            p->setValueNotifyingHost (p->convertTo0to1 (n.freq));
        if (auto* p = apvts.getParameter (ids::eqGain))
            p->setValueNotifyingHost (p->convertTo0to1 (n.gain));

        if (onNodeMoved)
            onNodeMoved (n.freq, n.gain);
    }
}

//==============================================================================
void EQDisplay::timerCallback()
{
    auto& engine = processor.getEngine();

    if (engine.readSpectrum (spectrum.data(), SpectralSuppressor::numBins))
    {
        if (! haveSpectrum)
        {
            smoothedSpectrum = spectrum;
            haveSpectrum = true;
        }
        else
        {
            for (size_t i = 0; i < spectrum.size(); ++i)
                smoothedSpectrum[i] = smoothedSpectrum[i] * 0.72f + spectrum[i] * 0.28f;
        }

        // A silent buffer still fills the bins with the -120 dB floor; treat
        // that as "no signal" so the analyser shows its idle silhouette
        // instead of a flat line stuck at the bottom.
        float peak = -120.0f;
        for (auto v : smoothedSpectrum)
            peak = juce::jmax (peak, v);

        signalPresent = peak > -78.0f;
    }

    phase += 0.06f;

    // Keep the main node in sync when the EQ steppers change the parameters.
    if (draggingNode != 1)
    {
        if (auto* f = apvts.getRawParameterValue (ids::eqFreq))
            nodes[1].freq = f->load();
        if (auto* g = apvts.getRawParameterValue (ids::eqGain))
            nodes[1].gain = g->load();
    }

    repaint();
}

//==============================================================================
void EQDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0a0810));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (theme::strokeSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    const auto plot = plotArea();

    // ---- gain-reduction read-out ------------------------------------------
    {
        const auto red = processor.getReductionDb();
        auto r = juce::Rectangle<float> (bounds.getX() + 4.0f, bounds.getY() + 5.0f,
                                         bounds.getWidth() - 14.0f, 13.0f);
        g.setColour (red < -0.4f ? theme::accentGlow : theme::textFaint);
        g.setFont (theme::monoFont (9.5f));
        g.drawText ("GR " + juce::String (red, 1) + " dB", r, juce::Justification::topRight, false);
    }

    // ---- grid --------------------------------------------------------------
    g.setColour (theme::strokeSoft.withAlpha (0.55f));
    const float dbLines[] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f };
    for (auto db : dbLines)
    {
        const auto y = dbToY (db);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (theme::textFaint);
        g.setFont (theme::font (10.0f));
        g.drawText (juce::String ((int) db) + " dB",
                    juce::Rectangle<float> (bounds.getX() + 4.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (theme::strokeSoft.withAlpha (0.55f));
    }

    const float hzLines[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                              2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (auto hz : hzLines)
    {
        const auto x = freqToX (hz);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (theme::textFaint);
        g.setFont (theme::font (10.0f));
        juce::String label = hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k"
                                           : juce::String ((int) hz);
        g.drawText (label, juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 5.0f, 32.0f, 14.0f),
                    juce::Justification::centred, false);
        g.setColour (theme::strokeSoft.withAlpha (0.55f));
    }

    // ---- live spectrum (mirrored area under the curve) --------------------
    {
        juce::Path spec;
        spec.startNewSubPath (plot.getX(), plot.getBottom());

        const int bins = SpectralSuppressor::numBins;

        // The engine's bins are linear in Hz; sample them logarithmically.
        const auto binHz = 48000.0f / (float) SpectralSuppressor::fftSize;

        const int steps = juce::jmax (64, (int) plot.getWidth());

        auto spectrumDbAt = [&] (int s) -> float
        {
            const auto t = (float) s / (float) steps;
            const auto hz = xToFreq (plot.getX() + t * plot.getWidth());
            const auto tilt = 5.0f * std::log2 (juce::jmax (100.0f, hz) / 1000.0f);

            if (haveSpectrum && signalPresent)
            {
                const auto bin = juce::jlimit (1, bins - 1, (int) std::round (hz / binHz));
                return smoothedSpectrum[(size_t) bin] + tilt - 6.0f;
            }

            // Idle: silhouette of a typical mix so the analyser never looks
            // dead before the transport rolls.
            const auto lf = std::log10 (juce::jmax (20.0f, hz));
            const auto base = -16.0f
                              - 13.0f * juce::jlimit (0.0f, 1.0f, (lf - std::log10 (60.0f)) / 2.6f)
                              + 7.0f * std::sin (lf * 6.1f)
                              + 4.5f * std::sin (lf * 15.7f)
                              + 2.5f * std::sin (lf * 31.3f);
            return base + tilt * 0.55f;
        };

        for (int s = 0; s <= steps; ++s)
        {
            const auto t = (float) s / (float) steps;
            const auto y = dbToY (juce::jlimit (minDb, maxDb, spectrumDbAt (s)));
            spec.lineTo (plot.getX() + t * plot.getWidth(), y);
        }

        spec.lineTo (plot.getRight(), plot.getBottom());
        spec.closeSubPath();

        juce::ColourGradient fill (theme::accent.withAlpha (signalPresent ? 0.55f : 0.38f),
                                   plot.getCentreX(), plot.getY(),
                                   theme::accentDeep.withAlpha (0.02f), plot.getCentreX(),
                                   plot.getBottom(), false);
        g.setGradientFill (fill);
        g.fillPath (spec);

        juce::Path top;
        bool first = true;
        for (int s = 0; s <= steps; ++s)
        {
            const auto t = (float) s / (float) steps;
            const juce::Point<float> pt (plot.getX() + t * plot.getWidth(),
                                         dbToY (juce::jlimit (minDb, maxDb, spectrumDbAt (s))));
            if (first) { top.startNewSubPath (pt); first = false; }
            else        top.lineTo (pt);
        }
        g.setColour (theme::accent.withAlpha (signalPresent ? 0.85f : 0.62f));
        g.strokePath (top, juce::PathStrokeType (1.4f));
    }

    // ---- suppression curve ------------------------------------------------
    juce::Path curve;
    constexpr int steps = 220;
    std::array<juce::Point<float>, 4> pts;
    for (int i = 0; i < 3; ++i)
        pts[(size_t) i] = nodePosition (i);

    // build a smooth curve through the three nodes with an extra anchor
    // at the far right at 0 dB
    pts[3] = { plot.getRight(), dbToY (0.5f) };

    juce::Point<float> prev = { plot.getX(), dbToY (nodes[0].gain * 0.6f) };
    curve.startNewSubPath (prev);

    auto addSegment = [&] (juce::Point<float> p0, juce::Point<float> p1)
    {
        const auto c1 = p0.translated ((p1.x - p0.x) * 0.45f, 0.0f);
        const auto c2 = p1.translated (-(p1.x - p0.x) * 0.45f, 0.0f);
        constexpr int seg = 74;
        for (int i = 1; i <= seg; ++i)
        {
            const auto t = (float) i / (float) seg;
            const auto u = 1.0f - t;
            const auto x = u * u * u * p0.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * p1.x;
            const auto y = u * u * u * p0.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * p1.y;
            curve.lineTo (x, y);
        }
    };

    addSegment (prev, pts[0]);
    addSegment (pts[0], pts[1]);
    addSegment (pts[1], pts[2]);
    addSegment (pts[2], pts[3]);

    theme::glow (g, curve, theme::accentBright, 2.4f, 9.0f);

    juce::ColourGradient stroke (theme::accentGlow, plot.getX(), plot.getY(),
                                 theme::accentBright, plot.getRight(), plot.getBottom(), false);
    g.setGradientFill (stroke);
    g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // ---- nodes ------------------------------------------------------------
    for (int i = 0; i < 3; ++i)
    {
        const auto p = nodePosition (i);
        const bool active = (i == draggingNode);
        const bool hot = (i == hoverNode);
        const auto r = active ? 9.0f : (hot ? 8.0f : 7.0f);

        g.setColour (theme::accentBright.withAlpha (active ? 0.35f : 0.20f));
        g.fillEllipse (juce::Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (p));

        g.setColour (i == 1 ? juce::Colours::white : theme::accent);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p), 1.0f);
    }
}

//==============================================================================
void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    draggingNode = -1;
    for (int i = 0; i < 3; ++i)
    {
        if (nodePosition (i).getDistanceFrom (e.position) < 18.0f)
        {
            draggingNode = i;
            break;
        }
    }

    if (draggingNode < 0 && plotArea().contains (e.position))
        draggingNode = 1;

    repaint();
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingNode < 0) return;

    auto& n = nodes[(size_t) draggingNode];
    n.freq = xToFreq (e.position.x);
    n.gain = yToDb (e.position.y);
    applyNode (draggingNode);
    repaint();
}

void EQDisplay::mouseUp (const juce::MouseEvent&)
{
    draggingNode = -1;
    repaint();
}

void EQDisplay::mouseMove (const juce::MouseEvent& e)
{
    int hover = -1;
    for (int i = 0; i < 3; ++i)
        if (nodePosition (i).getDistanceFrom (e.position) < 18.0f)
            hover = i;

    if (hover != hoverNode)
    {
        hoverNode = hover;
        setMouseCursor (hover >= 0 ? juce::MouseCursor::DraggingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}
} // namespace rimac
