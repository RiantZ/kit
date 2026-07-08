#include "kit/system.hpp"

#include <cstdio>
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

    char la_path[4096];
    int  li_len = static_cast<int>(readlink("/proc/self/exe", la_path, sizeof(la_path) - 1));

    if(li_len <= 0)
    {
        std::snprintf(op_buf, iz_buf, "%s", "unknown");
        return false;
    }

    la_path[li_len]     = '\0';

    const char *lp_name = la_path + li_len;
    while(lp_name > la_path && *(lp_name - 1) != '/')
    {
        --lp_name;
    }

    std::snprintf(op_buf, iz_buf, "%s", lp_name);
    return true;
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
