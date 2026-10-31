#include "Controls.h"
#include "../state/Parameters.h"

namespace fv::ui
{
FreddyLookAndFeel* freddyLnf (juce::Component& c)
{
    return dynamic_cast<FreddyLookAndFeel*> (&c.getLookAndFeel());
}

static juce::String tooltipFor (const juce::String& id, const juce::String& fallback)
{
    if (auto* info = findParamInfo (id)) return info->tooltip;
    return fallback;
}

// ===========================================================================
// FreddyKnob
// ===========================================================================
FreddyKnob::FreddyKnob (ControlHub& h, const juce::String& id, const juce::String& lbl)
    : hub (h), paramID (id), label (lbl)
{
    param = hub.apvts.getParameter (id);
    jassert (param != nullptr);
    if (label.isEmpty())
        if (auto* info = findParamInfo (id)) label = info->shortName;
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        value = param->convertTo0to1 (v);
        repaint();
    });
    attachment->sendInitialUpdate();
    setTooltip (param->getName (64) + " - " + tooltipFor (id, {}) + "  (double-click: reset, Cmd/Ctrl-drag: fine)");
    setTitle (param->getName (64));
    setWantsKeyboardFocus (false);
    setRepaintsOnMouseActivity (true);
}

FreddyKnob::~FreddyKnob() = default;

juce::Rectangle<float> FreddyKnob::getKnobArea() const
{
    auto b = getLocalBounds().toFloat();
    if (showLabel && hub.drawLabels)
        b.removeFromBottom (std::min (16.0f, b.getHeight() * 0.25f));
    return b.reduced (1.0f);
}

void FreddyKnob::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    KnobState s;
    s.value = value;
    s.defaultValue = param->getDefaultValue();
    s.bipolar = bipolar;
    s.hover = isMouseOver (true);
    s.dragging = dragging;
    s.enabled = isEnabled();
    s.accent = accent;
    s.label = label;
    s.valueText = param->getCurrentValueAsText();
    s.paramID = paramID;
    s.componentID = getComponentID();
    lnf->drawKnob (g, getKnobArea(), s);

    if (showLabel && hub.drawLabels)
    {
        auto lb = getLocalBounds().toFloat().removeFromBottom (std::min (16.0f, (float) getHeight() * 0.25f));
        lnf->drawControlLabel (g, lb, (dragging || s.hover) ? s.valueText : label, true);
    }
}

void FreddyKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    dragging = true;
    dragStartValue = value;
    lastDragPos = e.position;
    hub.touch (paramID);
    attachment->beginGesture();
    repaint();
}

void FreddyKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const auto delta = e.position - lastDragPos;
    lastDragPos = e.position;
    const bool fine = e.mods.isCommandDown() || e.mods.isCtrlDown();
    const float pixelsPerRange = fine ? 1200.0f : 220.0f;
    const float d = (delta.x - delta.y) / pixelsPerRange;
    value = juce::jlimit (0.0f, 1.0f, value + d);
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (value));
    repaint();
}

void FreddyKnob::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    attachment->endGesture();
    repaint();
}

void FreddyKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    hub.touch (paramID);
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
}

void FreddyKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    hub.touch (paramID);
    const float step = (e.mods.isCommandDown() || e.mods.isCtrlDown()) ? 0.002f : 0.02f;
    nudge ((w.deltaY + w.deltaX) > 0 ? step : -step);
}

void FreddyKnob::nudge (float d)
{
    value = juce::jlimit (0.0f, 1.0f, value + d);
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (value));
}

namespace
{
    struct KnobValueInterface : juce::AccessibilityRangedNumericValueInterface
    {
        explicit KnobValueInterface (juce::RangedAudioParameter& p) : param (p) {}
        bool isReadOnly() const override { return false; }
        double getCurrentValue() const override { return param.convertFrom0to1 (param.getValue()); }
        void setValue (double v) override
        {
            param.beginChangeGesture();
            param.setValueNotifyingHost (param.convertTo0to1 ((float) v));
            param.endChangeGesture();
        }
        AccessibleValueRange getRange() const override
        {
            auto r = param.getNormalisableRange();
            return { { r.start, r.end }, std::max (0.001, (double) (r.end - r.start) / 100.0) };
        }
        juce::RangedAudioParameter& param;
    };
}

std::unique_ptr<juce::AccessibilityHandler> FreddyKnob::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider,
        juce::AccessibilityActions(),
        juce::AccessibilityHandler::Interfaces { std::make_unique<KnobValueInterface> (*param) });
}

