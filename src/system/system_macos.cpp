#include "kit/system.hpp"

#include <cstdio>
#include <cstdlib>
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
        std::snprintf(op_buf, iz_buf, "%s", lp_name);
        return true;
    }

    std::snprintf(op_buf, iz_buf, "%s", "unknown");
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

    std::snprintf(op_buf, iz_buf, "%s", "unknown");
    return false;
}

} // namespace kit
