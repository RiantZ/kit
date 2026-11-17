#pragma once

#include <cstddef>
#include <cstdint>

#include "kit/ts_helpers.h"

namespace kit
{

/// @brief Cuts a UTF-8 string length to a limit without splitting a character.
/// @param ip_str The string; read only when iz_len > iz_max.
/// @param iz_len The string length in bytes.
/// @param iz_max The limit in bytes.
/// @return iz_len if it fits, otherwise iz_max moved back to the start of the character it falls in.
inline size_t cut_utf8(const char *ip_str, size_t iz_len, size_t iz_max) noexcept
{
    if(iz_len <= iz_max)
    {
        return iz_len;
    }

    // a character is at most 4 bytes, so valid UTF-8 needs at most 3 steps back
    size_t       lz_cut = iz_max;
    const size_t lz_min = (lz_cut > 3) ? lz_cut - 3 : 0;
    while(lz_cut > lz_min && 0x80 == (static_cast<uint8_t>(ip_str[lz_cut]) & 0xC0))
    {
        --lz_cut;
    }
    return lz_cut;
}

/// @brief Cuts a UTF-16 string length to a limit without splitting a surrogate pair.
/// @param ip_str The string; read only when iz_len > iz_max.
/// @param iz_len The string length in 16-bit units.
/// @param iz_max The limit in 16-bit units.
/// @return iz_len if it fits, otherwise iz_max, less one when a high surrogate would end the string.
inline size_t cut_utf16(const char16_t *ip_str, size_t iz_len, size_t iz_max) noexcept
{
    if(iz_len <= iz_max)
    {
        return iz_len;
    }

    // a high surrogate whose low pair is cut off is dropped
    size_t lz_cut = iz_max;
    if(lz_cut > 0 && 0xD800 == (ip_str[lz_cut - 1] & 0xFC00))
    {
        --lz_cut;
    }
    return lz_cut;
}

/// @brief Cuts a UTF-32 string length to a limit; every unit is a whole character.
/// @param ip_str The string; never read.
/// @param iz_len The string length in 32-bit units.
/// @param iz_max The limit in 32-bit units.
/// @return iz_len if it fits, otherwise iz_max.
inline size_t cut_utf32(const char32_t *ip_str, size_t iz_len, size_t iz_max) noexcept
{
    UNUSED_ARG(ip_str);
    return (iz_len <= iz_max) ? iz_len : iz_max;
}

// wchar_t holds UTF-16 on Windows and UTF-32 on Linux and macOS; the bodies repeat the
// char16_t / char32_t ones, so wchar_t data is never read through another type
#if defined(G_OS_WINDOWS)
/// @brief Cuts a UTF-16 wchar_t string length to a limit without splitting a surrogate pair.
/// @param ip_str The string; read only when iz_len > iz_max.
/// @param iz_len The string length in wchar_t units.
/// @param iz_max The limit in wchar_t units.
/// @return iz_len if it fits, otherwise iz_max, less one when a high surrogate would end the string.
inline size_t cut_utf16(const wchar_t *ip_str, size_t iz_len, size_t iz_max) noexcept
{
    if(iz_len <= iz_max)
    {
        return iz_len;
    }

    // a high surrogate whose low pair is cut off is dropped
    size_t lz_cut = iz_max;
    if(lz_cut > 0 && 0xD800 == (ip_str[lz_cut - 1] & 0xFC00))
    {
        --lz_cut;
    }
    return lz_cut;
}
#else
/// @brief Cuts a UTF-32 wchar_t string length to a limit; every unit is a whole character.
/// @param ip_str The string; never read.
/// @param iz_len The string length in wchar_t units.
/// @param iz_max The limit in wchar_t units.
/// @return iz_len if it fits, otherwise iz_max.
inline size_t cut_utf32(const wchar_t *ip_str, size_t iz_len, size_t iz_max) noexcept
{
    UNUSED_ARG(ip_str);
    return (iz_len <= iz_max) ? iz_len : iz_max;
}
#endif

} // namespace kit
