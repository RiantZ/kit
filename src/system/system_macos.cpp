#include "kit/system.hpp"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace kit
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t get_process_id()
{
    return static_cast<uint32_t>(getpid());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool get_process_name(char *op_buf, size_t iz_buf)
{
    if(!op_buf || iz_buf < 2)
    {
        return false;
    }

    const char *lp_name = getprogname();
    if(lp_name)
    {
        std::strncpy(op_buf, lp_name, iz_buf - 1);
        op_buf[iz_buf - 1] = '\0';
        return true;
    }

    std::strncpy(op_buf, "unknown", iz_buf - 1);
    op_buf[iz_buf - 1] = '\0';
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool get_host_name(char *op_buf, size_t iz_buf)
{
    if(!op_buf || iz_buf < 2)
    {
        return false;
    }

    if(gethostname(op_buf, iz_buf) == 0)
    {
        op_buf[iz_buf - 1] = '\0';
        return true;
    }

    std::strncpy(op_buf, "unknown", iz_buf - 1);
    op_buf[iz_buf - 1] = '\0';
    return false;
}

} // namespace kit
