#include "kit/shared_mem.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

////////////////////////////////////////////////////////////////////////////////
namespace
{

enum e_type
{
    e_type_mutex = 0,
    e_type_file,
    e_type_max
};

bool create_name(tXCHAR *op_name, size_t iz_name, e_type ie_type, const tXCHAR *ip_postfix)
{
    if((nullptr == op_name) || (16 >= iz_name) || (e_type_max <= ie_type) || (nullptr == ip_postfix))
    {
        return false;
    }

    FILETIME ls_process_time = { 0 };
    FILETIME ls_stub_01      = { 0 };
    FILETIME ls_stub_02      = { 0 };
    FILETIME ls_stub_03      = { 0 };

    GetProcessTimes(GetCurrentProcess(), &ls_process_time, &ls_stub_01, &ls_stub_02, &ls_stub_03);

    if(e_type_mutex == ie_type)
    {
        swprintf_s(op_name,
                   iz_name,
                   L"Local\\m%d%d%d%s",
                   GetCurrentProcessId(),
                   ls_process_time.dwHighDateTime,
                   ls_process_time.dwLowDateTime,
                   ip_postfix);
    }
    else if(e_type_file == ie_type)
    {
        swprintf_s(op_name,
                   iz_name,
                   L"Local\\f%d%d%d%s",
                   GetCurrentProcessId(),
                   ls_process_time.dwHighDateTime,
                   ls_process_time.dwLowDateTime,
                   ip_postfix);
    }

    return true;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
struct c_shared::s_shared
{
    HANDLE  mh_memory;
    HANDLE  mh_mutex;
    tXCHAR *mp_name;
};

////////////////////////////////////////////////////////////////////////////////
// c_shared::create
bool c_shared::create(h_shared *op_handle, const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size)
{
    s_shared *lp_shared  = nullptr;
    bool      lb_return  = true;
    DWORD     lu_len     = 0;
    wchar_t  *lp_name    = nullptr;
    BOOL      lb_release = FALSE;
    uint8_t  *lp_buffer  = nullptr;

    if((nullptr == ip_name) || (nullptr == ip_data) || (0 >= iu_size) || (nullptr == op_handle))
    {
        lb_return = false;
        goto l_lblExit;
    }

    lp_shared = new s_shared;
    if(nullptr == lp_shared)
    {
        lb_return = false;
        goto l_lblExit;
    }

    memset(lp_shared, 0, sizeof(s_shared));

    lu_len             = (DWORD)wcslen(ip_name) + 128;
    lp_name            = (wchar_t *)malloc(sizeof(wchar_t) * lu_len);
    lp_shared->mp_name = pstr_dup(ip_name);

    if((nullptr == lp_name) || (nullptr == lp_shared->mp_name))
    {
        lb_return = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // create mutex and own it
    create_name(lp_name, lu_len, e_type_mutex, ip_name);

    lp_shared->mh_mutex = CreateMutexW(nullptr, TRUE, lp_name);
    if((nullptr == lp_shared->mh_mutex) || (ERROR_ALREADY_EXISTS == GetLastError()))
    {
        lb_return = false;
        goto l_lblExit;
    }

    lb_release = TRUE;

    ////////////////////////////////////////////////////////////////////////////
    // create shared memory object
    create_name(lp_name, lu_len, e_type_file, ip_name);

    lp_shared->mh_memory = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, iu_size, lp_name);

    if((nullptr == lp_shared->mh_memory) || (ERROR_ALREADY_EXISTS == GetLastError()))
    {
        lb_return = false;
        goto l_lblExit;
    }

    lp_buffer = (uint8_t *)MapViewOfFile(lp_shared->mh_memory, FILE_MAP_ALL_ACCESS, 0, 0, iu_size);

    if(nullptr == lp_buffer)
    {
        lb_return = false;
        goto l_lblExit;
    }

    *op_handle = (c_shared::h_shared)lp_shared;

    __try
    {
        memcpy(lp_buffer, ip_data, iu_size);
    }

    __except(GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        lb_return = false;
        goto l_lblExit;
    }

l_lblExit:
    if(lp_name)
    {
        free(lp_name);
        lp_name = nullptr;
    }

    if(lp_buffer)
    {
        UnmapViewOfFile(lp_buffer);
        lp_buffer = nullptr;
    }

    if(lb_release)
    {
        ReleaseMutex(lp_shared->mh_mutex);
    }

    if(!lb_return)
    {
        close((h_shared)lp_shared);
        lp_shared = nullptr;

        if(op_handle)
        {
            *op_handle = nullptr;
        }
    }

    return lb_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::read
bool c_shared::read(const tXCHAR *ip_name, void *op_data, uint16_t iu_size)
{
    HANDLE   lh_memory = nullptr;
    bool     lb_return = true;
    DWORD    lu_len    = 0;
    wchar_t *lp_name   = nullptr;
    uint8_t *lp_buffer = nullptr;

    if((nullptr == ip_name) || (nullptr == op_data) || (0 >= iu_size))
    {
        lb_return = false;
        goto l_lblExit;
    }

    lu_len  = (DWORD)wcslen(ip_name) + 128;
    lp_name = (wchar_t *)malloc(sizeof(wchar_t) * lu_len);

    if(nullptr == lp_name)
    {
        lb_return = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(lp_name, lu_len, e_type_file, ip_name);

    lh_memory = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, lp_name);

    if(nullptr == lh_memory)
    {
        lb_return = false;
        goto l_lblExit;
    }

    lp_buffer = (uint8_t *)MapViewOfFile(lh_memory, FILE_MAP_READ, 0, 0, iu_size);

    if(nullptr == lp_buffer)
    {
        lb_return = false;
        goto l_lblExit;
    }

    __try
    {
        memcpy(op_data, lp_buffer, iu_size);
    }

    __except((GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        lb_return = false;
        goto l_lblExit;
    }

l_lblExit:
    if(lp_name)
    {
        free(lp_name);
        lp_name = nullptr;
    }

    if(lp_buffer)
    {
        UnmapViewOfFile(lp_buffer);
        lp_buffer = nullptr;
    }

    if(lh_memory)
    {
        CloseHandle(lh_memory);
        lh_memory = nullptr;
    }

    return lb_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::write
bool c_shared::write(const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size)
{
    HANDLE   lh_memory = nullptr;
    bool     lb_return = true;
    DWORD    lu_len    = 0;
    wchar_t *lp_name   = nullptr;
    uint8_t *lp_buffer = nullptr;

    if((nullptr == ip_name) || (nullptr == ip_data) || (0 >= iu_size))
    {
        lb_return = false;
        goto l_lblExit;
    }

    lu_len  = (DWORD)wcslen(ip_name) + 128;
    lp_name = (wchar_t *)malloc(sizeof(wchar_t) * lu_len);

    if(nullptr == lp_name)
    {
        lb_return = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(lp_name, lu_len, e_type_file, ip_name);

    lh_memory = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, lp_name);

    if(nullptr == lh_memory)
    {
        lb_return = false;
        goto l_lblExit;
    }

    lp_buffer = (uint8_t *)MapViewOfFile(lh_memory, FILE_MAP_WRITE, 0, 0, iu_size);

    if(nullptr == lp_buffer)
    {
        lb_return = false;
        goto l_lblExit;
    }

    __try
    {
        memcpy(lp_buffer, ip_data, iu_size);
    }

    __except((GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        lb_return = false;
        goto l_lblExit;
    }

l_lblExit:
    if(lp_name)
    {
        free(lp_name);
        lp_name = nullptr;
    }

    if(lp_buffer)
    {
        UnmapViewOfFile(lp_buffer);
        lp_buffer = nullptr;
    }

    if(lh_memory)
    {
        CloseHandle(lh_memory);
        lh_memory = nullptr;
    }

    return lb_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::lock
c_shared::e_lock c_shared::lock(const tXCHAR *ip_name, h_sem &or_sem, uint32_t iu_timeout_ms)
{
    HANDLE   lh_mutex  = nullptr;
    e_lock   le_return = c_shared::e_ok;
    DWORD    lu_len    = 0;
    wchar_t *lp_name   = nullptr;

    or_sem             = nullptr;

    if(nullptr == ip_name)
    {
        le_return = c_shared::e_error;
        goto l_lblExit;
    }

    lu_len  = (DWORD)wcslen(ip_name) + 128;
    lp_name = (wchar_t *)malloc(sizeof(wchar_t) * lu_len);

    if(nullptr == lp_name)
    {
        le_return = c_shared::e_error;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open mutex and own it
    create_name(lp_name, lu_len, e_type_mutex, ip_name);

    lh_mutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, lp_name);
    if(nullptr == lh_mutex)
    {
        le_return = c_shared::e_not_exists;
        goto l_lblExit;
    }

    if(WAIT_OBJECT_0 != WaitForSingleObject(lh_mutex, iu_timeout_ms))
    {
        le_return = c_shared::e_timeout;
        goto l_lblExit;
    }

l_lblExit:
    if(lp_name)
    {
        free(lp_name);
        lp_name = nullptr;
    }

    if(nullptr != lh_mutex)
    {
        if(c_shared::e_ok == le_return)
        {
            or_sem = (h_sem)lh_mutex;
        }
        else
        {
            CloseHandle(lh_mutex);
            lh_mutex = nullptr;
        }
    }

    return le_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlock
c_shared::e_lock c_shared::unlock(h_sem &ior_sem)
{
    e_lock le_return = c_shared::e_error;
    HANDLE lh_mutex  = (HANDLE)ior_sem;

    if(nullptr == ior_sem)
    {
        le_return = c_shared::e_not_exists;
        goto l_lblExit;
    }

    ReleaseMutex(lh_mutex);

l_lblExit:
    if(lh_mutex)
    {
        CloseHandle(lh_mutex);
        ior_sem = nullptr;
    }

    return le_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::get_name
const tXCHAR *c_shared::get_name(h_shared ih_shared)
{
    if(nullptr == ih_shared)
    {
        return nullptr;
    }

    return ih_shared->mp_name;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::close
bool c_shared::close(h_shared ih_shared)
{
    if(nullptr == ih_shared)
    {
        return false;
    }

    if(ih_shared->mh_memory)
    {
        CloseHandle(ih_shared->mh_memory);
        ih_shared->mh_memory = nullptr;
    }

    if(ih_shared->mh_mutex)
    {
        CloseHandle(ih_shared->mh_mutex);
        ih_shared->mh_mutex = nullptr;
    }

    if(ih_shared->mp_name)
    {
        pstr_free_dup(ih_shared->mp_name);
        ih_shared->mp_name = nullptr;
    }

    delete ih_shared;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlink
bool c_shared::unlink(const tXCHAR *ip_name)
{
    UNUSED_ARG(ip_name);
    return true;
}
