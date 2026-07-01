#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <kit/list.hpp>
#include <kit/mpsc_queue.hpp>

TEST(MpscQueueTests, FifoOrderSingleThread)
{
    kit::c_mpsc_queue<int> lc_queue(8);

    EXPECT_TRUE(lc_queue.empty());
    EXPECT_EQ(lc_queue.capacity(), 8u);

    for(int li_i = 0; li_i < 8; li_i++)
    {
        EXPECT_TRUE(lc_queue.try_push(li_i));
    }

    EXPECT_EQ(lc_queue.size(), 8u);

    int li_value = -1;
    for(int li_i = 0; li_i < 8; li_i++)
    {
        EXPECT_TRUE(lc_queue.try_pop(li_value));
        EXPECT_EQ(li_value, li_i);
    }

    EXPECT_TRUE(lc_queue.empty());
}

TEST(MpscQueueTests, FullAndEmptyReturnFalse)
{
    kit::c_mpsc_queue<int> lc_queue(4);

    for(int li_i = 0; li_i < 4; li_i++)
    {
        EXPECT_TRUE(lc_queue.try_push(li_i));
    }

    // Queue is full now.
    EXPECT_FALSE(lc_queue.try_push(42));

    int li_value = 0;
    for(int li_i = 0; li_i < 4; li_i++)
    {
        EXPECT_TRUE(lc_queue.try_pop(li_value));
    }

    // Queue is empty now.
    EXPECT_FALSE(lc_queue.try_pop(li_value));
}

TEST(MpscQueueTests, Concurrency)
{
    constexpr uint32_t lu_per_producer = 100'000;

    kit::c_mpsc_queue<uint64_t> lc_queue(1024);

    uint32_t lu_producers = std::thread::hardware_concurrency();
    if(lu_producers < 2)
    {
        lu_producers = 2;
    }

    const uint64_t lu_total        = (uint64_t)lu_producers * lu_per_producer;
    const uint64_t lu_expected_sum = lu_total * (lu_total - 1) / 2;

    std::atomic<uint64_t> lc_next_value { 0 };

    std::vector<std::thread> lc_threads;
    for(uint32_t lu_p = 0; lu_p < lu_producers; lu_p++)
    {
        lc_threads.push_back(std::thread(
            [&]
            {
                for(uint32_t lu_i = 0; lu_i < lu_per_producer; lu_i++)
                {
                    uint64_t lu_value = lc_next_value.fetch_add(1, std::memory_order_relaxed);
                    while(!lc_queue.try_push(lu_value))
                    {
                        std::this_thread::yield();
                    }
                }
            }));
    }

    uint64_t lu_received_count = 0;
    uint64_t lu_received_sum   = 0;
    uint64_t lu_value          = 0;

    while(lu_received_count < lu_total)
    {
        if(lc_queue.try_pop(lu_value))
        {
            lu_received_sum += lu_value;
            lu_received_count++;
        }
        else
        {
            std::this_thread::yield();
        }
    }

    for(auto &lr_thread : lc_threads)
    {
        lr_thread.join();
    }

    EXPECT_EQ(lu_received_count, lu_total);
    EXPECT_EQ(lu_received_sum, lu_expected_sum);
    EXPECT_TRUE(lc_queue.empty());
}

