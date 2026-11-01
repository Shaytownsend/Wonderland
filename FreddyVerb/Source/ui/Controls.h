#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FreddyLookAndFeel.h"

namespace fv::ui
{
/** Shared hub for all controls on the panel: knows the APVTS, tracks the
    last-touched parameter (for the data-entry jog wheel and the LCD readout). */
class ControlHub
{
public:
    explicit ControlHub (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    juce::AudioProcessorValueTreeState& apvts;

    void touch (const juce::String& paramID)
    {
        if (paramID.isEmpty()) return;
        lastTouched = paramID;
        listeners.call ([&] (Listener& l) { l.controlTouched (paramID); });
    }
    juce::String getLastTouched() const { return lastTouched; }

    struct Listener
    {
        virtual ~Listener() = default;
        virtual void controlTouched (const juce::String&) = 0;
        virtual void controlHovered (const juce::String&) {}
    };

    /** Hovering shows a control's value on the readout LCD without touching it
        (no undo step, the jog wheel keeps its target). Empty = nothing hovered. */
    void setHover (const juce::String& paramID)
    {
        if (paramID == hovered) return;
        hovered = paramID;
        listeners.call ([&] (Listener& l) { l.controlHovered (paramID); });
    }
    juce::String getHovered() const { return hovered; }
    void addListener (Listener* l)    { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Whether controls draw their own printed labels (photoreal skins bake labels into the art). */
    bool drawLabels = true;

private:
    juce::String lastTouched { "duck_depth_db" };
    juce::String hovered;
    juce::ListenerList<Listener> listeners;
};

FreddyLookAndFeel* freddyLnf (juce::Component&);

/** Rotary knob bound to one parameter. Vertical/horizontal drag, Cmd/Ctrl-drag for
    fine adjustment, double-click resets to default, mouse wheel, tooltips. */
class FreddyKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    FreddyKnob (ControlHub&, const juce::String& paramID, const juce::String& label = {});
    ~FreddyKnob() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hub.setHover (paramID); repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hub.setHover ({}); repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    juce::String getParamID() const { return paramID; }
    juce::RangedAudioParameter& getParameter() { return *param; }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setBipolar (bool b) { bipolar = b; repaint(); }
    void setShowLabel (bool b) { showLabel = b; repaint(); }
    juce::Rectangle<float> getKnobArea() const;

    /** Sets the normalised value from code (used by display handles and jog wheel). */
    void nudge (float normalisedDelta);

private:
    ControlHub& hub;
    juce::String paramID, label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float value = 0.0f;
    float dragStartValue = 0.0f;
    juce::Point<float> lastDragPos;
    bool dragging = false, bipolar = false, showLabel = true;
    juce::Colour accent;
};

/** Flat rectangular rubber pad with an LED. Bound to a bool parameter, or to one
    index of a choice parameter (radio behaviour), or used as a momentary button. */
class PadButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    /** Bool parameter toggle. */
    PadButton (ControlHub&, const juce::String& paramID, const juce::String& text);
    /** Choice parameter: selects `choiceIndex` when clicked. */
    PadButton (ControlHub&, const juce::String& paramID, int choiceIndex, const juce::String& text);
    /** Momentary/plain button (no parameter). */
    explicit PadButton (const juce::String& text);
    ~PadButton() override;

    std::function<void()> onClick;
    void setGlyph (int g) { glyph = g; repaint(); }
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setHasLed (bool b) { hasLed = b; repaint(); }
    void setText (const juce::String& s) { text = s; repaint(); }
    void setToggleState (bool b) { manualOn = b; repaint(); }
    bool isOn() const;
    juce::String getParamID() const { return paramID; }
    int getChoiceIndex() const { return choiceIndex; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { if (hub != nullptr && paramID.isNotEmpty()) hub->setHover (paramID); repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { if (hub != nullptr) hub->setHover ({}); repaint(); }
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    void init();
    ControlHub* hub = nullptr;
    juce::String paramID, text;
    int choiceIndex = -1, glyph = -1;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float value = 0.0f;
    bool down = false, hasLed = true, manualOn = false;
    juce::Colour accent;
};

/** Compact LCD-style stepper for choice parameters: "< EXPONENTIAL >". Click
    left/right halves to step, or click the centre for a popup menu. */
class ChoiceStepper : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceStepper (ControlHub&, const juce::String& paramID, const juce::String& label = {});
    ~ChoiceStepper() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hub.setHover (paramID); }
    void mouseExit (const juce::MouseEvent&) override  { hub.setHover ({}); }
    juce::String getParamID() const { return paramID; }

private:
    ControlHub& hub;
    juce::String paramID, label;
    juce::AudioParameterChoice* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int index = 0;
};

/** Data-entry jog wheel: edits whichever parameter was touched last. */
class JogWheel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit JogWheel (ControlHub&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void apply (float normDelta);
    ControlHub& hub;
    float angle = 0.0f;
    juce::Point<float> last;
    juce::RangedAudioParameter* active = nullptr;
};

/** Peak + RMS meter for a stereo pair. Values are pulled by the editor timer. */
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (const juce::String& label) : label (label) {}
    void setLevels (float peakDb, float rmsDb);
    void paint (juce::Graphics&) override;
private:
    juce::String label;
    float peak = -100, rms = -100, peakHold = -100;
    int holdCounter = 0;
};

class CorrelationMeter : public juce::Component
{
public:
    void setValue (float v) { if (std::abs (v - corr) > 0.005f) { corr = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        if (auto* l = freddyLnf (*this)) l->drawCorrelation (g, getLocalBounds().toFloat(), corr);
    }
private:
    float corr = 1.0f;
};

/** Static printed label on chassis/section. */
class PrintedLabel : public juce::Component
{
public:
    PrintedLabel (const juce::String& t, bool onSection = true) : text (t), onSection (onSection) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        if (auto* l = freddyLnf (*this)) l->drawControlLabel (g, getLocalBounds().toFloat(), text, onSection);
    }
    juce::String text;
    bool onSection;
};
} // namespace fv::ui
