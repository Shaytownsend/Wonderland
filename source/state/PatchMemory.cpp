#include "PatchMemory.h"
#include "../Parameters.h"

namespace queen
{
namespace
{
    // fixed precision so stored == recalled survives the 0..1 normalisation round trip
    juce::String formatValue (const ParamDef& d, float v)
    {
        return d.kind == Kind::Float ? juce::String (v, 5) : juce::String (juce::roundToInt (v));
    }

    struct FactoryPatch
    {
        const char* name;
        std::vector<std::pair<std::string_view, float>> values; // plain values over the defaults
    };

    // Hand-written factory bank (bank 1). Choice params use indices; ratios index kRatios
    // (0.5,1,2,3,4,5,6,7,8,10,12,16 -> 0..11).
    const std::vector<FactoryPatch>& factory()
    {
        static const std::vector<FactoryPatch> patches {
            { "Glass Bells",
              { { "fm_algorithm", 2 }, { "op1_ratio", 1 }, { "op2_ratio", 4 }, { "op2_level", 0.55f },
                { "op3_ratio", 7 }, { "op3_level", 0.45f }, { "op3_velocity", 0.8f },
                { "op4_ratio", 10 }, { "op4_level", 0.3f }, { "op4_feedback", 0.0f },
                { "vcf_freq", 0.92f }, { "vcf_env", 0.1f }, { "vcf_res", 0.05f }, { "vcf_lfo", 0.0f },
                { "attack", 0.002f }, { "decay", 3.5f }, { "sustain", 0.0f }, { "release", 2.5f }, { "hpf", 2 } } },
            { "Tine Piano",
              { { "fm_algorithm", 2 }, { "op1_ratio", 1 }, { "op2_ratio", 1 }, { "op2_level", 0.4f },
                { "op3_ratio", 1 }, { "op3_level", 0.35f }, { "op3_velocity", 0.9f },
                { "op4_ratio", 10 }, { "op4_level", 0.22f }, { "op4_velocity", 1.0f }, { "op4_feedback", 0.0f },
                { "vcf_freq", 0.8f }, { "vcf_env", 0.2f }, { "vcf_res", 0.1f }, { "vcf_lfo", 0.0f },
                { "attack", 0.002f }, { "decay", 2.5f }, { "sustain", 0.25f }, { "release", 0.6f } } },
            { "Juno Brass",
              { { "fm_algorithm", 4 }, { "op3_level", 0.7f }, { "op4_feedback", 0.55f }, { "op4_level", 0.4f },
                { "pwm_mode", 0 }, { "dco_pwm", 0.7f }, { "vcf_freq", 0.35f }, { "vcf_env", 0.55f },
                { "vcf_res", 0.15f }, { "vcf_lfo", 0.0f }, { "attack", 0.06f }, { "decay", 0.6f },
                { "sustain", 0.7f }, { "release", 0.25f }, { "chorus_1", 0 } } },
            { "Sub Bass",
              { { "fm_algorithm", 4 }, { "op2_ratio", 1 }, { "op2_level", 0.6f }, { "op3_ratio", 2 },
                { "op3_level", 0.4f }, { "op4_feedback", 0.6f }, { "dco_range", 0 }, { "vcf_freq", 0.25f },
                { "vcf_env", 0.7f }, { "vcf_res", 0.5f }, { "vcf_lfo", 0.0f }, { "vcf_kybd", 0.3f },
                { "attack", 0.001f }, { "decay", 0.35f }, { "sustain", 0.2f }, { "release", 0.08f },
                { "chorus_1", 0 }, { "hpf", 0 } } },
            { "Wide Pad",
              { { "fm_algorithm", 6 }, { "op2_ratio", 2 }, { "op2_detune", 7 }, { "op2_level", 0.5f },
                { "op3_ratio", 1 }, { "op3_detune", -7 }, { "op3_level", 0.8f }, { "wave_sub", 1 },
                { "op4_level", 0.35f }, { "op4_feedback", 0.2f }, { "attack", 1.2f }, { "decay", 2.0f },
                { "sustain", 0.8f }, { "release", 2.5f }, { "vcf_freq", 0.45f }, { "vcf_env", 0.25f },
                { "vcf_lfo", 0.15f }, { "lfo_rate", 0.5f }, { "dco_lfo", 0.1f }, { "lfo_delay", 1.0f },
                { "chorus_2", 1 } } },
            { "Pluck",
              { { "fm_algorithm", 1 }, { "op3_ratio", 2 }, { "op3_level", 0.5f }, { "op3_velocity", 0.8f },
                { "op4_ratio", 3 }, { "op4_level", 0.25f }, { "op4_feedback", 0.1f }, { "vcf_freq", 0.3f },
                { "vcf_env", 0.8f }, { "vcf_res", 0.35f }, { "vcf_lfo", 0.0f }, { "attack", 0.001f },
                { "decay", 0.25f }, { "sustain", 0.0f }, { "release", 0.3f }, { "hpf", 2 } } },
            { "Drawbar Organ",
              { { "fm_algorithm", 7 }, { "op1_level", 1.0f }, { "op2_ratio", 2 }, { "op2_level", 0.7f },
                { "op3_ratio", 1 }, { "op3_level", 0.8f }, { "wave_sub", 1 }, { "sub_level", 0.8f },
                { "op4_ratio", 4 }, { "op4_level", 0.5f }, { "op4_feedback", 0.0f }, { "vca_mode", 1 },
                { "vcf_freq", 1.0f }, { "vcf_env", 0.0f }, { "vcf_lfo", 0.0f }, { "vcf_kybd", 0.0f },
                { "attack", 0.001f }, { "release", 0.01f }, { "chorus_1", 0 }, { "chorus_2", 1 } } },
            { "Sync Lead",
              { { "fm_algorithm", 0 }, { "op2_ratio", 2 }, { "op2_level", 0.5f }, { "op3_level", 0.4f },
                { "op4_feedback", 0.7f }, { "vcf_freq", 0.5f }, { "vcf_env", 0.4f }, { "vcf_res", 0.4f },
                { "vcf_lfo", 0.0f }, { "dco_lfo", 0.25f }, { "lfo_delay", 0.6f }, { "lfo_rate", 5.5f },
                { "attack", 0.005f }, { "decay", 0.4f }, { "sustain", 0.8f }, { "release", 0.2f } } },
        };
        return patches;
    }

