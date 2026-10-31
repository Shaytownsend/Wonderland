#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fv
{
class IRManager;

/** Presets (XML .fvpreset), factory + user folders, categories, A/B compare.

    File format (versioned):
      <FreddyPreset version="1" name="Snare Bloom Plate" category="Snare" author="Freddy">
        <PARAM id="duck_depth_db" value="-24"/> ...            (real units, not normalised)
        <IR kind="factory" id="plate_bright_snare"/>
      </FreddyPreset>
    Parameters missing from a preset take their default, so old presets keep
    loading when new parameters are added. */
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, IRManager&);

    struct Preset
    {
        juce::String name, category;
        bool factory = true;
        juce::File file;          // user presets
        juce::String xml;         // factory presets (embedded)
    };

    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    void rescan();

    bool load (int index);
    bool loadXml (const juce::String& xml, const juce::String& name);
    bool saveUser (const juce::String& name, const juce::String& category);
    bool deleteUser (int index);
    void loadInit();
    /** Marks the current sound as the loaded preset `name` without applying anything (state restore). */
    void markLoaded (const juce::String& name) { loadedName = name; loadedSnapshot = captureXml (name, {}); }

    int getCurrentIndex() const noexcept { return current; }
    juce::String getCurrentName() const;
    /** True if parameters/IR differ from the loaded preset. */
    bool isDirty() const;
    void step (int delta);

    /** Serialises the current sound (all parameters + IR) as preset XML. */
    juce::String captureXml (const juce::String& name, const juce::String& category) const;

    // --- A/B compare -------------------------------------------------------
    int getABSlot() const noexcept { return abSlot; }
    void selectAB (int slot);   // 0 = A, 1 = B: stores the current sound in the active slot, recalls the other
    void copyAToB();

    static juce::File userFolder();
    static constexpr int kFormatVersion = 1;

private:
    void applyXml (const juce::XmlElement&);
    juce::AudioProcessorValueTreeState& apvts;
    IRManager& irManager;
    std::vector<Preset> presets;
    int current = -1;
    juce::String loadedName { "Init" };
    juce::String loadedSnapshot;
    juce::String ab[2];
    int abSlot = 0;
};
} // namespace fv
