#include <gtest/gtest.h>

#include <kit/kit.h>

#include "list_test.hpp"
#include "spin_lock_test.hpp"
#include "mpsc_queue_test.hpp"
#include "shared_test.hpp"
#include "event_test.hpp"
#include "thread_test.hpp"
#include "file_test.hpp"

TEST(VersionTest, ReturnsNonEmpty)
{
    const char *lp_version = kit_version();
    ASSERT_NE(lp_version, nullptr);
    EXPECT_GT(strlen(lp_version), 0u);
}

TEST(VersionTest, MatchesMacro)
{
    EXPECT_STREQ(kit_version(), KIT_VERSION_STRING);
}