    juce::String xmlFor (const std::vector<std::pair<std::string_view, float>>& overrides)
    {
        juce::XmlElement root ("QUEEN");
        for (const auto& d : kParams)
        {
            float v = d.def;
            for (const auto& [id, value] : overrides)
                if (id == d.id)
                    v = value;
            auto* e = root.createNewChildElement ("PARAM");
            e->setAttribute ("id", toString (d.id));
            e->setAttribute ("value", formatValue (d, v));
        }
        return root.toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
    }
} // namespace

PatchMemory::PatchMemory (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    loadFactory();
}

void PatchMemory::loadFactory()
{
    const auto init = xmlFor ({});
    for (int b = 0; b < numBanks; ++b)
        for (int p = 0; p < numPatches; ++p)
            slots[(size_t) b][(size_t) p] = { "Init " + juce::String (b + 1) + juce::String (p + 1), init };
    const auto& f = factory();
    for (size_t p = 0; p < f.size() && p < numPatches; ++p)
        slots[0][p] = { f[p].name, xmlFor (f[p].values) };
    bank = pendingBank = patch = 0;
    manual = writeArmed = false;
    changed();
}

void PatchMemory::applyInit()
{
    applyXml (xmlFor ({}));
    manual = true;
    writeArmed = false;
    changed();
}

void PatchMemory::changed()
{
    if (onChange)
        onChange();
}

juce::String PatchMemory::captureXml() const
{
    juce::XmlElement root ("QUEEN");
    for (const auto& d : kParams)
    {
        auto* p = apvts.getParameter (toString (d.id));
        auto* e = root.createNewChildElement ("PARAM");
        e->setAttribute ("id", toString (d.id));
        e->setAttribute ("value", formatValue (d, p->convertFrom0to1 (p->getValue())));
    }
    return root.toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
}

void PatchMemory::applyXml (const juce::String& xmlText)
{
    auto xml = juce::parseXML (xmlText);
    if (xml == nullptr)
        return;
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* p = apvts.getParameter (e->getStringAttribute ("id")))
        {
            const float v = p->convertTo0to1 ((float) e->getDoubleAttribute ("value"));
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
}

void PatchMemory::recall (int b, int p)
{
    bank = pendingBank = juce::jlimit (0, numBanks - 1, b);
    patch = juce::jlimit (0, numPatches - 1, p);
    manual = false;
    applyXml (slots[(size_t) bank][(size_t) patch].xml);
    changed();
}

void PatchMemory::store (int b, int p)
{
    slots[(size_t) b][(size_t) p].xml = captureXml();
    bank = pendingBank = b;
    patch = p;
    manual = false;
    changed();
}

void PatchMemory::setPatchName (int b, int p, const juce::String& n)
{
    slots[(size_t) b][(size_t) p].name = n;
    changed();
}

juce::String PatchMemory::getCurrentName() const
{
    return manual ? juce::String ("Manual") : slots[(size_t) bank][(size_t) patch].name;
}

void PatchMemory::pressBank (int b)
{
    pendingBank = juce::jlimit (0, numBanks - 1, b);
    changed();
}

void PatchMemory::pressPatch (int p)
{
    if (writeArmed)
    {
        writeArmed = false;
        store (pendingBank, p);
    }
    else
    {
        recall (pendingBank, p);
    }
}

void PatchMemory::pressManual()
{
    manual = true;
    writeArmed = false;
    changed();
}

void PatchMemory::pressWrite()
{
    writeArmed = ! writeArmed;
    changed();
}

juce::File PatchMemory::defaultDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("TheQueen").getChildFile ("Patches");
}

