#pragma once

#include <gtest/gtest.h>
#include <kit/utf.hpp>

#include <cwchar>

////////////////////////////////////////////////////////////////////////////////
// a UTF-8 string that fits comes back whole; a null string of zero length is not read
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf8FitsReturnsLen)
{
    const char *lp_null = nullptr;
    EXPECT_EQ(kit::cut_utf8("abc", 3, 5), 3u);
    EXPECT_EQ(kit::cut_utf8("abc", 3, 3), 3u);
    EXPECT_EQ(kit::cut_utf8(lp_null, 0, 0), 0u);
    EXPECT_EQ(kit::cut_utf8(lp_null, 0, 10), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// ASCII is cut exactly at the limit
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf8AsciiCutAtMax)
{
    EXPECT_EQ(kit::cut_utf8("abcdef", 6, 4), 4u);
}

////////////////////////////////////////////////////////////////////////////////
// a cut inside a character moves back to its first byte, a cut on a boundary stays
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf8StepsBackToCharStart)
{
    // literals are split after hex escapes, or "\xA9b" would read as one escape
    const char *lp_two = "a"
                         "\xC3\xA9"
                         "b";
    EXPECT_EQ(kit::cut_utf8(lp_two, 4, 1), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_two, 4, 2), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_two, 4, 3), 3u);

    const char *lp_three = "a"
                           "\xE2\x82\xAC"
                           "b";
    EXPECT_EQ(kit::cut_utf8(lp_three, 5, 1), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_three, 5, 2), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_three, 5, 3), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_three, 5, 4), 4u);

    const char *lp_four = "a"
                          "\xF0\x9F\x98\x80"
                          "b";
    EXPECT_EQ(kit::cut_utf8(lp_four, 6, 1), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_four, 6, 2), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_four, 6, 3), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_four, 6, 4), 1u);
    EXPECT_EQ(kit::cut_utf8(lp_four, 6, 5), 5u);

    const char *lp_four_first = "\xF0\x9F\x98\x80"
                                "b";
    EXPECT_EQ(kit::cut_utf8(lp_four_first, 5, 3), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// not valid UTF-8: the step back stops after 3 bytes, as no character has more
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf8StepBackStopsAfterThree)
{
    EXPECT_EQ(kit::cut_utf8("\x80\x80\x80\x80\x80\x80", 6, 5), 2u);
}

////////////////////////////////////////////////////////////////////////////////
// a zero limit leaves nothing
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf8ZeroMax)
{
    EXPECT_EQ(kit::cut_utf8("abc", 3, 0), 0u);
    EXPECT_EQ(kit::cut_utf8("\xC3\xA9", 2, 0), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// a UTF-16 string that fits comes back whole
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf16FitsReturnsLen)
{
    // a typed null: a bare nullptr is ambiguous with the wchar_t overload on Windows
    const char16_t *lp_null = nullptr;
    EXPECT_EQ(kit::cut_utf16(u"abc", 3, 5), 3u);
    EXPECT_EQ(kit::cut_utf16(u"abc", 3, 3), 3u);
    EXPECT_EQ(kit::cut_utf16(lp_null, 0, 0), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// a cut between the halves of a surrogate pair drops the high half
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf16DropsSplitSurrogatePair)
{
    // units: a, D83D, DE00, b
    EXPECT_EQ(kit::cut_utf16(u"a\U0001F600b", 4, 2), 1u);
}

////////////////////////////////////////////////////////////////////////////////
// a cut before or after a whole surrogate pair stays
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf16KeepsWholeSurrogatePair)
{
    const char16_t *lp_str = u"a\U0001F600b";
    EXPECT_EQ(kit::cut_utf16(lp_str, 4, 1), 1u);
    EXPECT_EQ(kit::cut_utf16(lp_str, 4, 3), 3u);
}

////////////////////////////////////////////////////////////////////////////////
// a lone low surrogate has no pair to protect, so it is kept
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf16KeepsLoneLowSurrogate)
{
    const char16_t la_str[] = { u'a', static_cast<char16_t>(0xDE00), u'b' };
    EXPECT_EQ(kit::cut_utf16(la_str, 3, 2), 2u);
}

////////////////////////////////////////////////////////////////////////////////
// a zero limit leaves nothing
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf16ZeroMax)
{
    EXPECT_EQ(kit::cut_utf16(u"\U0001F600", 2, 0), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// a UTF-32 string that fits comes back whole
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf32FitsReturnsLen)
{
    // a typed null: a bare nullptr is ambiguous with the wchar_t overload on Linux and macOS
    const char32_t *lp_null = nullptr;
    EXPECT_EQ(kit::cut_utf32(U"abc", 3, 5), 3u);
    EXPECT_EQ(kit::cut_utf32(U"abc", 3, 3), 3u);
    EXPECT_EQ(kit::cut_utf32(lp_null, 0, 0), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// every UTF-32 unit is a whole character, so the cut is exactly at the limit
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, Utf32CutAtMax)
{
    const char32_t *lp_str = U"a\U0001F600b";
    EXPECT_EQ(kit::cut_utf32(lp_str, 3, 2), 2u);
    EXPECT_EQ(kit::cut_utf32(lp_str, 3, 0), 0u);
}

////////////////////////////////////////////////////////////////////////////////
// wchar_t takes the overload of its platform encoding
////////////////////////////////////////////////////////////////////////////////
TEST(utf_test, WcharMatchesPlatformEncoding)
{
    const wchar_t *lp_str = L"a\U0001F600b";
#if defined(G_OS_WINDOWS)
    // UTF-16: a, D83D, DE00, b, so a cut at 2 splits the pair
    ASSERT_EQ(std::wcslen(lp_str), 4u);
    EXPECT_EQ(kit::cut_utf16(lp_str, std::wcslen(lp_str), 2), 1u);
#else
    // UTF-32: a, 1F600, b
    ASSERT_EQ(std::wcslen(lp_str), 3u);
    EXPECT_EQ(kit::cut_utf32(lp_str, std::wcslen(lp_str), 2), 2u);
#endif
}
