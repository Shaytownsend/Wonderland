#include "PanelComponent.h"

namespace queen
{
namespace
{
    juce::Rectangle<int> centred (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x - w / 2.0f, y - h / 2.0f, w, h).toNearestInt();
    }

    juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        auto* p = s.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }

    void flipSlider (juce::Slider& s)
    {
        const double target = s.getValue() > s.getMinimum() ? s.getMinimum() : s.getMaximum();
        s.setValue (target, juce::sendNotificationSync);
    }
} // namespace

// ------------------------------------------------------------------------------------------------ knob
KnobControl::KnobControl (const ControlSpec& spec, juce::AudioProcessorValueTreeState& state)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
      attachment (state, spec.param, *this)
{
    setComponentID (spec.id);
    getProperties().set ("strip", spec.spriteName ("strip"));
    setMouseDragSensitivity (220);
    setRotaryParameters (juce::degreesToRadians (-135.0f) + juce::MathConstants<float>::twoPi,
                         juce::degreesToRadians (135.0f) + juce::MathConstants<float>::twoPi, true);
    auto& p = param (state, spec.param);
    setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
    setPopupDisplayEnabled (false, false, nullptr);
    setTooltip (p.getName (64));
}

void KnobControl::simulateUserChange() { flipSlider (*this); }

// ------------------------------------------------------------------------------------------------ fader
FaderControl::FaderControl (const ControlSpec& spec, juce::AudioProcessorValueTreeState& state)
    : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox), attachment (state, spec.param, *this)
{
    setComponentID (spec.id);
    getProperties().set ("cap", spec.spriteName ("cap"));
    getProperties().set ("capH", spec.spriteFloat ("capH"));
    auto& p = param (state, spec.param);
    setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
    setSliderSnapsToMousePosition (false);
    setTooltip (p.getName (64));
}

void FaderControl::simulateUserChange() { flipSlider (*this); }

// ------------------------------------------------------------------------------------------------ switch
SwitchControl::SwitchControl (const ControlSpec& s, juce::AudioProcessorValueTreeState& state, const QueenLookAndFeel& l)
    : spec (s), lnf (l), attachment (param (state, s.param), [this] (float v) { current = v; repaint(); }, nullptr)
{
    setComponentID (spec.id);
    attachment.sendInitialUpdate();
}

int SwitchControl::getPosition() const
{
    const int v = juce::roundToInt (current);
    const int idx = spec.positions.indexOf (v);
    return juce::jmax (0, idx);
}

void SwitchControl::setPosition (int index)
{
    index = juce::jlimit (0, spec.positions.size() - 1, index);
    attachment.setValueAsCompleteGesture ((float) spec.positions[index]);
}

void SwitchControl::simulateUserChange() { setPosition (getPosition() == 0 ? 1 : 0); }

int SwitchControl::positionAt (float y) const
{
    const int n = spec.positions.size();
    const float pad = spec.spriteFloat ("h") > 0 ? (spec.spriteFloat ("h") - spec.h) / 2.0f : 0.0f;
    const float travel = spec.h - 20.0f; // lever travel inside the slot
    const float top = pad + (spec.h - travel) / 2.0f;
    const float t = juce::jlimit (0.0f, 1.0f, (y - top) / travel);
    return juce::roundToInt (t * (float) (n - 1));
}

void SwitchControl::mouseDown (const juce::MouseEvent& e) { setPosition (positionAt (e.position.y)); }
void SwitchControl::mouseDrag (const juce::MouseEvent& e) { setPosition (positionAt (e.position.y)); }

void SwitchControl::paint (juce::Graphics& g)
{
    lnf.strip (spec.spriteName ("strip")).draw (g, getPosition(), getLocalBounds().toFloat());
}

// ------------------------------------------------------------------------------------------------ button
PanelButton::PanelButton (const ControlSpec& s, const QueenLookAndFeel& l) : juce::Button (s.id), spec (s), lnf (l)
{
    setComponentID (spec.id);
    setTriggeredOnMouseDown (true);
}

void PanelButton::press()
{
    if (onPress)
        onPress();
}

