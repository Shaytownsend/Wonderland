#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace fv::dsp
{
/** 64-byte aligned double buffer (SIMD-friendly, required by PFFFT/AVX). */
class AlignedBuffer
{
public:
    AlignedBuffer() = default;
    explicit AlignedBuffer (size_t n) { resize (n); }
    AlignedBuffer (AlignedBuffer&& o) noexcept : ptr (o.ptr), count (o.count) { o.ptr = nullptr; o.count = 0; }
    AlignedBuffer& operator= (AlignedBuffer&& o) noexcept
    {
        if (this != &o) { release(); ptr = o.ptr; count = o.count; o.ptr = nullptr; o.count = 0; }
        return *this;
    }
    AlignedBuffer (const AlignedBuffer&) = delete;
    AlignedBuffer& operator= (const AlignedBuffer&) = delete;
    ~AlignedBuffer() { release(); }

    void resize (size_t n);           // contents are zeroed
    void clear() noexcept;            // zero contents
    double* data() noexcept { return ptr; }
    const double* data() const noexcept { return ptr; }
    size_t size() const noexcept { return count; }
    double& operator[] (size_t i) noexcept { return ptr[i]; }
    const double& operator[] (size_t i) const noexcept { return ptr[i]; }

private:
    void release() noexcept;
    double* ptr = nullptr;
    size_t count = 0;
};

/** Real double-precision FFT of size N (power of two, >= 32) with an opaque,
    backend-specific spectrum layout of N doubles. Only the operations the
    partitioned convolver needs are exposed:

        spec  = forward (time)
        acc  += a * b                (complex multiply-accumulate, opaque layout)
        time  = inverse (spec) * scale

    Backends: Apple vDSP (macOS/iOS), PFFFT double (everything else).
    getConvolutionScale() is measured at construction so that
    inverse (forward (x) * forward (h)) * scale == circular_conv (x, h). */
class RealFFT
{
public:
    explicit RealFFT (int size);
    ~RealFFT();
    RealFFT (const RealFFT&) = delete;
    RealFFT& operator= (const RealFFT&) = delete;

    int getSize() const noexcept { return n; }

    /** in: N real samples. out: N doubles (opaque spectrum). work: N doubles scratch. */
    void forward (const double* in, double* out, double* work) const noexcept;
    /** in: N doubles (opaque spectrum). out: N real samples (unscaled). */
    void inverse (const double* in, double* out, double* work) const noexcept;
    /** acc += a * b, all opaque spectra of this FFT size. */
    void multiplyAccumulate (const double* a, const double* b, double* acc) const noexcept;

    double getConvolutionScale() const noexcept { return scale; }
    static const char* backendName() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    int n = 0;
    double scale = 1.0;
};
} // namespace fv::dsp
