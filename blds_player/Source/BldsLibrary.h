#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace blds
{
    constexpr int numRates = 6;
    constexpr std::array<int, numRates> supportedRates { 44100, 48000, 88200, 96000, 176400, 192000 };

    std::string fileNameForRate (int rate);   // "BLDS_<rate>.wav"
    int slotForRate (int rate) noexcept;      // index into supportedRates, or -1

    // One decoded file. Everything here is immutable after the library is built,
    // so the UI may read it from any thread without synchronisation.
    struct Table
    {
        enum class State { missing, invalid, valid };

        int rate = 0;
        std::string fileName;
        State state = State::missing;
        std::string reason;                 // why it is invalid
        std::vector<float> samples;         // exact samples, non-empty when valid
        std::uint64_t length = 0;

        bool isValid() const noexcept       { return state == State::valid; }
    };

    // All six slots. Allocated and decoded once, at plugin construction.
    class Library
    {
    public:
        struct Blob
        {
            int rate = 0;
            const void* data = nullptr;
            std::size_t size = 0;
        };

        // Decodes the files embedded in the binary (JUCE BinaryData).
        static std::unique_ptr<Library> fromBinaryData();

        // Decodes caller-supplied blobs (used by the verification tests).
        static std::unique_ptr<Library> fromBlobs (const std::vector<Blob>& blobs);

        const Table& slot (int index) const noexcept     { return tables[(std::size_t) index]; }

        // Valid table for an exact integer rate, or nullptr.
        const Table* validTableForRate (int rate) const noexcept;

        // Any table (valid/invalid/missing) for a supported rate, or nullptr.
        const Table* tableForRate (int rate) const noexcept;

    private:
        Library();
        void load (int index, const void* data, std::size_t size);

        std::array<Table, numRates> tables;
    };
}