void PanelButton::buttonStateChanged()
{
    const bool down = isDown();
    if (down != wasDown && onHeld)
        onHeld (down);
    wasDown = down;
    repaint();
}

void PanelButton::paintButton (juce::Graphics& g, bool, bool down)
{
    const auto& img = lnf.image (spec.spriteName (down ? "down" : "up"));
    if (img.isValid())
        g.drawImage (img, getLocalBounds().toFloat());
}

// ------------------------------------------------------------------------------------------------ led
LedControl::LedControl (const ControlSpec& s, const QueenLookAndFeel& l) : spec (s), lnf (l)
{
    setComponentID (spec.id);
    setInterceptsMouseClicks (false, false);
}

void LedControl::setLit (bool shouldBeLit)
{
    if (shouldBeLit != lit)
    {
        lit = shouldBeLit;
        repaint();
    }
}

void LedControl::paint (juce::Graphics& g)
{
    const auto& img = lnf.image (spec.spriteName (lit ? "on" : "off"));
    if (img.isValid())
        g.drawImage (img, getLocalBounds().toFloat());
}

// ------------------------------------------------------------------------------------------------ display
DisplayControl::DisplayControl (const ControlSpec& s, const QueenLookAndFeel& l) : spec (s), lnf (l)
{
    setComponentID (spec.id);
    setInterceptsMouseClicks (false, false);
}

void DisplayControl::setDigits (int left, int right)
{
    if (left != digitL || right != digitR)
    {
        digitL = left;
        digitR = right;
        repaint();
    }
}

void DisplayControl::paint (juce::Graphics& g)
{
    // each digits frame shows "dd": the left half is drawn for the bank, the right half for the patch
    const auto& strip = lnf.strip (spec.spriteName ("strip"));
    const auto b = getLocalBounds().toFloat();
    auto half = [&] (int digit, bool right) {
        if (digit < 0)
            return;
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion ((right ? b.withTrimmedLeft (b.getWidth() / 2) : b.withTrimmedRight (b.getWidth() / 2)).toNearestInt());
        strip.draw (g, digit, b);
    };
    half (digitL, false);
    half (digitR, true);
}