juce::ValueTree PatchMemory::memoryTree() const
{
    juce::ValueTree mem ("MEMORY");
    mem.setProperty ("version", 1, nullptr);
    for (int b = 0; b < numBanks; ++b)
        for (int p = 0; p < numPatches; ++p)
        {
            juce::ValueTree slot ("PATCH");
            slot.setProperty ("bank", b, nullptr);
            slot.setProperty ("patch", p, nullptr);
            slot.setProperty ("name", slots[(size_t) b][(size_t) p].name, nullptr);
            slot.setProperty ("xml", slots[(size_t) b][(size_t) p].xml, nullptr);
            mem.appendChild (slot, nullptr);
        }
    return mem;
}

bool PatchMemory::saveToFile (const juce::File& f) const
{
    if (! f.getParentDirectory().createDirectory())
        return false;
    auto xml = memoryTree().createXml();
    return xml != nullptr && xml->writeTo (f);
}

bool PatchMemory::verifyFile (const juce::File& f) const
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr)
        return false;
    return juce::ValueTree::fromXml (*xml).isEquivalentTo (memoryTree());
}

bool PatchMemory::loadFromFile (const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr)
        return false;
    auto mem = juce::ValueTree::fromXml (*xml);
    if (! mem.hasType ("MEMORY"))
        return false;
    for (const auto& slot : mem)
    {
        const int b = slot["bank"], p = slot["patch"];
        if (b >= 0 && b < numBanks && p >= 0 && p < numPatches)
            slots[(size_t) b][(size_t) p] = { slot["name"].toString(), slot["xml"].toString() };
    }
    changed();
    return true;
}

juce::ValueTree PatchMemory::toValueTree() const
{
    auto mem = memoryTree();
    mem.setProperty ("currentBank", bank, nullptr);
    mem.setProperty ("currentPatch", patch, nullptr);
    mem.setProperty ("manual", manual, nullptr);
    return mem;
}

void PatchMemory::fromValueTree (const juce::ValueTree& mem)
{
    if (! mem.hasType ("MEMORY"))
        return;
    for (const auto& slot : mem)
    {
        const int b = slot["bank"], p = slot["patch"];
        if (b >= 0 && b < numBanks && p >= 0 && p < numPatches)
            slots[(size_t) b][(size_t) p] = { slot["name"].toString(), slot["xml"].toString() };
    }
    bank = pendingBank = juce::jlimit (0, numBanks - 1, (int) mem.getProperty ("currentBank", 0));
    patch = juce::jlimit (0, numPatches - 1, (int) mem.getProperty ("currentPatch", 0));
    manual = (bool) mem.getProperty ("manual", false);
    writeArmed = false;
    changed();
}
} // namespace queen