// ===========================================================================
// PadButton
// ===========================================================================
PadButton::PadButton (ControlHub& h, const juce::String& id, const juce::String& t)
    : hub (&h), paramID (id), text (t) { init(); }

PadButton::PadButton (ControlHub& h, const juce::String& id, int idx, const juce::String& t)
    : hub (&h), paramID (id), text (t), choiceIndex (idx) { init(); }

PadButton::PadButton (const juce::String& t) : text (t) { setRepaintsOnMouseActivity (true); setTitle (t); }

PadButton::~PadButton() = default;

void PadButton::init()
{
    param = hub->apvts.getParameter (paramID);
    jassert (param != nullptr);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); });
    attachment->sendInitialUpdate();
    setTooltip (tooltipFor (paramID, param->getName (64)));
    setTitle (text.isNotEmpty() ? text : param->getName (64));
    setRepaintsOnMouseActivity (true);
}

bool PadButton::isOn() const
{
    if (param == nullptr) return manualOn;
    if (choiceIndex >= 0) return juce::roundToInt (value) == choiceIndex;
    return value > 0.5f;
}

void PadButton::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    PadState s;
    s.text = text;
    s.on = isOn();
    s.hover = isMouseOver (true);
    s.down = down;
    s.enabled = isEnabled();
    s.hasLed = hasLed;
    s.accent = accent;
    s.paramID = paramID;
    s.componentID = getComponentID();
    s.glyph = glyph;
    lnf->drawPad (g, getLocalBounds().toFloat(), s);
}

void PadButton::mouseDown (const juce::MouseEvent&)
{
    down = true;
    if (hub != nullptr) hub->touch (paramID);
    repaint();
}

void PadButton::mouseUp (const juce::MouseEvent& e)
{
    down = false;
    if (! getLocalBounds().contains (e.getPosition())) { repaint(); return; }
    if (attachment != nullptr)
    {
        if (choiceIndex >= 0) attachment->setValueAsCompleteGesture ((float) choiceIndex);
        else                  attachment->setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
    }
    if (onClick) onClick();
    repaint();
}

std::unique_ptr<juce::AccessibilityHandler> PadButton::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::toggleButton,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this]
        {
            if (attachment != nullptr)
            {
                if (choiceIndex >= 0) attachment->setValueAsCompleteGesture ((float) choiceIndex);
                else                  attachment->setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
            }
            if (onClick) onClick();
        }));
}

// ===========================================================================
// ChoiceStepper
// ===========================================================================
ChoiceStepper::ChoiceStepper (ControlHub& h, const juce::String& id, const juce::String& lbl)
    : hub (h), paramID (id), label (lbl)
{
    param = dynamic_cast<juce::AudioParameterChoice*> (hub.apvts.getParameter (id));
    jassert (param != nullptr);
    if (label.isEmpty())
        if (auto* info = findParamInfo (id)) label = info->shortName;
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { index = juce::roundToInt (v); repaint(); });
    attachment->sendInitialUpdate();
    setTooltip (tooltipFor (id, param->getName (64)));
    setTitle (param->getName (64));
}

ChoiceStepper::~ChoiceStepper() = default;

void ChoiceStepper::paint (juce::Graphics& g)
{
    auto* lnf = freddyLnf (*this);
    if (lnf == nullptr) return;
    auto b = getLocalBounds().toFloat();
    if (label.isNotEmpty() && hub.drawLabels)
        lnf->drawControlLabel (g, b.removeFromTop (std::min (15.0f, b.getHeight() * 0.42f)), label, true);
    auto inner = lnf->drawDisplayFrame (g, b);
    auto txt = param->choices[index].toUpperCase();
    const float arrowW = std::max (7.0f, inner.getHeight() * 0.42f);
    lnf->drawDisplayText (g, "<", inner.removeFromLeft (arrowW).reduced (1, inner.getHeight() * 0.22f), juce::Justification::centred, true);
    lnf->drawDisplayText (g, ">", inner.removeFromRight (arrowW).reduced (1, inner.getHeight() * 0.22f), juce::Justification::centred, true);
    auto box = inner.reduced (1.0f, inner.getHeight() * 0.16f);
    // Like real hardware LCDs, abbreviate long names rather than shrinking them
    // below a readable dot pitch (the popup menu and tooltip show the full name).
    const float minPx = std::min (1.25f, box.getHeight() / 7.0f);
    if (PixelFont::textWidth (txt, minPx) - minPx > box.getWidth())
    {
        txt = txt.replace ("EXPONENTIAL", "EXPON").replace ("LOGARITHMIC", "LOGAR").replace ("ZERO LATENCY", "ZERO LAT")
                 .replace ("LINEAR", "LIN").replace (" DB/OCT", "DB").replace ("CONTINUE", "CONT").replace ("RESTART", "RSTRT");
        while (txt.length() > 3 && PixelFont::textWidth (txt, minPx) - minPx > box.getWidth()) txt = txt.dropLastCharacters (1);
    }
    lnf->drawDisplayText (g, txt, box, juce::Justification::centred);
    lnf->drawDisplayOverlay (g, inner);
}

