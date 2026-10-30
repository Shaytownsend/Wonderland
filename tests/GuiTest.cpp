#include <PluginEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <set>

TEST_CASE ("layout.json attaches exactly the panel parameters", "[gui]")
{
    const auto& layout = queen::Layout::get();
    REQUIRE (layout.controls.size() > 100);
    std::set<juce::String> attached;
    for (const auto& c : layout.controls)
        if (c.param.isNotEmpty() && (c.type == "knob" || c.type == "slider" || c.type == "switch" || c.type == "button"))
            attached.insert (c.param);
    int panelParams = 0;
    for (const auto& d : queen::kParams)
        panelParams += d.onPanel ? 1 : 0;
    CHECK ((int) attached.size() == panelParams);
}

TEST_CASE ("Every sprite named in layout.json is embedded", "[gui]")
{
    for (const auto& f : queen::Layout::get().spriteFiles())
    {
        INFO (f.toStdString());
        int size = 0;
        CHECK (queen::assets::find (f, size) != nullptr);
    }
}

TEST_CASE ("Editor: every control moves its parameter", "[gui]")
{
    PluginProcessor p;
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    REQUIRE (ed != nullptr);
    auto& panel = ed->getPanel();

    int moved = 0;
    for (const auto& c : queen::Layout::get().controls)
    {
        if (c.param.isEmpty())
            continue;
        INFO (c.id.toStdString());
        auto* comp = panel.findControl (c.id);
        REQUIRE (comp != nullptr);
        auto* ctl = dynamic_cast<queen::PanelControl*> (comp);
        REQUIRE (ctl != nullptr);
        auto* param = p.apvts.getParameter (c.param);

        if (c.value >= 0) // radio button: selecting it sets the choice
        {
            const float other = c.value == 0 ? 1.0f : 0.0f;
            param->setValueNotifyingHost (param->convertTo0to1 (other));
            ctl->simulateUserChange();
            CHECK (juce::roundToInt (param->convertFrom0to1 (param->getValue())) == c.value);
        }
        else if (c.momentary)
        {
            auto* b = dynamic_cast<queen::PanelButton*> (comp);
            REQUIRE (b != nullptr);
            REQUIRE (b->onHeld);
            b->onHeld (true);
            CHECK (param->getValue() > 0.5f);
            b->onHeld (false);
            CHECK (param->getValue() < 0.5f);
        }
        else
        {
            const float before = param->getValue();
            ctl->simulateUserChange();
            CHECK (param->getValue() != before);
        }
        ++moved;
    }
    CHECK (moved == 21 + 21 + 7 + 8 + 3 + 9); // knobs, sliders, switches, algo, range, toggles/momentary

    // memory buttons drive PatchMemory
    auto* bank3 = dynamic_cast<queen::PanelButton*> (panel.findControl ("bank_3"));
    auto* patch2 = dynamic_cast<queen::PanelButton*> (panel.findControl ("patch_2"));
    REQUIRE ((bank3 != nullptr && patch2 != nullptr));
    bank3->press();
    patch2->press();
    CHECK (p.memory.getBank() == 2);
    CHECK (p.memory.getPatch() == 1);
    panel.refresh();
    auto* led = dynamic_cast<queen::LedControl*> (panel.findControl ("patch_2_led"));
    REQUIRE (led != nullptr);
    CHECK (led->isLit());
}

TEST_CASE ("Editor resizes with a fixed aspect ratio", "[gui]")
{
    PluginProcessor p;
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    REQUIRE (ed != nullptr);
    ed->setSize (900, 450);
    CHECK (ed->getScale() == 0.5f);
    auto* c = ed->getConstrainer();
    CHECK (c->getFixedAspectRatio() == 2.0);
    CHECK (c->getMinimumWidth() == 900);
    CHECK (c->getMinimumHeight() == 450);
    ed->setSize (1800, 900);
    CHECK (ed->getScale() == 1.0f);
}

// Hidden: writes a PNG of the editor. QUEEN_SNAPSHOT=/path/to/file.png ./Tests "[.snapshot]"
TEST_CASE ("Editor snapshot", "[.snapshot]")
{
    PluginProcessor p;
    p.memory.recall (0, 3);
    std::unique_ptr<PluginEditor> ed (dynamic_cast<PluginEditor*> (p.createEditor()));
    const int w = juce::SystemStats::getEnvironmentVariable ("QUEEN_SNAPSHOT_WIDTH", "1800").getIntValue();
    ed->setSize (w, w / 2);
    ed->getPanel().refresh();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::SystemStats::getEnvironmentVariable ("QUEEN_SNAPSHOT", "/tmp/queen_ui.png"));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    REQUIRE (png.writeImageToStream (img, stream));
}
