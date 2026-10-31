#include "IRManager.h"
#include "Parameters.h"

namespace fv
{
IRManager::IRManager (juce::AudioProcessorValueTreeState& s, dsp::FreddyEngine& e, const IRLibrary& lib)
    : juce::Thread ("FreddyVerb IR loader"), apvts (s), engine (e), library (lib)
{
    loadFactory (IRLibrary::defaultIRId);
    startThread (juce::Thread::Priority::low);
}

IRManager::~IRManager()
{
    stopThread (2000);
    while (auto c = engine.takeRetiredConvolver()) c.reset();
}

dsp::IRShapeParams IRManager::readShape() const
{
    auto v = [&] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    dsp::IRShapeParams sp;
    sp.startMs = v (pid::irStart);
    sp.lengthPct = v (pid::irLength);
    sp.decayPct = v (pid::irDecay);
    sp.sizePct = v (pid::irSize);
    sp.xoverMs = v (pid::irXover);
    sp.earlyDb = v (pid::irEarly);
    sp.lateDb = v (pid::irLate);
    sp.dampingPct = v (pid::irDamping);
    sp.dampFreqHz = v (pid::irDampFreq);
    sp.reverse = v (pid::irReverse) > 0.5f;
    sp.normalize = v (pid::irNormalize) > 0.5f;
    return sp;
}

int IRManager::readQuality() const { return (int) apvts.getRawParameterValue (pid::quality)->load(); }

void IRManager::setSource (std::shared_ptr<const dsp::IRSource> src, const juce::String& id, const juce::String& name,
                           const juce::String& path, const juce::String& h, bool factory, juce::MemoryBlock emb)
{
    {
        const juce::ScopedLock sl (lock);
        source = std::move (src);
        status.id = id;
        status.name = name;
        status.path = path;
        status.factory = factory;
        status.missing = false;
        status.embedded = emb.getSize() > 0;
        if (auto* e = library.find (id); factory && e != nullptr) status.category = e->category;
        else status.category = "User";
        hash = h;
        embedded = std::move (emb);
    }
    dirty.store (true);
}

void IRManager::loadFactory (const juce::String& id)
{
    auto src = std::make_shared<dsp::IRSource>();
    library.loadFactory (id, *src);
    const auto* e = library.find (id);
    setSource (std::move (src), id, e != nullptr ? e->name : id, {}, {}, true, {});
}

bool IRManager::loadFile (const juce::File& file, juce::String& error)
{
    auto src = std::make_shared<dsp::IRSource>();
    if (! IRLibrary::loadFile (file, *src, error)) return false;
    juce::MemoryBlock emb;
    if (file.getSize() <= (juce::int64) kMaxEmbedBytes) file.loadFileAsData (emb);
    setSource (std::move (src), file.getFullPathName(), file.getFileNameWithoutExtension(), file.getFullPathName(),
               IRLibrary::hashFile (file), false, std::move (emb));
    return true;
}

juce::ValueTree IRManager::getState() const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree t ("IR");
    t.setProperty ("kind", status.factory ? "factory" : "file", nullptr);
    t.setProperty ("id", status.id, nullptr);
    t.setProperty ("name", status.name, nullptr);
    if (! status.factory)
    {
        t.setProperty ("path", status.path, nullptr);
        t.setProperty ("hash", hash, nullptr);
        if (embedded.getSize() > 0) t.setProperty ("embedded", embedded.toBase64Encoding(), nullptr);
    }
    return t;
}