void ChoiceStepper::mouseDown (const juce::MouseEvent& e)
{
    hub.touch (paramID);
    const int n = param->choices.size();
    const float x = e.position.x / (float) getWidth();
    if (x < 0.25f || x > 0.75f)
    {
        const int next = (index + (x < 0.25f ? n - 1 : 1)) % n;
        attachment->setValueAsCompleteGesture ((float) next);
        return;
    }
    juce::PopupMenu m;
    for (int i = 0; i < n; ++i) m.addItem (i + 1, param->choices[i], true, i == index);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe = juce::Component::SafePointer<ChoiceStepper> (this)] (int r)
                     {
                         if (safe != nullptr && r > 0) safe->attachment->setValueAsCompleteGesture ((float) (r - 1));
                     });
}

// ===========================================================================
// JogWheel
// ===========================================================================
JogWheel::JogWheel (ControlHub& h) : hub (h)
{
    setTooltip ("Data entry: edits the last touched parameter (Cmd/Ctrl for fine)");
    setTitle ("Data Entry");
}

void JogWheel::paint (juce::Graphics& g)
{
    if (auto* l = freddyLnf (*this)) l->drawJogWheel (g, getLocalBounds().toFloat().reduced (2), angle, active != nullptr);
}

void JogWheel::mouseDown (const juce::MouseEvent& e)
{
    last = e.position;
    active = hub.apvts.getParameter (hub.getLastTouched());
    if (active) active->beginChangeGesture();
    repaint();
}

void JogWheel::apply (float d)
{
    auto* p = active != nullptr ? active : hub.apvts.getParameter (hub.getLastTouched());
    if (p == nullptr) return;
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + d));
    angle += d * 12.0f;
    hub.touch (hub.getLastTouched());
    repaint();
}

void JogWheel::mouseDrag (const juce::MouseEvent& e)
{
    // circular drag around the centre
    const auto c = getLocalBounds().toFloat().getCentre();
    const float a0 = std::atan2 (last.y - c.y, last.x - c.x);
    const float a1 = std::atan2 (e.position.y - c.y, e.position.x - c.x);
    float da = a1 - a0;
    if (da > juce::MathConstants<float>::pi) da -= juce::MathConstants<float>::twoPi;
    if (da < -juce::MathConstants<float>::pi) da += juce::MathConstants<float>::twoPi;
    last = e.position;
    const bool fine = e.mods.isCommandDown() || e.mods.isCtrlDown();
    apply (da / (fine ? 60.0f : 12.0f));
}

void JogWheel::mouseUp (const juce::MouseEvent&)
{
    if (active) active->endChangeGesture();
    active = nullptr;
    repaint();
}

void JogWheel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    auto* p = hub.apvts.getParameter (hub.getLastTouched());
    if (p == nullptr) return;
    p->beginChangeGesture();
    active = p;
    apply ((w.deltaY > 0 ? 1.0f : -1.0f) * ((e.mods.isCommandDown() || e.mods.isCtrlDown()) ? 0.002f : 0.01f));
    p->endChangeGesture();
    active = nullptr;
}

// ===========================================================================
// LevelMeter
// ===========================================================================
void LevelMeter::setLevels (float peakDb, float rmsDb)
{
    if (peakDb >= peakHold) { peakHold = peakDb; holdCounter = 45; }
    else if (--holdCounter < 0) peakHold = std::max (peakDb, peakHold - 1.0f);
    // ballistics: fast attack, ~20 dB/s fall at 60 fps
    peak = std::max (peakDb, peak - 0.35f);
    rms  = std::max (rmsDb, rms - 0.35f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    if (auto* l = freddyLnf (*this)) l->drawMeter (g, getLocalBounds().toFloat(), peakHold, rms, label);
}
} // namespace fv::ui
