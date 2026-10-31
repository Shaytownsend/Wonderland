#pragma once

#include "../PluginProcessor.h"
#include "QueenKeyboard.h"
#include "QueenLookAndFeel.h"

namespace queen
{
// Common interface so tests (and the panel) can drive any control like a user would.
struct PanelControl
{
    virtual ~PanelControl() = default;
    virtual void simulateUserChange() = 0; // move the control to a different position
};

class KnobControl : public juce::Slider, public PanelControl
{
public:
    KnobControl (const ControlSpec&, juce::AudioProcessorValueTreeState&, QueenLookAndFeel&);
    ~KnobControl() override { setLookAndFeel (nullptr); }
    void simulateUserChange() override;

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class FaderControl : public juce::Slider, public PanelControl
{
public:
    FaderControl (const ControlSpec&, juce::AudioProcessorValueTreeState&, QueenLookAndFeel&);
    ~FaderControl() override { setLookAndFeel (nullptr); }
    void simulateUserChange() override;
    float capCentreY() const; // where the cap is drawn (local), straight from the value

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class SwitchControl : public juce::Component, public PanelControl
{
public:
    SwitchControl (const ControlSpec&, juce::AudioProcessorValueTreeState&, const QueenLookAndFeel&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setPosition (int index); // 0 = top
    int getPosition() const;
    void simulateUserChange() override;

private:
    int positionAt (float y) const;
    const ControlSpec& spec;
    const QueenLookAndFeel& lnf;
    juce::ParameterAttachment attachment;
    float current = 0.0f;
};

class PanelButton : public juce::Button, public PanelControl
{
public:
    PanelButton (const ControlSpec&, const QueenLookAndFeel&);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void simulateUserChange() override { press(); }
    void press();                     // same path as a mouse click
    std::function<void()> onPress;    // radio / action buttons
    std::function<void (bool)> onHeld; // momentary buttons: true on press, false on release

private:
    void clicked() override { press(); }
    void buttonStateChanged() override;
    const ControlSpec& spec;
    const QueenLookAndFeel& lnf;
    bool wasDown = false;
};

class LedControl : public juce::Component
{
public:
    LedControl (const ControlSpec&, const QueenLookAndFeel&);
    void paint (juce::Graphics&) override;
    void setLit (bool);
    bool isLit() const { return lit; }
    std::function<bool()> state;

private:
    const ControlSpec& spec;
    const QueenLookAndFeel& lnf;
    bool lit = false;
};

class DisplayControl : public juce::Component
{
public:
    DisplayControl (const ControlSpec&, const QueenLookAndFeel&);
    void paint (juce::Graphics&) override;
    void setDigits (int left, int right); // -1 = blank (unlit segments are in the background)

private:
    const ControlSpec& spec;
    const QueenLookAndFeel& lnf;
    int digitL = -1, digitR = -1;
};

class WheelControl : public juce::Component, public PanelControl
{
public:
    WheelControl (const ControlSpec&, const QueenLookAndFeel&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setDisplayValue (float v); // bend -1..1, mod 0..1
    void simulateUserChange() override { if (onChange) onChange (isBend ? 0.5f : 0.5f); }
    std::function<void (float)> onChange;

private:
    const ControlSpec& spec;
    const QueenLookAndFeel& lnf;
    bool isBend;
    float value = 0.0f, dragStartValue = 0.0f;
};

class PanelComponent : public juce::Component, private juce::Timer
{
public:
    PanelComponent (PluginProcessor&, QueenLookAndFeel&);
    ~PanelComponent() override;

    void paint (juce::Graphics&) override;
    void refresh(); // LEDs, display, wheels (also called by the timer)

    juce::Component* findControl (const juce::String& id) const;
    QueenKeyboard& getKeyboard() { return *keyboard; }

private:
    void timerCallback() override { refresh(); }
    void handleAction (const juce::String& action);
    void runFileAction (const juce::String& action);
    void flash (const juce::String& ledId, bool ok);

    PluginProcessor& processor;
    QueenLookAndFeel& lnf;
    const Layout& layout;
    juce::Image background;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<LedControl*> leds;
    DisplayControl* display = nullptr;
    WheelControl *bendWheel = nullptr, *modWheel = nullptr;
    std::unique_ptr<QueenKeyboard> keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    std::map<juce::String, std::pair<juce::uint32, bool>> flashes; // led id -> (until ms, ok)
    std::vector<std::unique_ptr<juce::ParameterAttachment>> radioAttachments;
    int blink = 0;
};
} // namespace queen