void IRManager::setState (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    if (t.getProperty ("kind").toString() == "file")
    {
        const juce::File f (t.getProperty ("path").toString());
        const auto wantHash = t.getProperty ("hash").toString();
        juce::String err;
        if (f.existsAsFile() && (wantHash.isEmpty() || IRLibrary::hashFile (f) == wantHash) && loadFile (f, err))
            return;
        // missing (or changed) file: fall back to the embedded copy if we have one
        juce::MemoryBlock emb;
        if (emb.fromBase64Encoding (t.getProperty ("embedded").toString()) && emb.getSize() > 0)
        {
            auto src = std::make_shared<dsp::IRSource>();
            if (IRLibrary::loadFromMemory (emb.getData(), emb.getSize(), *src, err))
            {
                setSource (std::move (src), f.getFullPathName(), t.getProperty ("name").toString(), f.getFullPathName(),
                           wantHash, false, std::move (emb));
                const juce::ScopedLock sl (lock);
                status.missing = ! f.existsAsFile();
                return;
            }
        }
        // keep the IR identity so the UI can offer to relink, and play the default meanwhile
        loadFactory (IRLibrary::defaultIRId);
        const juce::ScopedLock sl (lock);
        status.id = f.getFullPathName();
        status.name = t.getProperty ("name").toString();
        status.path = f.getFullPathName();
        status.factory = false;
        status.missing = true;
        hash = wantHash;
        return;
    }
    loadFactory (t.getProperty ("id", IRLibrary::defaultIRId).toString());
}

IRManager::Status IRManager::getStatus() const
{
    const juce::ScopedLock sl (lock);
    auto s = status;
    s.version = version.load();
    return s;
}

void IRManager::prepare (double sr, int block, bool nonRealtime)
{
    sampleRate.store (sr);
    maxBlock.store (block);
    offline.store (nonRealtime);
    rebuild (true);
}

void IRManager::rebuild (bool installNow)
{
    const juce::ScopedLock bl (buildLock);
    busy.store (true);
    std::shared_ptr<const dsp::IRSource> src;
    {
        const juce::ScopedLock sl (lock);
        src = source;
    }
    const auto shape = readShape();
    const int quality = readQuality();
    const double sr = sampleRate.load();
    dirty.store (false);

    dsp::IRDisplayInfo info;
    const auto ir = dsp::IRProcessor::render (src != nullptr ? *src : dsp::IRSource(), shape, sr, &info);
    dsp::Convolver::Config cfg;
    cfg.quality = quality;
    cfg.maxBlockSize = maxBlock.load();
    cfg.allowAsync = ! offline.load();
    auto conv = std::make_unique<dsp::Convolver> (ir, cfg, &engine.getWorker());

    if (installNow) engine.installConvolverNow (std::move (conv));
    else if (auto replaced = engine.offerConvolver (std::move (conv))) replaced.reset();

    lastShape = shape;
    lastQuality = quality;
    lastRate = sr;
    rt60.store (info.rt60 > 0 ? info.rt60 : 1.0);
    {
        const juce::ScopedLock sl (lock);
        status.display = std::move (info);
    }
    ++version;
    busy.store (false);
}

void IRManager::run()
{
    while (! threadShouldExit())
    {
        while (auto c = engine.takeRetiredConvolver()) c.reset();

        {
            const juce::ScopedLock bl (buildLock);
            const auto shape = readShape();
            const int quality = readQuality();
            const double sr = sampleRate.load();
            const auto now = juce::Time::getMillisecondCounter();
            // debounce: rebuild once the values have settled for 30 ms
            if (shape != lastShape || quality != lastQuality || sr != lastRate)
            {
                dirty.store (true);
                lastChangeMs = now;
                lastShape = shape; lastQuality = quality; lastRate = sr;
            }
            if (dirty.load() && now - lastChangeMs >= 30)
                rebuild (false);
        }
        wait (15);
    }
}

void IRManager::waitUntilIdle (int timeoutMs)
{
    const auto start = juce::Time::getMillisecondCounter();
    while (juce::Time::getMillisecondCounter() - start < (juce::uint32) timeoutMs)
    {
        if (! dirty.load() && ! busy.load() && readShape() == lastShape && readQuality() == lastQuality) return;
        juce::Thread::sleep (5);
    }
}
} // namespace fv
