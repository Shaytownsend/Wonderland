#include "FFT.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

#if FV_FFT_VDSP
 #include <Accelerate/Accelerate.h>
#elif FV_FFT_PFFFT
 #include "pffft_double.h"
#else
 #error "No FFT backend selected (FV_FFT_VDSP or FV_FFT_PFFFT)"
#endif

namespace fv::dsp
{
// ---------------------------------------------------------------------------
// AlignedBuffer
// ---------------------------------------------------------------------------
void AlignedBuffer::resize (size_t n)
{
    release();
    if (n == 0) return;
    const size_t bytes = ((n * sizeof (double) + 63) / 64) * 64;
   #if defined(_MSC_VER)
    ptr = static_cast<double*> (_aligned_malloc (bytes, 64));
   #else
    ptr = static_cast<double*> (std::aligned_alloc (64, bytes));
   #endif
    if (ptr == nullptr) throw std::bad_alloc();
    count = n;
    clear();
}

void AlignedBuffer::clear() noexcept
{
    if (ptr != nullptr) std::memset (ptr, 0, count * sizeof (double));
}

void AlignedBuffer::release() noexcept
{
    if (ptr == nullptr) return;
   #if defined(_MSC_VER)
    _aligned_free (ptr);
   #else
    std::free (ptr);
   #endif
    ptr = nullptr;
    count = 0;
}

// ---------------------------------------------------------------------------
// Backends
// ---------------------------------------------------------------------------
#if FV_FFT_VDSP
struct RealFFT::Impl
{
    explicit Impl (int size) : n (size)
    {
        log2n = 0;
        while ((1 << log2n) < n) ++log2n;
        setup = vDSP_create_fftsetupD (log2n, kFFTRadix2);
    }
    ~Impl() { vDSP_destroy_fftsetupD (setup); }

    // Spectrum layout: [ re(0..N/2-1) | im(0..N/2-1) ], re[0] = DC, im[0] = Nyquist.
    void forward (const double* in, double* out) const noexcept
    {
        const int h = n / 2;
        DSPDoubleSplitComplex s { out, out + h };
        vDSP_ctozD (reinterpret_cast<const DSPDoubleComplex*> (in), 2, &s, 1, (vDSP_Length) h);
        vDSP_fft_zripD (setup, &s, 1, (vDSP_Length) log2n, kFFTDirection_Forward);
    }
    void inverse (const double* in, double* out, double* work) const noexcept
    {
        const int h = n / 2;
        std::memcpy (work, in, sizeof (double) * (size_t) n);
        DSPDoubleSplitComplex s { work, work + h };
        vDSP_fft_zripD (setup, &s, 1, (vDSP_Length) log2n, kFFTDirection_Inverse);
        vDSP_ztocD (&s, 1, reinterpret_cast<DSPDoubleComplex*> (out), 2, (vDSP_Length) h);
    }
    void mac (const double* a, const double* b, double* acc) const noexcept
    {
        const int h = n / 2;
        // DC and Nyquist are packed real values
        const double dc = acc[0] + a[0] * b[0];
        const double ny = acc[h] + a[h] * b[h];
        DSPDoubleSplitComplex A { const_cast<double*> (a) + 1, const_cast<double*> (a) + h + 1 };
        DSPDoubleSplitComplex B { const_cast<double*> (b) + 1, const_cast<double*> (b) + h + 1 };
        DSPDoubleSplitComplex C { acc + 1, acc + h + 1 };
        vDSP_zvmaD (&A, 1, &B, 1, &C, 1, &C, 1, (vDSP_Length) (h - 1));
        acc[0] = dc;
        acc[h] = ny;
    }

    int n, log2n;
    FFTSetupD setup;
};

const char* RealFFT::backendName() noexcept { return "Apple vDSP (double)"; }

#elif FV_FFT_PFFFT
struct RealFFT::Impl
{
    explicit Impl (int size) : n (size) { setup = pffftd_new_setup (size, PFFFT_REAL); assert (setup != nullptr); }
    ~Impl() { pffftd_destroy_setup (setup); }

    // Spectrum layout: PFFFT internal "unordered" format (fastest for convolution).
    void forward (const double* in, double* out, double* work) const noexcept
    {
        pffftd_transform (setup, in, out, work, PFFFT_FORWARD);
    }
    void inverse (const double* in, double* out, double* work) const noexcept
    {
        pffftd_transform (setup, in, out, work, PFFFT_BACKWARD);
    }
    void mac (const double* a, const double* b, double* acc) const noexcept
    {
        pffftd_zconvolve_accumulate (setup, a, b, acc, 1.0);
    }

    int n;
    PFFFTD_Setup* setup;
};

const char* RealFFT::backendName() noexcept { return "PFFFT (double, SIMD)"; }
#endif

RealFFT::RealFFT (int size) : impl (std::make_unique<Impl> (size)), n (size)
{
    assert (size >= 32 && (size & (size - 1)) == 0);
    // Calibrate: inverse (F(delta) * F(delta)) gives scale^-1 at index 0.
    AlignedBuffer x ((size_t) n), X ((size_t) n), Y ((size_t) n), w ((size_t) n), y ((size_t) n);
    x[0] = 1.0;
    forward (x.data(), X.data(), w.data());
    multiplyAccumulate (X.data(), X.data(), Y.data());
    inverse (Y.data(), y.data(), w.data());
    scale = 1.0 / y[0];
}

RealFFT::~RealFFT() = default;

void RealFFT::forward (const double* in, double* out, double* work) const noexcept
{
   #if FV_FFT_VDSP
    (void) work;
    impl->forward (in, out);
   #else
    impl->forward (in, out, work);
   #endif
}

void RealFFT::inverse (const double* in, double* out, double* work) const noexcept
{
    impl->inverse (in, out, work);
}

void RealFFT::multiplyAccumulate (const double* a, const double* b, double* acc) const noexcept
{
    impl->mac (a, b, acc);
}
} // namespace fv::dsp
