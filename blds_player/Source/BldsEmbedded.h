#pragma once

#include <cstddef>

// The official BLDS files compiled into this binary (see CMakeLists.txt, which
// generates the definitions with .incbin so the bytes are embedded verbatim).
namespace blds
{
    struct EmbeddedFile
    {
        int rate;                       // from the file name
        const char* fileName;           // "BLDS_<rate>.wav"
        const unsigned char* begin;
        const unsigned char* end;
        std::size_t expectedSize;       // file size measured at configure time

        std::size_t size() const noexcept  { return (std::size_t) (end - begin); }
    };

    const EmbeddedFile* embeddedFiles() noexcept;
    int numEmbeddedFiles() noexcept;

    // The embedded file for a rate, or nullptr if none was embedded.
    inline const EmbeddedFile* embeddedFileForRate (int rate) noexcept
    {
        for (int i = 0; i < numEmbeddedFiles(); ++i)
            if (embeddedFiles()[i].rate == rate)
                return embeddedFiles() + i;

        return nullptr;
    }
}
