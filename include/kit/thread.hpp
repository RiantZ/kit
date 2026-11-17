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

/// @brief Reads the scheduling priority of the calling thread.
/// @param oe_priority Receives the priority.
/// @return true on success, false on failure.
bool get_thread_priority(e_thread_priority &oe_priority);

/// @brief Sets the scheduling priority of the calling thread.
/// @param ie_priority The new priority.
/// @return true on success; raising the priority may fail without privileges.
bool set_thread_priority(e_thread_priority ie_priority);

/// @brief Reads the OS thread ID of the calling thread: the value debuggers and system tools show.
/// @return The thread ID; unique among live threads, Linux and Windows may reuse it after the thread exits.
uint64_t get_thread_id();

} // namespace kit
