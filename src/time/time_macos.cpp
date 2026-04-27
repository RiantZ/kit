#include "kit/time.hpp"

#include <mach/mach_time.h>

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

} // namespace kit
