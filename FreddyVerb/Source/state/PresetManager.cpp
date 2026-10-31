#include "PresetManager.h"
#include "IRManager.h"
#include "Parameters.h"
#include "FreddyBinaryData.h"

namespace fv
{
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, IRManager& ir) : apvts (s), irManager (ir)
{
    rescan();
    loadedSnapshot = captureXml ("Init", "Init");
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Freddy Verb").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    presets.clear();
    for (int i = 0; i < FreddyBinaryData::namedResourceListSize; ++i)
    {
        const juce::String fn (FreddyBinaryData::originalFilenames[i]);
        if (! fn.endsWith (".fvpreset")) continue;
        int size = 0;
        const char* data = FreddyBinaryData::getNamedResource (FreddyBinaryData::namedResourceList[i], size);
        const auto xmlText = juce::String::fromUTF8 (data, size);
        if (auto xml = juce::XmlDocument::parse (xmlText))
            presets.push_back ({ xml->getStringAttribute ("name", fn), xml->getStringAttribute ("category", "Factory"), true, {}, xmlText });
    }
    std::sort (presets.begin(), presets.end(), [] (const Preset& a, const Preset& b)
               { return a.category != b.category ? a.category < b.category : a.name < b.name; });

    const auto dir = userFolder();
    std::vector<Preset> user;
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.fvpreset"))
            if (auto xml = juce::XmlDocument::parse (f))
                user.push_back ({ xml->getStringAttribute ("name", f.getFileNameWithoutExtension()), "User", false, f, {} });
    std::sort (user.begin(), user.end(), [] (const Preset& a, const Preset& b) { return a.name < b.name; });
    presets.insert (presets.end(), user.begin(), user.end());
}

juce::String PresetManager::captureXml (const juce::String& name, const juce::String& category) const
{
    juce::XmlElement root ("FreddyPreset");
    root.setAttribute ("version", kFormatVersion);
    root.setAttribute ("name", name);
    root.setAttribute ("category", category);
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == pid::bypass || rp->getParameterID() == pid::mixLock) continue;
            auto* e = root.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if (auto ir = irManager.getState().createXml())
        root.addChildElement (ir.release());
    return root.toString (juce::XmlElement::TextFormat().singleLine());
}

void PresetManager::applyXml (const juce::XmlElement& root)
{
    const bool lockMix = apvts.getRawParameterValue (pid::mixLock)->load() > 0.5f;
    std::map<juce::String, float> values;
    for (auto* e : root.getChildWithTagNameIterator ("PARAM"))
        values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");

    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id == pid::bypass || id == pid::mixLock) continue;
            if (lockMix && mixLockedIDs().contains (id)) continue;
            const auto it = values.find (id);
            const float norm = it != values.end() ? rp->convertTo0to1 (it->second) : rp->getDefaultValue();
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (norm);
            rp->endChangeGesture();
        }
    if (auto* ir = root.getChildByName ("IR"))
        irManager.setState (juce::ValueTree::fromXml (*ir));
}

bool PresetManager::loadXml (const juce::String& xmlText, const juce::String& name)
{
    auto xml = juce::XmlDocument::parse (xmlText);
    if (xml == nullptr || ! xml->hasTagName ("FreddyPreset")) return false;
    if (auto* um = apvts.undoManager) um->beginNewTransaction ("Load preset");
    applyXml (*xml);
    loadedName = name.isNotEmpty() ? name : xml->getStringAttribute ("name");
    loadedSnapshot = captureXml (loadedName, xml->getStringAttribute ("category"));
    return true;
}

bool PresetManager::load (int index)
{
    if (index < 0 || index >= (int) presets.size()) return false;
    const auto& p = presets[(size_t) index];
    const auto text = p.factory ? p.xml : p.file.loadFileAsString();
    if (! loadXml (text, p.name)) return false;
    current = index;
    return true;
}

void PresetManager::loadInit()
{
    juce::XmlElement root ("FreddyPreset");
    root.setAttribute ("name", "Init");
    auto* ir = root.createNewChildElement ("IR");
    ir->setAttribute ("kind", "factory");
    ir->setAttribute ("id", "plate_medium");
    loadXml (root.toString(), "Init");
    current = -1;
}

bool PresetManager::saveUser (const juce::String& name, const juce::String& category)
{
    const auto dir = userFolder();
    if (! dir.createDirectory()) return false;
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".fvpreset");
    if (! file.replaceWithText (captureXml (name, category))) return false;
    rescan();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) current = i;
    loadedName = name;
    loadedSnapshot = captureXml (name, category);
    return true;
}

bool PresetManager::deleteUser (int index)
{
    if (index < 0 || index >= (int) presets.size() || presets[(size_t) index].factory) return false;
    const bool ok = presets[(size_t) index].file.deleteFile();
    rescan();
    current = -1;
    return ok;
}

juce::String PresetManager::getCurrentName() const { return loadedName; }

bool PresetManager::isDirty() const
{
    // compare everything except the name/category attributes
    auto a = juce::XmlDocument::parse (loadedSnapshot), b = juce::XmlDocument::parse (captureXml (loadedName, {}));
    if (a == nullptr || b == nullptr) return false;
    a->setAttribute ("category", juce::String()); b->setAttribute ("category", juce::String());
    for (auto* x : { a.get(), b.get() })
        for (auto* e : x->getChildWithTagNameIterator ("PARAM"))
            e->setAttribute ("value", juce::String (e->getDoubleAttribute ("value"), 4));
    return ! a->isEquivalentTo (b.get(), false);
}

void PresetManager::step (int delta)
{
    if (presets.empty()) return;
    const int n = (int) presets.size();
    const int next = current < 0 ? (delta > 0 ? 0 : n - 1) : (current + delta + n) % n;
    load (next);
}

void PresetManager::selectAB (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == abSlot) return;
    ab[abSlot] = captureXml (loadedName, {});
    abSlot = slot;
    if (ab[slot].isNotEmpty())
    {
        const auto keepName = loadedName, keepSnap = loadedSnapshot;
        if (auto* um = apvts.undoManager) um->beginNewTransaction ("A/B");
        if (auto xml = juce::XmlDocument::parse (ab[slot])) applyXml (*xml);
        loadedName = keepName; loadedSnapshot = keepSnap;
    }
    else
        ab[slot] = ab[1 - slot];   // first visit: B starts as a copy of A
}

void PresetManager::copyAToB()
{
    if (abSlot == 0)
    {
        ab[0] = ab[1] = captureXml (loadedName, {});
        return;
    }
    // editing B: overwrite B (the current sound) with A
    if (ab[0].isEmpty()) return;
    if (auto* um = apvts.undoManager) um->beginNewTransaction ("Copy A to B");
    if (auto xml = juce::XmlDocument::parse (ab[0])) applyXml (*xml);
    ab[1] = ab[0];
}
} // namespace fv
