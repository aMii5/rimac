#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace rimac
{
/** The right-hand artwork panel.

    Loads `portrait.png` from the binary resources when available and falls
    back to a generated halftone face so the UI never shows an empty box.
*/
class PortraitPanel : public juce::Component
{
public:
    PortraitPanel();
    ~PortraitPanel() override = default;

    void paint (juce::Graphics&) override;

    void setLevel (float db) { levelDb = db; repaint(); }

private:
    juce::Image artwork;
    float levelDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PortraitPanel)
};
} // namespace rimac
