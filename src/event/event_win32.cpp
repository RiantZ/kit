#include "kit/event.hpp"
#include "kit/types.h" // pulls in <windows.h> (HANDLE, DWORD, CreateEvent, ...) via ts_helpers.h

#include <cstring>

////////////////////////////////////////////////////////////////////////////////
struct c_event::s_impl
{
    struct s_mevent
    {
        e_type me_type;
        HANDLE mh_event;
    };

    uint8_t   mu_count;
    s_mevent *mp_events;
    bool      mb_init;
    HANDLE   *mp_handles;
};

////////////////////////////////////////////////////////////////////////////////
// helpers
namespace
{

void cleanup(c_event::s_impl *ip_impl)
{
    if(nullptr == ip_impl)
    {
        return;
    }

    if(ip_impl->mp_handles)
    {
        delete[] ip_impl->mp_handles;
        ip_impl->mp_handles = nullptr;
    }

    if(ip_impl->mp_events)
    {
        for(uint32_t lu_i = 0; lu_i < ip_impl->mu_count; lu_i++)
        {
            if(ip_impl->mp_events[lu_i].mh_event)
            {
                CloseHandle(ip_impl->mp_events[lu_i].mh_event);
                ip_impl->mp_events[lu_i].mh_event = nullptr;
            }
        }

        delete[] ip_impl->mp_events;
        ip_impl->mp_events = nullptr;
    }
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// c_event::c_event
c_event::c_event()
    : mp_impl(new s_impl)
{
    mp_impl->mu_count   = 0;
    mp_impl->mp_events  = nullptr;
    mp_impl->mb_init    = false;
    mp_impl->mp_handles = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::~c_event
c_event::~c_event()
{
    cleanup(mp_impl);
    delete mp_impl;
    mp_impl = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::init
bool c_event::init(uint8_t iu_count, const e_type *ip_types)
{
    if((true == mp_impl->mb_init) || (0 >= iu_count) || (nullptr == ip_types))
    {
        return false;
    }

    uint8_t lu_idx     = 0;

    mp_impl->mp_events = new s_impl::s_mevent[iu_count];
    if(nullptr == mp_impl->mp_events)
    {
        goto l_lblExit;
    }

    mp_impl->mp_handles = new HANDLE[iu_count];
    if(nullptr == mp_impl->mp_handles)
    {
        goto l_lblExit;
    }

    memset(mp_impl->mp_events, 0, sizeof(s_impl::s_mevent) * iu_count);

    while(lu_idx < iu_count)
    {
        mp_impl->mp_events[lu_idx].me_type = ip_types[lu_idx];

        if(e_single_auto == mp_impl->mp_events[lu_idx].me_type)
        {
            mp_impl->mp_events[lu_idx].mh_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        }
        else if(e_single_manual == mp_impl->mp_events[lu_idx].me_type)
        {
            mp_impl->mp_events[lu_idx].mh_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        }
        else if(e_multi == mp_impl->mp_events[lu_idx].me_type)
        {
            mp_impl->mp_events[lu_idx].mh_event = CreateSemaphore(nullptr, 0, 0xFFFFFF, nullptr);
        }

        mp_impl->mp_handles[lu_idx] = mp_impl->mp_events[lu_idx].mh_event;

        lu_idx++;
    }

    mp_impl->mb_init  = true;
    mp_impl->mu_count = iu_count;

l_lblExit:
    if(false == mp_impl->mb_init)
    {
        cleanup(mp_impl);
    }

    return mp_impl->mb_init;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::set
bool c_event::set(uint32_t iu_id)
{
    if(iu_id >= mp_impl->mu_count)
    {
        return false;
    }

    if(e_multi == mp_impl->mp_events[iu_id].me_type)
    {
        ReleaseSemaphore(mp_impl->mp_events[iu_id].mh_event, 1, nullptr);
    }
    else
    {
        SetEvent(mp_impl->mp_events[iu_id].mh_event);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::clr
bool c_event::clr(uint32_t iu_id)
{
    if((iu_id >= mp_impl->mu_count) || (e_single_manual != mp_impl->mp_events[iu_id].me_type))
    {
        return false;
    }

    ResetEvent(mp_impl->mp_events[iu_id].mh_event);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_event::wait
uint32_t c_event::wait()
{
    return wait(INFINITE);
}

////////////////////////////////////////////////////////////////////////////////
// c_event::wait
uint32_t c_event::wait(uint32_t iu_timeout_ms)
{
    DWORD lu_res = WaitForMultipleObjects(mp_impl->mu_count, mp_impl->mp_handles, FALSE, iu_timeout_ms);

    if((WAIT_TIMEOUT == lu_res) || (WAIT_FAILED == lu_res))
    {
        return c_event::timeout;
    }

    return (uint32_t)(lu_res - WAIT_OBJECT_0);
}
