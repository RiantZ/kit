#pragma once

#include <cstdint>

#if defined(_MSC_VER)
    #include <stdlib.h>
#endif

namespace kit
{

inline uint16_t bswap16(uint16_t iu_val)
{
#if defined(_MSC_VER)
    return _byteswap_ushort(iu_val);
#else
    return __builtin_bswap16(iu_val);
#endif
}

inline uint32_t bswap32(uint32_t iu_val)
{
#if defined(_MSC_VER)
    return _byteswap_ulong(iu_val);
#else
    return __builtin_bswap32(iu_val);
#endif
}

inline uint64_t bswap64(uint64_t iu_val)
{
#if defined(_MSC_VER)
    return _byteswap_uint64(iu_val);
#else
    return __builtin_bswap64(iu_val);
#endif
}

} // namespace kit
