#include "kit/system.hpp"

#include <cstring>
#include <windows.h>

namespace kit
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint32_t get_process_id()
{
    return static_cast<uint32_t>(GetCurrentProcessId());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool get_process_name(char *op_buf, size_t iz_buf)
{
    if(!op_buf || iz_buf < 2)
    {
        return false;
    }

    char  la_path[MAX_PATH];
    DWORD lu_len = GetModuleFileNameA(nullptr, la_path, MAX_PATH);

    if(lu_len == 0)
    {
        std::strncpy(op_buf, "unknown", iz_buf - 1);
        op_buf[iz_buf - 1] = '\0';
        return false;
    }

    la_path[MAX_PATH - 1] = '\0';

    const char *lp_name   = la_path + lu_len;
    while(lp_name > la_path && *(lp_name - 1) != '\\' && *(lp_name - 1) != '/')
    {
        --lp_name;
    }

    std::strncpy(op_buf, lp_name, iz_buf - 1);
    op_buf[iz_buf - 1] = '\0';
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool get_host_name(char *op_buf, size_t iz_buf)
{
    if(!op_buf || iz_buf < 2)
    {
        return false;
    }

    DWORD lu_size = static_cast<DWORD>(iz_buf);
    if(GetComputerNameA(op_buf, &lu_size))
    {
        return true;
    }

    std::strncpy(op_buf, "unknown", iz_buf - 1);
    op_buf[iz_buf - 1] = '\0';
    return false;
}

} // namespace kit
