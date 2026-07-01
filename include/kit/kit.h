#pragma once

#include "kit/export.h"
#include "kit/version.h"
#include "kit/ts_helpers.h"
#include "kit/types.h"
#include "kit/list.hpp"
#include "kit/spin_lock.hpp"
#include "kit/mpsc_queue.hpp"
#include "kit/shared_mem.hpp"
#include "kit/event.hpp"
#include "kit/thread.hpp"
#include "kit/file.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    KIT_API const char *kit_version(void);

#ifdef __cplusplus
}
#endif
