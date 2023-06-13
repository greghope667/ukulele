#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Machine assumptions for project
_Static_assert(__CHAR_BIT__ == 8, "");
_Static_assert(sizeof(void*) == 8, "");
_Static_assert(__LITTLE_ENDIAN__, "");

typedef intptr_t ssize_t;

/*
 * Physical address space value. We don't use void* here as these are not valid
 * pointers. Must be converted to virtual addresses before dereferencing
 */
typedef uintptr_t physical_t;
