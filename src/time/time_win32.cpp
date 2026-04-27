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
    o_numer = 1000000000ULL;
    o_denom = static_cast<uint64_t>(l_sFreq.QuadPart);
}

uint64_t get_system_time()
{
    FILETIME l_sFt;
    GetSystemTimePreciseAsFileTime(&l_sFt);
    return (static_cast<uint64_t>(l_sFt.dwHighDateTime) << 32) | static_cast<uint64_t>(l_sFt.dwLowDateTime);
}

int32_t get_utc_offset_seconds()
{
    TIME_ZONE_INFORMATION l_sTz = {};
    GetTimeZoneInformation(&l_sTz);
    return static_cast<int32_t>(l_sTz.Bias) * -60;
}

} // namespace kit
