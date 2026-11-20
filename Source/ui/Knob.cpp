#include "Knob.h"

namespace rimac
{
//==============================================================================
ArcKnob::ArcKnob()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                         juce::MathConstants<float>::pi * 2.75f,
                         true);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
}

void ArcKnob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    bounds.removeFromTop (captionHeight);

    const auto pillArea = pillVisible ? bounds.removeFromBottom (pillHeight) : juce::Rectangle<float>();
    if (pillVisible) bounds.removeFromBottom (4.0f);

    const auto size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto radius = size * 0.5f;

    const auto outer = radius;
    const auto arcR  = outer * 0.88f;
    const auto thick = juce::jmax (3.0f, radius * 0.16f);

    // --- track -------------------------------------------------------------
    const auto startAngle = juce::MathConstants<float>::pi * 1.25f;
    const auto endAngle   = juce::MathConstants<float>::pi * 2.75f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);

    g.setColour (theme::track);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // --- value arc ---------------------------------------------------------
    const auto norm = (float) valueToProportionOfLength (getValue());
    if (norm > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle,
                           startAngle + (endAngle - startAngle) * norm, true);

        theme::glow (g, arc, theme::accentBright, thick, radius * 0.30f);

        juce::ColourGradient grad (theme::accentGlow, centre.x - arcR, centre.y - arcR,
                                   theme::accentDeep, centre.x + arcR, centre.y + arcR, false);
        g.setGradientFill (grad);
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // --- knob body ---------------------------------------------------------
    const auto bodyR = arcR - thick * 1.15f;
    const auto bodyRect = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre);

    g.setColour (juce::Colour (0xff141019));
    g.fillEllipse (bodyRect);

    juce::ColourGradient bodyGrad (juce::Colour (0xff3a3346), bodyRect.getCentreX(), bodyRect.getY(),
                                   juce::Colour (0xff0b0910), bodyRect.getCentreX(), bodyRect.getBottom(), false);
    bodyGrad.addColour (0.35, juce::Colour (0xff241d31));
    g.setGradientFill (bodyGrad);
    g.fillEllipse (bodyRect.reduced (1.0f));

    // concentric machining rings
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 1; i <= 3; ++i)
    {
        const auto r = bodyR * (1.0f - (float) i * 0.16f);
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 0.7f);
    }

    g.setColour (theme::stroke.withAlpha (0.9f));
    g.drawEllipse (bodyRect.reduced (0.5f), 1.0f);

    // --- pointer -----------------------------------------------------------
    const auto angle = startAngle + (endAngle - startAngle) * norm;
    const auto dir   = juce::Point<float> (std::sin (angle), -std::cos (angle));
    const auto inner = centre + dir * bodyR * 0.30f;
    const auto outerP = centre + dir * bodyR * 0.88f;

    juce::Path pointer;
    pointer.startNewSubPath (inner);
    pointer.lineTo (outerP);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.strokePath (pointer, juce::PathStrokeType (juce::jmax (1.6f, bodyR * 0.12f),
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // --- caption -----------------------------------------------------------
    if (caption.isNotEmpty())
    {
        g.setColour (theme::text);
        g.setFont (theme::font (12.0f));
        g.drawText (caption, getLocalBounds().removeFromTop ((int) captionHeight),
                    juce::Justification::centred, false);
    }

    // --- value pill --------------------------------------------------------
    if (pillVisible && ! pillArea.isEmpty())
    {
        g.setColour (juce::Colour (0xff131019));
        g.fillRoundedRectangle (pillArea.reduced (2.0f, 0.0f), 5.0f);
        g.setColour (theme::strokeSoft);
        g.drawRoundedRectangle (pillArea.reduced (2.5f, 0.5f), 5.0f, 1.0f);

        g.setColour (theme::text);
        g.setFont (theme::font (12.0f));
        g.drawText (valueText.isNotEmpty() ? valueText : juce::String (getValue(), 1),
                    pillArea, juce::Justification::centred, false);
    }
}

//==============================================================================
void MiniSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto on = getToggleState();

    const auto h = juce::jmin (b.getHeight(), 20.0f);
    const auto w = juce::jmin (b.getWidth(), 38.0f);
    auto track = juce::Rectangle<float> (w, h).withCentre (b.getCentre());

    g.setColour (on ? theme::accentDeep : juce::Colour (0xff241d31));
    g.fillRoundedRectangle (track, h * 0.5f);
    g.setColour (on ? theme::accentBright.withAlpha (0.75f) : theme::stroke);
    g.drawRoundedRectangle (track.reduced (0.5f), h * 0.5f, 1.0f);

    const auto knobD = h - 5.0f;
    const auto cx = on ? track.getRight() - knobD * 0.5f - 2.5f : track.getX() + knobD * 0.5f + 2.5f;

    g.setColour (on ? theme::accentGlow : juce::Colour (0xff6a6178));
    if (on)
        g.setColour (theme::accentGlow.withAlpha (highlighted || down ? 1.0f : 0.92f));
    g.fillEllipse (cx - knobD * 0.5f, track.getCentreY() - knobD * 0.5f, knobD, knobD);
}

//==============================================================================
SegmentedPill::SegmentedPill (juce::StringArray options) : items (std::move (options))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> SegmentedPill::segmentBounds (int index) const
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto w = b.getWidth() / (float) juce::jmax (1, items.size());
    return { b.getX() + w * (float) index, b.getY(), w, b.getHeight() };
}

void SegmentedPill::setSelectedIndex (int index, juce::NotificationType n)
{
    index = juce::jlimit (0, juce::jmax (0, items.size() - 1), index);
    if (index == selected) return;
    selected = index;
    repaint();
    if (n != juce::dontSendNotification && onChange)
        onChange (selected);
}

