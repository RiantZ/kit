#include "kit/file.hpp"
#include "kit/ts_helpers.h" // pulls in <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace kit
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// s_impl definition
struct c_file::s_impl
{
    std::atomic<int32_t> ma_ref { 1 };
    HANDLE               mh_file { INVALID_HANDLE_VALUE };
    int                  mi_last_err { 0 };
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// helpers
namespace
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// A one-shot per-call ReadFile chunk size. Win32 count parameter is DWORD, so
// a single syscall cannot move more than 4 GiB - 1.
constexpr DWORD gd_max_chunk = 0x7FFFFFFFu;

bool has_flag(uint32_t iu_flags, uint32_t iu_bit)
{
    return (iu_flags & iu_bit) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Map kit open parameters to CreateFileW arguments. Returns false when the
// combination is invalid.
bool build_open_args(e_file_open_mode ie_mode,
                     uint32_t         iu_flags,
                     DWORD           &or_access,
                     DWORD           &or_share,
                     DWORD           &or_disp,
                     DWORD           &or_attrs)
{
    or_access     = 0;
    or_share      = 0;
    or_disp       = 0;
    or_attrs      = FILE_ATTRIBUTE_NORMAL;

    bool lb_read  = has_flag(iu_flags, e_ff_read);
    bool lb_write = has_flag(iu_flags, e_ff_write);
    bool lb_app   = has_flag(iu_flags, e_ff_append);
    if(!lb_read && !lb_write)
    {
        return false;
    }
    if(lb_read)
    {
        or_access |= GENERIC_READ;
    }
    if(lb_write)
    {
        // Append mode restricts write access to atomically appending at EOF.
        or_access |= (lb_app ? FILE_APPEND_DATA : GENERIC_WRITE);
    }

    if(has_flag(iu_flags, e_ff_hint_sequential) && has_flag(iu_flags, e_ff_hint_random))
    {
        return false;
    }

    if(has_flag(iu_flags, e_ff_share_read))
    {
        or_share |= FILE_SHARE_READ;
    }
    if(has_flag(iu_flags, e_ff_share_write))
    {
        or_share |= FILE_SHARE_WRITE;
    }
    if(has_flag(iu_flags, e_ff_share_delete))
    {
        or_share |= FILE_SHARE_DELETE;
    }

    switch(ie_mode)
    {
    case e_fom_open_existing:
        or_disp = OPEN_EXISTING;
        break;
    case e_fom_create_new:
        or_disp = CREATE_NEW;
        break;
    case e_fom_create_always:
        or_disp = CREATE_ALWAYS;
        break;
    case e_fom_open_or_create:
        or_disp = OPEN_ALWAYS;
        break;
    case e_fom_truncate_existing:
        or_disp = TRUNCATE_EXISTING;
        break;
    default:
        return false;
    }

    if(has_flag(iu_flags, e_ff_hint_sequential))
    {
        or_attrs |= FILE_FLAG_SEQUENTIAL_SCAN;
    }
    if(has_flag(iu_flags, e_ff_hint_random))
    {
        or_attrs |= FILE_FLAG_RANDOM_ACCESS;
    }
    if(has_flag(iu_flags, e_ff_no_buffering))
    {
        or_attrs |= FILE_FLAG_NO_BUFFERING;
    }
    if(has_flag(iu_flags, e_ff_write_through))
    {
        or_attrs |= FILE_FLAG_WRITE_THROUGH;
    }

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
        if(INVALID_HANDLE_VALUE != mp_impl->mh_file)
        {
            CloseHandle(mp_impl->mh_file);
            mp_impl->mh_file = INVALID_HANDLE_VALUE;
        }
        delete mp_impl;
        mp_impl = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ref-counting
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
// lifecycle
bool c_file::is_open() const
{
    return INVALID_HANDLE_VALUE != mp_impl->mh_file;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::open(const std::filesystem::path &ir_path, e_file_open_mode ie_mode, uint32_t iu_flags)
{
    if(INVALID_HANDLE_VALUE != mp_impl->mh_file)
    {
        close(false);
    }

    DWORD ld_access = 0;
    DWORD ld_share  = 0;
    DWORD ld_disp   = 0;
    DWORD ld_attrs  = 0;
    if(!build_open_args(ie_mode, iu_flags, ld_access, ld_share, ld_disp, ld_attrs))
    {
        mp_impl->mi_last_err = static_cast<int>(ERROR_INVALID_PARAMETER);
        return false;
    }

    HANDLE lh_file = CreateFileW(ir_path.c_str(), ld_access, ld_share, nullptr, ld_disp, ld_attrs, nullptr);
    if(INVALID_HANDLE_VALUE == lh_file)
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        return false;
    }

    mp_impl->mh_file     = lh_file;
    mp_impl->mi_last_err = 0;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::close(bool ib_flush)
{
    if(INVALID_HANDLE_VALUE == mp_impl->mh_file)
    {
        return true;
    }

    bool lb_ok = true;
    if(ib_flush)
    {
        if(!FlushFileBuffers(mp_impl->mh_file))
        {
            mp_impl->mi_last_err = static_cast<int>(GetLastError());
            lb_ok                = false;
        }
    }

    HANDLE lh_tmp    = mp_impl->mh_file;
    mp_impl->mh_file = INVALID_HANDLE_VALUE;

    if(!CloseHandle(lh_tmp))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        lb_ok                = false;
    }
    return lb_ok;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// stream I/O
size_t c_file::read(void *op_buf, size_t iz_size)
{
    if((INVALID_HANDLE_VALUE == mp_impl->mh_file) || (nullptr == op_buf) || (0 == iz_size))
    {
        return 0;
    }

    uint8_t *lp_out  = static_cast<uint8_t *>(op_buf);
    size_t   lz_done = 0;

    while(lz_done < iz_size)
    {
        size_t lz_remain = iz_size - lz_done;
        DWORD  ld_want   = (lz_remain > gd_max_chunk) ? gd_max_chunk : static_cast<DWORD>(lz_remain);
        DWORD  ld_got    = 0;

        if(!ReadFile(mp_impl->mh_file, lp_out + lz_done, ld_want, &ld_got, nullptr))
        {
            DWORD ld_err = GetLastError();
            if(ERROR_HANDLE_EOF == ld_err)
            {
                break;
            }
            mp_impl->mi_last_err = static_cast<int>(ld_err);
            break;
        }
        if(0 == ld_got)
        {
            break; // EOF
        }
        lz_done += ld_got;
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::write(const void *ip_buf, size_t iz_size, bool ib_flush)
{
    if((INVALID_HANDLE_VALUE == mp_impl->mh_file) || (nullptr == ip_buf) || (0 == iz_size))
    {
        return 0;
    }

    const uint8_t *lp_in   = static_cast<const uint8_t *>(ip_buf);
    size_t         lz_done = 0;

    while(lz_done < iz_size)
    {
        size_t lz_remain = iz_size - lz_done;
        DWORD  ld_want   = (lz_remain > gd_max_chunk) ? gd_max_chunk : static_cast<DWORD>(lz_remain);
        DWORD  ld_put    = 0;

        if(!WriteFile(mp_impl->mh_file, lp_in + lz_done, ld_want, &ld_put, nullptr))
        {
            mp_impl->mi_last_err  = static_cast<int>(GetLastError());
            lz_done              += ld_put;
            break;
        }
        if(0 == ld_put)
        {
            break;
        }
        lz_done += ld_put;
    }

    if(ib_flush && lz_done > 0)
    {
        if(!FlushFileBuffers(mp_impl->mh_file))
        {
            mp_impl->mi_last_err = static_cast<int>(GetLastError());
        }
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// positional I/O
//
// For handles opened synchronously (no FILE_FLAG_OVERLAPPED) an OVERLAPPED
// struct is still honoured for the starting offset and the call blocks until
// completion (documented behaviour). The handle's implicit file pointer is
// updated as a side effect, which is why mixing stream and positional I/O
// from different threads on the same instance is a documented usage error.
size_t c_file::read_at(uint64_t iu_offset, void *op_buf, size_t iz_size)
{
    if((INVALID_HANDLE_VALUE == mp_impl->mh_file) || (nullptr == op_buf) || (0 == iz_size))
    {
        return 0;
    }

    uint8_t *lp_out  = static_cast<uint8_t *>(op_buf);
    size_t   lz_done = 0;

    while(lz_done < iz_size)
    {
        size_t     lz_remain = iz_size - lz_done;
        DWORD      ld_want   = (lz_remain > gd_max_chunk) ? gd_max_chunk : static_cast<DWORD>(lz_remain);
        DWORD      ld_got    = 0;
        uint64_t   lu_off    = iu_offset + lz_done;
        OVERLAPPED ls_ov     = {};
        ls_ov.Offset         = static_cast<DWORD>(lu_off & 0xFFFFFFFFULL);
        ls_ov.OffsetHigh     = static_cast<DWORD>(lu_off >> 32);

        if(!ReadFile(mp_impl->mh_file, lp_out + lz_done, ld_want, &ld_got, &ls_ov))
        {
            DWORD ld_err = GetLastError();
            if(ERROR_HANDLE_EOF == ld_err)
            {
                break;
            }
            mp_impl->mi_last_err = static_cast<int>(ld_err);
            break;
        }
        if(0 == ld_got)
        {
            break;
        }
        lz_done += ld_got;
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::write_at(uint64_t iu_offset, const void *ip_buf, size_t iz_size)
{
    if((INVALID_HANDLE_VALUE == mp_impl->mh_file) || (nullptr == ip_buf) || (0 == iz_size))
    {
        return 0;
    }

    const uint8_t *lp_in   = static_cast<const uint8_t *>(ip_buf);
    size_t         lz_done = 0;

    while(lz_done < iz_size)
    {
        size_t     lz_remain = iz_size - lz_done;
        DWORD      ld_want   = (lz_remain > gd_max_chunk) ? gd_max_chunk : static_cast<DWORD>(lz_remain);
        DWORD      ld_put    = 0;
        uint64_t   lu_off    = iu_offset + lz_done;
        OVERLAPPED ls_ov     = {};
        ls_ov.Offset         = static_cast<DWORD>(lu_off & 0xFFFFFFFFULL);
        ls_ov.OffsetHigh     = static_cast<DWORD>(lu_off >> 32);

        if(!WriteFile(mp_impl->mh_file, lp_in + lz_done, ld_want, &ld_put, &ls_ov))
        {
            mp_impl->mi_last_err  = static_cast<int>(GetLastError());
            lz_done              += ld_put;
            break;
        }
        if(0 == ld_put)
        {
            break;
        }
        lz_done += ld_put;
    }
    return lz_done;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// scatter/gather
//
// WriteFileGather / ReadFileScatter cannot be used because they require
// page-aligned buffers of exactly VM_PAGE_SIZE, FILE_FLAG_OVERLAPPED and
// FILE_FLAG_NO_BUFFERING. Instead we loop over the iovec calling ReadFile
// / WriteFile on the current file position - observable semantics match
// POSIX readv/writev.
size_t c_file::read_v(const s_iovec *ip_iov, size_t iz_count)
{
    if((INVALID_HANDLE_VALUE == mp_impl->mh_file) || (nullptr == ip_iov) || (0 == iz_count))
    {
        return 0;
    }

    size_t lz_total = 0;
    for(size_t lz_i = 0; lz_i < iz_count; ++lz_i)
    {
        size_t lz_got  = read(ip_iov[lz_i].mp_data, ip_iov[lz_i].mz_size);
        lz_total      += lz_got;
        if(lz_got != ip_iov[lz_i].mz_size)
        {
            break; // short or EOF - stop here to match writev-style aggregation
        }
    }
    return lz_total;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t c_file::write_v(const s_iovec *ip_iov, size_t iz_count, bool ib_flush)
{
    if((INVALID_HANDLE_VALUE == mp_impl->mh_file) || (nullptr == ip_iov) || (0 == iz_count))
    {
        return 0;
    }

    size_t lz_total = 0;
    for(size_t lz_i = 0; lz_i < iz_count; ++lz_i)
    {
        size_t lz_put  = write(ip_iov[lz_i].mp_data, ip_iov[lz_i].mz_size, false);
        lz_total      += lz_put;
        if(lz_put != ip_iov[lz_i].mz_size)
        {
            break;
        }
    }

    if(ib_flush && lz_total > 0)
    {
        if(!FlushFileBuffers(mp_impl->mh_file))
        {
            mp_impl->mi_last_err = static_cast<int>(GetLastError());
        }
    }
    return lz_total;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// position / size
bool c_file::set_position(uint64_t iu_offset)
{
    if(INVALID_HANDLE_VALUE == mp_impl->mh_file)
    {
        return false;
    }
    LARGE_INTEGER ls_off = {};
    ls_off.QuadPart      = static_cast<LONGLONG>(iu_offset);
    if(!SetFilePointerEx(mp_impl->mh_file, ls_off, nullptr, FILE_BEGIN))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t c_file::get_position() const
{
    if(INVALID_HANDLE_VALUE == mp_impl->mh_file)
    {
        return 0;
    }
    LARGE_INTEGER ls_zero = {};
    LARGE_INTEGER ls_cur  = {};
    if(!SetFilePointerEx(mp_impl->mh_file, ls_zero, &ls_cur, FILE_CURRENT))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        return 0;
    }
    return static_cast<uint64_t>(ls_cur.QuadPart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t c_file::get_size() const
{
    if(INVALID_HANDLE_VALUE == mp_impl->mh_file)
    {
        return 0;
    }
    LARGE_INTEGER ls_sz = {};
    if(!GetFileSizeEx(mp_impl->mh_file, &ls_sz))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        return 0;
    }
    return static_cast<uint64_t>(ls_sz.QuadPart);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sizing / preallocation
bool c_file::set_size(uint64_t iu_size)
{
    if(INVALID_HANDLE_VALUE == mp_impl->mh_file)
    {
        return false;
    }

    // Save the current file pointer so that shrinking the file below the
    // caller's position leaves us at a sane offset.
    LARGE_INTEGER ls_zero = {};
    LARGE_INTEGER ls_save = {};
    if(!SetFilePointerEx(mp_impl->mh_file, ls_zero, &ls_save, FILE_CURRENT))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        return false;
    }

    LARGE_INTEGER ls_target = {};
    ls_target.QuadPart      = static_cast<LONGLONG>(iu_size);
    if(!SetFilePointerEx(mp_impl->mh_file, ls_target, nullptr, FILE_BEGIN))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        SetFilePointerEx(mp_impl->mh_file, ls_save, nullptr, FILE_BEGIN);
        return false;
    }

    if(!SetEndOfFile(mp_impl->mh_file))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
        SetFilePointerEx(mp_impl->mh_file, ls_save, nullptr, FILE_BEGIN);
        return false;
    }

    // Clamp the restored position to the new size.
    LARGE_INTEGER ls_restore = ls_save;
    if(ls_restore.QuadPart > ls_target.QuadPart)
    {
        ls_restore = ls_target;
    }
    SetFilePointerEx(mp_impl->mh_file, ls_restore, nullptr, FILE_BEGIN);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool c_file::preallocate(uint64_t iu_size)
{
    // set_size() already reserves logical space; on Windows SetEndOfFile
    // instructs the FS to allocate the underlying clusters. SetFileValidData
    // could avoid the zero-fill of newly-allocated blocks but it requires
    // SE_MANAGE_VOLUME_NAME privilege - try it, fall back silently.
    if(!set_size(iu_size))
    {
        return false;
    }

    LARGE_INTEGER ls_valid = {};
    ls_valid.QuadPart      = static_cast<LONGLONG>(iu_size);
    if(!SetFileValidData(mp_impl->mh_file, ls_valid.QuadPart))
    {
        DWORD ld_err = GetLastError();
        if(ERROR_PRIVILEGE_NOT_HELD != ld_err && ERROR_INVALID_FUNCTION != ld_err)
        {
            // Real error - report it but keep the reservation from set_size().
            mp_impl->mi_last_err = static_cast<int>(ld_err);
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// durability
bool c_file::sync()
{
    if(INVALID_HANDLE_VALUE == mp_impl->mh_file)
    {
        return false;
    }
    if(!FlushFileBuffers(mp_impl->mh_file))
    {
        mp_impl->mi_last_err = static_cast<int>(GetLastError());
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
