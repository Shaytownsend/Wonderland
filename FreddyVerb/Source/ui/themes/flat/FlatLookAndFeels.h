#pragma once

#include "../../FreddyLookAndFeel.h"

namespace fv::ui
{
/** 02 Studio Black Rack: rack ears, screw dots, dense small labels. */
class StudioRackLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawRackEars (juce::Graphics&, juce::Rectangle<float>, float) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
};

/** 03 Cream Workstation: soft rounded sections, skirted knobs, pill pads. */
class CreamLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawPad (juce::Graphics&, juce::Rectangle<float>, const PadState&) override;
    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
};

/** 04 Pad Station: chunky square pads with big LEDs. */
class PadStationLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawPad (juce::Graphics&, juce::Rectangle<float>, const PadState&) override;
    void drawKnob (juce::Graphics&, juce::Rectangle<float>, const KnobState&) override;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
};

/** 05 Sunbleached '89: flat colour wear patches and uneven fading. */
class SunbleachedLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawPad (juce::Graphics&, juce::Rectangle<float>, const PadState&) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
};

/** 06 Night Session: dark, 7-segment readouts, minimal labels. */
class NightSessionLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawPad (juce::Graphics&, juce::Rectangle<float>, const PadState&) override;
    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
    float traceGlow() const override { return 0.6f; }
};

/** 07 Service Manual: two-tone line art. */
class ServiceManualLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawKnob (juce::Graphics&, juce::Rectangle<float>, const KnobState&) override;
    void drawPad (juce::Graphics&, juce::Rectangle<float>, const PadState&) override;
    juce::Rectangle<float> drawDisplayFrame (juce::Graphics&, juce::Rectangle<float>) override;
    void drawSignalArrow (juce::Graphics&, juce::Point<float>, juce::Point<float>) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
    void drawJogWheel (juce::Graphics&, juce::Rectangle<float>, float, bool) override;
};

/** 08 Floppy Era Pro: a colour band per stage. */
class FloppyLookAndFeel : public FreddyLookAndFeel
{
public:
    using FreddyLookAndFeel::FreddyLookAndFeel;
    void drawSection (juce::Graphics&, juce::Rectangle<float>, const SectionInfo&) override;
    void drawChassis (juce::Graphics&, juce::Rectangle<float>) override;
    void drawPanelDetail (juce::Graphics&, juce::Rectangle<float>) override;
    static juce::Colour stripeColour (int sectionIndex);
};
} // namespace fv::ui