// ------------------------------------------------------------------------------------------------ wheel
WheelControl::WheelControl (const ControlSpec& s, const QueenLookAndFeel& l)
    : spec (s), lnf (l), isBend (s.source == "pitchbend")
{
    setComponentID (spec.id);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void WheelControl::setDisplayValue (float v)
{
    if (! juce::approximatelyEqual (v, value))
    {
        value = v;
        repaint();
    }
}

void WheelControl::mouseDown (const juce::MouseEvent&) { dragStartValue = value; }

void WheelControl::mouseDrag (const juce::MouseEvent& e)
{
    const float range = isBend ? 2.0f : 1.0f;
    float v = dragStartValue - (float) e.getDistanceFromDragStartY() / ((float) getHeight() * 0.6f) * range;
    v = isBend ? juce::jlimit (-1.0f, 1.0f, v) : juce::jlimit (0.0f, 1.0f, v);
    setDisplayValue (v);
    if (onChange)
        onChange (v);
}

void WheelControl::mouseUp (const juce::MouseEvent&)
{
    if (isBend)
    {
        setDisplayValue (0.0f); // spring return
        if (onChange)
            onChange (0.0f);
    }
}

void WheelControl::paint (juce::Graphics& g)
{
    const auto& img = lnf.image (spec.spriteName ("sprite"));
    if (! img.isValid())
        return;
    const float travel = 18.0f;
    const float offset = -value * travel; // 0 = the rendered rest position (bend centre, mod down)
    g.reduceClipRegion (getLocalBounds().reduced (0, 30));
    g.drawImage (img, getLocalBounds().toFloat().translated (0.0f, offset));
}

// ------------------------------------------------------------------------------------------------ panel
PanelComponent::PanelComponent (PluginProcessor& p, QueenLookAndFeel& l)
    : processor (p), lnf (l), layout (Layout::get())
{
    setSize ((int) layout.width, (int) layout.height);
    background = lnf.image (layout.background);
    auto& state = processor.apvts;

    std::map<juce::String, std::function<bool()>> buttonState; // for LEDs

    for (const auto& c : layout.controls)
    {
        std::unique_ptr<juce::Component> comp;
        if (c.type == "knob")
        {
            comp = std::make_unique<KnobControl> (c, state);
            comp->setBounds (centred (c.x, c.y, c.spriteFloat ("size"), c.spriteFloat ("size")));
        }
        else if (c.type == "slider")
        {
            comp = std::make_unique<FaderControl> (c, state);
            comp->setBounds (centred (c.x, c.y, c.spriteFloat ("capW"), c.h + c.spriteFloat ("capH")));
        }
        else if (c.type == "switch")
        {
            comp = std::make_unique<SwitchControl> (c, state, lnf);
            comp->setBounds (centred (c.x, c.y, c.spriteFloat ("w"), c.spriteFloat ("h")));
        }
        else if (c.type == "button")
        {
            auto b = std::make_unique<PanelButton> (c, lnf);
            if (c.param.isNotEmpty())
            {
                auto att = std::make_unique<juce::ParameterAttachment> (param (state, c.param), [] (float) {}, nullptr);
                auto* a = att.get();
                auto* raw = state.getRawParameterValue (c.param);
                if (c.momentary)
                    b->onHeld = [a] (bool down) { a->setValueAsCompleteGesture (down ? 1.0f : 0.0f); };
                else if (c.value >= 0)
                    b->onPress = [a, v = c.value] { a->setValueAsCompleteGesture ((float) v); };
                else
                    b->onPress = [a, raw] { a->setValueAsCompleteGesture (raw->load() > 0.5f ? 0.0f : 1.0f); };
                radioAttachments.push_back (std::move (att));

                if (c.value >= 0)
                    buttonState[c.id] = [raw, v = c.value] { return juce::roundToInt (raw->load()) == v; };
                else
                    buttonState[c.id] = [raw] { return raw->load() > 0.5f; };
            }
            else
            {
                b->onPress = [this, action = c.action] { handleAction (action); };
                auto& mem = processor.memory;
                const auto a = c.action;
                if (a.startsWith ("bank:"))
                    buttonState[c.id] = [&mem, n = a.fromFirstOccurrenceOf (":", false, false).getIntValue()] { return mem.getPendingBank() == n; };
                else if (a.startsWith ("patch:"))
                    buttonState[c.id] = [&mem, n = a.fromFirstOccurrenceOf (":", false, false).getIntValue()] {
                        return ! mem.isManual() && mem.getPatch() == n && mem.getBank() == mem.getPendingBank();
                    };
                else if (a == "manual")
                    buttonState[c.id] = [&mem] { return mem.isManual(); };
                else if (a == "write")
                    buttonState[c.id] = [this, &mem] { return mem.isWriteArmed() && (blink / 6) % 2 == 0; };
                else
                    buttonState[c.id] = [this, id = c.id] {
                        auto it = flashes.find (id);
                        if (it == flashes.end() || juce::Time::getMillisecondCounter() > it->second.first)
                            return false;
                        return it->second.second || (blink / 3) % 2 == 0; // ok = steady, failed = fast blink
                    };
            }
            b->setBounds (centred (c.x, c.y, c.spriteFloat ("w"), c.spriteFloat ("h")));
            comp = std::move (b);
        }
        else if (c.type == "led")
        {
            auto led = std::make_unique<LedControl> (c, lnf);
            led->setBounds (centred (c.x, c.y, c.spriteFloat ("w"), c.spriteFloat ("h")));
            leds.push_back (led.get());
            comp = std::move (led);
        }
        else if (c.type == "display")
        {
            auto d = std::make_unique<DisplayControl> (c, lnf);
            d->setBounds (centred (c.x, c.y, c.spriteFloat ("w"), c.spriteFloat ("h")));
            display = d.get();
            comp = std::move (d);
        }
        else if (c.type == "wheel")
        {
            auto w = std::make_unique<WheelControl> (c, lnf);
            w->setBounds (centred (c.x, c.y, c.spriteFloat ("w"), c.spriteFloat ("h")));
            const bool bend = c.source == "pitchbend";
            w->onChange = [this, bend] (float v) { processor.setWheel (bend, v); };
            (bend ? bendWheel : modWheel) = w.get();
            comp = std::move (w);
        }
        else if (c.type == "keys")
        {
            keyboard = std::make_unique<QueenKeyboard> (processor.keyboardState, c, lnf);
            keyboard->setComponentID (c.id);
            keyboard->setBounds (juce::Rectangle<float> (c.x, c.y, c.w, c.h).toNearestInt());
            keyboard->setKeyWidth (c.w / 36.0f);
            addAndMakeVisible (*keyboard);
            continue;
        }
        if (comp)
        {
            addAndMakeVisible (*comp);
            controls.push_back (std::move (comp));
        }
    }

    for (auto* led : leds)
    {
        const auto* spec = layout.find (led->getComponentID());
        if (spec != nullptr && buttonState.contains (spec->ledFor))
            led->state = buttonState[spec->ledFor];
    }

    processor.memory.onChange = [this] { refresh(); };
    refresh();
    startTimerHz (30);
}

PanelComponent::~PanelComponent()
{
    processor.memory.onChange = nullptr;
}

juce::Component* PanelComponent::findControl (const juce::String& id) const
{
    if (keyboard != nullptr && keyboard->getComponentID() == id)
        return keyboard.get();
    for (const auto& c : controls)
        if (c->getComponentID() == id)
            return c.get();
    return nullptr;
}

void PanelComponent::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());
}

