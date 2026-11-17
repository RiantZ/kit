#pragma once

#include <cstddef>
#include <cstdint>

namespace kit
{

/// @brief Reads the ID of the current process.
/// @return The process ID.
uint32_t get_process_id();

/// @brief Writes the executable name, without its path, to op_buf and null-terminates it.
/// @param op_buf Receives the name.
/// @param iz_buf The size of op_buf in bytes.
/// @return true on success, false on failure (op_buf set to "unknown").
bool get_process_name(char *op_buf, size_t iz_buf);

/// @brief Writes the host name to op_buf and null-terminates it.
/// @param op_buf Receives the name.
/// @param iz_buf The size of op_buf in bytes.
/// @return true on success, false on failure (op_buf set to "unknown").
bool get_host_name(char *op_buf, size_t iz_buf);

} // namespace kit
