# Introduction
The document gives base introduction into code style.
File `.clang-format` at project root helps in formatting.

# Motivation
_"Indeed, the ratio of time spent reading versus writing is well over 10 to 1. We are constantly reading old code as part of the effort to write new code. …[Therefore,] making it easy to read makes it easier to write."_
(c) Robert C. Martin, Clean Code: A Handbook of Agile Software Craftsmanship

* Objective #1: Code must be easy to read
* Objective #2: Code must be acceptably comfortable to write
* Objective #3: consistent rules

# Code style
## General
 * all names must be in **lower case** letters
 * words & prefixes delimiter is **_**
 * No camelCase, no PascalCase

Different pattern were compared from *Objective #1* standpoint of view to choose the most appropriate:
  1) *myFileForSomething.cpp* << **hard** to read, **hard** to remember what is lowercase, what is caps, but slightly **shorter**
  2) *myfileforsomething.cpp* << **even harder** to read
  3) *myFile_for_Something.cpp* << mix of styles, more **difficult to follow and remember**
  4) *my_file_for_something.hpp* << **easy** to read, **easy** to remember the style, but slightly **longer**

## File names
 Example:
  * `my_file_for_something.cpp`
  * `my_file_for_something.hpp`
  * `my_file_for_something.h`

Never `MyFile.cpp`, never `myFile.cpp`, never `my-file.cpp`.

## Prefixes
Each variable or type **must** have prefix, separated from name with standard delimiter **_**, see chapter [Examples](#examples) below.

### Types
Each declared type **must** have one of the prefixes:

| Prefix | Meaning              | Example                   |
|--------|----------------------|---------------------------|
| `i`    | signed integer       | `typedef int32_t i_count;`|
| `u`    | unsigned integer     | `uint32_t mu_val;`        |
| `x`    | utf char             | `x_char ix_letter;`       |
| `z`    | size_t               | `size_t lz_len;`          |
| `b`    | bool                 | `bool ib_flag;`           |
| `c`    | class                | `class c_my_class`        |
| `d`    | double               | `double md_value;`        |
| `f`    | float                | `float lf_ratio;`         |
| `p`    | pointer              | `void *mp_data;`          |
| `r`    | reference            | `c_obj &or_out;`          |
| `v`    | r-value reference    | `c_obj &&iv_tmp;`         |
| `o`    | object (instance)    | `c_my_class lo_inst;`     |
| `s`    | structure            | `struct s_my_struct`      |
| `e`    | enum type / values   | `enum class e_state { e_idle, e_run };` |
| `t`    | template             | `template<typename t_x>`  |
| `l`    | function ptr / lambda| `typedef int (*l_cb)(int);`|
| `h`    | handle / custom type | `typedef int32_t h_p8_id;`|

### Variables
Variable prefix must consist of access rights/visibility, one of the following:

* Function parameters:
   * `i` - input
   * `io` - input & output
   * `o` - output (input value is ignored)
* Scoped:
   * `g` - global / file-static
   * `l` - local
* Members:
  * `m` - member of class or structure

and followed by type from chapter above, see chapter [Examples](#examples) below.

Examples:
- `ip_ctx`     — input pointer
- `iou_count`  — in/out unsigned
- `op_result`  — output pointer
- `gs_config`  — global struct instance
- `lz_size`    — local size_t
- `mp_data`    — member pointer
- `ml_timer_value` — member function-pointer/lambda
- `mu_val1`    — member unsigned

#### `o` Disambiguation

> Note: `o` as type prefix (object) always appears **second** in a compound prefix; `o` as access prefix (output) always appears **first**. Example: `lo_data` = local object, `oc_data` = output class.

## Functions
 * C++ class method: starts with a verb in lowercase snake_case — `do_something`, `get_size`, `set_name`, `is_ready`
 * C functions: first word (prefix) is module name — `p8_register_current_thread`, `p8_mtk_emit`
 * Constructors/destructors follow class name (`c_my_class::c_my_class(...)`)

## Preprocessor defines
 Must use all capitals. **The only exception** to the lowercase rule.

```cpp
#define P8_MAX_CHANNELS 64
#define MY_DEFINE       0
```

# Examples
```cpp
#include "my_header_file.h"

#define MY_DEFINE 0

enum class e_my_enum
{
    e_val_1,
    e_val_2,
    e_vals_count
};

struct s_my_struct
{
    uint32_t mu_val1;
    int32_t  mi_val2;
};

// global variable
static const s_my_struct gs_my_global_val = {0, 0};

class c_my_class
{
    void *mp_data = nullptr;
public:
    explicit c_my_class(uint32_t iu_init_var);
    bool do_process(const s_my_struct *ip_in, s_my_struct *op_out);
};

void my_function(c_my_class *&or_object)
{
    or_object = new c_my_class(10);
}
```
