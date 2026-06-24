#include "kit/thread.hpp"

#include <pthread.h>
#include <sched.h>

namespace kit
{

static constexpr int gi_level_count = e_tp_time_critical + 1;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int map_to_native(e_thread_priority ie_priority)
{
    int li_min = sched_get_priority_min(SCHED_OTHER);
    int li_max = sched_get_priority_max(SCHED_OTHER);

    // Spread the levels evenly across the policy's valid range.
    return li_min + (static_cast<int>(ie_priority) * (li_max - li_min)) / (gi_level_count - 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static e_thread_priority map_from_native(int ii_native)
{
    e_thread_priority le_best = e_tp_normal;
    int               li_best = 1 << 30;

    for(int li_idx = 0; li_idx < gi_level_count; li_idx++)
    {
        int li_diff = map_to_native(static_cast<e_thread_priority>(li_idx)) - ii_native;
        if(li_diff < 0)
        {
            li_diff = -li_diff;
        }

        if(li_diff < li_best)
        {
            li_best = li_diff;
            le_best = static_cast<e_thread_priority>(li_idx);
        }
    }

    return le_best;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool get_thread_priority(e_thread_priority &oe_priority)
{
    int                li_policy = 0;
    struct sched_param lo_param  = {};

    if(pthread_getschedparam(pthread_self(), &li_policy, &lo_param) != 0)
    {
        return false;
    }

    oe_priority = map_from_native(lo_param.sched_priority);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool set_thread_priority(e_thread_priority ie_priority)
{
    if(ie_priority < e_tp_idle || ie_priority > e_tp_time_critical)
    {
        return false;
    }

    struct sched_param lo_param = {};
    lo_param.sched_priority     = map_to_native(ie_priority);

    return pthread_setschedparam(pthread_self(), SCHED_OTHER, &lo_param) == 0;
}

} // namespace kit
