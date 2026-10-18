#include "BldsWav.h"

#include <cstring>

namespace blds
{
namespace
{
    std::uint16_t readU16 (const unsigned char* p) noexcept
    {
        return (std::uint16_t) (p[0] | (p[1] << 8));
    }

    std::uint32_t readU32 (const unsigned char* p) noexcept
    {
        return (std::uint32_t) p[0]
             | ((std::uint32_t) p[1] << 8)
             | ((std::uint32_t) p[2] << 16)
             | ((std::uint32_t) p[3] << 24);
    }

    bool idIs (const unsigned char* p, const char* id) noexcept
    {
        return std::memcmp (p, id, 4) == 0;
    }

    std::string printableId (const unsigned char* p)
    {
        std::string s;

        for (int i = 0; i < 4; ++i)
            s += (p[i] >= 32 && p[i] < 127) ? (char) p[i] : '?';

        return s;
    }

    WavDecodeResult fail (std::string reason)
    {
        WavDecodeResult r;
        r.error = std::move (reason);
        return r;
    }

    // KSDATAFORMAT_SUBTYPE_PCM {00000001-0000-0010-8000-00aa00389b71}
    const unsigned char pcmSubFormat[16] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
                                             0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };
}

WavDecodeResult decodeBldsWav (const void* data, std::size_t numBytes, std::uint32_t expectedSampleRate)
{
    const auto* bytes = static_cast<const unsigned char*> (data);

    if (bytes == nullptr || numBytes < 12)
        return fail ("file too short to be a WAV (" + std::to_string (numBytes) + " bytes)");

    if (! idIs (bytes, "RIFF"))
    {
        if (idIs (bytes, "RF64") || idIs (bytes, "BW64"))
            return fail ("RF64/BW64 container, expected plain RIFF/WAVE");

        return fail ("not a RIFF file");
    }

    if (! idIs (bytes + 8, "WAVE"))
        return fail ("RIFF type is '" + printableId (bytes + 8) + "', expected 'WAVE'");

    const std::uint64_t riffSize = readU32 (bytes + 4);

    if (riffSize + 8 > numBytes)
        return fail ("file truncated (RIFF header says " + std::to_string (riffSize + 8)
                     + " bytes, file has " + std::to_string (numBytes) + ")");

    const std::size_t end = (std::size_t) (riffSize + 8);
    std::size_t pos = 12;

    bool haveFmt = false;
    const unsigned char* audio = nullptr;
    std::size_t audioBytes = 0;
    std::uint32_t sampleRate = 0;

    while (pos + 8 <= end)
    {
        const unsigned char* chunk = bytes + pos;
        const std::uint64_t chunkSize = readU32 (chunk + 4);
        const std::size_t body = pos + 8;

        if (body + chunkSize > end)
            return fail ("chunk '" + printableId (chunk) + "' is truncated");

        if (idIs (chunk, "fmt "))
        {
            if (haveFmt)
                return fail ("more than one 'fmt ' chunk");

            if (chunkSize < 16)
                return fail ("'fmt ' chunk too small (" + std::to_string (chunkSize) + " bytes)");

            const unsigned char* f = bytes + body;
            const std::uint16_t formatTag     = readU16 (f + 0);
            const std::uint16_t channels      = readU16 (f + 2);
            sampleRate                        = readU32 (f + 4);
            const std::uint32_t byteRate      = readU32 (f + 8);
            const std::uint16_t blockAlign    = readU16 (f + 12);
            const std::uint16_t bitsPerSample = readU16 (f + 14);

            if (formatTag == 0xFFFE)
            {
                if (chunkSize < 40 || readU16 (f + 16) < 22)
                    return fail ("WAVE_FORMAT_EXTENSIBLE header is incomplete");

                const std::uint16_t validBits = readU16 (f + 18);

                if (std::memcmp (f + 24, pcmSubFormat, 16) != 0)
                    return fail ("extensible sub-format is not integer PCM");

                if (validBits != 24)
                    return fail (std::to_string (validBits) + " valid bits per sample, expected 24");
            }
            else if (formatTag == 3)
            {
                return fail ("32-bit float format, expected 24-bit integer PCM");
            }
            else if (formatTag != 1)
            {
                return fail ("format tag " + std::to_string (formatTag) + " is not PCM");
            }

            if (channels != 1)
                return fail (std::to_string (channels) + " channels, expected mono");

            if (bitsPerSample != 24)
                return fail (std::to_string (bitsPerSample) + "-bit, expected 24-bit");

            if (blockAlign != 3)
                return fail ("block align " + std::to_string (blockAlign) + ", expected 3");

            if (sampleRate != expectedSampleRate)
                return fail ("header says " + std::to_string (sampleRate) + " Hz but filename says "
                             + std::to_string (expectedSampleRate) + " Hz");

            if (byteRate != sampleRate * 3u)
                return fail ("inconsistent byte rate " + std::to_string (byteRate));

            haveFmt = true;
        }
        else if (idIs (chunk, "data"))
        {
            if (audio != nullptr)
                return fail ("more than one 'data' chunk");

            audio = bytes + body;
            audioBytes = (std::size_t) chunkSize;
        }

        // Chunks are word aligned; a pad byte follows odd-sized chunks.
        pos = body + (std::size_t) chunkSize + (std::size_t) (chunkSize & 1u);
    }

    if (! haveFmt)
        return fail ("no 'fmt ' chunk");

    if (audio == nullptr)
        return fail ("no 'data' chunk");

    if (audioBytes % 3 != 0)
        return fail ("data size " + std::to_string (audioBytes) + " is not a whole number of 24-bit samples");

    const std::size_t numSamples = audioBytes / 3;

    if (numSamples == 0)
        return fail ("contains no audio samples (length 0)");

    WavDecodeResult result;
    result.headerSampleRate = sampleRate;
    result.samples.resize (numSamples);

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        const unsigned char* p = audio + i * 3;
        std::int32_t s = (std::int32_t) ((std::uint32_t) p[0] | ((std::uint32_t) p[1] << 8) | ((std::uint32_t) p[2] << 16));

        if (s & 0x800000)
            s -= 0x1000000;   // sign-extend 24 -> 32 bits

        result.samples[i] = int24ToFloat (s);
    }

    result.ok = true;
    return result;
}
}
