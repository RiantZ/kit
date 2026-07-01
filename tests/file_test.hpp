#pragma once

#include <gtest/gtest.h>
#include <kit/file.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////
// helper: build a unique temp path under the OS temp dir for a given test
namespace c_file_test_helpers
{

inline std::filesystem::path make_tmp_path(const char *ip_stem)
{
    static std::atomic<uint32_t> sa_counter { 0 };

    uint32_t              lu_seq  = sa_counter.fetch_add(1, std::memory_order_relaxed);
    std::filesystem::path lo_p    = std::filesystem::temp_directory_path();
    lo_p                         /= std::string("kit_file_test_") + ip_stem + "_" + std::to_string(lu_seq);
    std::filesystem::remove(lo_p);
    return lo_p;
}

////////////////////////////////////////////////////////////////////////////////
// RAII helper: removes a path on scope exit
struct s_scoped_path
{
    std::filesystem::path mo_path;

    explicit s_scoped_path(const std::filesystem::path &ir_p)
        : mo_path(ir_p)
    {
    }

    ~s_scoped_path()
    {
        std::error_code lo_ec;
        std::filesystem::remove(mo_path, lo_ec);
    }
};

} // namespace c_file_test_helpers

using c_file_test_helpers::make_tmp_path;
using c_file_test_helpers::s_scoped_path;

////////////////////////////////////////////////////////////////////////////////
// lifecycle: is_open() before/after open/close
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, LifecycleOpenCloseIsOpen)
{
    auto          lo_path = make_tmp_path("lifecycle");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    EXPECT_FALSE(lo_file.is_open());
    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    EXPECT_TRUE(lo_file.is_open());
    EXPECT_TRUE(lo_file.close(false));
    EXPECT_FALSE(lo_file.is_open());
}

////////////////////////////////////////////////////////////////////////////////
// ref-count: N add_ref + (N+1) release destroys the object exactly once
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, RefCountAddReleaseDeletesOnce)
{
    // We cannot directly observe the destructor from outside, but we can
    // observe intermediate ref counts and rely on the final release() to
    // free memory (leak-sanitizer / valgrind will flag if it does not).
    kit::c_file *lp_file = new kit::c_file();
    EXPECT_EQ(2, lp_file->add_ref());
    EXPECT_EQ(3, lp_file->add_ref());
    EXPECT_EQ(2, lp_file->release());
    EXPECT_EQ(1, lp_file->release());
    EXPECT_EQ(0, lp_file->release()); // triggers delete this
}

////////////////////////////////////////////////////////////////////////////////
// open dispositions
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, OpenModeCreateNewSucceedsThenFails)
{
    auto          lo_path = make_tmp_path("create_new");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_new, kit::e_ff_read | kit::e_ff_write));
    lo_file.close(false);

    // second create_new must fail because the file already exists
    EXPECT_FALSE(lo_file.open(lo_path, kit::e_fom_create_new, kit::e_ff_read | kit::e_ff_write));
    EXPECT_NE(0, lo_file.get_last_error());
}

TEST(c_file_test, OpenModeOpenExistingMissingFails)
{
    auto        lo_path = make_tmp_path("open_existing_missing");
    kit::c_file lo_file;

    EXPECT_FALSE(lo_file.open(lo_path, kit::e_fom_open_existing, kit::e_ff_read));
    EXPECT_NE(0, lo_file.get_last_error());
}

TEST(c_file_test, OpenModeCreateAlwaysTruncates)
{
    auto          lo_path = make_tmp_path("create_always");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    // seed some content
    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), true));
    lo_file.close(false);

    // create_always should zero it out
    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    EXPECT_EQ(0u, lo_file.get_size());
    lo_file.close(false);
}

TEST(c_file_test, OpenModeOpenOrCreatePreservesContent)
{
    auto          lo_path = make_tmp_path("open_or_create");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), true));
    lo_file.close(false);

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_open_or_create, kit::e_ff_read | kit::e_ff_write));
    EXPECT_EQ(sizeof(la_seed), lo_file.get_size());
    lo_file.close(false);
}

TEST(c_file_test, OpenModeTruncateExistingRequiresExisting)
{
    auto        lo_path = make_tmp_path("trunc_existing_missing");
    kit::c_file lo_file;

    EXPECT_FALSE(lo_file.open(lo_path, kit::e_fom_truncate_existing, kit::e_ff_read | kit::e_ff_write));
}

