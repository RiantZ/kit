#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "kit/ts_helpers.h"

#ifdef G_OS_WINDOWS
inline wchar_t *pstr_dup(const wchar_t *i_pStr)
{
    return _wcsdup(i_pStr);
}

inline void pstr_free_dup(wchar_t *i_pStr)
{
    free(i_pStr);
}
#endif
