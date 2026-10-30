#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace queen
{
// Juno-style programmer memory: 5 banks x 8 patches of APVTS parameter XML.
// Panel flow: BANK n then PATCH m recalls; WRITE arms, the next PATCH press stores the edit buffer there;
// MANUAL detaches the panel from memory (edit buffer = current knob positions).
// SAVE / VERIFY / LOAD move the whole memory to/from a .queen file (like the Juno's tape interface).
// Message thread only.
class PatchMemory
{
public:
    static constexpr int numBanks = 5, numPatches = 8;
    static constexpr const char* fileExtension = ".queen";

    explicit PatchMemory (juce::AudioProcessorValueTreeState& state);

    void loadFactory();
    void applyInit(); // default parameters into the edit buffer (manual)

    // panel buttons
    void pressBank (int b);
    void pressPatch (int p);
    void pressManual();
    void pressWrite();

    // direct access
    void recall (int b, int p);
    void store (int b, int p);
    juce::String captureXml() const;     // current edit buffer (parameters only)
    void applyXml (const juce::String&); // set every parameter, notifying the host
    juce::String getPatchXml (int b, int p) const { return slots[(size_t) b][(size_t) p].xml; }
    juce::String getPatchName (int b, int p) const { return slots[(size_t) b][(size_t) p].name; }
    void setPatchName (int b, int p, const juce::String& n);
    juce::String getCurrentName() const;

    int getBank() const { return bank; }
    int getPendingBank() const { return pendingBank; }
    int getPatch() const { return patch; }
    bool isManual() const { return manual; }
    bool isWriteArmed() const { return writeArmed; }

    // whole-memory files in ~/Documents/TheQueen/Patches
    static juce::File defaultDirectory();
    bool saveToFile (const juce::File&) const;
    bool verifyFile (const juce::File&) const;
    bool loadFromFile (const juce::File&);

    // plugin state
    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

    std::function<void()> onChange;

private:
    struct Slot
    {
        juce::String name, xml;
    };
    juce::ValueTree memoryTree() const;
    void changed();

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::array<Slot, numPatches>, numBanks> slots;
    int bank = 0, pendingBank = 0, patch = 0;
    bool manual = false, writeArmed = false;
};
} // namespace queen