////////////////////////////////////////////////////////////////////////////////
// stream write/read round-trip: three writes, one read of the concatenation
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, StreamWriteReadRoundTrip)
{
    auto          lo_path = make_tmp_path("stream_rw");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    const uint8_t la_p1[3] = { 'A', 'B', 'C' };
    const uint8_t la_p2[4] = { '1', '2', '3', '4' };
    const uint8_t la_p3[5] = { 'x', 'y', 'z', '!', '?' };
    ASSERT_EQ(sizeof(la_p1), lo_file.write(la_p1, sizeof(la_p1), false));
    ASSERT_EQ(sizeof(la_p2), lo_file.write(la_p2, sizeof(la_p2), false));
    ASSERT_EQ(sizeof(la_p3), lo_file.write(la_p3, sizeof(la_p3), true));

    ASSERT_TRUE(lo_file.set_position(0));

    uint8_t la_out[12] = {};
    ASSERT_EQ(sizeof(la_out), lo_file.read(la_out, sizeof(la_out)));

    const uint8_t la_exp[12] = { 'A', 'B', 'C', '1', '2', '3', '4', 'x', 'y', 'z', '!', '?' };
    EXPECT_EQ(0, std::memcmp(la_out, la_exp, sizeof(la_exp)));
}

////////////////////////////////////////////////////////////////////////////////
// positional I/O: multiple write_at at distinct offsets, read_at each,
// and get_position() must not be affected on POSIX. On Windows the implicit
// pointer moves — we still verify that read_at at fixed offsets returns
// expected content.
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, PositionalReadAtWriteAt)
{
    auto          lo_path = make_tmp_path("positional");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    const uint8_t la_a[4] = { 'A', 'A', 'A', 'A' };
    const uint8_t la_b[4] = { 'B', 'B', 'B', 'B' };
    const uint8_t la_c[4] = { 'C', 'C', 'C', 'C' };

    // write into a sparse layout at 0, 16 and 32
    EXPECT_EQ(sizeof(la_a), lo_file.write_at(0, la_a, sizeof(la_a)));
    EXPECT_EQ(sizeof(la_b), lo_file.write_at(16, la_b, sizeof(la_b)));
    EXPECT_EQ(sizeof(la_c), lo_file.write_at(32, la_c, sizeof(la_c)));

    uint8_t la_out[4] = {};

    ASSERT_EQ(sizeof(la_out), lo_file.read_at(16, la_out, sizeof(la_out)));
    EXPECT_EQ(0, std::memcmp(la_out, la_b, sizeof(la_out)));

    ASSERT_EQ(sizeof(la_out), lo_file.read_at(0, la_out, sizeof(la_out)));
    EXPECT_EQ(0, std::memcmp(la_out, la_a, sizeof(la_out)));

    ASSERT_EQ(sizeof(la_out), lo_file.read_at(32, la_out, sizeof(la_out)));
    EXPECT_EQ(0, std::memcmp(la_out, la_c, sizeof(la_out)));
}

////////////////////////////////////////////////////////////////////////////////
// scatter/gather: write_v of 4 buffers, read the concatenation
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, ScatterGatherWriteVReadConcat)
{
    auto          lo_path = make_tmp_path("scatter");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    uint8_t la_h[2]        = { 'H', 'i' };
    uint8_t la_s[1]        = { '-' };
    uint8_t la_w[5]        = { 'w', 'o', 'r', 'l', 'd' };
    uint8_t la_e[1]        = { '!' };

    kit::s_iovec la_iov[4] = {
        { la_h, sizeof(la_h) },
        { la_s, sizeof(la_s) },
        { la_w, sizeof(la_w) },
        { la_e, sizeof(la_e) },
    };

    size_t lz_expect = sizeof(la_h) + sizeof(la_s) + sizeof(la_w) + sizeof(la_e);
    EXPECT_EQ(lz_expect, lo_file.write_v(la_iov, 4, true));

    ASSERT_TRUE(lo_file.set_position(0));

    uint8_t la_out[9] = {};
    ASSERT_EQ(sizeof(la_out), lo_file.read(la_out, sizeof(la_out)));
    EXPECT_EQ(0, std::memcmp(la_out, "Hi-world!", 9));
}

