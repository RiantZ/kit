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

    static bool create(h_shared *op_handle, const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size);

    static bool read(const tXCHAR *ip_name, void *op_data, uint16_t iu_size);

    static bool write(const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size);

    static e_lock lock(const tXCHAR *ip_name, h_sem &or_sem, uint32_t iu_timeout_ms);

    static e_lock unlock(h_sem &ior_sem);

    static const tXCHAR *get_name(h_shared ih_shared);

    static bool close(h_shared ih_shared);

    static bool unlink(const tXCHAR *ip_name);
};
