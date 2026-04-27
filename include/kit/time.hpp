#pragma once

#include <cstdint>

namespace kit
{

/// Returns the current value of a high-resolution monotonic tick counter.
uint64_t get_hires_ticks();

/// Returns the tick-to-nanosecond conversion ratio: nanoseconds = ticks * o_numer / o_denom.
void get_hires_ticks_freq(uint64_t &o_numer, uint64_t &o_denom);

/// Returns system time as 100-nanosecond intervals since January 1, 1601 (UTC).
uint64_t get_system_time();

/// Returns the local UTC offset in seconds (e.g. +3600 for UTC+1, -18000 for UTC-5).
int32_t get_utc_offset_seconds();

} // namespace kit
