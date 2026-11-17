#pragma once

#include <cstdint>

namespace kit
{

/// @brief Reads the high-resolution monotonic tick counter.
/// @return The current tick count; get_hires_ticks_freq() converts ticks to nanoseconds.
uint64_t get_hires_ticks();

/// @brief Gives the tick-to-nanosecond ratio: nanoseconds = ticks * o_numer / o_denom.
/// @param o_numer Receives the numerator.
/// @param o_denom Receives the denominator.
void get_hires_ticks_freq(uint64_t &o_numer, uint64_t &o_denom);

/// @brief Reads the system (wall clock) time.
/// @return 100-nanosecond intervals since January 1, 1601 (UTC).
uint64_t get_system_time();

/// @brief Reads the local offset from UTC.
/// @return The offset in seconds, e.g. +3600 for UTC+1, -18000 for UTC-5.
int32_t get_utc_offset_seconds();

} // namespace kit
