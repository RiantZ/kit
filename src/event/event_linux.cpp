#include "kit/event.hpp"

#include <pthread.h>
#include <semaphore.h>
#include <ctime>
#include <cstring>

////////////////////////////////////////////////////////////////////////////////
// CMEvent design: a single counting semaphore tracks the total number of
// pending signals across all events, while per-event counters + a round-robin
// scan decide which event index a wait() returns. This avoids the abs.-time
// "wait" POSIX functions that are affected by system time changes.
struct c_event::s_impl
{
    struct s_event
    {
        int32_t  mi_counter;
        e_type   me_type;
        uint32_t mu_idx;
        s_event *mp_next;
    };

    pthread_mutex_t ms_mutex;
    sem_t           ms_sem;
    uint8_t         mu_count;
    bool            mb_init;
    bool            mb_error;
    s_event        *mp_events;
    s_event        *mp_event_cur;
    int32_t         mi_signals;
};

////////////////////////////////////////////////////////////////////////////////
// helpers
namespace
{

void cleanup(c_event::s_impl *ip_impl)
{
    if((nullptr != ip_impl) && (ip_impl->mp_events))
    {
        delete[] ip_impl->mp_events;
        ip_impl->mp_events = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////
// walk through all events to find a pending signal, returns its index or
// c_event::timeout. Must be called under the mutex.
uint32_t get_signal(c_event::s_impl *ip_impl)
{
    uint32_t lu_return = c_event::timeout;

    if(0 == ip_impl->mi_signals)
    {
        return lu_return;
    }

    c_event::s_impl::s_event *lp_start = ip_impl->mp_event_cur;
    do
    {
        ip_impl->mp_event_cur = ip_impl->mp_event_cur->mp_next;

        if(ip_impl->mp_event_cur->mi_counter)
        {
            lu_return = ip_impl->mp_event_cur->mu_idx;
            if(c_event::e_single_manual == ip_impl->mp_event_cur->me_type)
            {
                sem_post(&ip_impl->ms_sem); // keep manual events level-triggered
            }
            else
            {
                ip_impl->mp_event_cur->mi_counter--;
                ip_impl->mi_signals--;
            }

            break;
        }
    } while(lp_start != ip_impl->mp_event_cur);

    if(c_event::timeout == lu_return)
    {
        ip_impl->mi_signals--;
    }

    return lu_return;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// c_event::c_event
c_event::c_event()
    : mp_impl(new s_impl)
{
    mp_impl->mu_count     = 0;
    mp_impl->mb_init      = false;
    mp_impl->mb_error     = false;
    mp_impl->mp_events    = nullptr;
    mp_impl->mp_event_cur = nullptr;
    mp_impl->mi_signals   = 0;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::~c_event
c_event::~c_event()
{
    if(mp_impl->mb_init)
    {
        pthread_mutex_destroy(&mp_impl->ms_mutex);
        sem_destroy(&mp_impl->ms_sem);
    }

    cleanup(mp_impl);

    delete mp_impl;
    mp_impl = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::init
bool c_event::init(uint8_t iu_count, const e_type *ip_types)
{
    bool    lb_sig   = false;
    bool    lb_mutex = false;
    uint8_t lu_idx   = 0;

    if((true == mp_impl->mb_init) || (true == mp_impl->mb_error) || (0 >= iu_count) || (nullptr == ip_types))
    {
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////
    // initialize semaphore
    if(0 != sem_init(&mp_impl->ms_sem, 0, 0))
    {
        goto l_lblExit;
    }
    else
    {
        lb_sig = true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // initialize mutex
    if(0 != pthread_mutex_init(&mp_impl->ms_mutex, nullptr))
    {
        mp_impl->mb_error = true;
        goto l_lblExit;
    }
    else
    {
        lb_mutex = true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // initialize events structure
    mp_impl->mp_events = new s_impl::s_event[iu_count];
    if(nullptr == mp_impl->mp_events)
    {
        goto l_lblExit;
    }

    memset(mp_impl->mp_events, 0, sizeof(s_impl::s_event) * iu_count);

    mp_impl->mp_event_cur = &mp_impl->mp_events[0];

    while(lu_idx < iu_count)
    {
        mp_impl->mp_events[lu_idx].me_type    = ip_types[lu_idx];
        mp_impl->mp_events[lu_idx].mi_counter = 0;
        mp_impl->mp_events[lu_idx].mu_idx     = lu_idx;

        // make round trip links
        if((lu_idx + 1) < iu_count)
        {
            mp_impl->mp_events[lu_idx].mp_next = &mp_impl->mp_events[lu_idx + 1];
        }
        else
        {
            mp_impl->mp_events[lu_idx].mp_next = &mp_impl->mp_events[0];
        }

        lu_idx++;
    }

    mp_impl->mb_init  = true;
    mp_impl->mu_count = iu_count;

l_lblExit:
    if(false == mp_impl->mb_init)
    {
        mp_impl->mb_error = true;

        if(lb_mutex)
        {
            pthread_mutex_destroy(&mp_impl->ms_mutex);
        }

        if(lb_sig)
        {
            sem_destroy(&mp_impl->ms_sem);
        }

        cleanup(mp_impl);
    }

    return mp_impl->mb_init;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::set
bool c_event::set(uint32_t iu_id)
{
    if((iu_id >= mp_impl->mu_count) || (false == mp_impl->mb_init))
    {
        return false;
    }

    pthread_mutex_lock(&mp_impl->ms_mutex);

    mp_impl->mi_signals++;
    mp_impl->mp_events[iu_id].mi_counter++;

    sem_post(&mp_impl->ms_sem);

    pthread_mutex_unlock(&mp_impl->ms_mutex);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::clr
bool c_event::clr(uint32_t iu_id)
{
    bool lb_return = false;

    if((iu_id >= mp_impl->mu_count) || (e_single_manual != mp_impl->mp_events[iu_id].me_type))
    {
        return lb_return;
    }

    pthread_mutex_lock(&mp_impl->ms_mutex);

    if(mp_impl->mp_events[iu_id].mi_counter)
    {
        mp_impl->mi_signals--;
        mp_impl->mp_events[iu_id].mi_counter--;

        sem_trywait(&mp_impl->ms_sem); // decrease semaphore

        lb_return = true;
    }

    pthread_mutex_unlock(&mp_impl->ms_mutex);

    return lb_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::wait
uint32_t c_event::wait()
{
    uint32_t lu_return = c_event::timeout;

    sem_wait(&mp_impl->ms_sem);

    pthread_mutex_lock(&mp_impl->ms_mutex);
    lu_return = get_signal(mp_impl);
    pthread_mutex_unlock(&mp_impl->ms_mutex);

    return lu_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::wait
uint32_t c_event::wait(uint32_t iu_timeout_ms)
{
    uint64_t        lu_nano   = (uint64_t)iu_timeout_ms * 1000000ULL;
    uint32_t        lu_return = c_event::timeout;
    struct timespec ls_time   = { 0, 0 };

    if(0 == iu_timeout_ms)
    {
        if(0 == sem_trywait(&mp_impl->ms_sem))
        {
            pthread_mutex_lock(&mp_impl->ms_mutex);
            lu_return = get_signal(mp_impl);
            pthread_mutex_unlock(&mp_impl->ms_mutex);
        }
    }
    else
    {
        clock_gettime(CLOCK_REALTIME, &ls_time);

        lu_nano         += ls_time.tv_nsec;
        ls_time.tv_sec  += (time_t)(lu_nano / 1000000000ULL);
        ls_time.tv_nsec  = (long)(lu_nano % 1000000000ULL);

        if(0 == sem_timedwait(&mp_impl->ms_sem, &ls_time))
        {
            pthread_mutex_lock(&mp_impl->ms_mutex);
            lu_return = get_signal(mp_impl);
            pthread_mutex_unlock(&mp_impl->ms_mutex);
        }
    }

    return lu_return;
}
