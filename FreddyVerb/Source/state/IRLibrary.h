#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include "../dsp/IRProcessor.h"

namespace fv
{
/** Factory IR library (embedded FLACs + library.json from tools/ir_forge) plus
    user IR files. Decoding helpers are thread-safe (no shared mutable state). */
class IRLibrary
{
public:
    struct Entry
    {
        juce::String id, file, name, category, description;
        double rt60 = 0.0, lengthSeconds = 0.0;
        juce::Colour colour { 0xff888888 };
    };

    IRLibrary();
    const std::vector<Entry>& factory() const noexcept { return entries; }
    const Entry* find (const juce::String& id) const;

    /** Decodes a factory IR. Falls back to a built-in synthetic IR if the
        embedded data is missing (e.g. a build without the factory library). */
    bool loadFactory (const juce::String& id, dsp::IRSource& out) const;

    /** Decodes a user IR file (WAV / AIFF / FLAC, 1, 2 or 4 channels, <= 20 s). */
    static bool loadFile (const juce::File& f, dsp::IRSource& out, juce::String& error);
    static bool loadFromMemory (const void* data, size_t size, dsp::IRSource& out, juce::String& error);

    /** FNV-1a 64-bit hash of the file contents, hex. */
    static juce::String hashFile (const juce::File& f);
    static juce::String hashData (const void* data, size_t size);

    static constexpr double kMaxSeconds = 20.0;
    static const juce::String defaultIRId;     // "plate_medium"

    /** Synthetic stand-in IRs used when the factory library is not embedded. */
    static dsp::IRSource makeSyntheticIR (const juce::String& id);

private:
    std::vector<Entry> entries;
};
} // namespace fv
