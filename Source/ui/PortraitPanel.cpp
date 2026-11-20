#include "PortraitPanel.h"

namespace rimac
{
PortraitPanel::PortraitPanel()
{
}

void PortraitPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff050409));
    g.fillRect (b);

    if (artwork.isValid())
    {
        const auto scale = juce::jmax (b.getWidth() / (float) artwork.getWidth(),
                                       b.getHeight() / (float) artwork.getHeight());
        auto src = juce::Rectangle<float> (0.0f, 0.0f,
                                           b.getWidth() / scale, b.getHeight() / scale)
                       .withCentre ({ artwork.getWidth() * 0.5f, artwork.getHeight() * 0.42f });

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (artwork, b, src, true);

        // violet tint + vignette so it belongs to the palette
        juce::ColourGradient tint (theme::accent.withAlpha (0.20f), b.getCentreX(), b.getY(),
                                   theme::accentDeep.withAlpha (0.05f), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (tint);
        g.setBlendMode (juce::BlendMode::screen);
        g.fillRect (b);
        g.setBlendMode (juce::BlendMode::normal);
    }
    else
    {
        // --- generated fallback: halftone "face" --------------------------
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (b.toNearestInt());

        juce::ColourGradient bg (juce::Colour (0xff120d1c), b.getCentreX(), b.getY(),
                                 juce::Colour (0xff040309), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRect (b);

        const auto cx = b.getCentreX();
        const auto headW = b.getWidth() * 0.62f;
        const auto headH = b.getHeight() * 0.60f;
        auto head = juce::Rectangle<float> (headW, headH)
                        .withCentre ({ cx, b.getY() + b.getHeight() * 0.44f });

        juce::Path skull;
        skull.addEllipse (head);
        skull.addRoundedRectangle (head.getX() + headW * 0.18f,
                                   head.getBottom() - headH * 0.04f,
                                   headW * 0.64f, headH * 0.46f, 30.0f);

        g.setColour (juce::Colour (0xff0d0a14));
        g.fillPath (skull);

        juce::Graphics::ScopedSaveState clip (g);
        juce::Path clipPath (skull);
        g.reduceClipRegion (clipPath.toNearestInt());

        theme::drawHalftoneField (g, head.expanded (headW * 0.12f), 0.85f,
                                  juce::jmax (2.0f, b.getWidth() * 0.010f),
                                  juce::jmax (5.0f, b.getWidth() * 0.026f));

        // eyes + mouth slits
        g.setColour (juce::Colour (0xff06050a));
        g.fillEllipse (cx - headW * 0.26f, head.getY() + headH * 0.30f,
                       headW * 0.20f, headH * 0.09f);
        g.fillEllipse (cx + headW * 0.06f, head.getY() + headH * 0.30f,
                       headW * 0.20f, headH * 0.09f);

        g.setColour (juce::Colour (0xff06050a));
        g.fillRoundedRectangle (cx - headW * 0.20f, head.getY() + headH * 0.63f,
                                headW * 0.40f, headH * 0.035f, 3.0f);

        g.setColour (theme::accent.withAlpha (0.5f));
        g.drawPath (skull, juce::PathStrokeType (1.4f));
    }

    // --- level meter overlay (thin violet bars on the right) ---------------
    auto meter = b.removeFromRight (18.0f).reduced (5.0f, 10.0f);
    const int bars = 18;
    const auto barH = meter.getHeight() / (float) bars;
    const auto lit = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);

    for (int i = 0; i < bars; ++i)
    {
        const auto t = 1.0f - (float) i / (float) (bars - 1);
        const auto y = meter.getY() + (float) i * barH;
        const bool on = t < lit;

        g.setColour (on ? theme::accentBright.withAlpha (0.85f)
                        : theme::accentDeep.withAlpha (0.22f));
        g.fillRoundedRectangle (meter.getX(), y + barH * 0.12f, meter.getWidth(), barH * 0.72f, 1.0f);
    }

    g.setColour (theme::textFaint);
    g.setFont (theme::font (8.0f));
    g.drawText ("OUT", meter.translated (0.0f, meter.getHeight() + 2.0f)
                           .withHeight (10.0f),
                juce::Justification::centred, false);

    // frame
    g.setColour (theme::stroke);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}
} // namespace rimac
