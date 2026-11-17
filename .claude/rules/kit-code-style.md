---
paths:
  - "**/*.cpp"
  - "**/*.c"
  - "**/*.h"
  - "**/*.hpp"
---

# kit code style

`doc/code_style.md` is the single source of truth: read it before writing or reviewing C/C++ code here and follow it literally.

Rules that are easy to miss:
- Every identifier is lowercase snake_case; every type and variable carries its prefix (`c_`, `s_`, `e_`, `ip_`, `iz_`, `lz_`, `mu_`, ...).
- Comments are short and plain: one line by default, two at most; they say why, not how.
- Functions and methods in headers are documented with `///` Doxygen tags: `@brief` in one line, `@param <name>` for every parameter, `@return` unless `void`. See "Doc comments" in `doc/code_style.md`.
- When nearby files use different styles, follow `doc/code_style.md` and tell the user about the mismatch instead of copying the closest file.
- Format with the repo `.clang-format`; check with `python3 scripts/code_format.py --check --all`.
