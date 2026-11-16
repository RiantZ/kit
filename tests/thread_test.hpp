#pragma once

#include <gtest/gtest.h>
#include <kit/thread.hpp>

#include <cstdint>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#if defined(__APPLE__)
    #include <mach/mach.h>
#endif

////////////////////////////////////////////////////////////////////////////////
// get returns a value for the calling thread
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, GetReturnsValue)
{
    kit::e_thread_priority le_priority = kit::e_tp_normal;
    EXPECT_TRUE(kit::get_thread_priority(le_priority));
}

////////////////////////////////////////////////////////////////////////////////
// setting normal priority succeeds and round-trips
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, SetNormalRoundTrips)
{
    ASSERT_TRUE(kit::set_thread_priority(kit::e_tp_normal));

    kit::e_thread_priority le_priority = kit::e_tp_idle;
    ASSERT_TRUE(kit::get_thread_priority(le_priority));
    EXPECT_EQ(kit::e_tp_normal, le_priority);
}

////////////////////////////////////////////////////////////////////////////////
// lowering priority works without privileges and round-trips to nearest bucket
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, SetBelowNormalRoundTrips)
{
    if(!kit::set_thread_priority(kit::e_tp_below_normal))
    {
        GTEST_SKIP() << "lowering priority not permitted in this environment";
    }

    kit::e_thread_priority le_priority = kit::e_tp_normal;
    ASSERT_TRUE(kit::get_thread_priority(le_priority));
    EXPECT_EQ(kit::e_tp_below_normal, le_priority);

    // restore
    kit::set_thread_priority(kit::e_tp_normal);
}

////////////////////////////////////////////////////////////////////////////////
// thread ID of the calling thread is never zero
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, IdNonZero)
{
    EXPECT_NE(0u, kit::get_thread_id());
}

////////////////////////////////////////////////////////////////////////////////
// repeated calls in one thread return the same ID
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, IdStableWithinThread)
{
    EXPECT_EQ(kit::get_thread_id(), kit::get_thread_id());
}

////////////////////////////////////////////////////////////////////////////////
// IDs differ between threads alive at the same time
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, IdDiffersAcrossLiveThreads)
{
    uint64_t lu_main   = kit::get_thread_id();
    uint64_t lu_worker = 0;

    std::thread lo_worker([&lu_worker]() { lu_worker = kit::get_thread_id(); });
    lo_worker.join();

    EXPECT_NE(0u, lu_worker);
    EXPECT_NE(lu_main, lu_worker);
}

////////////////////////////////////////////////////////////////////////////////
// ID matches an independent OS source
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, IdMatchesOs)
{
#if defined(_WIN32) || defined(_WIN64)
    EXPECT_EQ(static_cast<uint64_t>(GetThreadId(GetCurrentThread())), kit::get_thread_id());
#elif defined(__APPLE__)
    thread_identifier_info_data_t ls_info  = {};
    mach_msg_type_number_t        lu_count = THREAD_IDENTIFIER_INFO_COUNT;
    thread_info_t                 lp_info  = reinterpret_cast<thread_info_t>(&ls_info);
    mach_port_t                   lu_port  = mach_thread_self();
    kern_return_t                 li_ret   = thread_info(lu_port, THREAD_IDENTIFIER_INFO, lp_info, &lu_count);
    mach_port_deallocate(mach_task_self(), lu_port);

    ASSERT_EQ(KERN_SUCCESS, li_ret);
    EXPECT_EQ(ls_info.thread_id, kit::get_thread_id());
#elif defined(__linux__)
    // gtest runs tests on the main thread, whose ID equals the process ID
    EXPECT_EQ(static_cast<uint64_t>(getpid()), kit::get_thread_id());
#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
////////////////////////////////////////////////////////////////////////////////
// forked child gets its own ID, so the ID must never be cached
////////////////////////////////////////////////////////////////////////////////
TEST(thread_test, IdChangesAfterFork)
{
    uint64_t lu_parent = kit::get_thread_id();

    pid_t li_pid       = fork();
    ASSERT_NE(-1, li_pid);

    if(0 == li_pid)
    {
        // gtest assertions do not work in the child: report through the exit code
        uint64_t lu_child = kit::get_thread_id();
        bool     lb_ok    = lu_child != 0 && lu_child != lu_parent;
    #if defined(__linux__)
        // the child's only thread is its main thread, whose ID equals the process ID
        lb_ok = lb_ok && lu_child == static_cast<uint64_t>(getpid());
    #endif
        _exit(lb_ok ? 0 : 1);
    }

    int li_status = 0;
    ASSERT_EQ(li_pid, waitpid(li_pid, &li_status, 0));
    ASSERT_TRUE(WIFEXITED(li_status));
    EXPECT_EQ(0, WEXITSTATUS(li_status));
}
#endif
