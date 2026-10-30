#include "HostBar.h"

namespace queen
{
namespace
{
    const juce::Colour text (QueenLookAndFeel::hostTextColour), dim (QueenLookAndFeel::hostDimTextColour),
        box (QueenLookAndFeel::hostBoxColour), line (QueenLookAndFeel::hostLineColour), bar (QueenLookAndFeel::hostBarColour);

    juce::Font uiFont (float size, bool bold = true)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    void triangle (juce::Graphics& g, juce::Rectangle<float> r, bool up)
    {
        juce::Path p;
        const auto c = r.getCentre();
        const float w = r.getWidth() * 0.32f, h = r.getHeight() * 0.22f;
        if (up)
            p.addTriangle (c.x - w, c.y + h, c.x + w, c.y + h, c.x, c.y - h);
        else
            p.addTriangle (c.x - w, c.y - h, c.x + w, c.y - h, c.x, c.y + h);
        g.setColour (text);
        g.fillPath (p);
    }
} // namespace

void HostBar::IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    if (over || down)
    {
        g.setColour (line.withAlpha (down ? 0.9f : 0.5f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);
    }
    drawIcon (g, getLocalBounds().toFloat());
}

HostBar::HostBar (PluginProcessor& p)
    : processor (p),
      prev ("prev", [] (auto& g, auto r) { triangle (g, r, true); }),
      next ("next", [] (auto& g, auto r) { triangle (g, r, false); }),
      saveButton ("save", [] (auto& g, auto r) {
          const auto c = r.getCentre();
          g.setColour (text);
          juce::Path arrow;
          arrow.startNewSubPath (c.x, c.y - 13);
          arrow.lineTo (c.x, c.y + 6);
          arrow.startNewSubPath (c.x - 8, c.y - 2);
          arrow.lineTo (c.x, c.y + 6);
          arrow.lineTo (c.x + 8, c.y - 2);
          arrow.startNewSubPath (c.x - 14, c.y + 13);
          arrow.lineTo (c.x + 14, c.y + 13);
          g.strokePath (arrow, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
      }),
      initButton ("init", [] (auto& g, auto r) {
          g.setColour (dim);
          g.setFont (uiFont (15.0f).withExtraKerningFactor (0.15f));
          g.drawText ("INIT", r, juce::Justification::centred);
      }),
      settingsButton ("settings", [] (auto& g, auto r) {
          const auto c = r.getCentre();
          g.setColour (text);
          g.drawEllipse (juce::Rectangle<float> (14, 14).withCentre (c), 2.2f);
          for (int i = 0; i < 12; ++i)
          {
              const float a = juce::MathConstants<float>::twoPi * (float) i / 12.0f;
              g.drawLine (c.x + 10 * std::sin (a), c.y - 10 * std::cos (a), c.x + 14 * std::sin (a), c.y - 14 * std::cos (a), 2.0f);
          }
      }),
      nameButton ("name", [this] (auto& g, auto r) {
          g.setColour (text);
          g.setFont (uiFont (22.0f));
          g.drawText (getPresetText(), r.reduced (12, 0), juce::Justification::centred);
      })
{
    for (auto* b : { &prev, &next, &saveButton, &initButton, &settingsButton, &nameButton })
        addAndMakeVisible (*b);
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    saveButton.onClick = [this] { save(); };
    initButton.onClick = [this] { init(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    nameButton.onClick = [this] { showPresetMenu(); };
    saveButton.setTooltip ("Store the panel into the current memory slot");
    startTimerHz (4);
}

juce::String HostBar::getPresetText() const
{
    const auto& m = processor.memory;
    if (m.isManual())
        return "Manual";
    return juce::String (m.getBank() + 1) + "-" + juce::String (m.getPatch() + 1) + "   " + m.getCurrentName();
}

void HostBar::step (int delta)
{
    auto& m = processor.memory;
    const int total = PatchMemory::numBanks * PatchMemory::numPatches;
    const int index = (m.getBank() * PatchMemory::numPatches + m.getPatch() + delta + total) % total;
    m.recall (index / PatchMemory::numPatches, index % PatchMemory::numPatches);
    repaint();
}

void HostBar::save()
{
    auto& m = processor.memory;
    m.store (m.getPendingBank(), m.getPatch());
    repaint();
}

void HostBar::init()
{
    processor.memory.applyInit();
    repaint();
}

void HostBar::showPresetMenu()
{
    auto& m = processor.memory;
    juce::PopupMenu menu;
    for (int b = 0; b < PatchMemory::numBanks; ++b)
    {
        juce::PopupMenu bank;
        for (int p = 0; p < PatchMemory::numPatches; ++p)
            bank.addItem (b * PatchMemory::numPatches + p + 1, juce::String (b + 1) + "-" + juce::String (p + 1) + "  " + m.getPatchName (b, p),
                          true, ! m.isManual() && m.getBank() == b && m.getPatch() == p);
        menu.addSubMenu ("Bank " + juce::String (b + 1), bank);
    }
    juce::Component::SafePointer<HostBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton), [safe] (int r) {
        if (safe != nullptr && r > 0)
        {
            safe->processor.memory.recall ((r - 1) / PatchMemory::numPatches, (r - 1) % PatchMemory::numPatches);
            safe->repaint();
        }
    });
}

void HostBar::showSettingsMenu()
{
    juce::PopupMenu menu, os, tune;
    os.addItem (1, "Off", true, processor.getOversampling() == 1);
    os.addItem (2, "2x", true, processor.getOversampling() == 2);
    for (int hz : { 430, 432, 435, 438, 440, 442, 444, 446, 450 })
        tune.addItem (1000 + hz, "A4 = " + juce::String (hz) + " Hz", true, juce::roundToInt (processor.getMasterTune()) == hz);
    menu.addSubMenu ("Oversampling", os);
    menu.addSubMenu ("Master tune", tune);
    auto* sync = processor.apvts.getParameter ("arp_sync");
    menu.addItem (3, "Arp sync to host (1/16)", true, sync->getValue() > 0.5f);

    juce::Component::SafePointer<HostBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [safe, sync] (int r) {
        if (safe == nullptr || r == 0)
            return;
        if (r == 1 || r == 2)
            safe->processor.setOversampling (r);
        else if (r == 3)
            sync->setValueNotifyingHost (sync->getValue() > 0.5f ? 0.0f : 1.0f);
        else if (r > 1000)
            safe->processor.setMasterTune ((double) (r - 1000));
    });
}

void HostBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.fillAll (bar);
    g.setColour (juce::Colours::black);
    g.fillRect (r.removeFromBottom (2.0f));

