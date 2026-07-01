#pragma once

#include "kit/export.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace kit
{

////////////////////////////////////////////////////////////////////////////////
// Open disposition: exactly one value describing what should happen when the
// target file does or does not already exist. Kept separate from the additive
// flag set below so that mutually exclusive choices cannot be combined.
enum e_file_open_mode : uint32_t
{
    e_fom_open_existing = 0, // open existing, fail if missing
    e_fom_create_new,        // create, fail if already exists
    e_fom_create_always,     // create, truncate if it already exists
    e_fom_open_or_create,    // open if exists, create otherwise; do not truncate
    e_fom_truncate_existing  // open existing and truncate to zero; fail if missing
};

////////////////////////////////////////////////////////////////////////////////
// Additional bitwise-OR-able flags. Share flags are effective on Windows only;
// on POSIX targets they are accepted but have no effect.
//
// Notes on the more subtle flags:
//   * e_ff_no_buffering  - bypasses the OS page cache. Caller-provided buffers
//                          and file offsets must be aligned to the underlying
//                          sector size (typically 512 or 4096 bytes) or the OS
//                          will reject the request with EINVAL / ERROR_INVALID_PARAMETER.
//   * e_ff_write_through - forces every write to reach persistent media before
//                          returning. Cheaper than an explicit sync() after
//                          each write but still much slower than buffered I/O.
//   * hints              - e_ff_hint_sequential and e_ff_hint_random are
//                          mutually exclusive; passing both makes open() fail.
enum e_file_flag : uint32_t
{
    e_ff_read            = 1u << 0, // allow reads
    e_ff_write           = 1u << 1, // allow writes
    e_ff_append          = 1u << 2, // always write at end (atomic append on POSIX)

    e_ff_share_read      = 1u << 3, // Win: FILE_SHARE_READ
    e_ff_share_write     = 1u << 4, // Win: FILE_SHARE_WRITE
    e_ff_share_delete    = 1u << 5, // Win: FILE_SHARE_DELETE

    e_ff_hint_sequential = 1u << 6, // fadvise SEQUENTIAL / FILE_FLAG_SEQUENTIAL_SCAN / F_RDAHEAD on
    e_ff_hint_random     = 1u << 7, // fadvise RANDOM / FILE_FLAG_RANDOM_ACCESS / F_RDAHEAD off

    e_ff_no_buffering    = 1u << 8, // O_DIRECT / F_NOCACHE / FILE_FLAG_NO_BUFFERING
    e_ff_write_through   = 1u << 9  // O_SYNC / FILE_FLAG_WRITE_THROUGH
};

////////////////////////////////////////////////////////////////////////////////
// Buffer descriptor for scatter/gather I/O. Layout mirrors POSIX iovec so it
// can be forwarded to preadv/pwritev without copying on Linux/macOS.
struct s_iovec
{
    void  *mp_data;
    size_t mz_size;
};

////////////////////////////////////////////////////////////////////////////////
// Abstract file interface. Ref-counted lifetime plus I/O operations. Kept as
// a base class so that alternative backends (in-memory, network, archive)
// can be slotted in later without breaking existing consumers.
class KIT_API c_file_base
{
public:
    virtual ~c_file_base()                                                                               = default;

    // Reference counting. add_ref() returns the new count; release() returns
    // the new count as well and destroys the object when it reaches zero.
    virtual int32_t add_ref()                                                                            = 0;
    virtual int32_t release()                                                                            = 0;

    // Lifecycle.
    virtual bool is_open() const                                                                         = 0;
    virtual bool open(const std::filesystem::path &ir_path, e_file_open_mode ie_mode, uint32_t iu_flags) = 0;
    virtual bool close(bool ib_flush)                                                                    = 0;

    // Stream I/O. Uses the internal file position; not safe for parallel
    // callers on the same instance.
    virtual size_t read(void *op_buf, size_t iz_size)                                                    = 0;
    virtual size_t write(const void *ip_buf, size_t iz_size, bool ib_flush)                              = 0;

    // Positional I/O. Does not touch the internal position and is safe for
    // parallel readers on the same instance on POSIX. On Windows the handle's
    // implicit file pointer still advances, so mixing stream and positional
    // I/O on the same instance from different threads is a usage error.
    virtual size_t read_at(uint64_t iu_offset, void *op_buf, size_t iz_size)                             = 0;
    virtual size_t write_at(uint64_t iu_offset, const void *ip_buf, size_t iz_size)                      = 0;

    // Scatter/gather at the current position. On POSIX this is a single
    // readv/writev syscall; on Windows it is emulated with a per-buffer
    // ReadFile/WriteFile loop with identical observable semantics.
    virtual size_t read_v(const s_iovec *ip_iov, size_t iz_count)                                        = 0;
    virtual size_t write_v(const s_iovec *ip_iov, size_t iz_count, bool ib_flush)                        = 0;

    // Position / size.
    virtual bool     set_position(uint64_t iu_offset)                                                    = 0;
    virtual uint64_t get_position() const                                                                = 0;
    virtual uint64_t get_size() const                                                                    = 0;

    // Logical size / on-disk preallocation.
    virtual bool set_size(uint64_t iu_size)    = 0; // truncate or extend
    virtual bool preallocate(uint64_t iu_size) = 0; // reserve space on disk

    // Durability: fsync / FlushFileBuffers on the underlying descriptor.
    virtual bool sync()                        = 0;

    // Last errno / GetLastError observed by this instance. Reset to zero on
    // successful open().
    virtual int get_last_error() const         = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Concrete disk-backed implementation. All virtual overrides are declared
// here so that the class is complete at the point of use; their bodies live
// in the platform-specific translation units alongside the definition of
// s_impl.
class KIT_API c_file final : public c_file_base
{
public:
    struct s_impl;

    c_file();
    ~c_file() override;

    int32_t add_ref() override;
    int32_t release() override;

    bool is_open() const override;
    bool open(const std::filesystem::path &ir_path, e_file_open_mode ie_mode, uint32_t iu_flags) override;
    bool close(bool ib_flush) override;

    size_t read(void *op_buf, size_t iz_size) override;
    size_t write(const void *ip_buf, size_t iz_size, bool ib_flush) override;

    size_t read_at(uint64_t iu_offset, void *op_buf, size_t iz_size) override;
    size_t write_at(uint64_t iu_offset, const void *ip_buf, size_t iz_size) override;

    size_t read_v(const s_iovec *ip_iov, size_t iz_count) override;
    size_t write_v(const s_iovec *ip_iov, size_t iz_count, bool ib_flush) override;

    bool     set_position(uint64_t iu_offset) override;
    uint64_t get_position() const override;
    uint64_t get_size() const override;

    bool set_size(uint64_t iu_size) override;
    bool preallocate(uint64_t iu_size) override;

    bool sync() override;

    int get_last_error() const override;

private:
    c_file(const c_file &)            = delete;
    c_file &operator=(const c_file &) = delete;
    c_file(c_file &&)                 = delete;
    c_file &operator=(c_file &&)      = delete;

    s_impl *mp_impl;
};

} // namespace kit