////////////////////////////////////////////////////////////////////////////////
// Performance tests
//
// Each producer attempts a fixed number of pushes. When the bounded queue (or the
// capped list in the locked variant) is full, the item is dropped and counted as a
// loss instead of being retried, so the tests both measure throughput and report
// how much data was lost under back-pressure.
namespace
{

/// @brief Outcome of a single producer/consumer performance run.
struct s_perf_result
{
    uint64_t mu_pushed;   ///< Items successfully enqueued
    uint64_t mu_dropped;  ///< Items dropped because the queue was full (loss)
    uint64_t mu_received; ///< Items dequeued by the consumer
    double   md_mips;     ///< Throughput in millions of received items per second
};

/// @brief Converts an item count and time span into millions of items per second.
inline double mpsc_mitems_per_sec(uint64_t                                              iu_items,
                                  const std::chrono::high_resolution_clock::time_point &ir_begin,
                                  const std::chrono::high_resolution_clock::time_point &ir_end)
{
    uint64_t lu_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(ir_end - ir_begin).count();
    if(0 == lu_ns)
    {
        return 0.0;
    }

    return (double)iu_items * 1000.0 / (double)lu_ns;
}

/// @brief Runs a multi-producer / single-consumer scenario on the lock-free queue.
inline s_perf_result run_lockfree_mpsc(uint32_t iu_producers, uint32_t iu_per_producer, size_t iz_capacity)
{
    kit::c_mpsc_queue<uint64_t> lc_queue(iz_capacity);

    std::atomic<uint64_t> lc_pushed { 0 };
    std::atomic<uint64_t> lc_dropped { 0 };
    std::atomic<uint32_t> lc_active { iu_producers };

    auto lo_begin = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> lc_threads;
    for(uint32_t lu_p = 0; lu_p < iu_producers; lu_p++)
    {
        lc_threads.push_back(std::thread(
            [&, lu_p]
            {
                uint64_t lu_base    = (uint64_t)lu_p * iu_per_producer;
                uint64_t lu_pushed  = 0;
                uint64_t lu_dropped = 0;

                for(uint32_t lu_i = 0; lu_i < iu_per_producer; lu_i++)
                {
                    if(lc_queue.try_push(lu_base + lu_i))
                    {
                        lu_pushed++;
                    }
                    else
                    {
                        lu_dropped++;
                    }
                }

                lc_pushed.fetch_add(lu_pushed, std::memory_order_relaxed);
                lc_dropped.fetch_add(lu_dropped, std::memory_order_relaxed);
                lc_active.fetch_sub(1, std::memory_order_release);
            }));
    }

    uint64_t lu_received = 0;
    uint64_t lu_value    = 0;
    for(;;)
    {
        if(lc_queue.try_pop(lu_value))
        {
            lu_received++;
            continue;
        }

        // Once every producer has finished, no new items can appear, so a final
        // drain guarantees we account for everything that was pushed.
        if(0 == lc_active.load(std::memory_order_acquire))
        {
            while(lc_queue.try_pop(lu_value))
            {
                lu_received++;
            }
            break;
        }
    }

    auto lo_end = std::chrono::high_resolution_clock::now();

    for(auto &lr_thread : lc_threads)
    {
        lr_thread.join();
    }

    s_perf_result lo_result;
    lo_result.mu_pushed   = lc_pushed.load(std::memory_order_relaxed);
    lo_result.mu_dropped  = lc_dropped.load(std::memory_order_relaxed);
    lo_result.mu_received = lu_received;
    lo_result.md_mips     = mpsc_mitems_per_sec(lu_received, lo_begin, lo_end);
    return lo_result;
}

/// @brief Runs the same scenario using a classic mutex-protected list (kit::c_lst).
inline s_perf_result run_locked_list_mpsc(uint32_t iu_producers, uint32_t iu_per_producer, size_t iz_capacity)
{
    kit::c_lst<uint64_t> lc_list(16384);
    std::mutex           lc_mutex;

    std::atomic<uint64_t> lc_pushed { 0 };
    std::atomic<uint64_t> lc_dropped { 0 };
    std::atomic<uint32_t> lc_active { iu_producers };

    auto lo_begin = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> lc_threads;
    for(uint32_t lu_p = 0; lu_p < iu_producers; lu_p++)
    {
        lc_threads.push_back(std::thread(
            [&, lu_p]
            {
                uint64_t lu_base    = (uint64_t)lu_p * iu_per_producer;
                uint64_t lu_pushed  = 0;
                uint64_t lu_dropped = 0;

                for(uint32_t lu_i = 0; lu_i < iu_per_producer; lu_i++)
                {
                    bool lb_pushed = false;
                    {
                        std::lock_guard<std::mutex> lo_guard(lc_mutex);
                        // if(lc_list.size() < iz_capacity)
                        //{
                        lc_list.push_last(lu_base + lu_i);
                        lb_pushed = true;
                        //}
                    }

                    if(lb_pushed)
                    {
                        lu_pushed++;
                    }
                    else
                    {
                        lu_dropped++;
                    }
                }

                lc_pushed.fetch_add(lu_pushed, std::memory_order_relaxed);
                lc_dropped.fetch_add(lu_dropped, std::memory_order_relaxed);
                lc_active.fetch_sub(1, std::memory_order_release);
            }));
    }

    uint64_t lu_received = 0;
    for(;;)
    {
        bool lb_got = false;
        {
            std::lock_guard<std::mutex> lo_guard(lc_mutex);
            if(lc_list.size() > 0)
            {
                lc_list.pull_first();
                lb_got = true;
            }
        }

        if(lb_got)
        {
            lu_received++;
            continue;
        }

        if(0 == lc_active.load(std::memory_order_acquire))
        {
            for(;;)
            {
                std::lock_guard<std::mutex> lo_guard(lc_mutex);
                if(0 == lc_list.size())
                {
                    break;
                }
                lc_list.pull_first();
                lu_received++;
            }
            break;
        }
    }

    auto lo_end = std::chrono::high_resolution_clock::now();

    for(auto &lr_thread : lc_threads)
    {
        lr_thread.join();
    }

    s_perf_result lo_result;
    lo_result.mu_pushed   = lc_pushed.load(std::memory_order_relaxed);
    lo_result.mu_dropped  = lc_dropped.load(std::memory_order_relaxed);
    lo_result.mu_received = lu_received;
    lo_result.md_mips     = mpsc_mitems_per_sec(lu_received, lo_begin, lo_end);
    return lo_result;
}

} // anonymous namespace

