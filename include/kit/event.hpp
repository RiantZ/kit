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

    /// @brief Creates an object with no events; call init() before use.
    c_event();

    /// @brief Releases the events.
    ~c_event();

    /// @brief Creates the events, one per entry of ip_types; succeeds only once per object.
    /// @param iu_count The number of events, at least 1.
    /// @param ip_types The type of each event, iu_count entries.
    /// @return true on success, false if already initialized or the arguments are invalid.
    bool init(uint8_t iu_count, const e_type *ip_types);

    /// @brief Signals an event.
    /// @param iu_id The event index, 0..(count - 1).
    /// @return true on success, false if iu_id is out of range or init() has not succeeded.
    bool set(uint32_t iu_id);

    /// @brief Clears a signaled e_single_manual event.
    /// @param iu_id The event index, 0..(count - 1).
    /// @return true if the event was cleared, false if it is out of range, not manual or not signaled.
    bool clr(uint32_t iu_id);

    /// @brief Waits with no timeout until any event is signaled.
    /// @return The index of the signaled event; an auto or multi event loses that signal.
    uint32_t wait();

    /// @brief Waits until any event is signaled or the timeout passes.
    /// @param iu_timeout_ms The timeout in milliseconds; 0 only checks the events.
    /// @return The index of the signaled event, or c_event::timeout.
    uint32_t wait(uint32_t iu_timeout_ms);

private:
    c_event(const c_event &)            = delete;
    c_event &operator=(const c_event &) = delete;

    s_impl *mp_impl;
};
