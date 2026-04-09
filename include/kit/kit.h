#pragma once

#include "kit/export.h"
#include "kit/version.h"
#include "kit/ts_helpers.h"
#include "kit/types.h"
#include "kit/list.hpp"
#include "kit/shared_mem.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    KIT_API const char *kit_version(void);

#ifdef __cplusplus
}
#endif
