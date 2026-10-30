#include "helpers/render.h"
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>

static void setPlain (PluginProcessor& p, const char* id, float v)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (v));
}

TEST_CASE ("PatchRoundTrip: write -> load -> XML equal", "[patch]")
{
    PluginProcessor p;
    auto& m = p.memory;
    setPlain (p, "vcf_freq", 0.123f);
    setPlain (p, "fm_algorithm", 6);
    setPlain (p, "op3_ratio", 9);
    const auto written = m.captureXml();

    m.pressBank (2);
    m.pressWrite();
    CHECK (m.isWriteArmed());
    m.pressPatch (5); // stores into bank 3 / patch 6
    CHECK_FALSE (m.isWriteArmed());
    CHECK (m.getPatchXml (2, 5) == written);

    m.recall (0, 0); // something else
    CHECK (m.captureXml() != written);
    m.pressBank (2);
    m.pressPatch (5);
    CHECK (m.captureXml() == written);
    CHECK (m.getBank() == 2);
    CHECK (m.getPatch() == 5);

    m.pressManual();
    CHECK (m.isManual());
    CHECK (m.getCurrentName() == "Manual");
}

TEST_CASE ("8 factory patches load and play", "[patch]")
{
    PluginProcessor p;
    const char* names[] { "Glass Bells", "Tine Piano", "Juno Brass", "Sub Bass", "Wide Pad", "Pluck", "Drawbar Organ", "Sync Lead" };
    for (int i = 0; i < 8; ++i)
    {
        INFO (names[i]);
        CHECK (p.memory.getPatchName (0, i) == names[i]);
        p.memory.recall (0, i);
        CHECK (p.memory.captureXml() == p.memory.getPatchXml (0, i));

        p.setPlayConfigDetails (0, 2, 48000.0, 128);
        p.prepareToPlay (48000.0, 128);
        auto out = renderNotes (p.engine, { 60 }, 1.0, 0.7);
        auto s = stats (out);
        CHECK_FALSE (s.hasNaN);
        CHECK (s.rms > 1.0e-3);
        CHECK (s.peak < 1.0f);
    }
}

TEST_CASE (".queen save / verify / load", "[patch]")
{
    PluginProcessor p;
    auto& m = p.memory;
    auto dir = juce::File::createTempFile ("queen_test");
    dir.createDirectory();
    auto file = dir.getChildFile ("memory").withFileExtension (queen::PatchMemory::fileExtension);

    m.setPatchName (4, 7, "Custom");
    REQUIRE (m.saveToFile (file));
    CHECK (m.verifyFile (file));

    const auto before = m.getPatchXml (4, 7);
    setPlain (p, "volume", 0.1f);
    m.store (4, 7);
    m.setPatchName (4, 7, "Changed");
    CHECK_FALSE (m.verifyFile (file)); // memory no longer matches the file

    REQUIRE (m.loadFromFile (file));
    CHECK (m.getPatchName (4, 7) == "Custom");
    CHECK (m.getPatchXml (4, 7) == before);
    CHECK (m.verifyFile (file));
    CHECK (queen::PatchMemory::defaultDirectory().getFullPathName().endsWith ("TheQueen/Patches"));
    dir.deleteRecursively();
}

TEST_CASE ("Plugin state carries params, memory and settings", "[patch]")
{
    juce::MemoryBlock blob;
    {
        PluginProcessor a;
        setPlain (a, "vcf_res", 0.77f);
        a.memory.setPatchName (1, 1, "Saved In State");
        a.setMasterTune (442.0);
        a.getStateInformation (blob);
    }
    PluginProcessor b;
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (std::abs (b.apvts.getRawParameterValue ("vcf_res")->load() - 0.77f) < 1e-4f);
    CHECK (b.memory.getPatchName (1, 1) == "Saved In State");
    CHECK (b.getMasterTune() == 442.0);
}
