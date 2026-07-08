#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kit/ts_helpers.h"

#ifdef G_OS_WINDOWS
inline wchar_t *pstr_dup(const wchar_t *ip_str)
{
    return _wcsdup(ip_str);
}

inline void pstr_free_dup(wchar_t *ip_str)
{
    free(ip_str);
}
#endif

// case-insensitive compare of narrow (ASCII) C-strings. strcasecmp is POSIX and
// lives in <strings.h>; MSVC spells the same routine _stricmp in <string.h>.
#ifdef G_OS_WINDOWS
inline int str_casecmp(const char *ip_a, const char *ip_b)
{
    return _stricmp(ip_a, ip_b);
}
#else
    #include <strings.h>

inline int str_casecmp(const char *ip_a, const char *ip_b)
{
    return strcasecmp(ip_a, ip_b);
}
#endif
