#pragma once

#include <cstdint>

namespace kit
{

/// Abstract, OS-independent thread scheduling priority levels.
/// Each level is mapped to a platform-specific value by the implementation.
enum e_thread_priority
{
    e_tp_idle = 0,
    e_tp_lowest,
    e_tp_below_normal,
    e_tp_normal,
    e_tp_above_normal,
    e_tp_highest,
    e_tp_time_critical
};

/// Reads the scheduling priority of the calling thread.
/// @return true on success (oe_priority set), false on failure.
bool get_thread_priority(e_thread_priority &oe_priority);

/// Sets the scheduling priority of the calling thread.
/// @return true on success. Raising priority may fail without privileges.
bool set_thread_priority(e_thread_priority ie_priority);

/// Returns the OS thread ID of the calling thread: the value debuggers and system tools show.
/// Unique among live threads; Linux and Windows may reuse it after the thread exits.
/// @return the OS thread ID.
uint64_t get_thread_id();

} // namespace kit
