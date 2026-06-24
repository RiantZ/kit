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
// macOS limits POSIX shared memory / semaphore names to PSHMNAMELEN (31 chars).
// We use a compact format with a djb2 hash of the user-supplied name to ensure
// the OS-level name always fits: "/k<type>_<pid>_<8-hex-hash>" (~20 chars max.
////////////////////////////////////////////////////////////////////////////////
namespace
{

enum e_type
{
    e_type_mutex = 0,
    e_type_file,
    e_type_max
};

const size_t SHARED_NAME_BUF_LEN = 32;

uint32_t hash_name(const char *ip_str)
{
    uint32_t lu_hash = 5381;
    for(const char *lp_ch = ip_str; *lp_ch; lp_ch++)
    {
        lu_hash = ((lu_hash << 5) + lu_hash) + (uint32_t)*lp_ch;
    }
    return lu_hash;
}

bool create_name(char *op_name, size_t iz_name, e_type ie_type, const char *ip_postfix)
{
    if((nullptr == op_name) || (16 >= iz_name) || (e_type_max <= ie_type) || (nullptr == ip_postfix))
    {
        return false;
    }

    snprintf(op_name, iz_name, "/k%d_%d_%08x", ie_type, getpid(), hash_name(ip_postfix));
    return true;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
struct c_shared::s_shared
{
    int    mi_mfd;
    sem_t *mh_semaphore;
    char   mp_sem_name[SHARED_NAME_BUF_LEN];
    char   mp_mem_name[SHARED_NAME_BUF_LEN];
    char  *mp_name;
};

////////////////////////////////////////////////////////////////////////////////
// c_shared::create
bool c_shared::create(h_shared *op_handle, const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size)
{
    h_shared lp_shared  = nullptr;
    bool     lb_result  = true;
    bool     lb_release = false;
    void    *lp_buffer  = nullptr;

    if((nullptr == ip_name) || (nullptr == ip_data) || (0 >= iu_size) || (nullptr == op_handle))
    {
        lb_result = false;
        goto l_lblExit;
    }

    lp_shared = (h_shared)malloc(sizeof(s_shared));
    if(nullptr == lp_shared)
    {
        lb_result = false;
        goto l_lblExit;
    }

    memset(lp_shared, 0, sizeof(s_shared));
    lp_shared->mh_semaphore = SEM_FAILED;
    lp_shared->mi_mfd       = -1;
    lp_shared->mp_name      = strdup(ip_name);

    if(nullptr == lp_shared->mp_name)
    {
        lb_result = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // create semaphore and own it
    create_name(lp_shared->mp_sem_name, SHARED_NAME_BUF_LEN, e_type_mutex, ip_name);

    lp_shared->mh_semaphore = sem_open(lp_shared->mp_sem_name, O_CREAT | O_EXCL, 0666, 0);
    if(SEM_FAILED == lp_shared->mh_semaphore)
    {
        lp_shared->mp_sem_name[0] = '\0';
        lb_result                 = false;
        goto l_lblExit;
    }

    lb_release = true;

    ////////////////////////////////////////////////////////////////////////////
    // share memory
    create_name(lp_shared->mp_mem_name, SHARED_NAME_BUF_LEN, e_type_file, ip_name);

    lp_shared->mi_mfd = shm_open(lp_shared->mp_mem_name, O_RDWR | O_CREAT | O_EXCL, 0666);
    if(0 > lp_shared->mi_mfd)
    {
        lp_shared->mp_mem_name[0] = '\0';
        lb_result                 = false;
        goto l_lblExit;
    }

    if(0 != ftruncate(lp_shared->mi_mfd, (off_t)iu_size))
    {
        lb_result = false;
        goto l_lblExit;
    }

    lp_buffer = mmap(0, (size_t)iu_size, PROT_READ | PROT_WRITE, MAP_SHARED, lp_shared->mi_mfd, 0);

    if((nullptr == lp_buffer) || (MAP_FAILED == lp_buffer))
    {
        lb_result = false;
        goto l_lblExit;
    }

    *op_handle = lp_shared;

    memcpy(lp_buffer, ip_data, (size_t)iu_size);

    if(0 != munmap(lp_buffer, (size_t)iu_size))
    {
    }

l_lblExit:
    if(lb_release)
    {
        sem_post(lp_shared->mh_semaphore);
    }

    if(!lb_result)
    {
        close(lp_shared);
        lp_shared = nullptr;

        if(op_handle)
        {
            *op_handle = nullptr;
        }
    }

    return lb_result;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::read
bool c_shared::read(const tXCHAR *ip_name, void *op_data, uint16_t iu_size)
{
    bool        lb_result = true;
    char        lp_name[SHARED_NAME_BUF_LEN];
    void       *lp_buffer = nullptr;
    int         li_mfd    = -1;
    struct stat ls_stat;

    if((nullptr == ip_name) || (nullptr == op_data) || (0 >= iu_size))
    {
        lb_result = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(lp_name, SHARED_NAME_BUF_LEN, e_type_file, ip_name);

    li_mfd = shm_open(lp_name, O_RDONLY, 0444);

    if(0 > li_mfd)
    {
        lb_result = false;
        goto l_lblExit;
    }

    memset(&ls_stat, 0, sizeof(ls_stat));
    if(-1 == fstat(li_mfd, &ls_stat))
    {
        lb_result = false;
        goto l_lblExit;
    }

    if((size_t)ls_stat.st_size < (size_t)iu_size)
    {
        lb_result = false;
        goto l_lblExit;
    }

    lp_buffer = mmap(0, (size_t)ls_stat.st_size, PROT_READ, MAP_SHARED, li_mfd, 0);

    if((nullptr == lp_buffer) || (MAP_FAILED == lp_buffer))
    {
        lb_result = false;
        goto l_lblExit;
    }

    memcpy(op_data, lp_buffer, (size_t)iu_size);

    if(0 == munmap(lp_buffer, (size_t)ls_stat.st_size))
    {
        lp_buffer = nullptr;
    }

l_lblExit:
    if(0 <= li_mfd)
    {
        ::close(li_mfd);
        li_mfd = -1;
    }

    return lb_result;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::write
bool c_shared::write(const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size)
{
    bool        lb_result = true;
    char        lp_name[SHARED_NAME_BUF_LEN];
    void       *lp_buffer = nullptr;
    int         li_mfd    = -1;
    struct stat ls_stat;

    if((nullptr == ip_name) || (nullptr == ip_data) || (0 >= iu_size))
    {
        lb_result = false;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open shared memory object
    create_name(lp_name, SHARED_NAME_BUF_LEN, e_type_file, ip_name);

    li_mfd = shm_open(lp_name, O_RDWR, 0666);

    if(0 > li_mfd)
    {
        lb_result = false;
        goto l_lblExit;
    }

    memset(&ls_stat, 0, sizeof(ls_stat));
    if(-1 == fstat(li_mfd, &ls_stat))
    {
        lb_result = false;
        goto l_lblExit;
    }

    if((size_t)ls_stat.st_size < (size_t)iu_size)
    {
        lb_result = false;
        goto l_lblExit;
    }

    lp_buffer = mmap(0, (size_t)ls_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, li_mfd, 0);

    if((nullptr == lp_buffer) || (MAP_FAILED == lp_buffer))
    {
        lb_result = false;
        goto l_lblExit;
    }

    memcpy(lp_buffer, ip_data, (size_t)iu_size);

    if(0 == munmap(lp_buffer, (size_t)ls_stat.st_size))
    {
        lp_buffer = nullptr;
    }

l_lblExit:
    if(0 <= li_mfd)
    {
        ::close(li_mfd);
        li_mfd = -1;
    }

    return lb_result;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::lock
c_shared::e_lock c_shared::lock(const tXCHAR *ip_name, h_sem &or_sem, uint32_t iu_timeout_ms)
{
    e_lock    le_return = c_shared::e_timeout;
    char      lp_name[SHARED_NAME_BUF_LEN];
    const int li_1ms  = 1000;
    int64_t   li_wait = (int64_t)iu_timeout_ms * 1000LL;
    sem_t    *lh_sem  = SEM_FAILED;

    or_sem            = nullptr;

    if(nullptr == ip_name)
    {
        le_return = c_shared::e_error;
        goto l_lblExit;
    }

    ////////////////////////////////////////////////////////////////////////////
    // open semaphore
    create_name(lp_name, SHARED_NAME_BUF_LEN, e_type_mutex, ip_name);

    lh_sem = sem_open(lp_name, 0);
    if(SEM_FAILED == lh_sem)
    {
        le_return = c_shared::e_not_exists;
        goto l_lblExit;
    }

    le_return = c_shared::e_timeout;

    while(0 < li_wait)
    {
        if(0 == sem_trywait(lh_sem))
        {
            le_return = c_shared::e_ok;
            break;
        }
        else
        {
            usleep(li_1ms); // 1 ms
            li_wait -= li_1ms;
        }
    }

l_lblExit:
    if(SEM_FAILED != lh_sem)
    {
        if(c_shared::e_ok == le_return)
        {
            or_sem = (h_sem)lh_sem;
        }
        else
        {
            sem_close(lh_sem);
            lh_sem = SEM_FAILED;
        }
    }

    return le_return;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlock
c_shared::e_lock c_shared::unlock(h_sem &ior_sem)
{
    e_lock le_return = c_shared::e_error;
    sem_t *lh_sem    = (sem_t *)ior_sem;

    if(nullptr == ior_sem)
    {
        le_return = c_shared::e_not_exists;
        goto l_lblExit;
    }

    if(0 == sem_post(lh_sem))
    {
        le_return = c_shared::e_ok;
    }

    sem_close(lh_sem);
    ior_sem = nullptr;

l_lblExit:
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

    if(0 <= ih_shared->mi_mfd)
    {
        ::close(ih_shared->mi_mfd);
        ih_shared->mi_mfd = -1;
    }

    if(ih_shared->mp_mem_name[0])
    {
        shm_unlink(ih_shared->mp_mem_name);
    }

    if(SEM_FAILED != ih_shared->mh_semaphore)
    {
        int li_res              = sem_close(ih_shared->mh_semaphore);
        ih_shared->mh_semaphore = SEM_FAILED;
        UNUSED_ARG(li_res);
    }

    if(ih_shared->mp_sem_name[0])
    {
        sem_unlink(ih_shared->mp_sem_name);
    }

    if(ih_shared->mp_name)
    {
        free(ih_shared->mp_name);
        ih_shared->mp_name = nullptr;
    }

    free(ih_shared);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// c_shared::unlink
bool c_shared::unlink(const tXCHAR *ip_name)
{
    bool lb_return = true;
    char lp_name[SHARED_NAME_BUF_LEN];

    if(nullptr == ip_name)
    {
        return false;
    }

    create_name(lp_name, SHARED_NAME_BUF_LEN, e_type_mutex, ip_name);
    if(0 != sem_unlink(lp_name))
    {
        lb_return = false;
    }

    create_name(lp_name, SHARED_NAME_BUF_LEN, e_type_file, ip_name);
    if(0 != shm_unlink(lp_name))
    {
        lb_return = false;
    }

    return lb_return;
}
