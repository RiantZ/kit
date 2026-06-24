#pragma once

#include <gtest/gtest.h>
#include <kit/thread.hpp>

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
