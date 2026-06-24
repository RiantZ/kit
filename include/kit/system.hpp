#pragma once

#include <cstddef>
#include <cstdint>

namespace kit
{

/// Returns the current process ID.
uint32_t get_process_id();

/// Fills op_buf with the executable name (without path). Null-terminates.
/// @return true on success, false on failure (op_buf set to "unknown").
bool get_process_name(char *op_buf, size_t iz_buf);

/// Fills op_buf with the host name. Null-terminates.
/// @return true on success, false on failure (op_buf set to "unknown").
bool get_host_name(char *op_buf, size_t iz_buf);

} // namespace kit
