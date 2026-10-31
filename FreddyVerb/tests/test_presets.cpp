#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "PluginProcessor.h"
#include "state/Parameters.h"

using namespace fv;
using Catch::Matchers::WithinAbs;

namespace
{
    float value (FreddyVerbProcessor& p, const char* id) { return p.getAPVTS().getRawParameterValue (id)->load(); }
    void set (FreddyVerbProcessor& p, const char* id, float v)
    {
        auto* prm = p.getAPVTS().getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }
}

TEST_CASE ("At least 30 factory presets; every preset uses valid parameters and an existing IR", "[presets]")
{
    FreddyVerbProcessor p;
    auto& pm = p.getPresetManager();
    int factory = 0;
    for (auto& pr : pm.getPresets())
    {
        if (! pr.factory) continue;
        ++factory;
        auto xml = juce::XmlDocument::parse (pr.xml);
        REQUIRE (xml != nullptr);
        INFO (pr.name);
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        {
            auto* prm = p.getAPVTS().getParameter (e->getStringAttribute ("id"));
            INFO (e->getStringAttribute ("id"));
            REQUIRE (prm != nullptr);
            auto range = dynamic_cast<juce::RangedAudioParameter*> (prm)->getNormalisableRange();
            const auto v = (float) e->getDoubleAttribute ("value");
            CHECK (v >= range.start);
            CHECK (v <= range.end);
        }
        auto* ir = xml->getChildByName ("IR");
        REQUIRE (ir != nullptr);
        CHECK (p.getIRLibrary().find (ir->getStringAttribute ("id")) != nullptr);
    }
    CHECK (factory >= 30);
    // the spec's named examples exist
    for (auto* name : { "Snare Bloom Plate", "Kick Room Pump", "Tom Swell Hall", "Tight Gated '80s Snare",
                        "Rim Chamber Flick", "Clap Reverse Bloom", "Perc Spring Pop", "Hat Air Room" })
    {
        INFO (name);
        CHECK (std::any_of (pm.getPresets().begin(), pm.getPresets().end(), [&] (auto& pr) { return pr.name == name; }));
    }
}

TEST_CASE ("Loading a preset applies values, defaults the rest, and loads its IR", "[presets]")
{
    FreddyVerbProcessor p;
    auto& pm = p.getPresetManager();
    set (p, pid::width, 180.0f);    // not in "Snare Bloom Plate": must go back to default
    int idx = -1;
    for (int i = 0; i < (int) pm.getPresets().size(); ++i) if (pm.getPresets()[(size_t) i].name == "Snare Bloom Plate") idx = i;
    REQUIRE (pm.load (idx));
    CHECK_THAT (value (p, pid::duckDepth), WithinAbs (-24.0, 1e-3));
    CHECK_THAT (value (p, pid::width), WithinAbs (100.0, 1e-3));
    CHECK (p.getIRManager().getStatus().id == "plate_bright_snare");
    CHECK_FALSE (pm.isDirty());
    set (p, pid::duckDepth, -10.0f);
    CHECK (pm.isDirty());
}

TEST_CASE ("Mix Lock keeps the mix while browsing presets", "[presets]")
{
    FreddyVerbProcessor p;
    auto& pm = p.getPresetManager();
    set (p, pid::mix, 77.0f);
    set (p, pid::mixLock, 1.0f);
    pm.step (1);
    pm.step (1);
    CHECK_THAT (value (p, pid::mix), WithinAbs (77.0, 1e-3));
    set (p, pid::mixLock, 0.0f);
    pm.step (1);
    CHECK (std::abs (value (p, pid::mix) - 77.0f) > 0.1f);
}

TEST_CASE ("A/B compare keeps two independent sounds", "[presets]")
{
    FreddyVerbProcessor p;
    auto& pm = p.getPresetManager();
    set (p, pid::duckDepth, -30.0f);      // A
    pm.selectAB (1);                      // B starts as a copy of A
    CHECK_THAT (value (p, pid::duckDepth), WithinAbs (-30.0, 1e-3));
    set (p, pid::duckDepth, -6.0f);       // edit B
    pm.selectAB (0);
    CHECK_THAT (value (p, pid::duckDepth), WithinAbs (-30.0, 1e-3));
    pm.selectAB (1);
    CHECK_THAT (value (p, pid::duckDepth), WithinAbs (-6.0, 1e-3));
    pm.copyAToB();                        // B := A
    CHECK_THAT (value (p, pid::duckDepth), WithinAbs (-30.0, 1e-3));
}

TEST_CASE ("User presets save, rescan and delete", "[presets]")
{
    FreddyVerbProcessor p;
    auto& pm = p.getPresetManager();
    set (p, pid::gateHold, 444.0f);
    const juce::String name = "zz Test Preset " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
    REQUIRE (pm.saveUser (name, "User"));
    int idx = -1;
    for (int i = 0; i < (int) pm.getPresets().size(); ++i) if (pm.getPresets()[(size_t) i].name == name) idx = i;
    REQUIRE (idx >= 0);
    set (p, pid::gateHold, 10.0f);
    REQUIRE (pm.load (idx));
    CHECK_THAT (value (p, pid::gateHold), WithinAbs (444.0, 1e-2));
    CHECK (pm.deleteUser (idx));
}

TEST_CASE ("Undo/redo across parameter changes", "[presets][undo]")
{
    FreddyVerbProcessor p;
    auto& um = p.getUndoManager();
    um.beginNewTransaction();
    set (p, pid::duckRelease, 900.0f);
    // the APVTS copies parameter values into its tree on a timer
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    um.beginNewTransaction();
    REQUIRE (um.canUndo());
    um.undo();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK_THAT (value (p, pid::duckRelease), WithinAbs (250.0, 1e-2));
    um.redo();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    CHECK_THAT (value (p, pid::duckRelease), WithinAbs (900.0, 1e-2));
}
