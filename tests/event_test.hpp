#pragma once

#include <gtest/gtest.h>
#include <kit/event.hpp>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

////////////////////////////////////////////////////////////////////////////////
// init with nullptr types / zero count fails
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, InitInvalidArgs)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto };

    EXPECT_FALSE(lo_event.init(0, lp_types));
    EXPECT_FALSE(lo_event.init(1, nullptr));
}

////////////////////////////////////////////////////////////////////////////////
// init twice fails (already initialized)
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, InitTwiceFails)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto };

    ASSERT_TRUE(lo_event.init(1, lp_types));
    EXPECT_FALSE(lo_event.init(1, lp_types));
}

////////////////////////////////////////////////////////////////////////////////
// single auto: set then wait returns its index, second wait times out
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, SingleAutoSetWait)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto };
    ASSERT_TRUE(lo_event.init(1, lp_types));

    ASSERT_TRUE(lo_event.set(0));
    EXPECT_EQ(0u, lo_event.wait(1000));

    // auto-reset: no more signals pending
    EXPECT_EQ(c_event::timeout, lo_event.wait(0));
}

////////////////////////////////////////////////////////////////////////////////
// wait with no signal times out
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, WaitTimeout)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto };
    ASSERT_TRUE(lo_event.init(1, lp_types));

    auto lo_start  = std::chrono::steady_clock::now();
    auto lu_result = lo_event.wait(50);
    auto li_elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lo_start).count();

    EXPECT_EQ(c_event::timeout, lu_result);
    EXPECT_GE(li_elapsed, 40); // allow some scheduling slack below the 50 ms request
}

////////////////////////////////////////////////////////////////////////////////
// single manual stays signaled across waits until clr()
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, SingleManualStaysSignaled)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_manual };
    ASSERT_TRUE(lo_event.init(1, lp_types));

    ASSERT_TRUE(lo_event.set(0));

    // manual-reset: repeated waits keep returning the same index
    EXPECT_EQ(0u, lo_event.wait(1000));
    EXPECT_EQ(0u, lo_event.wait(1000));

    ASSERT_TRUE(lo_event.clr(0));
    EXPECT_EQ(c_event::timeout, lo_event.wait(0));
}

////////////////////////////////////////////////////////////////////////////////
// clr is rejected for non-manual events
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, ClrRejectedForAuto)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto };
    ASSERT_TRUE(lo_event.init(1, lp_types));

    EXPECT_FALSE(lo_event.clr(0));
}

////////////////////////////////////////////////////////////////////////////////
// multi (counting) accumulates multiple sets
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, MultiCounting)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_multi };
    ASSERT_TRUE(lo_event.init(1, lp_types));

    ASSERT_TRUE(lo_event.set(0));
    ASSERT_TRUE(lo_event.set(0));
    ASSERT_TRUE(lo_event.set(0));

    EXPECT_EQ(0u, lo_event.wait(1000));
    EXPECT_EQ(0u, lo_event.wait(1000));
    EXPECT_EQ(0u, lo_event.wait(1000));
    EXPECT_EQ(c_event::timeout, lo_event.wait(0));
}

////////////////////////////////////////////////////////////////////////////////
// set / wait out of range
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, SetOutOfRange)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto, c_event::e_single_auto };
    ASSERT_TRUE(lo_event.init(2, lp_types));

    EXPECT_FALSE(lo_event.set(2));
    EXPECT_FALSE(lo_event.clr(2));
}

////////////////////////////////////////////////////////////////////////////////
// wait-any across several events returns a signaled index
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, WaitAny)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto, c_event::e_single_auto, c_event::e_single_auto };
    ASSERT_TRUE(lo_event.init(3, lp_types));

    ASSERT_TRUE(lo_event.set(2));
    EXPECT_EQ(2u, lo_event.wait(1000));

    ASSERT_TRUE(lo_event.set(0));
    EXPECT_EQ(0u, lo_event.wait(1000));
}

////////////////////////////////////////////////////////////////////////////////
// producer thread signals while consumer blocks in wait()
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, ProducerConsumer)
{
    c_event               lo_event;
    const c_event::e_type lp_types[] = { c_event::e_single_auto };
    ASSERT_TRUE(lo_event.init(1, lp_types));

    std::atomic<bool> lb_started { false };

    std::thread lo_producer(
        [&]()
        {
            lb_started.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            lo_event.set(0);
        });

    while(!lb_started.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    EXPECT_EQ(0u, lo_event.wait(5000));

    lo_producer.join();
}

////////////////////////////////////////////////////////////////////////////////
// many producers each set their own auto event; consumer drains them all
////////////////////////////////////////////////////////////////////////////////
TEST(c_event_test, ConcurrentProducers)
{
    static constexpr uint8_t EVENT_COUNT = 8;

    c_event                      lo_event;
    std::vector<c_event::e_type> lo_types(EVENT_COUNT, c_event::e_multi);
    ASSERT_TRUE(lo_event.init(EVENT_COUNT, lo_types.data()));

    std::atomic<bool>        lb_start { false };
    std::vector<std::thread> lo_threads;
    lo_threads.reserve(EVENT_COUNT);

    for(uint8_t i = 0; i < EVENT_COUNT; i++)
    {
        lo_threads.emplace_back(
            [&, i]()
            {
                while(!lb_start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                lo_event.set(i);
            });
    }

    lb_start.store(true, std::memory_order_release);

    // drain exactly EVENT_COUNT signals and verify each index appears once
    std::vector<int> lo_seen(EVENT_COUNT, 0);
    for(uint8_t i = 0; i < EVENT_COUNT; i++)
    {
        uint32_t lu_idx = lo_event.wait(5000);
        ASSERT_NE(c_event::timeout, lu_idx);
        ASSERT_LT(lu_idx, (uint32_t)EVENT_COUNT);
        lo_seen[lu_idx]++;
    }

    for(auto &t : lo_threads)
    {
        t.join();
    }

    for(uint8_t i = 0; i < EVENT_COUNT; i++)
    {
        EXPECT_EQ(1, lo_seen[i]) << "event " << (int)i << " signaled " << lo_seen[i] << " times";
    }

    EXPECT_EQ(c_event::timeout, lo_event.wait(0));
}