////////////////////////////////////////////////////////////////////////////////
// set_size / preallocate
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, SetSizeGrowsAndShrinks)
{
    auto          lo_path = make_tmp_path("set_size");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    // grow to 4 KiB - zero fill
    ASSERT_TRUE(lo_file.set_size(4096));
    EXPECT_EQ(4096u, lo_file.get_size());

    uint8_t la_out[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    ASSERT_EQ(sizeof(la_out), lo_file.read_at(1000, la_out, sizeof(la_out)));
    EXPECT_EQ(0u, la_out[0]);
    EXPECT_EQ(0u, la_out[3]);

    // shrink back to 64 bytes
    ASSERT_TRUE(lo_file.set_size(64));
    EXPECT_EQ(64u, lo_file.get_size());
}

TEST(c_file_test, PreallocateReservesSpace)
{
    auto          lo_path = make_tmp_path("prealloc");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    ASSERT_TRUE(lo_file.preallocate(64 * 1024));
    EXPECT_GE(lo_file.get_size(), 64u * 1024u);
}

////////////////////////////////////////////////////////////////////////////////
// append semantics: seek back to 0, write still lands at EOF
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, AppendAlwaysWritesAtEnd)
{
    auto          lo_path = make_tmp_path("append");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    // seed with three bytes
    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[3] = { 'a', 'b', 'c' };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), true));
    lo_file.close(false);

    // reopen in append mode, seek to 0 and write - it should still go to EOF
    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_open_or_create, kit::e_ff_read | kit::e_ff_write | kit::e_ff_append));
    (void)lo_file.set_position(0);
    const uint8_t la_tail[2] = { 'X', 'Y' };
    ASSERT_EQ(sizeof(la_tail), lo_file.write(la_tail, sizeof(la_tail), true));

    EXPECT_EQ(5u, lo_file.get_size());

    uint8_t la_out[5] = {};
    ASSERT_EQ(sizeof(la_out), lo_file.read_at(0, la_out, sizeof(la_out)));
    EXPECT_EQ(0, std::memcmp(la_out, "abcXY", 5));
}

////////////////////////////////////////////////////////////////////////////////
// hints: sequential and random independently OK, both together fail
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, HintsAcceptedIndividually)
{
    auto          lo_path = make_tmp_path("hint_seq");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_seq;

    EXPECT_TRUE(
        lo_seq.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write | kit::e_ff_hint_sequential));
    lo_seq.close(false);

    auto          lo_path2 = make_tmp_path("hint_rnd");
    s_scoped_path lo_guard2(lo_path2);
    kit::c_file   lo_rnd;
    EXPECT_TRUE(
        lo_rnd.open(lo_path2, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write | kit::e_ff_hint_random));
    lo_rnd.close(false);
}

TEST(c_file_test, HintsBothRejected)
{
    auto          lo_path = make_tmp_path("hint_both");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    EXPECT_FALSE(lo_file.open(lo_path,
                              kit::e_fom_create_always,
                              kit::e_ff_read | kit::e_ff_write | kit::e_ff_hint_sequential | kit::e_ff_hint_random));
    EXPECT_NE(0, lo_file.get_last_error());
}

////////////////////////////////////////////////////////////////////////////////
// get_last_error is non-zero after a failing open
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, LastErrorReportedOnMissingOpen)
{
    auto        lo_path = make_tmp_path("last_err");
    kit::c_file lo_file;

    EXPECT_FALSE(lo_file.open(lo_path, kit::e_fom_open_existing, kit::e_ff_read));
    EXPECT_NE(0, lo_file.get_last_error());
}

////////////////////////////////////////////////////////////////////////////////
// close is idempotent
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, CloseIsIdempotent)
{
    auto          lo_path = make_tmp_path("close_idem");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    EXPECT_TRUE(lo_file.close(true));
    EXPECT_TRUE(lo_file.close(true));
    EXPECT_FALSE(lo_file.is_open());
}