TEST(MpscQueuePerf, SingleProducerSingleConsumer)
{
    constexpr uint32_t lu_items    = 5'000'000;
    constexpr size_t   lz_capacity = 1024;

    s_perf_result lo_res           = run_lockfree_mpsc(1, lu_items, lz_capacity);

    std::cout << "[1P/1C ] lock-free MPSC: " << lo_res.md_mips << "M items/s"
              << ", pushed=" << lo_res.mu_pushed << ", received=" << lo_res.mu_received
              << ", dropped(loss)=" << lo_res.mu_dropped << " ("
              << (100.0 * (double)lo_res.mu_dropped / (double)lu_items) << "%)" << std::endl;

    EXPECT_EQ(lo_res.mu_received, lo_res.mu_pushed);
    EXPECT_EQ(lo_res.mu_pushed + lo_res.mu_dropped, (uint64_t)lu_items);
}

TEST(MpscQueuePerf, TenProducersSingleConsumer)
{
    constexpr uint32_t lu_producers    = 10;
    constexpr uint32_t lu_per_producer = 1'000'000;
    constexpr size_t   lz_capacity     = 16384;

    s_perf_result lo_res               = run_lockfree_mpsc(lu_producers, lu_per_producer, lz_capacity);

    const uint64_t lu_total            = (uint64_t)lu_producers * lu_per_producer;

    std::cout << "[10P/1C] lock-free MPSC: " << lo_res.md_mips << "M items/s"
              << ", pushed=" << lo_res.mu_pushed << ", received=" << lo_res.mu_received
              << ", dropped(loss)=" << lo_res.mu_dropped << " ("
              << (100.0 * (double)lo_res.mu_dropped / (double)lu_total) << "%)" << std::endl;

    EXPECT_EQ(lo_res.mu_received, lo_res.mu_pushed);
    EXPECT_EQ(lo_res.mu_pushed + lo_res.mu_dropped, lu_total);
}

TEST(MpscQueuePerf, LockFreeVsLockedListTenProducers)
{
    constexpr uint32_t lu_producers    = 10;
    constexpr uint32_t lu_per_producer = 1'000'000;
    constexpr size_t   lz_capacity     = 16384;

    const uint64_t lu_total            = (uint64_t)lu_producers * lu_per_producer;

    s_perf_result lo_lockfree          = run_lockfree_mpsc(lu_producers, lu_per_producer, lz_capacity);
    s_perf_result lo_locked            = run_locked_list_mpsc(lu_producers, lu_per_producer, lz_capacity);

    std::cout << "[10P/1C] lock-free MPSC : " << lo_lockfree.md_mips << "M items/s"
              << ", loss=" << lo_lockfree.mu_dropped << " ("
              << (100.0 * (double)lo_lockfree.mu_dropped / (double)lu_total) << "%)" << std::endl;
    std::cout << "[10P/1C] mutex + c_lst  : " << lo_locked.md_mips << "M items/s"
              << ", loss=" << lo_locked.mu_dropped << " (" << (100.0 * (double)lo_locked.mu_dropped / (double)lu_total)
              << "%)" << std::endl;

    double ld_speedup = (lo_locked.md_mips > 0.0) ? (lo_lockfree.md_mips / lo_locked.md_mips) : 0.0;
    std::cout << "[10P/1C] speedup (lock-free / locked): " << ld_speedup << "x" << std::endl;

    EXPECT_EQ(lo_lockfree.mu_received, lo_lockfree.mu_pushed);
    EXPECT_EQ(lo_locked.mu_received, lo_locked.mu_pushed);
    EXPECT_GT(lo_lockfree.md_mips, 0.0);
    EXPECT_GT(lo_locked.md_mips, 0.0);
}