void SegmentedPill::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff0e0b15));
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (theme::stroke);
    g.drawRoundedRectangle (b.reduced (0.5f), b.getHeight() * 0.5f, 1.0f);

    const auto sel = segmentBounds (selected).reduced (2.0f);
    juce::ColourGradient grad (theme::accent, sel.getCentreX(), sel.getY(),
                               theme::accentDeep, sel.getCentreX(), sel.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (sel, sel.getHeight() * 0.5f);

    for (int i = 0; i < items.size(); ++i)
    {
        g.setColour (i == selected ? juce::Colours::white : theme::textDim);
        g.setFont (theme::font (13.0f, i == selected));
        g.drawText (items[i], segmentBounds (i), juce::Justification::centred, false);
    }
}

void SegmentedPill::mouseDown (const juce::MouseEvent& e)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto w = b.getWidth() / (float) juce::jmax (1, items.size());
    const int idx = juce::jlimit (0, items.size() - 1, (int) ((e.position.x - b.getX()) / w));
    setSelectedIndex (idx);
}

//==============================================================================
RadioList::RadioList (juce::StringArray options) : items (std::move (options))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void RadioList::setSelectedIndex (int index, juce::NotificationType n)
{
    index = juce::jlimit (0, juce::jmax (0, items.size() - 1), index);
    if (index == selected) return;
    selected = index;
    repaint();
    if (n != juce::dontSendNotification && onChange)
        onChange (selected);
}

void RadioList::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    b = b.withSizeKeepingCentre (b.getWidth(), juce::jmin (b.getHeight(),
                                                           (float) (26 * items.size())));

    const auto rowH = b.getHeight() / (float) juce::jmax (1, items.size());
    const auto dot = juce::jmin (14.0f, rowH * 0.55f);

    for (int i = 0; i < items.size(); ++i)
    {
        auto row = b.withY (b.getY() + rowH * (float) i).withHeight (rowH);
        auto dotArea = row.removeFromLeft (dot + 10.0f).withSizeKeepingCentre (dot, dot);

        const bool on = (i == selected);

        if (on)
        {
            g.setColour (theme::accent.withAlpha (0.30f));
            g.fillEllipse (dotArea.expanded (4.0f));
            g.setColour (theme::accent);
            g.fillEllipse (dotArea);
            g.setColour (theme::accentGlow);
            g.drawEllipse (dotArea.reduced (0.4f), 1.4f);
        }
        else
        {
            g.setColour (theme::stroke);
            g.drawEllipse (dotArea.reduced (0.7f), 1.4f);
        }

        g.setColour (on ? theme::text : theme::textDim);
        g.setFont (theme::font (13.0f, on));
        g.drawText (items[i], row, juce::Justification::centredLeft, false);
    }
}

void RadioList::mouseDown (const juce::MouseEvent& e)
{
    auto b = getLocalBounds().toFloat();
    b = b.withSizeKeepingCentre (b.getWidth(), juce::jmin (b.getHeight(),
                                                           (float) (26 * items.size())));
    const auto rowH = b.getHeight() / (float) juce::jmax (1, items.size());
    const int idx = juce::jlimit (0, items.size() - 1,
                                  (int) ((e.position.y - b.getY()) / juce::jmax (1.0f, rowH)));
    setSelectedIndex (idx);
}

//==============================================================================
ValueStepper::ValueStepper (juce::String cap, double lo, double hi, double step)
    : caption (std::move (cap)), minValue (lo), maxValue (hi), intervalValue (step),
      value (juce::jlimit (lo, hi, 0.0))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    formatter = [] (double v) { return juce::String (v, 1); };
}

void ValueStepper::setValue (double v, juce::NotificationType n)
{
    v = juce::jlimit (minValue, maxValue, v);
    // snap to the interval so repeated clicks land on exact values
    if (intervalValue > 0.0)
        v = minValue + std::round ((v - minValue) / intervalValue) * intervalValue;
    v = juce::jlimit (minValue, maxValue, v);

    const bool changed = std::abs (v - value) > 1.0e-9;
    value = v;
    repaint();
    if (changed && n != juce::dontSendNotification && onChange)
        onChange (value);
}

void ValueStepper::nudge (double delta)
{
    setValue (value + delta, juce::sendNotificationSync);
}

void ValueStepper::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    auto captionArea = b.removeFromTop (15.0f);
    g.setColour (theme::textDim);
    g.setFont (theme::font (11.0f));
    g.drawText (caption, captionArea, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff12101a));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (theme::stroke);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);

    const auto arrowW = 18.0f;
    minusBox = b.withWidth (arrowW);
    plusBox  = b.withTrimmedLeft (b.getWidth() - arrowW);

    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f, true));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x80\xb9"), minusBox, juce::Justification::centred, false);
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x80\xba"), plusBox, juce::Justification::centred, false);

    g.setColour (theme::text);
    g.setFont (theme::monoFont (12.0f));
    g.drawText (formatter ? formatter (value) : juce::String (value, 1),
                b.withTrimmedLeft (arrowW).withTrimmedRight (arrowW),
                juce::Justification::centred, false);
}

void ValueStepper::mouseDown (const juce::MouseEvent& e)
{
    if (minusBox.contains (e.position))      nudge (-intervalValue);
    else if (plusBox.contains (e.position))  nudge (intervalValue);
}

void ValueStepper::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return; // plain clicks are handled by mouseDown

    if (! dragging)
    {
        dragging = true;
        dragStartValue = value;
    }

    const auto steps = (double) -e.getDistanceFromDragStartY() * 0.35;
    setValue (dragStartValue + steps * intervalValue, juce::sendNotificationSync);
}

void ValueStepper::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}
} // namespace rimac
