#pragma once

#include <cstdint>

////////////////////////////////////////////////////////////////////////////////
// c_event - multi-event wait primitive (wait-any). Manages a small set of
//           events, each of type e_single_auto, e_single_manual or e_multi,
//           and lets a thread wait until ANY of them is signaled, returning
//           the index of the signaled event.
class c_event
{
public:
    // Opaque, platform-specific implementation (defined in each event_*.cpp).
    struct s_impl;

    enum e_type
    {
        e_single_auto = 0, // auto-reset, releases one waiter per set()
        e_single_manual,   // manual-reset, stays signaled until clr()
        e_multi            // counting / semaphore
    };

    // Returned by wait() on timeout; valid signal indices are 0..(count - 1).
    static constexpr uint32_t timeout = 0xFFFFFFFFu;

    c_event();
    ~c_event();

    // Array-based replacement for the original va_list Init(count, ...).
    bool init(uint8_t iu_count, const e_type *ip_types);

    bool set(uint32_t iu_id);

    bool clr(uint32_t iu_id); // only meaningful for e_single_manual

    uint32_t wait();

    uint32_t wait(uint32_t iu_timeout_ms);

private:
    c_event(const c_event &)            = delete;
    c_event &operator=(const c_event &) = delete;

    s_impl *mp_impl;
};
