#include "kit/time.hpp"

#include <time.h>

static constexpr uint64_t gc_offset_1601_1970 = 116444736000000000ULL;
static constexpr uint64_t gc_100ns_per_sec    = 10000000ULL;

namespace kit
{

uint64_t get_hires_ticks()
{
    struct timespec l_sTs;
    clock_gettime(CLOCK_MONOTONIC, &l_sTs);
    return static_cast<uint64_t>(l_sTs.tv_sec) * 1000000000ULL + static_cast<uint64_t>(l_sTs.tv_nsec);
}

void get_hires_ticks_freq(uint64_t &o_numer, uint64_t &o_denom)
{
    o_numer = 1;
    o_denom = 1;
}

uint64_t get_system_time()
{
    struct timespec l_sTs;
    clock_gettime(CLOCK_REALTIME, &l_sTs);
    uint64_t lu_100ns
        = static_cast<uint64_t>(l_sTs.tv_sec) * gc_100ns_per_sec + static_cast<uint64_t>(l_sTs.tv_nsec) / 100ULL;
    return lu_100ns + gc_offset_1601_1970;
}

int32_t get_utc_offset_seconds()
{
    time_t    li_time  = time(nullptr);
    struct tm lo_local = {};
    localtime_r(&li_time, &lo_local);
    return static_cast<int32_t>(lo_local.tm_gmtoff);
}

} // namespace kit
