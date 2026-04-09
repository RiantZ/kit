#pragma once

#include <cstddef>
#include <cstdint>

#include "kit/types.h"

class c_shared
{
    struct s_shared;

public:
    typedef s_shared *h_shared;
    typedef void     *h_sem;

    enum e_lock
    {
        e_ok,
        e_timeout,
        e_error,
        e_not_exists
    };

    static bool create(h_shared *o_pHandle, const tXCHAR *i_pName, const uint8_t *i_pData, uint16_t i_wSize);

    static bool read(const tXCHAR *i_pName, void *o_pData, uint16_t i_wSize);

    static bool write(const tXCHAR *i_pName, const uint8_t *i_pData, uint16_t i_wSize);

    static e_lock lock(const tXCHAR *i_pName, h_sem &o_rSem, uint32_t i_dwTimeout_ms);

    static e_lock unlock(h_sem &io_rSem);

    static const tXCHAR *get_name(h_shared i_pShared);

    static bool close(h_shared i_pShared);

    static bool unlink(const tXCHAR *i_pName);
};