////////////////////////////////////////////////////////////////////////////////
// read_v splits the file contents across multiple output buffers
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, ReadVIntoMultipleBuffers)
{
    auto          lo_path = make_tmp_path("read_v");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[10] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j' };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), true));
    ASSERT_TRUE(lo_file.set_position(0));

    uint8_t la_b1[3]       = {};
    uint8_t la_b2[4]       = {};
    uint8_t la_b3[3]       = {};

    kit::s_iovec la_iov[3] = {
        { la_b1, sizeof(la_b1) },
        { la_b2, sizeof(la_b2) },
        { la_b3, sizeof(la_b3) },
    };

    EXPECT_EQ(10u, lo_file.read_v(la_iov, 3));
    EXPECT_EQ(0, std::memcmp(la_b1, "abc", 3));
    EXPECT_EQ(0, std::memcmp(la_b2, "defg", 4));
    EXPECT_EQ(0, std::memcmp(la_b3, "hij", 3));
}

////////////////////////////////////////////////////////////////////////////////
// get_position tracks the stream position across writes and explicit seeks
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, GetPositionAdvancesWithWrite)
{
    auto          lo_path = make_tmp_path("get_pos");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    EXPECT_EQ(0u, lo_file.get_position());

    const uint8_t la_data[5] = { 'H', 'e', 'l', 'l', 'o' };
    ASSERT_EQ(sizeof(la_data), lo_file.write(la_data, sizeof(la_data), false));
    EXPECT_EQ(5u, lo_file.get_position());

    ASSERT_TRUE(lo_file.set_position(2));
    EXPECT_EQ(2u, lo_file.get_position());
}

////////////////////////////////////////////////////////////////////////////////
// sync succeeds on an open handle and fails once the handle is closed
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, SyncSucceedsOnOpenFile)
{
    auto          lo_path = make_tmp_path("sync");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_data[3] = { 1, 2, 3 };
    ASSERT_EQ(sizeof(la_data), lo_file.write(la_data, sizeof(la_data), false));
    EXPECT_TRUE(lo_file.sync());

    lo_file.close(false);
    EXPECT_FALSE(lo_file.sync());
}

////////////////////////////////////////////////////////////////////////////////
// open_existing succeeds on an existing file and exposes its content
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, OpenExistingSuccessPath)
{
    auto          lo_path = make_tmp_path("open_existing_ok");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[4] = { 'a', 'b', 'c', 'd' };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), true));
    lo_file.close(false);

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_open_existing, kit::e_ff_read));
    EXPECT_TRUE(lo_file.is_open());
    EXPECT_EQ(4u, lo_file.get_size());
}

////////////////////////////////////////////////////////////////////////////////
// open_or_create creates the file when it does not exist yet
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, OpenOrCreateCreatesWhenMissing)
{
    auto          lo_path = make_tmp_path("or_create_new");
    s_scoped_path lo_guard(lo_path);

    ASSERT_FALSE(std::filesystem::exists(lo_path));

    kit::c_file lo_file;
    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_open_or_create, kit::e_ff_read | kit::e_ff_write));
    EXPECT_TRUE(lo_file.is_open());
    EXPECT_EQ(0u, lo_file.get_size());
    lo_file.close(false);

    EXPECT_TRUE(std::filesystem::exists(lo_path));
}

////////////////////////////////////////////////////////////////////////////////
// truncate_existing opens an existing file and clears its content
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, TruncateExistingSuccessPath)
{
    auto          lo_path = make_tmp_path("trunc_ok");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), true));
    lo_file.close(false);

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_truncate_existing, kit::e_ff_read | kit::e_ff_write));
    EXPECT_EQ(0u, lo_file.get_size());
}

////////////////////////////////////////////////////////////////////////////////
// open with neither read nor write is refused
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, OpenNeitherReadNorWriteRejected)
{
    auto          lo_path = make_tmp_path("no_rw");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    EXPECT_FALSE(lo_file.open(lo_path, kit::e_fom_create_always, 0u));
    EXPECT_NE(0, lo_file.get_last_error());
}

