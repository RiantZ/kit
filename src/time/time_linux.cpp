#include "kit/time.hpp"

#include <time.h>

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
    // clock_gettime already returns nanoseconds, ratio is 1:1
    o_numer = 1;
    o_denom = 1;
}

} // namespace kit
