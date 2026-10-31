#include "IRLibrary.h"
#include "FreddyBinaryData.h"
#include "../dsp/DspCommon.h"
#include <random>

namespace fv
{
const juce::String IRLibrary::defaultIRId { "plate_medium" };

namespace
{
    const char* findResource (const juce::String& fileName, int& size)
    {
        for (int i = 0; i < FreddyBinaryData::namedResourceListSize; ++i)
            if (fileName == FreddyBinaryData::originalFilenames[i])
                return FreddyBinaryData::getNamedResource (FreddyBinaryData::namedResourceList[i], size);
        size = 0;
        return nullptr;
    }

    juce::AudioFormatManager& formats()
    {
        // read-only after construction; createReaderFor is thread-safe
        static std::unique_ptr<juce::AudioFormatManager> fm = []
        {
            auto m = std::make_unique<juce::AudioFormatManager>();
            m->registerBasicFormats();
            return m;
        }();
        return *fm;
    }

    bool decode (std::unique_ptr<juce::AudioFormatReader> reader, dsp::IRSource& out, juce::String& error)
    {
        if (reader == nullptr) { error = "Unsupported or unreadable audio file"; return false; }
        const int ch = (int) reader->numChannels;
        if (ch != 1 && ch != 2 && ch != 4) { error = "IRs must have 1, 2 or 4 channels (this file has " + juce::String (ch) + ")"; return false; }
        if (reader->sampleRate <= 0) { error = "Invalid sample rate"; return false; }
        const auto maxLen = (juce::int64) (IRLibrary::kMaxSeconds * reader->sampleRate);
        if (reader->lengthInSamples > maxLen) { error = "IR is longer than 20 seconds"; return false; }
        const int n = (int) reader->lengthInSamples;
        if (n <= 0) { error = "IR is empty"; return false; }
        juce::AudioBuffer<float> buf (ch, n);
        reader->read (&buf, 0, n, 0, true, true);
        out.sampleRate = reader->sampleRate;
        out.channels.assign ((size_t) ch, std::vector<double> ((size_t) n, 0.0));
        for (int c = 0; c < ch; ++c)
        {
            const float* s = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i) out.channels[(size_t) c][(size_t) i] = s[i];
        }
        return true;
    }

    uint64_t fnv1a (const void* data, size_t size, uint64_t h = 1469598103934665603ULL)
    {
        auto* p = static_cast<const uint8_t*> (data);
        for (size_t i = 0; i < size; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
        return h;
    }
}

IRLibrary::IRLibrary()
{
    int size = 0;
    if (const char* json = findResource ("library.json", size))
    {
        auto v = juce::JSON::parse (juce::String::fromUTF8 (json, size));
        if (auto* arr = v.getArray())
            for (auto& e : *arr)
            {
                Entry en;
                en.id = e.getProperty ("id", {}).toString();
                en.file = e.getProperty ("file", {}).toString();
                en.name = e.getProperty ("name", en.id).toString();
                en.category = e.getProperty ("category", "FX").toString();
                en.description = e.getProperty ("description", {}).toString();
                en.rt60 = (double) e.getProperty ("rt60", 0.0);
                en.lengthSeconds = (double) e.getProperty ("length_s", 0.0);
                if (auto* col = e.getProperty ("color", {}).getArray(); col != nullptr && col->size() == 3)
                    en.colour = juce::Colour ((uint8_t) (int) (*col)[0], (uint8_t) (int) (*col)[1], (uint8_t) (int) (*col)[2]);
                int sz = 0;
                if (en.id.isNotEmpty() && findResource (en.file, sz) != nullptr)
                    entries.push_back (en);
            }
    }
    if (entries.empty())
    {
        // built-in synthetic stand-ins (no factory library embedded in this build)
        entries.push_back ({ "plate_medium", {}, "Medium Plate (built-in)", "Plates", "Synthetic stand-in plate.", 1.6, 2.0 });
        entries.push_back ({ "room_kick_tight", {}, "Tight Room (built-in)", "Rooms", "Synthetic stand-in room.", 0.45, 0.7 });
        entries.push_back ({ "hall_tom", {}, "Hall (built-in)", "Halls", "Synthetic stand-in hall.", 2.6, 3.2 });
    }
}

const IRLibrary::Entry* IRLibrary::find (const juce::String& id) const
{
    for (auto& e : entries) if (e.id == id) return &e;
    return nullptr;
}

bool IRLibrary::loadFactory (const juce::String& id, dsp::IRSource& out) const
{
    if (auto* e = find (id); e != nullptr && e->file.isNotEmpty())
    {
        int size = 0;
        if (const char* data = findResource (e->file, size))
        {
            juce::String err;
            if (loadFromMemory (data, (size_t) size, out, err)) return true;
        }
    }
    out = makeSyntheticIR (id);
    return find (id) != nullptr || id == defaultIRId;
}

bool IRLibrary::loadFile (const juce::File& f, dsp::IRSource& out, juce::String& error)
{
    if (! f.existsAsFile()) { error = "File not found: " + f.getFullPathName(); return false; }
    return decode (std::unique_ptr<juce::AudioFormatReader> (formats().createReaderFor (f)), out, error);
}

bool IRLibrary::loadFromMemory (const void* data, size_t size, dsp::IRSource& out, juce::String& error)
{
    auto stream = std::make_unique<juce::MemoryInputStream> (data, size, false);
    return decode (std::unique_ptr<juce::AudioFormatReader> (formats().createReaderFor (std::move (stream))), out, error);
}

juce::String IRLibrary::hashFile (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) return {};
    return hashData (mb.getData(), mb.getSize());
}

juce::String IRLibrary::hashData (const void* data, size_t size)
{
    return juce::String::toHexString ((juce::int64) fnv1a (data, size)).paddedLeft ('0', 16);
}

dsp::IRSource IRLibrary::makeSyntheticIR (const juce::String& id)
{
    // Decaying, slightly band-limited stereo noise with a soft attack: a usable
    // stand-in until the ir_forge library is embedded.
    const double sr = 48000.0;
    double rt = 1.6, len = 2.0, hfDamp = 0.35;
    if (id.startsWith ("room")) { rt = 0.45; len = 0.7; hfDamp = 0.5; }
    else if (id.startsWith ("hall")) { rt = 2.6; len = 3.2; hfDamp = 0.6; }
    dsp::IRSource s;
    s.sampleRate = sr;
    const int n = (int) (len * sr);
    s.channels.assign (2, std::vector<double> ((size_t) n, 0.0));
    std::mt19937 rng ((unsigned) std::hash<std::string>() (id.toStdString()));
    std::normal_distribution<double> g (0.0, 1.0);
    for (int c = 0; c < 2; ++c)
    {
        double lp = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double env = std::exp (-6.907755 * t / rt) * (1.0 - std::exp (-t / 0.004));
            const double a = std::min (0.95, hfDamp + t * 0.25);
            lp = lp * a + g (rng) * (1.0 - a);
            s.channels[(size_t) c][(size_t) i] = lp * env;
        }
        // fade the end
        const int fade = (int) (0.05 * sr);
        for (int i = 0; i < fade; ++i) s.channels[(size_t) c][(size_t) (n - fade + i)] *= 0.5 + 0.5 * std::cos (dsp::kPi * (i + 1) / fade);
    }
    return s;
}
} // namespace fv
