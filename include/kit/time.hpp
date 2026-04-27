#pragma once

#include <cstdint>

namespace kit
{

/// Returns the current value of a high-resolution monotonic tick counter.
uint64_t get_hires_ticks();

/// Returns the tick-to-nanosecond conversion ratio: nanoseconds = ticks * o_numer / o_denom.
void get_hires_ticks_freq(uint64_t &o_numer, uint64_t &o_denom);

} // namespace kit
