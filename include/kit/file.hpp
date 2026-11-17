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
    /// @brief Destroys the file object.
    virtual ~c_file_base()                                                                               = default;

    /// @brief Adds a reference.
    /// @return The new reference count.
    virtual int32_t add_ref()                                                                            = 0;

    /// @brief Drops a reference and destroys the object when the count reaches zero.
    /// @return The new reference count.
    virtual int32_t release()                                                                            = 0;

    // Lifecycle.

    /// @brief Tells whether the file is open.
    /// @return true if the file is open.
    virtual bool is_open() const                                                                         = 0;

    /// @brief Opens a file; success resets get_last_error() to 0.
    /// @param ir_path The file path.
    /// @param ie_mode What to do when the file exists or is missing.
    /// @param iu_flags A combination of e_file_flag values.
    /// @return true on success; on failure get_last_error() gives the reason.
    virtual bool open(const std::filesystem::path &ir_path, e_file_open_mode ie_mode, uint32_t iu_flags) = 0;

    /// @brief Closes the file; does nothing if it is not open.
    /// @param ib_flush true to sync the data to disk before closing.
    /// @return true on success, false if the sync or the close failed.
    virtual bool close(bool ib_flush)                                                                    = 0;

    // Stream I/O. Uses the internal file position; not safe for parallel
    // callers on the same instance.

    /// @brief Reads up to iz_size bytes at the current position and moves it forward.
    /// @param op_buf Receives the data.
    /// @param iz_size The number of bytes to read.
    /// @return The number of bytes read; fewer at the end of the file or on an error.
    virtual size_t read(void *op_buf, size_t iz_size)                                                    = 0;

    /// @brief Writes iz_size bytes at the current position and moves it forward.
    /// @param ip_buf The data to write.
    /// @param iz_size The number of bytes to write.
    /// @param ib_flush true to sync the data to disk after writing.
    /// @return The number of bytes written; fewer on an error.
    virtual size_t write(const void *ip_buf, size_t iz_size, bool ib_flush)                              = 0;

    // Positional I/O. Does not touch the internal position and is safe for
    // parallel readers on the same instance on POSIX. On Windows the handle's
    // implicit file pointer still advances, so mixing stream and positional
    // I/O on the same instance from different threads is a usage error.

    /// @brief Reads up to iz_size bytes at iu_offset without moving the current position.
    /// @param iu_offset The file offset to read from.
    /// @param op_buf Receives the data.
    /// @param iz_size The number of bytes to read.
    /// @return The number of bytes read; fewer at the end of the file or on an error.
    virtual size_t read_at(uint64_t iu_offset, void *op_buf, size_t iz_size)                             = 0;

    /// @brief Writes iz_size bytes at iu_offset without moving the current position.
    /// @param iu_offset The file offset to write at.
    /// @param ip_buf The data to write.
    /// @param iz_size The number of bytes to write.
    /// @return The number of bytes written; fewer on an error.
    virtual size_t write_at(uint64_t iu_offset, const void *ip_buf, size_t iz_size)                      = 0;

    // Scatter/gather at the current position. On POSIX this is a single
    // readv/writev syscall; on Windows it is emulated with a per-buffer
    // ReadFile/WriteFile loop with identical observable semantics.

    /// @brief Reads into several buffers, in order, at the current position and moves it forward.
    /// @param ip_iov The buffers.
    /// @param iz_count The number of buffers.
    /// @return The number of bytes read, 0 on an error.
    virtual size_t read_v(const s_iovec *ip_iov, size_t iz_count)                                        = 0;

    /// @brief Writes several buffers, in order, at the current position and moves it forward.
    /// @param ip_iov The buffers.
    /// @param iz_count The number of buffers.
    /// @param ib_flush true to sync the data to disk after writing.
    /// @return The number of bytes written, 0 on an error.
    virtual size_t write_v(const s_iovec *ip_iov, size_t iz_count, bool ib_flush)                        = 0;

    // Position / size.

    /// @brief Moves the current position.
    /// @param iu_offset The new position from the start of the file.
    /// @return true on success.
    virtual bool set_position(uint64_t iu_offset)                                                        = 0;

    /// @brief Reads the current position.
    /// @return The position from the start of the file, 0 on an error.
    virtual uint64_t get_position() const                                                                = 0;

    /// @brief Reads the file size.
    /// @return The size in bytes, 0 on an error.
    virtual uint64_t get_size() const                                                                    = 0;

    // Logical size / on-disk preallocation.

    /// @brief Truncates or extends the file to iu_size bytes.
    /// @param iu_size The new size in bytes.
    /// @return true on success.
    virtual bool set_size(uint64_t iu_size)                                                              = 0;

    /// @brief Reserves disk space for iu_size bytes and extends the file to that size.
    /// @param iu_size The size in bytes.
    /// @return true on success.
    virtual bool preallocate(uint64_t iu_size)                                                           = 0;

    /// @brief Syncs the file data to disk: fsync / FlushFileBuffers.
    /// @return true on success.
    virtual bool sync()                                                                                  = 0;

    /// @brief Reads the last errno / GetLastError seen by this object; a successful open() resets it to 0.
    /// @return The error code.
    virtual int get_last_error() const                                                                   = 0;
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

    /// @brief Creates a closed file object.
    c_file();

    /// @brief Closes the file without a sync; call close(true) or sync() first when the data must reach the disk.
    ~c_file() override;

    /// @brief Adds a reference.
    /// @return The new reference count.
    int32_t add_ref() override;

    /// @brief Drops a reference and destroys the object when the count reaches zero.
    /// @return The new reference count.
    int32_t release() override;

    /// @brief Tells whether the file is open.
    /// @return true if the file is open.
    bool is_open() const override;

    /// @brief Opens a file; success resets get_last_error() to 0.
    /// @param ir_path The file path.
    /// @param ie_mode What to do when the file exists or is missing.
    /// @param iu_flags A combination of e_file_flag values.
    /// @return true on success; on failure get_last_error() gives the reason.
    bool open(const std::filesystem::path &ir_path, e_file_open_mode ie_mode, uint32_t iu_flags) override;

    /// @brief Closes the file; does nothing if it is not open.
    /// @param ib_flush true to sync the data to disk before closing.
    /// @return true on success, false if the sync or the close failed.
    bool close(bool ib_flush) override;

    /// @brief Reads up to iz_size bytes at the current position and moves it forward.
    /// @param op_buf Receives the data.
    /// @param iz_size The number of bytes to read.
    /// @return The number of bytes read; fewer at the end of the file or on an error.
    size_t read(void *op_buf, size_t iz_size) override;

    /// @brief Writes iz_size bytes at the current position and moves it forward.
    /// @param ip_buf The data to write.
    /// @param iz_size The number of bytes to write.
    /// @param ib_flush true to sync the data to disk after writing.
    /// @return The number of bytes written; fewer on an error.
    size_t write(const void *ip_buf, size_t iz_size, bool ib_flush) override;

    /// @brief Reads up to iz_size bytes at iu_offset without moving the current position.
    /// @param iu_offset The file offset to read from.
    /// @param op_buf Receives the data.
    /// @param iz_size The number of bytes to read.
    /// @return The number of bytes read; fewer at the end of the file or on an error.
    size_t read_at(uint64_t iu_offset, void *op_buf, size_t iz_size) override;

    /// @brief Writes iz_size bytes at iu_offset without moving the current position.
    /// @param iu_offset The file offset to write at.
    /// @param ip_buf The data to write.
    /// @param iz_size The number of bytes to write.
    /// @return The number of bytes written; fewer on an error.
    size_t write_at(uint64_t iu_offset, const void *ip_buf, size_t iz_size) override;

    /// @brief Reads into several buffers, in order, at the current position and moves it forward.
    /// @param ip_iov The buffers.
    /// @param iz_count The number of buffers.
    /// @return The number of bytes read, 0 on an error.
    size_t read_v(const s_iovec *ip_iov, size_t iz_count) override;

    /// @brief Writes several buffers, in order, at the current position and moves it forward.
    /// @param ip_iov The buffers.
    /// @param iz_count The number of buffers.
    /// @param ib_flush true to sync the data to disk after writing.
    /// @return The number of bytes written, 0 on an error.
    size_t write_v(const s_iovec *ip_iov, size_t iz_count, bool ib_flush) override;

    /// @brief Moves the current position.
    /// @param iu_offset The new position from the start of the file.
    /// @return true on success.
    bool set_position(uint64_t iu_offset) override;

    /// @brief Reads the current position.
    /// @return The position from the start of the file, 0 on an error.
    uint64_t get_position() const override;

    /// @brief Reads the file size.
    /// @return The size in bytes, 0 on an error.
    uint64_t get_size() const override;

    /// @brief Truncates or extends the file to iu_size bytes.
    /// @param iu_size The new size in bytes.
    /// @return true on success.
    bool set_size(uint64_t iu_size) override;

    /// @brief Reserves disk space for iu_size bytes and extends the file to that size.
    /// @param iu_size The size in bytes.
    /// @return true on success.
    bool preallocate(uint64_t iu_size) override;

    /// @brief Syncs the file data to disk: fsync / FlushFileBuffers.
    /// @return true on success.
    bool sync() override;

    /// @brief Reads the last errno / GetLastError seen by this object; a successful open() resets it to 0.
    /// @return The error code.
    int get_last_error() const override;

private:
    c_file(const c_file &)            = delete;
    c_file &operator=(const c_file &) = delete;
    c_file(c_file &&)                 = delete;
    c_file &operator=(c_file &&)      = delete;

    s_impl *mp_impl;
};

} // namespace kit
