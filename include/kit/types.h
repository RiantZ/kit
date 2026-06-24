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