////////////////////////////////////////////////////////////////////////////////
// opening a second file on the same instance auto-closes the first
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, OpenAutoClosesPrevious)
{
    auto          lo_path_a = make_tmp_path("auto_a");
    auto          lo_path_b = make_tmp_path("auto_b");
    s_scoped_path lo_g1(lo_path_a);
    s_scoped_path lo_g2(lo_path_b);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path_a, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed_a[3] = { 'a', 'a', 'a' };
    ASSERT_EQ(sizeof(la_seed_a), lo_file.write(la_seed_a, sizeof(la_seed_a), true));

    // reopen on the second file without an explicit close - the first must
    // be released transparently
    ASSERT_TRUE(lo_file.open(lo_path_b, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed_b[5] = { 'b', 'b', 'b', 'b', 'b' };
    ASSERT_EQ(sizeof(la_seed_b), lo_file.write(la_seed_b, sizeof(la_seed_b), true));
    EXPECT_EQ(5u, lo_file.get_size());
    lo_file.close(false);

    // path A must still be there with its original content
    ASSERT_TRUE(lo_file.open(lo_path_a, kit::e_fom_open_existing, kit::e_ff_read));
    EXPECT_EQ(3u, lo_file.get_size());
}

////////////////////////////////////////////////////////////////////////////////
// read with a null buffer or zero size returns zero
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, ReadNullOrZeroReturnsZero)
{
    auto          lo_path = make_tmp_path("read_null");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[4] = { 1, 2, 3, 4 };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), false));
    ASSERT_TRUE(lo_file.set_position(0));

    uint8_t la_out[4] = {};
    EXPECT_EQ(0u, lo_file.read(nullptr, sizeof(la_out)));
    EXPECT_EQ(0u, lo_file.read(la_out, 0));
}

////////////////////////////////////////////////////////////////////////////////
// write with a null buffer or zero size returns zero and produces no bytes
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, WriteNullOrZeroReturnsZero)
{
    auto          lo_path = make_tmp_path("write_null");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_data[4] = { 1, 2, 3, 4 };
    EXPECT_EQ(0u, lo_file.write(nullptr, sizeof(la_data), false));
    EXPECT_EQ(0u, lo_file.write(la_data, 0, false));
    EXPECT_EQ(0u, lo_file.get_size());
}

////////////////////////////////////////////////////////////////////////////////
// every operation is a no-op or returns a "fail" indicator on a closed file
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, OperationsOnClosedFileFail)
{
    kit::c_file lo_file;
    uint8_t     la_buf[4] = {};

    // I/O
    EXPECT_EQ(0u, lo_file.read(la_buf, sizeof(la_buf)));
    EXPECT_EQ(0u, lo_file.write(la_buf, sizeof(la_buf), false));
    EXPECT_EQ(0u, lo_file.read_at(0, la_buf, sizeof(la_buf)));
    EXPECT_EQ(0u, lo_file.write_at(0, la_buf, sizeof(la_buf)));

    kit::s_iovec la_iov[1] = {
        { la_buf, sizeof(la_buf) }
    };
    EXPECT_EQ(0u, lo_file.read_v(la_iov, 1));
    EXPECT_EQ(0u, lo_file.write_v(la_iov, 1, false));

    // metadata
    EXPECT_FALSE(lo_file.set_position(0));
    EXPECT_EQ(0u, lo_file.get_position());
    EXPECT_EQ(0u, lo_file.get_size());
    EXPECT_FALSE(lo_file.set_size(100));
    EXPECT_FALSE(lo_file.preallocate(100));

    // durability
    EXPECT_FALSE(lo_file.sync());

    // close on an unopened instance is idempotent
    EXPECT_TRUE(lo_file.close(false));
    EXPECT_TRUE(lo_file.close(true));
}

////////////////////////////////////////////////////////////////////////////////
// reading past EOF returns a short count, not an error
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, ReadPastEofReturnsShort)
{
    auto          lo_path = make_tmp_path("eof");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    const uint8_t la_seed[5] = { 1, 2, 3, 4, 5 };
    ASSERT_EQ(sizeof(la_seed), lo_file.write(la_seed, sizeof(la_seed), false));
    ASSERT_TRUE(lo_file.set_position(0));

    uint8_t la_big[32] = {};
    EXPECT_EQ(5u, lo_file.read(la_big, sizeof(la_big)));
}

