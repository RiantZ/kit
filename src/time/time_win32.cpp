#include "kit/time.hpp"

#if !defined(_WINSOCKAPI_)
    #include <winsock2.h>
#endif
#include <windows.h>

namespace kit
{

uint64_t get_hires_ticks()
{
    LARGE_INTEGER l_sCounter;
    QueryPerformanceCounter(&l_sCounter);
    return static_cast<uint64_t>(l_sCounter.QuadPart);
}

void get_hires_ticks_freq(uint64_t &o_numer, uint64_t &o_denom)
{
    LARGE_INTEGER l_sFreq;
    QueryPerformanceFrequency(&l_sFreq);
    // ticks * 1e9 / freq = nanoseconds, so numer=1e9, denom=freq
    o_numer = 1000000000ULL;
    o_denom = static_cast<uint64_t>(l_sFreq.QuadPart);
}

} // namespace kit
