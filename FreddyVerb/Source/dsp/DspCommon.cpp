#include "DspCommon.h"

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define FV_X86 1
#endif

namespace fv::dsp
{
ScopedNoDenormalsFV::ScopedNoDenormalsFV() noexcept
{
   #if FV_X86
    saved = (intptr_t) _mm_getcsr();
    _mm_setcsr ((unsigned int) saved | 0x8040); // FTZ | DAZ
   #elif defined(__aarch64__)
    intptr_t fpcr;
    asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
    saved = fpcr;
    asm volatile ("msr fpcr, %0" : : "ri"(fpcr | (1 << 24)));
   #endif
}

ScopedNoDenormalsFV::~ScopedNoDenormalsFV() noexcept
{
   #if FV_X86
    _mm_setcsr ((unsigned int) saved);
   #elif defined(__aarch64__)
    asm volatile ("msr fpcr, %0" : : "ri"(saved));
   #endif
}
} // namespace fv::dsp