////////////////////////////////////////////////////////////////////////////////
// read_v beyond IOV_MAX is rejected on POSIX; the Windows loop degrades to
// a 0 return once buffers exhaust, so we only assert last_error there
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, ReadVIovMaxOverflow)
{
    auto          lo_path = make_tmp_path("rv_overflow");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    // Well over any platform's IOV_MAX (Linux/macOS: 1024).
    static constexpr size_t OVER = 65536u;

    std::vector<kit::s_iovec> lo_iov(OVER);
    uint8_t                   la_dummy[1] = { 0 };
    for(auto &io : lo_iov)
    {
        io.mp_data = la_dummy;
        io.mz_size = 0;
    }

    EXPECT_EQ(0u, lo_file.read_v(lo_iov.data(), OVER));
#ifndef _WIN32
    EXPECT_NE(0, lo_file.get_last_error());
#endif
}

////////////////////////////////////////////////////////////////////////////////
// write_v beyond IOV_MAX is rejected on POSIX (same caveat as read_v)
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, WriteVIovMaxOverflow)
{
    auto          lo_path = make_tmp_path("wv_overflow");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));

    static constexpr size_t OVER = 65536u;

    std::vector<kit::s_iovec> lo_iov(OVER);
    uint8_t                   la_dummy[1] = { 0 };
    for(auto &io : lo_iov)
    {
        io.mp_data = la_dummy;
        io.mz_size = 0;
    }

    EXPECT_EQ(0u, lo_file.write_v(lo_iov.data(), OVER, false));
#ifndef _WIN32
    EXPECT_NE(0, lo_file.get_last_error());
#endif
}

////////////////////////////////////////////////////////////////////////////////
// last_error is cleared by the next successful open on the same instance
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, GetLastErrorClearedOnSuccessfulOpen)
{
    auto          lo_missing = make_tmp_path("last_err_missing");
    auto          lo_ok      = make_tmp_path("last_err_ok");
    s_scoped_path lo_guard(lo_ok);
    kit::c_file   lo_file;

    EXPECT_FALSE(lo_file.open(lo_missing, kit::e_fom_open_existing, kit::e_ff_read));
    EXPECT_NE(0, lo_file.get_last_error());

    ASSERT_TRUE(lo_file.open(lo_ok, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    EXPECT_EQ(0, lo_file.get_last_error());
}

////////////////////////////////////////////////////////////////////////////////
// share_* flags flow through validation cleanly (no-op on POSIX, effective
// on Windows) so open must still succeed
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, AllShareFlagsAcceptedAtOpen)
{
    auto          lo_path = make_tmp_path("share");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    uint32_t lu_flags
        = kit::e_ff_read | kit::e_ff_write | kit::e_ff_share_read | kit::e_ff_share_write | kit::e_ff_share_delete;
    EXPECT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, lu_flags));
}

////////////////////////////////////////////////////////////////////////////////
// write_through opens fine everywhere (O_SYNC / FILE_FLAG_WRITE_THROUGH are
// unconditionally accepted by the kernel/OS)
////////////////////////////////////////////////////////////////////////////////
TEST(c_file_test, WriteThroughFlagAcceptedAtOpen)
{
    auto          lo_path = make_tmp_path("wt");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    EXPECT_TRUE(
        lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write | kit::e_ff_write_through));
}

////////////////////////////////////////////////////////////////////////////////
// On POSIX pread/pwrite must not move the shared file pointer. On Windows the
// implicit pointer is updated by OVERLAPPED-based reads and this invariant
// does not hold - the check is intentionally gated by the OS.
////////////////////////////////////////////////////////////////////////////////
#ifndef _WIN32
TEST(c_file_test, PositionalDoesNotDisturbStreamPosOnPosix)
{
    auto          lo_path = make_tmp_path("pos_isolate");
    s_scoped_path lo_guard(lo_path);
    kit::c_file   lo_file;

    ASSERT_TRUE(lo_file.open(lo_path, kit::e_fom_create_always, kit::e_ff_read | kit::e_ff_write));
    ASSERT_TRUE(lo_file.set_size(32));
    ASSERT_TRUE(lo_file.set_position(10));
    EXPECT_EQ(10u, lo_file.get_position());

    uint8_t la_out[4] = {};
    lo_file.read_at(20, la_out, sizeof(la_out));
    EXPECT_EQ(10u, lo_file.get_position());

    lo_file.write_at(24, la_out, sizeof(la_out));
    EXPECT_EQ(10u, lo_file.get_position());
}
#endif
