#include "BldsLibrary.h"
#include "BldsWav.h"

#include "BldsEmbedded.h"

namespace blds
{
std::string fileNameForRate (int rate)
{
    return "BLDS_" + std::to_string (rate) + ".wav";
}

int slotForRate (int rate) noexcept
{
    for (int i = 0; i < numRates; ++i)
        if (supportedRates[(std::size_t) i] == rate)
            return i;

    return -1;
}

Library::Library()
{
    for (int i = 0; i < numRates; ++i)
    {
        auto& t = tables[(std::size_t) i];
        t.rate = supportedRates[(std::size_t) i];
        t.fileName = fileNameForRate (t.rate);
        t.state = Table::State::missing;
    }
}

void Library::load (int index, const void* data, std::size_t size)
{
    auto& t = tables[(std::size_t) index];
    auto decoded = decodeBldsWav (data, size, (std::uint32_t) t.rate);

    if (! decoded.ok)
    {
        t.state = Table::State::invalid;
        t.reason = decoded.error;
        t.samples.clear();
        t.length = 0;
        return;
    }

    t.state = Table::State::valid;
    t.reason.clear();
    t.samples = std::move (decoded.samples);
    t.length = (std::uint64_t) t.samples.size();
}

std::unique_ptr<Library> Library::fromEmbeddedFiles()
{
    std::unique_ptr<Library> lib (new Library());

    for (int i = 0; i < numRates; ++i)
    {
        const auto* f = embeddedFileForRate (lib->tables[(std::size_t) i].rate);

        if (f == nullptr)
            continue;

        if (f->size() != f->expectedSize)
        {
            auto& t = lib->tables[(std::size_t) i];
            t.state = Table::State::invalid;
            t.reason = "embedded size " + std::to_string (f->size()) + " bytes differs from file size "
                       + std::to_string (f->expectedSize) + " (rebuild the plugin)";
            continue;
        }

        lib->load (i, f->begin, f->size());
    }

    return lib;
}

std::unique_ptr<Library> Library::fromBlobs (const std::vector<Blob>& blobs)
{
    std::unique_ptr<Library> lib (new Library());

    for (const auto& b : blobs)
    {
        const int index = slotForRate (b.rate);

        if (index >= 0)
            lib->load (index, b.data, b.size);
    }

    return lib;
}

const Table* Library::tableForRate (int rate) const noexcept
{
    const int index = slotForRate (rate);
    return index >= 0 ? &tables[(std::size_t) index] : nullptr;
}

const Table* Library::validTableForRate (int rate) const noexcept
{
    const auto* t = tableForRate (rate);
    return (t != nullptr && t->isValid()) ? t : nullptr;
}
}
