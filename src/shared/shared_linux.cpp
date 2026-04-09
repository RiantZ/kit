#include "kit/shared_mem.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

////////////////////////////////////////////////////////////////////////////////
namespace
{

const char  *SHARED_NAME_FORMAT_STRING = "/P7_Type%d_PID%d_Text%s";
const size_t SHARED_NAME_EXTRA_LEN     = 64;

enum e_type
{
    e_type_mutex = 0,
    e_type_file,
    e_type_max
};

size_t shared_name_len(const char *i_pName)
{
    return strlen(SHARED_NAME_FORMAT_STRING) + SHARED_NAME_EXTRA_LEN + strlen(i_pName);
}

bool create_name(tXCHAR *o_pName, size_t i_szName, e_type i_eType, const tXCHAR *i_pPostfix)
{
    if((nullptr == o_pName) || (16 >= i_szName) || (e_type_max <= i_eType) || (nullptr == i_pPostfix))
    {
        return false;
    }

    snprintf(o_pName, i_szName, SHARED_NAME_FORMAT_STRING, i_eType, getpid(), i_pPostfix);
    return true;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
struct c_shared::s_shared
{
    int    iMFD;
    sem_t *hSemaphore;
    size_t szName;
    char  *pName;
    char  *pSemName;
    char  *pMemName;
};

////////////////////////////////////////////////////////////////////////////////
// c_shared::create
bool c_shared::create(h_shared *o_pHandle, const tXCHAR *i_pName, const uint8_t *i_pData, uint16_t i_wSize)
{
    h_shared l_pShared  = nullptr;
    bool     l_bResult  = true;
    bool     l_bRelease = false;
    void    *l_pBuffer  = nullptr;

    if((nullptr == i_pName) || (nullptr == i_pData) || (0 >= i_wSize) || (nullptr == o_pHandle))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    l_pShared = (h_shared)malloc(sizeof(s_shared));
    if(nullptr == l_pShared)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    memset(l_pShared, 0, sizeof(s_shared));
    l_pShared->hSemaphore = SEM_FAILED;
    l_pShared->iMFD       = -1;

    l_pShared->szName     = shared_name_len(i_pName);
    l_pShared->pName      = strdup(i_pName);
    l_pShared->pSemName   = (char *)malloc(l_pShared->szName);
    l_pShared->pMemName   = (char *)malloc(l_pShared->szName);

    if((nullptr == l_pShared->pSemName) || (nullptr == l_pShared->pMemName) || (nullptr == l_pShared->pName))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // create semaphore and own it
    create_name(l_pShared->pSemName, l_pShared->szName, e_type_mutex, i_pName);

    l_pShared->hSemaphore = sem_open(l_pShared->pSemName, O_CREAT | O_EXCL, 0666, 0);
    if(SEM_FAILED == l_pShared->hSemaphore)
    {
        free(l_pShared->pSemName);
        l_pShared->pSemName = nullptr;
        free(l_pShared->pMemName);
        l_pShared->pMemName = nullptr;
        l_bResult           = false;
        goto l_lblExit;
    }

    l_bRelease = true;

    ////////////////////////////////////////////////////////////////////////////
    // share memory
    create_name(l_pShared->pMemName, l_pShared->szName, e_type_file, i_pName);

    l_pShared->iMFD = shm_open(l_pShared->pMemName, O_RDWR | O_CREAT | O_EXCL, 0666);
    if(0 > l_pShared->iMFD)
    {
        free(l_pShared->pMemName);
        l_pShared->pMemName = nullptr;
        l_bResult           = false;
        goto l_lblExit;
    }

    if(0 != ftruncate(l_pShared->iMFD, (off_t)i_wSize))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    l_pBuffer = mmap(0, (size_t)i_wSize, PROT_READ | PROT_WRITE, MAP_SHARED, l_pShared->iMFD, 0);

    if((nullptr == l_pBuffer) || (MAP_FAILED == l_pBuffer))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    *o_pHandle = l_pShared;

    memcpy(l_pBuffer, i_pData, (size_t)i_wSize);

    if(0 != munmap(l_pBuffer, (size_t)i_wSize))
    {
    }

l_lblExit:
    if(l_bRelease)
    {
        sem_post(l_pShared->hSemaphore);
    }

    if(!l_bResult)
    {
        close(l_pShared);
        l_pShared = nullptr;

        if(o_pHandle)
        {
            *o_pHandle = nullptr;
        }
    }

    return l_bResult;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::read
bool c_shared::read(const tXCHAR *i_pName, void *o_pData, uint16_t i_wSize)
{
    bool        l_bResult = true;
    size_t      l_szName  = 0;
    char       *l_pName   = nullptr;
    void       *l_pBuffer = nullptr;
    int         l_iMFD    = -1;
    struct stat l_sStat;

    if((nullptr == i_pName) || (nullptr == o_pData) || (0 >= i_wSize))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    l_szName = shared_name_len(i_pName);
    l_pName  = (char *)malloc(l_szName);

    if(nullptr == l_pName)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(l_pName, l_szName, e_type_file, i_pName);

    l_iMFD = shm_open(l_pName, O_RDONLY, 0444);

    if(0 > l_iMFD)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    memset(&l_sStat, 0, sizeof(l_sStat));
    if(-1 == fstat(l_iMFD, &l_sStat))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    if((size_t)l_sStat.st_size < (size_t)i_wSize)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    l_pBuffer = mmap(0, (size_t)l_sStat.st_size, PROT_READ, MAP_SHARED, l_iMFD, 0);

    if((nullptr == l_pBuffer) || (MAP_FAILED == l_pBuffer))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    memcpy(o_pData, l_pBuffer, (size_t)i_wSize);

    if(0 == munmap(l_pBuffer, (size_t)l_sStat.st_size))
    {
        l_pBuffer = nullptr;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(0 <= l_iMFD)
    {
        ::close(l_iMFD);
        l_iMFD = -1;
    }

    return l_bResult;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::write
bool c_shared::write(const tXCHAR *i_pName, const uint8_t *i_pData, uint16_t i_wSize)
{
    bool        l_bResult = true;
    size_t      l_szName  = 0;
    char       *l_pName   = nullptr;
    void       *l_pBuffer = nullptr;
    int         l_iMFD    = -1;
    struct stat l_sStat;

    if((nullptr == i_pName) || (nullptr == i_pData) || (0 >= i_wSize))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    l_szName = shared_name_len(i_pName);
    l_pName  = (char *)malloc(l_szName);

    if(nullptr == l_pName)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(l_pName, l_szName, e_type_file, i_pName);

    l_iMFD = shm_open(l_pName, O_RDWR, 0666);

    if(0 > l_iMFD)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    memset(&l_sStat, 0, sizeof(l_sStat));
    if(-1 == fstat(l_iMFD, &l_sStat))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    if((size_t)l_sStat.st_size < (size_t)i_wSize)
    {
        l_bResult = false;
        goto l_lblExit;
    }

    l_pBuffer = mmap(0, (size_t)l_sStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, l_iMFD, 0);

    if((nullptr == l_pBuffer) || (MAP_FAILED == l_pBuffer))
    {
        l_bResult = false;
        goto l_lblExit;
    }

    memcpy(l_pBuffer, i_pData, (size_t)i_wSize);

    if(0 == munmap(l_pBuffer, (size_t)l_sStat.st_size))
    {
        l_pBuffer = nullptr;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(0 <= l_iMFD)
    {
        ::close(l_iMFD);
        l_iMFD = -1;
    }

    return l_bResult;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::lock
c_shared::e_lock c_shared::lock(const tXCHAR *i_pName, h_sem &o_rSem, uint32_t i_dwTimeout_ms)
{
    e_lock    l_eReturn = c_shared::e_timeout;
    size_t    l_szName  = 0;
    char     *l_pName   = nullptr;
    const int l_i1ms    = 1000;
    int64_t   l_llWait  = (int64_t)i_dwTimeout_ms * 1000LL;
    sem_t    *l_hSem    = SEM_FAILED;

    o_rSem              = nullptr;

    if(nullptr == i_pName)
    {
        l_eReturn = c_shared::e_error;
        goto l_lblExit;
    }

    l_szName = shared_name_len(i_pName);
    l_pName  = (char *)malloc(l_szName);

    if(nullptr == l_pName)
    {
        l_eReturn = c_shared::e_error;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open semaphore
    create_name(l_pName, l_szName, e_type_mutex, i_pName);

    l_hSem = sem_open(l_pName, 0);
    if(SEM_FAILED == l_hSem)
    {
        l_eReturn = c_shared::e_not_exists;
        goto l_lblExit;
    }

    l_eReturn = c_shared::e_timeout;

    while(0 < l_llWait)
    {
        if(0 == sem_trywait(l_hSem))
        {
            l_eReturn = c_shared::e_ok;
            break;
        }
        else
        {
            usleep(l_i1ms); // 1 ms
            l_llWait -= l_i1ms;
        }
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }

    if(SEM_FAILED != l_hSem)
    {
        if(c_shared::e_ok == l_eReturn)
        {
            o_rSem = (h_sem)l_hSem;
        }
        else
        {
            sem_close(l_hSem);
            l_hSem = SEM_FAILED;
        }
    }

    return l_eReturn;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlock
c_shared::e_lock c_shared::unlock(h_sem &io_rSem)
{
    e_lock l_eReturn = c_shared::e_error;
    sem_t *l_hSem    = (sem_t *)io_rSem;

    if(nullptr == io_rSem)
    {
        l_eReturn = c_shared::e_not_exists;
        goto l_lblExit;
    }

    if(0 == sem_post(l_hSem))
    {
        l_eReturn = c_shared::e_ok;
    }

    sem_close(l_hSem);
    io_rSem = nullptr;

l_lblExit:
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
bool c_shared::close(h_shared i_pShared)
{
    if(nullptr == i_pShared)
    {
        return false;
    }

    if(0 <= i_pShared->iMFD)
    {
        ::close(i_pShared->iMFD);
        i_pShared->iMFD = -1;
    }

    if(i_pShared->pMemName)
    {
        shm_unlink(i_pShared->pMemName);
    }

    if(SEM_FAILED != i_pShared->hSemaphore)
    {
        int l_iRes            = -1;
        l_iRes                = sem_close(i_pShared->hSemaphore);
        i_pShared->hSemaphore = SEM_FAILED;
        UNUSED_ARG(l_iRes);
    }

    if(i_pShared->pSemName)
    {
        sem_unlink(i_pShared->pSemName);
        free(i_pShared->pSemName);
        i_pShared->pSemName = nullptr;
    }

    if(i_pShared->pMemName)
    {
        free(i_pShared->pMemName);
        i_pShared->pMemName = nullptr;
    }

    if(i_pShared->pName)
    {
        free(i_pShared->pName);
        i_pShared->pName = nullptr;
    }

    i_pShared->szName = 0;

    free(i_pShared);
    i_pShared = nullptr;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlink
bool c_shared::unlink(const tXCHAR *i_pName)
{
    bool   l_bReturn = true;
    size_t l_szName  = 0;
    char  *l_pName   = nullptr;

    if(nullptr == i_pName)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    l_szName = shared_name_len(i_pName);
    l_pName  = (char *)malloc(l_szName);

    if(nullptr == l_pName)
    {
        l_bReturn = false;
        goto l_lblExit;
    }

    create_name(l_pName, l_szName, e_type_mutex, i_pName);
    if(0 != sem_unlink(l_pName))
    {
        l_bReturn = false;
    }

    create_name(l_pName, l_szName, e_type_file, i_pName);
    if(0 != shm_unlink(l_pName))
    {
        l_bReturn = false;
    }

l_lblExit:
    if(l_pName)
    {
        free(l_pName);
        l_pName = nullptr;
    }
    return l_bReturn;
}
