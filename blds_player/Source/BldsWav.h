#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Strict decoder for official DirectOut "BLDS Generator v1.1" files:
// RIFF/WAVE, integer PCM, 24-bit, mono, header sample rate == expected rate.
// Runs only at construction time (never on the audio thread).
namespace blds
{
    // The one and only sample conversion used anywhere in this plugin.
    // Every 24-bit integer is exactly representable in float32, and dividing by a
    // power of two is exact, so this is lossless and bit-transparent on output.
    inline float int24ToFloat (std::int32_t s) noexcept
    {
        return float (s) / 8388608.0f;
    }

    struct WavDecodeResult
    {
        bool ok = false;
        std::string error;              // specific reason when !ok
        std::uint32_t headerSampleRate = 0;
        std::vector<float> samples;     // exact converted samples when ok
    };

    WavDecodeResult decodeBldsWav (const void* data, std::size_t numBytes, std::uint32_t expectedSampleRate);
}
