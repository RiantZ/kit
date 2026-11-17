#pragma once

#include <cstddef>
#include <cstdint>

#include "kit/types.h"

class c_shared
{
    struct s_shared;

public:
    typedef s_shared *h_shared;
    typedef void     *h_sem;

    enum e_lock
    {
        e_ok,
        e_timeout,
        e_error,
        e_not_exists
    };

    /// @brief Creates a named shared memory block with its named lock and copies ip_data into it.
    /// @param op_handle Receives the handle for get_name() and close(); nullptr on failure.
    /// @param ip_name The block name.
    /// @param ip_data The initial content, iu_size bytes.
    /// @param iu_size The block size in bytes, at least 1.
    /// @return true on success, false if the name is taken or the arguments are invalid.
    static bool create(h_shared *op_handle, const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size);

    /// @brief Copies the first iu_size bytes of a named block to op_data.
    /// @param ip_name The block name.
    /// @param op_data Receives iu_size bytes.
    /// @param iu_size The number of bytes to copy.
    /// @return true on success, false if the block is missing or smaller than iu_size.
    static bool read(const tXCHAR *ip_name, void *op_data, uint16_t iu_size);

    /// @brief Copies iu_size bytes from ip_data to the start of a named block.
    /// @param ip_name The block name.
    /// @param ip_data The bytes to copy.
    /// @param iu_size The number of bytes to copy.
    /// @return true on success, false if the block is missing or smaller than iu_size.
    static bool write(const tXCHAR *ip_name, const uint8_t *ip_data, uint16_t iu_size);

    /// @brief Takes the lock of a named block, waiting up to iu_timeout_ms.
    /// @param ip_name The block name.
    /// @param or_sem Receives the lock to pass to unlock(); nullptr unless e_ok is returned.
    /// @param iu_timeout_ms How long to wait, in milliseconds.
    /// @return e_ok, e_timeout, e_not_exists if no block has this name, e_error if ip_name is nullptr.
    static e_lock lock(const tXCHAR *ip_name, h_sem &or_sem, uint32_t iu_timeout_ms);

    /// @brief Releases a lock taken by lock().
    /// @param ior_sem The lock from lock(); set to nullptr.
    /// @return e_ok, e_not_exists if ior_sem is nullptr, e_error if the release fails.
    static e_lock unlock(h_sem &ior_sem);

    /// @brief Gives the name a block was created with.
    /// @param ih_shared The handle from create().
    /// @return The name, or nullptr if ih_shared is nullptr.
    static const tXCHAR *get_name(h_shared ih_shared);

    /// @brief Destroys a block made by create(): its memory, its lock and the handle.
    /// @param ih_shared The handle from create().
    /// @return true on success, false if ih_shared is nullptr.
    static bool close(h_shared ih_shared);

    /// @brief Removes the memory and lock of a named block whose creator did not close it.
    /// @param ip_name The block name.
    /// @return true if both were removed; always true on Windows, where they go with the last handle.
    static bool unlink(const tXCHAR *ip_name);
};
