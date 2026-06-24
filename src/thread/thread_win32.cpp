#include "kit/thread.hpp"

#include <windows.h>

namespace kit
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int map_to_native(e_thread_priority ie_priority)
{
    switch(ie_priority)
    {
    case e_tp_idle:
        return THREAD_PRIORITY_IDLE;
    case e_tp_lowest:
        return THREAD_PRIORITY_LOWEST;
    case e_tp_below_normal:
        return THREAD_PRIORITY_BELOW_NORMAL;
    case e_tp_normal:
        return THREAD_PRIORITY_NORMAL;
    case e_tp_above_normal:
        return THREAD_PRIORITY_ABOVE_NORMAL;
    case e_tp_highest:
        return THREAD_PRIORITY_HIGHEST;
    case e_tp_time_critical:
        return THREAD_PRIORITY_TIME_CRITICAL;
    }
    return THREAD_PRIORITY_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static e_thread_priority map_from_native(int ii_native)
{
    if(ii_native <= THREAD_PRIORITY_IDLE)
    {
        return e_tp_idle;
    }
    if(ii_native <= THREAD_PRIORITY_LOWEST)
    {
        return e_tp_lowest;
    }
    if(ii_native <= THREAD_PRIORITY_BELOW_NORMAL)
    {
        return e_tp_below_normal;
    }
    if(ii_native <= THREAD_PRIORITY_NORMAL)
    {
        return e_tp_normal;
    }
    if(ii_native <= THREAD_PRIORITY_ABOVE_NORMAL)
    {
        return e_tp_above_normal;
    }
    if(ii_native <= THREAD_PRIORITY_HIGHEST)
    {
        return e_tp_highest;
    }
    return e_tp_time_critical;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool get_thread_priority(e_thread_priority &oe_priority)
{
    int li_native = GetThreadPriority(GetCurrentThread());
    if(li_native == THREAD_PRIORITY_ERROR_RETURN)
    {
        return false;
    }

    oe_priority = map_from_native(li_native);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool set_thread_priority(e_thread_priority ie_priority)
{
    return SetThreadPriority(GetCurrentThread(), map_to_native(ie_priority)) != 0;
}

} // namespace kit
