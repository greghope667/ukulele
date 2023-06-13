# Coding Style/guidelines

Here's some coding ideas I try to stick with for this project.
In general, try to stick with Linux kernel style.

Remember, these are more like _guidelines_.

## Typedefs

Structs where direct access is expected - no typedef.
Structs with no (or very rare) direct access - use typedef, _t suffix.

Typedefs should be for the 'standard' usage. If a type should be used by-value,
the typedef the value. Else, typedef the pointer. Taking a `type_t*` parameter
is unusual:

```c
struct by_value;
struct by_reference;

// Typical usage
typedef struct by_value by_value_t;
typedef struct by_reference by_reference_t;
void f (by_value_t v);
void g (by_reference_t v);

// Unusual!
void h (by_value_t* v);
void i (struct by_reference v);
void j (typeof(*(by_reference_t)0) v); // Please don't do this
```

## Function names

Some functions are copied from libc. These should have, as much as possible,
be standard c compliant. For changed behaviour, consider a different name
(e.g. our malloc is not standard, so we use kmalloc instead).

Exported function names should be prefixed with the file or module they are in.

Standard (ish) name recommendations:

 - `size` should be in bytes, prefer `length`/`count` for number of elements

 - For initialising an object from a known buffer, use `<type>_initialise`

 - Object creation/destruction with dynamic memory should look like
   `<type>_new (allocator,...)` and `<type>_delete (...)`

 - Predecate functions (returning bool) can be `_is_predecate`

## Assembly

While sometimes necessary, usage of assembly should be kept minimal. I'd like
for this to run on both x86 and arm without trouble. Inline assembly should be
put in seperate architecture-dependent functions. Architecture dependent files
should be suffixed with `_x86.c`/`_arm.c`.

Where possible, provide fall-back C only implementations with weak linkage that
are overriden with optimised defaults (e.g. memset). Prefer GCC/Clang builtins
for where available.

## Integers

Use fixed sized integers for structures where size/alignment is important.
Prefer unsized `int` when specific size is not a hard requirement.
Raw bytes can be `char` (or sometimes `void*` for pointers), `CHAR_BIT==8` is
assumed. (Side note: Many projects claim to be portable and independent of byte
size. This is very rarely true - hidden assumptions always sneak into the code.
Bugs almost never get found as almost nobody actually uses anything other than
8-bit bytes. I'd rather not pretend to support something that I can't reasonably
test/run)

## Misc

Try to avoid name collisions with C++ keywords. While the code is currently
only C, we might want to use a bit of C++ at some point.
