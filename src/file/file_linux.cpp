// Ensure GNU extensions (O_DIRECT, fallocate) are visible.
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include "kit/file.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

namespace kit
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// s_impl definition
struct c_file::s_impl
{
    std::atomic<int32_t> ma_ref { 1 };
    int                  mi_fd { -1 };
    int                  mi_last_err { 0 };
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// s_iovec must overlay struct iovec so we can pass a s_iovec* to readv/writev
// without an intermediate copy.
static_assert(sizeof(s_iovec) == sizeof(struct iovec), "s_iovec must match POSIX iovec layout");
static_assert(offsetof(s_iovec, mp_data) == offsetof(struct iovec, iov_base),
              "s_iovec::mp_data must overlay iov_base");
static_assert(offsetof(s_iovec, mz_size) == offsetof(struct iovec, iov_len), "s_iovec::mz_size must overlay iov_len");

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

bool has_flag(uint32_t iu_flags, uint32_t iu_bit)
{
    return (iu_flags & iu_bit) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Map kit open parameters to Linux open() flags. Returns false when the
// combination is invalid.
bool build_open_flags(e_file_open_mode ie_mode, uint32_t iu_flags, int &or_flags)
{
    or_flags      = 0;

    bool lb_read  = has_flag(iu_flags, e_ff_read);
    bool lb_write = has_flag(iu_flags, e_ff_write);
    if(!lb_read && !lb_write)
    {
        return false;
    }
    if(lb_read && lb_write)
    {
        or_flags |= O_RDWR;
    }
    else if(lb_write)
    {
        or_flags |= O_WRONLY;
    }
    else
    {
        or_flags |= O_RDONLY;
    }

    if(has_flag(iu_flags, e_ff_hint_sequential) && has_flag(iu_flags, e_ff_hint_random))
    {
        return false;
    }

    switch(ie_mode)
    {
    case e_fom_open_existing:
        break;
    case e_fom_create_new:
        or_flags |= O_CREAT | O_EXCL;
        break;
    case e_fom_create_always:
        or_flags |= O_CREAT | O_TRUNC;
        break;
    case e_fom_open_or_create:
        or_flags |= O_CREAT;
        break;
    case e_fom_truncate_existing:
        or_flags |= O_TRUNC;
        break;
    default:
        return false;
    }

    if(has_flag(iu_flags, e_ff_append))
    {
        or_flags |= O_APPEND;
    }
    if(has_flag(iu_flags, e_ff_write_through))
    {
        or_flags |= O_SYNC;
    }
#ifdef O_DIRECT
    if(has_flag(iu_flags, e_ff_no_buffering))
    {
        or_flags |= O_DIRECT;
    }
#endif

    return true;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ctor / dtor
c_file::c_file()
    : mp_impl(new s_impl)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
c_file::~c_file()
{
    if(nullptr != mp_impl)
    {
        if(mp_impl->mi_fd >= 0)
        {
            ::close(mp_impl->mi_fd);
            mp_impl->mi_fd = -1;
        }
        delete mp_impl;
        mp_impl = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t c_file::add_ref()
{
    return mp_impl->ma_ref.fetch_add(1, std::memory_order_relaxed) + 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t c_file::release()
{
    int32_t li_new = mp_impl->ma_ref.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if(li_new <= 0)
    {
        delete this;
    }
    return li_new;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::is_open() const
{
    return mp_impl->mi_fd >= 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::open(const std::filesystem::path &ir_path, e_file_open_mode ie_mode, uint32_t iu_flags)
{
    if(mp_impl->mi_fd >= 0)
    {
        close(false);
    }

    int li_flags = 0;
    if(!build_open_flags(ie_mode, iu_flags, li_flags))
    {
        mp_impl->mi_last_err = EINVAL;
        return false;
    }

    int li_fd = ::open(ir_path.c_str(), li_flags, 0644);
    if(li_fd < 0)
    {
        mp_impl->mi_last_err = errno;
        return false;
    }

    // Access-pattern hints (best-effort; failure does not fail open()).
    if(has_flag(iu_flags, e_ff_hint_sequential))
    {
        posix_fadvise(li_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
    }
    if(has_flag(iu_flags, e_ff_hint_random))
    {
        posix_fadvise(li_fd, 0, 0, POSIX_FADV_RANDOM);
    }

    mp_impl->mi_fd       = li_fd;
    mp_impl->mi_last_err = 0;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::close(bool ib_flush)
{
    if(mp_impl->mi_fd < 0)
    {
        return true;
    }

    bool lb_ok = true;

    if(ib_flush)
    {
        if(fsync(mp_impl->mi_fd) == -1)
        {
            mp_impl->mi_last_err = errno;
            lb_ok                = false;
        }
    }

    int li_fd      = mp_impl->mi_fd;
    mp_impl->mi_fd = -1;

    if(::close(li_fd) == -1)
    {
        mp_impl->mi_last_err = errno;
        lb_ok                = false;
    }

    return lb_ok;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::read(void *op_buf, size_t iz_size)
{
    if((mp_impl->mi_fd < 0) || (nullptr == op_buf) || (0 == iz_size))
    {
        return 0;
    }

    uint8_t *lp_out  = static_cast<uint8_t *>(op_buf);
    size_t   lz_done = 0;

    while(lz_done < iz_size)
    {
        ssize_t li_n = ::read(mp_impl->mi_fd, lp_out + lz_done, iz_size - lz_done);
        if(li_n > 0)
        {
            lz_done += static_cast<size_t>(li_n);
        }
        else if(li_n == 0)
        {
            break; // EOF
        }
        else
        {
            if(errno == EINTR)
            {
                continue;
            }
            mp_impl->mi_last_err = errno;
            break;
        }
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::write(const void *ip_buf, size_t iz_size, bool ib_flush)
{
    if((mp_impl->mi_fd < 0) || (nullptr == ip_buf) || (0 == iz_size))
    {
        return 0;
    }

    const uint8_t *lp_in   = static_cast<const uint8_t *>(ip_buf);
    size_t         lz_done = 0;

    while(lz_done < iz_size)
    {
        ssize_t li_n = ::write(mp_impl->mi_fd, lp_in + lz_done, iz_size - lz_done);
        if(li_n > 0)
        {
            lz_done += static_cast<size_t>(li_n);
        }
        else if(li_n == 0)
        {
            break;
        }
        else
        {
            if(errno == EINTR)
            {
                continue;
            }
            mp_impl->mi_last_err = errno;
            break;
        }
    }

    if(ib_flush && lz_done > 0)
    {
        if(fsync(mp_impl->mi_fd) == -1)
        {
            mp_impl->mi_last_err = errno;
        }
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// positional I/O
size_t c_file::read_at(uint64_t iu_offset, void *op_buf, size_t iz_size)
{
    if((mp_impl->mi_fd < 0) || (nullptr == op_buf) || (0 == iz_size))
    {
        return 0;
    }

    uint8_t *lp_out  = static_cast<uint8_t *>(op_buf);
    size_t   lz_done = 0;

    while(lz_done < iz_size)
    {
        ssize_t li_n
            = pread(mp_impl->mi_fd, lp_out + lz_done, iz_size - lz_done, static_cast<off_t>(iu_offset + lz_done));
        if(li_n > 0)
        {
            lz_done += static_cast<size_t>(li_n);
        }
        else if(li_n == 0)
        {
            break;
        }
        else
        {
            if(errno == EINTR)
            {
                continue;
            }
            mp_impl->mi_last_err = errno;
            break;
        }
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::write_at(uint64_t iu_offset, const void *ip_buf, size_t iz_size)
{
    if((mp_impl->mi_fd < 0) || (nullptr == ip_buf) || (0 == iz_size))
    {
        return 0;
    }

    const uint8_t *lp_in   = static_cast<const uint8_t *>(ip_buf);
    size_t         lz_done = 0;

    while(lz_done < iz_size)
    {
        ssize_t li_n
            = pwrite(mp_impl->mi_fd, lp_in + lz_done, iz_size - lz_done, static_cast<off_t>(iu_offset + lz_done));
        if(li_n > 0)
        {
            lz_done += static_cast<size_t>(li_n);
        }
        else if(li_n == 0)
        {
            break;
        }
        else
        {
            if(errno == EINTR)
            {
                continue;
            }
            mp_impl->mi_last_err = errno;
            break;
        }
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// scatter/gather
size_t c_file::read_v(const s_iovec *ip_iov, size_t iz_count)
{
    if((mp_impl->mi_fd < 0) || (nullptr == ip_iov) || (0 == iz_count))
    {
        return 0;
    }
    if(iz_count > static_cast<size_t>(IOV_MAX))
    {
        mp_impl->mi_last_err = EINVAL;
        return 0;
    }

    while(true)
    {
        ssize_t li_n
            = readv(mp_impl->mi_fd, reinterpret_cast<const struct iovec *>(ip_iov), static_cast<int>(iz_count));
        if(li_n >= 0)
        {
            return static_cast<size_t>(li_n);
        }
        if(errno == EINTR)
        {
            continue;
        }
        mp_impl->mi_last_err = errno;
        return 0;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::write_v(const s_iovec *ip_iov, size_t iz_count, bool ib_flush)
{
    if((mp_impl->mi_fd < 0) || (nullptr == ip_iov) || (0 == iz_count))
    {
        return 0;
    }
    if(iz_count > static_cast<size_t>(IOV_MAX))
    {
        mp_impl->mi_last_err = EINVAL;
        return 0;
    }

    ssize_t li_n = 0;
    while(true)
    {
        li_n = writev(mp_impl->mi_fd, reinterpret_cast<const struct iovec *>(ip_iov), static_cast<int>(iz_count));
        if(li_n >= 0)
        {
            break;
        }
        if(errno == EINTR)
        {
            continue;
        }
        mp_impl->mi_last_err = errno;
        return 0;
    }

    if(ib_flush && li_n > 0)
    {
        if(fsync(mp_impl->mi_fd) == -1)
        {
            mp_impl->mi_last_err = errno;
        }
    }
    return static_cast<size_t>(li_n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// position / size
bool c_file::set_position(uint64_t iu_offset)
{
    if(mp_impl->mi_fd < 0)
    {
        return false;
    }
    off_t li_r = lseek(mp_impl->mi_fd, static_cast<off_t>(iu_offset), SEEK_SET);
    if(li_r < 0)
    {
        mp_impl->mi_last_err = errno;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t c_file::get_position() const
{
    if(mp_impl->mi_fd < 0)
    {
        return 0;
    }
    off_t li_r = lseek(mp_impl->mi_fd, 0, SEEK_CUR);
    if(li_r < 0)
    {
        mp_impl->mi_last_err = errno;
        return 0;
    }
    return static_cast<uint64_t>(li_r);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t c_file::get_size() const
{
    if(mp_impl->mi_fd < 0)
    {
        return 0;
    }
    struct stat ls_st = {};
    if(fstat(mp_impl->mi_fd, &ls_st) == -1)
    {
        mp_impl->mi_last_err = errno;
        return 0;
    }
    return static_cast<uint64_t>(ls_st.st_size);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sizing / preallocation
bool c_file::set_size(uint64_t iu_size)
{
    if(mp_impl->mi_fd < 0)
    {
        return false;
    }
    if(ftruncate(mp_impl->mi_fd, static_cast<off_t>(iu_size)) == -1)
    {
        mp_impl->mi_last_err = errno;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::preallocate(uint64_t iu_size)
{
    if(mp_impl->mi_fd < 0)
    {
        return false;
    }

    // fallocate() extends the logical size and reserves the space in one
    // syscall on filesystems that support it. Fall back to posix_fallocate
    // on ENOSYS (e.g. tmpfs).
    int li_rc = fallocate(mp_impl->mi_fd, 0, 0, static_cast<off_t>(iu_size));
    if(li_rc == 0)
    {
        return true;
    }
    if(errno != ENOSYS && errno != EOPNOTSUPP)
    {
        mp_impl->mi_last_err = errno;
        return false;
    }

    int li_pf = posix_fallocate(mp_impl->mi_fd, 0, static_cast<off_t>(iu_size));
    if(li_pf != 0)
    {
        mp_impl->mi_last_err = li_pf;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// durability
bool c_file::sync()
{
    if(mp_impl->mi_fd < 0)
    {
        return false;
    }
    if(fsync(mp_impl->mi_fd) == -1)
    {
        mp_impl->mi_last_err = errno;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// diagnostics
int c_file::get_last_error() const
{
    return mp_impl->mi_last_err;
}

} // namespace kit
