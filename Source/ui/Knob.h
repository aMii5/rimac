#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace rimac
{
/** Rotary control drawn as a violet arc with a machined metal body. */
class ArcKnob : public juce::Slider
{
public:
    ArcKnob();
    ~ArcKnob() override = default;

    void paint (juce::Graphics&) override;

    void setCaption (juce::String c)  { caption = std::move (c); repaint(); }
    juce::String getCaption() const   { return caption; }

    /** Optional formatting hook for the pill under the knob. */
    std::function<juce::String (double)> valueFormatter;

    void setPillVisible (bool v)      { pillVisible = v; repaint(); }
    void setValueText (juce::String t) { valueText = std::move (t); repaint(); }

    float captionHeight = 16.0f;
    float pillHeight    = 20.0f;

private:
    juce::String caption, valueText;
    bool pillVisible = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArcKnob)
};

/** Small on/off switch with a violet track, as used in the reference design. */
class MiniSwitch : public juce::Button
{
public:
    MiniSwitch (const juce::String& id) : juce::Button (id) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics&, bool, bool) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniSwitch)
};

/** Invisible button used by the header; the editor draws its glyph in paint(). */
class IconButton : public juce::Button
{
public:
    explicit IconButton (const juce::String& id) : juce::Button (id) {}

    void paintButton (juce::Graphics&, bool, bool) override {}
};

/** Two-option segmented pill (Soft / Hard). */
class SegmentedPill : public juce::Component
{
public:
    SegmentedPill (juce::StringArray options);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int)> onChange;
    void setSelectedIndex (int index, juce::NotificationType n = juce::sendNotificationAsync);
    int getSelectedIndex() const noexcept { return selected; }

private:
    juce::Rectangle<float> segmentBounds (int index) const;

    juce::StringArray items;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedPill)
};

/** Vertical radio list (dot + label), used for Mid / Side. */
class RadioList : public juce::Component
{
public:
    RadioList (juce::StringArray options);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int)> onChange;
    void setSelectedIndex (int index, juce::NotificationType n = juce::sendNotificationAsync);
    int getSelectedIndex() const noexcept { return selected; }

private:
    juce::StringArray items;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioList)
};

/** Compact numeric stepper with < > arrows, used for Freq / Gain / Sens. */
class ValueStepper : public juce::Component
{
public:
    ValueStepper (juce::String caption, double min, double max, double interval);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setValue (double v, juce::NotificationType n = juce::dontSendNotification);
    double getValue() const noexcept { return value; }

    /** Converts a normalised 0..1 position into the displayed string. */
    std::function<juce::String (double)> formatter;

    std::function<void (double)> onChange;

private:
    void nudge (double delta);

    juce::String caption;
    double minValue, maxValue, intervalValue, value;
    double dragStartValue = 0.0;
    bool dragging = false;
    juce::Rectangle<float> minusBox, plusBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValueStepper)
};
} // namespace rimac
