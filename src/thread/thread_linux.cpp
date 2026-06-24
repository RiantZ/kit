#include "kit/thread.hpp"

#include <cerrno>
#include <sys/resource.h>

namespace kit
{

// Linux nice values are per-thread when addressed via who == 0. Lower nice
// means higher priority (range -20..19). Map the abstract levels onto a
// representative spread of nice values.
static const int ga_nice[] = {
    19,  // e_tp_idle
    10,  // e_tp_lowest
    5,   // e_tp_below_normal
    0,   // e_tp_normal
    -5,  // e_tp_above_normal
    -10, // e_tp_highest
    -19  // e_tp_time_critical
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static e_thread_priority map_from_nice(int ii_nice)
{
    e_thread_priority le_best = e_tp_normal;
    int               li_best = 1000;

    for(int li_idx = 0; li_idx <= e_tp_time_critical; li_idx++)
    {
        int li_diff = ga_nice[li_idx] - ii_nice;
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
    errno       = 0;
    int li_nice = getpriority(PRIO_PROCESS, 0);
    if(li_nice == -1 && errno != 0)
    {
        return false;
    }

    oe_priority = map_from_nice(li_nice);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool set_thread_priority(e_thread_priority ie_priority)
{
    if(ie_priority < e_tp_idle || ie_priority > e_tp_time_critical)
    {
        return false;
    }

    return setpriority(PRIO_PROCESS, 0, ga_nice[ie_priority]) == 0;
}

} // namespace kit
