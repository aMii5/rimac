#include "PortraitPanel.h"

#if RIMAC_HAS_PORTRAIT
 #include "BinaryData.h"
#endif

namespace rimac
{
PortraitPanel::PortraitPanel()
{
   #if RIMAC_HAS_PORTRAIT
    if (BinaryData::portrait_pngSize > 0)
        artwork = juce::ImageFileFormat::loadFrom (BinaryData::portrait_png,
                                                   (size_t) BinaryData::portrait_pngSize);
   #endif
}

void PortraitPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff040309));
    g.fillRect (b);

    if (artwork.isValid())
    {
        juce::Graphics::ScopedSaveState save (g);

        juce::Path frame;
        frame.addRoundedRectangle (b, 6.0f);
        g.reduceClipRegion (frame);

        // Cover the panel preserving the artwork's aspect ratio (crop overflow).
        const auto panelAR = b.getWidth() / juce::jmax (1.0f, b.getHeight());
        const auto imgAR   = (float) artwork.getWidth() / (float) artwork.getHeight();

        juce::Rectangle<float> dest;

        if (imgAR > panelAR)
            dest = juce::Rectangle<float> (b.getHeight() * imgAR, b.getHeight()).withCentre (b.getCentre());
        else
            dest = juce::Rectangle<float> (b.getWidth(), b.getWidth() / imgAR)
                       .withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.46f });

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (artwork, dest, juce::RectanglePlacement::stretchToFit, false);

        // Very light violet wash so it belongs to the palette without
        // destroying the original contrast.
        juce::ColourGradient tint (theme::accent.withAlpha (0.10f), b.getCentreX(), b.getY(),
                                   theme::accentDeep.withAlpha (0.02f), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (tint);
        g.fillRect (b);
    }
    else
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (b.toNearestInt());

        // ---- backdrop -----------------------------------------------------
        juce::ColourGradient bg (juce::Colour (0xff171029), b.getCentreX(), b.getY(),
                                 juce::Colour (0xff030207), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRect (b);

        // Soft violet halo behind the subject.
        juce::ColourGradient halo (theme::accent.withAlpha (0.20f), b.getCentreX(), b.getCentreY() * 0.78f,
                                   juce::Colours::transparentBlack, b.getCentreX(), b.getBottom(), true);
        g.setGradientFill (halo);
        g.fillRect (b);

        // ---- subject silhouette ------------------------------------------
        const auto cx = b.getCentreX();
        const auto headW = b.getWidth() * 0.60f;
        const auto headH = b.getHeight() * 0.62f;
        const auto headTop = b.getY() + b.getHeight() * 0.14f;

        juce::Path subject;
        subject.addEllipse (cx - headW * 0.5f, headTop, headW, headH);
        subject.addRoundedRectangle (cx - headW * 0.78f, headTop + headH * 0.72f,
                                     headW * 1.56f, headH * 0.95f, headW * 0.28f);

        g.setColour (juce::Colour (0xff0b0810));
        g.fillPath (subject);

        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (subject);

            // Halftone dots whose density follows a diagonal gradient.
            juce::Random rnd (0x5a17c3);
            const auto step = juce::jmax (5.0f, b.getWidth() * 0.021f);

            for (float y = b.getY(); y < b.getBottom(); y += step)
            {
                for (float x = b.getX(); x < b.getRight(); x += step)
                {
                    const auto nx = (x - b.getX()) / b.getWidth();
                    const auto ny = (y - b.getY()) / b.getHeight();

                    // light from the upper left, plus a face-ish falloff
                    const auto radial = 1.0f - juce::jlimit (0.0f, 1.0f,
                                                             juce::Point<float> (nx, ny)
                                                                 .getDistanceFrom ({ 0.5f, 0.42f }) * 1.7f);
                    const auto shade = juce::jlimit (0.0f, 1.0f,
                                                     radial * 0.85f + (1.0f - nx) * 0.25f + (1.0f - ny) * 0.2f);
                    const auto d = step * (0.16f + shade * 0.78f) * (0.75f + rnd.nextFloat() * 0.45f);

                    g.setColour (juce::Colour (0xffd9cdf0).withAlpha (0.20f + shade * 0.78f));
                    g.fillEllipse (x, y, d, d);
                }
            }

            // Glitch strips: horizontal slices pushed sideways.
            const int strips = 7;
            for (int i = 0; i < strips; ++i)
            {
                const auto sy = b.getY() + b.getHeight() * (0.18f + 0.09f * (float) i);
                const auto sh = b.getHeight() * (0.008f + 0.006f * (float) (i % 3));
                const auto dx = (i % 2 == 0 ? 1.0f : -1.0f) * b.getWidth() * (0.02f + 0.02f * (float) (i % 4));

                g.setColour (theme::accentGlow.withAlpha (0.13f));
                g.fillRect (b.getX() + dx, sy, b.getWidth(), sh);
            }

            // Scanlines.
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            for (float y = b.getY(); y < b.getBottom(); y += 3.0f)
                g.fillRect (b.getX(), y, b.getWidth(), 1.0f);
        }

        // Face details: eyes + mouth, kept minimal and graphic.
        g.setColour (juce::Colour (0xff040308));
        const auto eyeY = headTop + headH * 0.36f;
        const auto eyeW = headW * 0.17f;
        const auto eyeH = headH * 0.055f;
        g.fillEllipse (cx - headW * 0.29f, eyeY, eyeW, eyeH);
        g.fillEllipse (cx + headW * 0.12f, eyeY, eyeW, eyeH);

        g.setColour (juce::Colour (0xff040308));
        g.fillRoundedRectangle (cx - headW * 0.17f, eyeY + headH * 0.30f,
                                headW * 0.34f, headH * 0.022f, 2.0f);

        // Rim light on the silhouette.
        juce::Path rim = subject;
        g.setColour (theme::accent.withAlpha (0.45f));
        g.strokePath (rim, juce::PathStrokeType (1.3f));

        // Vignette.
        juce::ColourGradient vig (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                  juce::Colours::black.withAlpha (0.75f), b.getCentreX(), b.getBottom(), true);
        g.setGradientFill (vig);
        g.fillRect (b);
    }

    // --- output meter ------------------------------------------------------
    auto meter = b.removeFromRight (16.0f).reduced (4.0f, 12.0f);
    const int bars = 16;
    const auto barH = meter.getHeight() / (float) bars;
    const auto lit = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);

    for (int i = 0; i < bars; ++i)
    {
        const auto t = 1.0f - (float) i / (float) (bars - 1);
        const auto y = meter.getY() + (float) i * barH;
        const bool on = t < lit;

        g.setColour (on ? theme::accentBright.withAlpha (0.9f)
                        : theme::accentDeep.withAlpha (0.20f));
        g.fillRoundedRectangle (meter.getX(), y + barH * 0.14f,
                                meter.getWidth(), barH * 0.66f, 1.0f);
    }

    g.setColour (theme::stroke);
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}
} // namespace rimac
