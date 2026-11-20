#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rimac::theme
{
    // -------------------------------------------------------------------------
    // Palette — dark UI with a violet accent, matching the reference artwork.
    // -------------------------------------------------------------------------
    inline const juce::Colour bg          { 0xff07060b };
    inline const juce::Colour panel       { 0xff0c0a12 };
    inline const juce::Colour panelLight  { 0xff15111e };
    inline const juce::Colour stroke      { 0xff261d36 };
    inline const juce::Colour strokeSoft  { 0xff1a1425 };
    inline const juce::Colour accent      { 0xff8b5cf6 };
    inline const juce::Colour accentBright{ 0xffa855f7 };
    inline const juce::Colour accentGlow  { 0xffc084fc };
    inline const juce::Colour accentDeep  { 0xff5320b3 };
    inline const juce::Colour text        { 0xffe9e5f2 };
    inline const juce::Colour textDim     { 0xff8b8299 };
    inline const juce::Colour textFaint   { 0xff5d556b };
    inline const juce::Colour track       { 0xff221a30 };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold
                                                           : juce::Font::plain));
    }

    inline juce::Font displayFont (float height)
    {
        return font (height, true).withExtraKerningFactor (0.02f);
    }

    inline juce::Font monoFont (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              height, juce::Font::plain));
    }

    /** Soft outer glow, drawn as a set of translucent strokes. */
    inline void glow (juce::Graphics& g, const juce::Path& path, juce::Colour colour,
                      float thickness, float spread = 6.0f)
    {
        for (int i = 3; i >= 1; --i)
        {
            const auto w = thickness + spread * (float) i * 0.55f;
            g.setColour (colour.withAlpha (0.055f * (float) (4 - i)));
            g.strokePath (path, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }

    inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r, radius);
        g.setColour (stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    inline void drawLabel (juce::Graphics& g, juce::StringRef txt,
                           juce::Rectangle<int> r, juce::Colour c, float size, bool bold = false)
    {
        g.setColour (c);
        g.setFont (font (size, bold));
        g.drawText (txt, r, juce::Justification::centredLeft, true);
    }

    /** Halftone dot field used as the fallback artwork texture. */
    inline void drawHalftoneField (juce::Graphics& g, juce::Rectangle<float> r,
                                   float alpha, float dotSize = 2.6f, float spacing = 7.0f)
    {
        juce::Random rnd (0x5221ac);
        for (float y = r.getY(); y < r.getBottom(); y += spacing)
        {
            for (float x = r.getX(); x < r.getRight(); x += spacing)
            {
                const auto centred = juce::Point<float> (x, y)
                                         .transformedBy (juce::AffineTransform::rotation (0.35f, r.getCentreX(), r.getCentreY()));
                const auto nx = (centred.x - r.getX()) / juce::jmax (1.0f, r.getWidth());
                const auto ny = (centred.y - r.getY()) / juce::jmax (1.0f, r.getHeight());
                const auto radial = 1.0f - juce::jlimit (0.0f, 1.0f,
                                                         juce::Point<float> (nx, ny)
                                                             .getDistanceFrom (juce::Point<float> (0.5f, 0.46f)) * 1.9f);
                const auto d = dotSize * (0.25f + radial * 1.25f) * (0.55f + rnd.nextFloat() * 0.75f);
                g.setColour (accentGlow.withAlpha (alpha * (0.25f + radial * 0.75f)));
                g.fillEllipse (x, y, d, d);
            }
        }
    }
}
