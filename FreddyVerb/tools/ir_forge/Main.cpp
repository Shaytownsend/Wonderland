// ir_forge - offline renderer for the Freddy Verb factory impulse-response library.
//
//   ir_forge --list                 list all IR ids
//   ir_forge --all [--wav]          render everything: Resources/irs/<id>.flac + library.json (+ WAV masters)
//   ir_forge --id <id> [--id ..]    render selected IRs (library.json is updated, other entries kept)
//   options: --root <repo root>  --jobs <n>
//
// Output: 24-bit / 96 kHz, 4 channels in order L->L, L->R, R->L, R->R.
#include <juce_audio_formats/juce_audio_formats.h>

#include "Library.h"
#include "PlateReverb.h"

#include <atomic>
#include <cstdio>
#include <map>
#include <mutex>
#include <thread>

using namespace irf;

namespace
{
struct Rendered
{
    std::string id;
    double rt60 = 0, lengthS = 0;
    int64_t flacBytes = 0;
};

juce::File findRoot (const juce::String& given)
{
    if (given.isNotEmpty()) return juce::File::getCurrentWorkingDirectory().getChildFile (given);
    for (auto dir = juce::File::getCurrentWorkingDirectory(); ! dir.isRoot(); dir = dir.getParentDirectory())
        if (dir.getChildFile ("tools/ir_forge/CMakeLists.txt").existsAsFile()) return dir;
    for (auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile); ! dir.isRoot(); dir = dir.getParentDirectory())
        if (dir.getChildFile ("tools/ir_forge/CMakeLists.txt").existsAsFile()) return dir;
    return juce::File::getCurrentWorkingDirectory();
}

bool writeAudio (const Ir4& ir, const juce::File& file, bool flac)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
    if (! static_cast<juce::FileOutputStream*> (stream.get())->openedOk()) return false;

    std::unique_ptr<juce::AudioFormat> format;
    if (flac) format = std::make_unique<juce::FlacAudioFormat>();
    else format = std::make_unique<juce::WavAudioFormat>();

    const auto opts = juce::AudioFormatWriterOptions()
                          .withSampleRate (ir.fs)
                          .withNumChannels (4)
                          .withBitsPerSample (24)
                          .withQualityOptionIndex (flac ? 8 : 0);
    auto writer = format->createWriterFor (stream, opts);
    if (writer == nullptr) return false;

    // explicit rounding to 24 bit (left-justified int32 as the writer expects)
    const size_t n = ir.size();
    std::array<std::vector<int>, 4> data;
    const int* ptrs[5] {};
    for (int c = 0; c < 4; ++c)
    {
        data[(size_t) c].resize (n);
        for (size_t i = 0; i < n; ++i)
        {
            const long v = std::clamp (std::lround (ir.ch[(size_t) c][i] * 8388608.0), -8388608L, 8388607L);
            data[(size_t) c][i] = (int) (v * 256);
        }
        ptrs[c] = data[(size_t) c].data();
    }
    ptrs[4] = nullptr;
    return writer->write (ptrs, (int) n);
}

/** Decode the file again with JUCE (as the plugin will) and compare it with what we meant to write. */
bool verifyReadBack (const Ir4& ir, const juce::File& file, bool flac)
{
    std::unique_ptr<juce::AudioFormat> format;
    if (flac) format = std::make_unique<juce::FlacAudioFormat>();
    else format = std::make_unique<juce::WavAudioFormat>();
    std::unique_ptr<juce::AudioFormatReader> reader (format->createReaderFor (file.createInputStream().release(), true));
    if (reader == nullptr || reader->numChannels != 4 || (size_t) reader->lengthInSamples != ir.size()
        || (int) reader->sampleRate != 96000 || reader->bitsPerSample != 24)
        return false;
    juce::AudioBuffer<float> buf (4, (int) ir.size());
    reader->read (&buf, 0, (int) ir.size(), 0, true, true);
    for (int c = 0; c < 4; ++c)
        for (size_t i = 0; i < ir.size(); ++i)
            if (std::abs ((double) buf.getSample (c, (int) i) - ir.ch[(size_t) c][i]) > 1.5 / 8388608.0) return false;
    for (int c = 0; c < 4; ++c)
        if (buf.getSample (c, (int) ir.size() - 1) != 0.0f) return false;
    return true;
}

juce::String jsonString (const std::string& s) { return juce::JSON::toString (juce::var (juce::String (s))); }

juce::String fmt (double v, int dp) { return juce::String (v, dp); }

void writeLibraryJson (const juce::File& irDir, const std::map<std::string, Rendered>& fresh)
{
    // keep previous measurements for entries not re-rendered this run
    std::map<std::string, std::pair<double, double>> previous;
    const auto jsonFile = irDir.getChildFile ("library.json");
    if (jsonFile.existsAsFile())
    {
        const auto parsed = juce::JSON::parse (jsonFile);
        if (auto* arr = parsed.getArray())
            for (auto& e : *arr)
                previous[e["id"].toString().toStdString()] = { (double) e["rt60"], (double) e["length_s"] };
    }

    juce::String out = "[\n";
    bool first = true;
    for (const auto& d : factoryLibrary())
    {
        double rt = 0, len = 0;
        if (auto it = fresh.find (d.id); it != fresh.end()) { rt = it->second.rt60; len = it->second.lengthS; }
        else if (auto pit = previous.find (d.id); pit != previous.end() && irDir.getChildFile (d.id + ".flac").existsAsFile())
        { rt = pit->second.first; len = pit->second.second; }
        else continue;

        const auto col = colourFor (d.category, d.id);
        if (! first) out << ",\n";
        first = false;
        out << "  {\n"
            << "    \"id\": " << jsonString (d.id) << ",\n"
            << "    \"file\": " << jsonString (d.id + ".flac") << ",\n"
            << "    \"name\": " << jsonString (d.name) << ",\n"
            << "    \"category\": " << jsonString (d.category) << ",\n"
            << "    \"rt60\": " << fmt (rt, 3) << ",\n"
            << "    \"description\": " << jsonString (d.description) << ",\n"
            << "    \"channels\": 4,\n"
            << "    \"sample_rate\": 96000,\n"
            << "    \"length_s\": " << fmt (len, 3) << ",\n"
            << "    \"generator\": " << jsonString (d.generator) << ",\n"
            << "    \"seed\": " << juce::String ((juce::int64) d.seed) << ",\n"
            << "    \"color\": [" << col[0] << ", " << col[1] << ", " << col[2] << "]\n"
            << "  }";
    }
    out << "\n]\n";
    jsonFile.replaceWithText (out, false, false, "\n");
}

