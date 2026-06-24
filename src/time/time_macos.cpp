#include "kit/time.hpp"

#include <mach/mach_time.h>
#include <time.h>

static constexpr uint64_t gc_offset_1601_1970 = 116444736000000000ULL;
static constexpr uint64_t gc_100ns_per_sec    = 10000000ULL;

namespace kit
{

uint64_t get_hires_ticks()
{
    return mach_absolute_time();
}

void get_hires_ticks_freq(uint64_t &o_numer, uint64_t &o_denom)
{
    mach_timebase_info_data_t l_sInfo;
    mach_timebase_info(&l_sInfo);
    o_numer = l_sInfo.numer;
    o_denom = l_sInfo.denom;
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
