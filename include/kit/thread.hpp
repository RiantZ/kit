#pragma once

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

} // namespace kit
