# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [0.2.0] - 2026-06-24

### Added
- `thread.hpp` - cross-platform thread priority API (`kit::get_thread_priority` / `kit::set_thread_priority`) with abstract `e_thread_priority` levels; platform implementations (Linux, macOS, Windows)
- `thread.hpp` exposed via the `kit/kit.h` aggregator header
- Test suite for thread priority (`tests/thread_test.hpp`)

### Changed
- `shared` module internals renamed to follow `doc/code_style.md` conventions; public function signatures unchanged
- Minor naming fixes in `types.h` (`pstr_dup` / `pstr_free_dup` parameters) and `list.hpp` (template parameter)

## [0.1.0] - 2026-04-09

### Added
- Initial project structure with CMake 4.0+
- `ts_helpers.h` - cross-platform macros and type definitions
- `types.h` - common type aliases (tBOOL, tUINT8, tUINT16, tUINT32, tINT64, tXCHAR)
- `list.hpp` - doubly-linked list with memory pool allocator (`kit::c_lst`)
- `shared.h` - shared memory module with platform implementations (Linux, Windows)
- `export.h` - KIT_API visibility macro
- `version.h` - compile-time and runtime version info
- Google Test integration via FetchContent
- Full test suite for `kit::c_lst`