void PanelComponent::refresh()
{
    ++blink;
    for (auto* led : leds)
        if (led->state)
            led->setLit (led->state());

    if (display != nullptr)
    {
        const auto& m = processor.memory;
        const bool showPatch = ! m.isManual() && m.getBank() == m.getPendingBank();
        const bool hideForBlink = m.isWriteArmed() && (blink / 6) % 2 == 1;
        display->setDigits (hideForBlink ? -1 : m.getPendingBank() + 1, showPatch && ! hideForBlink ? m.getPatch() + 1 : -1);
    }
    if (bendWheel != nullptr && ! bendWheel->isMouseButtonDown())
        bendWheel->setDisplayValue (processor.engine.getBend());
    if (modWheel != nullptr && ! modWheel->isMouseButtonDown())
        modWheel->setDisplayValue (processor.engine.getModWheel());
}

void PanelComponent::flash (const juce::String& ledId, bool ok)
{
    flashes[ledId] = { juce::Time::getMillisecondCounter() + 1500, ok };
    refresh();
}

void PanelComponent::handleAction (const juce::String& action)
{
    auto& m = processor.memory;
    if (action.startsWith ("bank:"))
        m.pressBank (action.fromFirstOccurrenceOf (":", false, false).getIntValue());
    else if (action.startsWith ("patch:"))
        m.pressPatch (action.fromFirstOccurrenceOf (":", false, false).getIntValue());
    else if (action == "manual")
        m.pressManual();
    else if (action == "write")
        m.pressWrite();
    else if (action == "save" || action == "verify" || action == "load")
        runFileAction (action);
    refresh();
}

void PanelComponent::runFileAction (const juce::String& action)
{
    auto dir = PatchMemory::defaultDirectory();
    dir.createDirectory();
    const bool saving = action == "save";
    chooser = std::make_unique<juce::FileChooser> (saving ? "Save memory" : (action == "load" ? "Load memory" : "Verify memory"),
                                                   dir.getChildFile ("Memory").withFileExtension (PatchMemory::fileExtension),
                                                   juce::String ("*") + PatchMemory::fileExtension);
    const int flags = (saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                              : juce::FileBrowserComponent::openMode)
                      | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<PanelComponent> safe (this);
    chooser->launchAsync (flags, [safe, action] (const juce::FileChooser& fc) {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;
        auto file = fc.getResult();
        if (file.getFileExtension().isEmpty())
            file = file.withFileExtension (PatchMemory::fileExtension);
        auto& mem = safe->processor.memory;
        bool ok = false;
        if (action == "save")
            ok = mem.saveToFile (file);
        else if (action == "verify")
            ok = mem.verifyFile (file);
        else
            ok = mem.loadFromFile (file);
        safe->flash (action, ok);
    });
}
} // namespace queen