void usage()
{
    std::printf ("usage: ir_forge (--all | --id <id> [--id <id> ...] | --list) [--wav] [--root <dir>] [--jobs <n>]\n");
}
} // namespace

int main (int argc, char** argv)
{
    bool all = false, list = false, wav = false;
    std::vector<std::string> ids;
    juce::String rootArg;
    int jobs = (int) std::max (1u, std::thread::hardware_concurrency());

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--all") all = true;
        else if (a == "--list") list = true;
        else if (a == "--wav") wav = true;
        else if (a == "--id" && i + 1 < argc) ids.push_back (argv[++i]);
        else if (a == "--root" && i + 1 < argc) rootArg = argv[++i];
        else if (a == "--jobs" && i + 1 < argc) jobs = std::max (1, std::atoi (argv[++i]));
        else { usage(); return 1; }
    }

    const auto& lib = factoryLibrary();
    if (list)
    {
        for (const auto& d : lib)
            std::printf ("%-24s %-12s %s\n", d.id.c_str(), d.category.c_str(), d.name.c_str());
        return 0;
    }
    if (! all && ids.empty()) { usage(); return 1; }

    std::vector<const IrDef*> todo;
    if (all) for (const auto& d : lib) todo.push_back (&d);
    for (const auto& id : ids)
    {
        auto it = std::find_if (lib.begin(), lib.end(), [&] (const IrDef& d) { return d.id == id; });
        if (it == lib.end()) { std::fprintf (stderr, "unknown id: %s\n", id.c_str()); return 1; }
        if (std::find (todo.begin(), todo.end(), &*it) == todo.end()) todo.push_back (&*it);
    }

    const auto root = findRoot (rootArg);
    const auto irDir = root.getChildFile ("Resources/irs");
    const auto wavDir = root.getChildFile ("tools/ir_forge/out");
    irDir.createDirectory();
    if (wav) wavDir.createDirectory();

    const int outer = std::min (jobs, (int) todo.size());
    gInnerThreads = outer > 1 ? 1 : jobs;

    std::map<std::string, Rendered> results;
    std::mutex mtx;
    std::atomic<size_t> next { 0 };
    std::atomic<bool> failed { false };

    auto worker = [&] {
        for (;;)
        {
            const size_t k = next++;
            if (k >= todo.size()) return;
            const auto& d = *todo[k];
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            Ir4 ir = d.render (d.seed);
            {
                size_t bad = 0;
                for (auto& c : ir.ch) for (auto v : c) if (! std::isfinite (v)) ++bad;
                const auto edcProbe = measureDecay (ir, 0.0);
                if (bad > 0 || ir.peak() <= 0)
                    std::fprintf (stderr, "WARNING %s: %zu non-finite samples, peak %g, raw len %zu, T30 %g\n",
                                  d.id.c_str(), bad, ir.peak(), ir.size(), edcProbe.t30);
            }
            finishIr (ir, d.finish);
            Rendered r;
            r.id = d.id;
            r.rt60 = reportedRt60 (ir);
            r.lengthS = (double) ir.size() / ir.fs;

            const auto flacFile = irDir.getChildFile (d.id + ".flac");
            bool ok = writeAudio (ir, flacFile, true) && verifyReadBack (ir, flacFile, true);
            if (wav)
            {
                const auto wavFile = wavDir.getChildFile (d.id + ".wav");
                ok = writeAudio (ir, wavFile, false) && verifyReadBack (ir, wavFile, false) && ok;
            }
            r.flacBytes = flacFile.getSize();

            std::lock_guard<std::mutex> lock (mtx);
            if (! ok) { failed = true; std::fprintf (stderr, "FAILED writing %s\n", d.id.c_str()); }
            results[d.id] = r;
            std::printf ("%-24s rt60(1k,T30)=%5.2fs  len=%5.2fs  flac=%6.2f MB  (%.1fs)\n", d.id.c_str(), r.rt60, r.lengthS,
                         (double) r.flacBytes / 1.0e6, (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
            std::fflush (stdout);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < outer; ++t) pool.emplace_back (worker);
    for (auto& t : pool) t.join();

    writeLibraryJson (irDir, results);

    int64_t total = 0;
    for (auto& f : irDir.findChildFiles (juce::File::findFiles, false, "*.flac")) total += f.getSize();
    std::printf ("Resources/irs total: %.2f MB (%d files)\n", (double) total / 1.0e6,
                 irDir.getNumberOfChildFiles (juce::File::findFiles, "*.flac"));
    return failed ? 2 : 0;
}
