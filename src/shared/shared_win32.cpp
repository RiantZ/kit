#include "kit/shared_mem.hpp"

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

bool create_name(tXCHAR *o_pName, size_t i_szName, e_type i_eType, const tXCHAR *i_pPostfix)
{
    if((nullptr == o_pName) || (16 >= i_szName) || (e_type_max <= i_eType) || (nullptr == i_pPostfix))
    {
        return false;
    }

    FILETIME l_tProcess_Time = { 0 };
    FILETIME l_tStub_01      = { 0 };
    FILETIME l_tStub_02      = { 0 };
    FILETIME l_tStub_03      = { 0 };

    GetProcessTimes(GetCurrentProcess(), &l_tProcess_Time, &l_tStub_01, &l_tStub_02, &l_tStub_03);

    if(e_type_mutex == i_eType)
    {
        swprintf_s(o_pName,
                   i_szName,
                   L"Local\\m%d%d%d%s",
                   GetCurrentProcessId(),
                   l_tProcess_Time.dwHighDateTime,
                   l_tProcess_Time.dwLowDateTime,
                   i_pPostfix);
    }
    else if(e_type_file == i_eType)
    {
        swprintf_s(o_pName,
                   i_szName,
                   L"Local\\f%d%d%d%s",
                   GetCurrentProcessId(),
                   l_tProcess_Time.dwHighDateTime,
                   l_tProcess_Time.dwLowDateTime,
                   i_pPostfix);
    }

    return true;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
struct c_shared::s_shared
{
    HANDLE  hMemory;
    HANDLE  hMutex;
    tXCHAR *pName;
};

////////////////////////////////////////////////////////////////////////////////
// c_shared::create
bool c_shared::create(h_shared *o_pHandle, const tXCHAR *i_pName, const uint8_t *i_pData, uint16_t i_wSize)
{
    s_shared *l_pShared  = nullptr;
    bool      l_bReturn  = true;
    DWORD     l_dwLen    = 0;
    wchar_t  *l_pName    = nullptr;
    BOOL      l_bRelease = FALSE;
    uint8_t  *l_pBuffer  = nullptr;

    if((nullptr == i_pName) || (nullptr == i_pData) || (0 >= i_wSize) || (nullptr == o_pHandle))
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_pShared = new s_shared;
    if(nullptr == l_pShared)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    memset(l_pShared, 0, sizeof(s_shared));

    l_dwLen          = (DWORD)wcslen(i_pName) + 128;
    l_pName          = (wchar_t *)malloc(sizeof(wchar_t) * l_dwLen);
    l_pShared->pName = pstr_dup(i_pName);

    if((nullptr == l_pName) || (nullptr == l_pShared->pName))
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // create mutex and own it
    create_name(l_pName, l_dwLen, e_type_mutex, i_pName);

    l_pShared->hMutex = CreateMutexW(nullptr, TRUE, l_pName);
    if((nullptr == l_pShared->hMutex) || (ERROR_ALREADY_EXISTS == GetLastError()))
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_bRelease = TRUE;

    ////////////////////////////////////////////////////////////////////////////
    // create shared memory object
    create_name(l_pName, l_dwLen, e_type_file, i_pName);

    l_pShared->hMemory = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, i_wSize, l_pName);

    if((nullptr == l_pShared->hMemory) || (ERROR_ALREADY_EXISTS == GetLastError()))
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_pBuffer = (uint8_t *)MapViewOfFile(l_pShared->hMemory, FILE_MAP_ALL_ACCESS, 0, 0, i_wSize);

    if(nullptr == l_pBuffer)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    *o_pHandle = (c_shared::h_shared)l_pShared;

    __try
    {
        memcpy(l_pBuffer, i_pData, i_wSize);
    }

    __except(GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(l_pBuffer)
    {
        UnmapViewOfFile(l_pBuffer);
        l_pBuffer = nullptr;
    }

    if(l_bRelease)
    {
        ReleaseMutex(l_pShared->hMutex);
    }

    if(!l_bReturn)
    {
        close((h_shared)l_pShared);
        l_pShared = nullptr;

        if(o_pHandle)
        {
            *o_pHandle = nullptr;
        }
    }

    return l_bReturn;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::read
bool c_shared::read(const tXCHAR *i_pName, void *o_pData, uint16_t i_wSize)
{
    HANDLE   l_hMemory = nullptr;
    bool     l_bReturn = true;
    DWORD    l_dwLen   = 0;
    wchar_t *l_pName   = nullptr;
    uint8_t *l_pBuffer = nullptr;

    if((nullptr == i_pName) || (nullptr == o_pData) || (0 >= i_wSize))
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_dwLen = (DWORD)wcslen(i_pName) + 128;
    l_pName = (wchar_t *)malloc(sizeof(wchar_t) * l_dwLen);

    if(nullptr == l_pName)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(l_pName, l_dwLen, e_type_file, i_pName);

    l_hMemory = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, l_pName);

    if(nullptr == l_hMemory)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_pBuffer = (uint8_t *)MapViewOfFile(l_hMemory, FILE_MAP_READ, 0, 0, i_wSize);

    if(nullptr == l_pBuffer)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    __try
    {
        memcpy(o_pData, l_pBuffer, i_wSize);
    }

    __except((GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(l_pBuffer)
    {
        UnmapViewOfFile(l_pBuffer);
        l_pBuffer = nullptr;
    }

    if(l_hMemory)
    {
        CloseHandle(l_hMemory);
        l_hMemory = nullptr;
    }

    return l_bReturn;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::write
bool c_shared::write(const tXCHAR *i_pName, const uint8_t *i_pData, uint16_t i_wSize)
{
    HANDLE   l_hMemory = nullptr;
    bool     l_bReturn = true;
    DWORD    l_dwLen   = 0;
    wchar_t *l_pName   = nullptr;
    uint8_t *l_pBuffer = nullptr;

    if((nullptr == i_pName) || (nullptr == i_pData) || (0 >= i_wSize))
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_dwLen = (DWORD)wcslen(i_pName) + 128;
    l_pName = (wchar_t *)malloc(sizeof(wchar_t) * l_dwLen);

    if(nullptr == l_pName)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(l_pName, l_dwLen, e_type_file, i_pName);

    l_hMemory = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, l_pName);

    if(nullptr == l_hMemory)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_pBuffer = (uint8_t *)MapViewOfFile(l_hMemory, FILE_MAP_WRITE, 0, 0, i_wSize);

    if(nullptr == l_pBuffer)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    __try
    {
        memcpy(l_pBuffer, i_pData, i_wSize);
    }

    __except((GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(l_pBuffer)
    {
        UnmapViewOfFile(l_pBuffer);
        l_pBuffer = nullptr;
    }

    if(l_hMemory)
    {
        CloseHandle(l_hMemory);
        l_hMemory = nullptr;
    }

    return l_bReturn;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::lock
c_shared::e_lock c_shared::lock(const tXCHAR *i_pName, h_sem &o_rSem, uint32_t i_dwTimeout_ms)
{
    HANDLE   l_hMutex  = nullptr;
    e_lock   l_eReturn = c_shared::e_ok;
    DWORD    l_dwLen   = 0;
    wchar_t *l_pName   = nullptr;

    o_rSem             = nullptr;

    if(nullptr == i_pName)
    {
        l_eReturn = c_shared::e_error;
        goto l_lblExit;
    }

    l_dwLen = (DWORD)wcslen(i_pName) + 128;
    l_pName = (wchar_t *)malloc(sizeof(wchar_t) * l_dwLen);

    if(nullptr == l_pName)
    {
        l_eReturn = c_shared::e_error;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open mutex and own it
    create_name(l_pName, l_dwLen, e_type_mutex, i_pName);

    l_hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, l_pName);
    if(nullptr == l_hMutex)
    {
        l_eReturn = c_shared::e_not_exists;
        goto l_lblExit;
    }

    if(WAIT_OBJECT_0 != WaitForSingleObject(l_hMutex, i_dwTimeout_ms))
    {
        l_eReturn = c_shared::e_timeout;
        goto l_lblExit;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(nullptr != l_hMutex)
    {
        if(c_shared::e_ok == l_eReturn)
        {
            o_rSem = (h_sem)l_hMutex;
        }
        else
        {
            CloseHandle(l_hMutex);
            l_hMutex = nullptr;
        }
    }

    return l_eReturn;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlock
c_shared::e_lock c_shared::unlock(h_sem &io_rSem)
{
    e_lock l_eReturn = c_shared::e_error;
    HANDLE l_hMutex  = (HANDLE)io_rSem;

    if(nullptr == io_rSem)
    {
        l_eReturn = c_shared::e_not_exists;
        goto l_lblExit;
    }

    ReleaseMutex(l_hMutex);

l_lblExit:
    if(l_hMutex)
    {
        CloseHandle(l_hMutex);
        io_rSem = nullptr;
    }

    return l_eReturn;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::get_name
const tXCHAR *c_shared::get_name(h_shared i_pShared)
{
    if(nullptr == i_pShared)
    {
        return nullptr;
    }

    return i_pShared->pName;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::close
bool c_shared::close(h_shared i_hShared)
{
    if(nullptr == i_hShared)
    {
        return false;
    }

    if(i_hShared->hMemory)
    {
        CloseHandle(i_hShared->hMemory);
        i_hShared->hMemory = nullptr;
    }

    if(i_hShared->hMutex)
    {
        CloseHandle(i_hShared->hMutex);
        i_hShared->hMutex = nullptr;
    }

    if(i_hShared->pName)
    {
        pstr_free_dup(i_hShared->pName);
        i_hShared->pName = nullptr;
    }

    delete i_hShared;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlink
bool c_shared::unlink(const tXCHAR *i_pName)
{
    UNUSED_ARG(i_pName);
    return true;
}