    g.setColour (dim);
    g.setFont (uiFont (14.0f).withExtraKerningFactor (0.35f));
    g.drawText ("THE QUEEN", juce::Rectangle<float> (24, 0, 220, (float) height), juce::Justification::centredLeft);

    const auto boxR = nameButton.getBounds().getUnion (prev.getBounds()).getUnion (next.getBounds()).toFloat();
    g.setColour (box);
    g.fillRoundedRectangle (boxR, 6.0f);
    g.setColour (line);
    g.drawRoundedRectangle (boxR, 6.0f, 1.5f);

    g.setColour (dim);
    g.setFont (uiFont (13.0f).withExtraKerningFactor (0.2f));
    const juce::String os = processor.getOversampling() > 1 ? "  2x" : "";
    g.drawText (juce::String (processor.getMasterTune(), 0) + " Hz" + os, juce::Rectangle<float> (1240, 0, 160, (float) height),
                juce::Justification::centredLeft);
}

void HostBar::resized()
{
    const int boxX = 590, boxW = 620, boxY = 12, boxH = 46;
    nameButton.setBounds (boxX, boxY, boxW - 50, boxH);
    prev.setBounds (boxX + boxW - 50, boxY, 50, boxH / 2);
    next.setBounds (boxX + boxW - 50, boxY + boxH / 2, 50, boxH / 2);
    initButton.setBounds (1420, 12, 80, 46);
    saveButton.setBounds (1530, 12, 60, 46);
    settingsButton.setBounds (1620, 12, 60, 46);
}
} // namespace queen
