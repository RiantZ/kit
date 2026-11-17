#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kit/ts_helpers.h"

#ifdef G_OS_WINDOWS
/// @brief Duplicates a wide C-string.
/// @param ip_str The string to copy.
/// @return A new copy to release with pstr_free_dup(), or NULL when out of memory.
inline wchar_t *pstr_dup(const wchar_t *ip_str)
{
    return _wcsdup(ip_str);
}

/// @brief Releases a string returned by pstr_dup().
/// @param ip_str The string to release.
inline void pstr_free_dup(wchar_t *ip_str)
{
    free(ip_str);
}
#endif

// strcasecmp is POSIX and lives in <strings.h>; MSVC spells the same routine _stricmp in <string.h>
#ifdef G_OS_WINDOWS
/// @brief Compares two narrow (ASCII) C-strings ignoring case.
/// @param ip_a The first string.
/// @param ip_b The second string.
/// @return Less than, equal to or greater than zero when ip_a sorts before, equal to or after ip_b.
inline int str_casecmp(const char *ip_a, const char *ip_b)
{
    return _stricmp(ip_a, ip_b);
}
#else
    #include <strings.h>

/// @brief Compares two narrow (ASCII) C-strings ignoring case.
/// @param ip_a The first string.
/// @param ip_b The second string.
/// @return Less than, equal to or greater than zero when ip_a sorts before, equal to or after ip_b.
inline int str_casecmp(const char *ip_a, const char *ip_b)
{
    return strcasecmp(ip_a, ip_b);
}
#endif

/// @brief Duplicates a narrow (ASCII) C-string.
/// @param ip_str The string to copy.
/// @return A new copy to release with free(), or NULL when out of memory.
inline char *str_dup(const char *ip_str)
{
    // strdup is POSIX; MSVC deprecates that spelling and provides the same routine as _strdup
#ifdef G_OS_WINDOWS
    return _strdup(ip_str);
#else
    return strdup(ip_str);
#endif
}
