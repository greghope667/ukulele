#pragma once
/*
 * Any general macros used commonly can go here
 */

/* Panic if pointer is null. Can be used as an expression: x = NOT_NULL(p)->x */
#define NOT_NULL(p)	\
	((p) ?: (panic("Pointer must never be null"), (typeof(p))NULL))

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof(arr[0]))

#define ROUND_UP(val, mult) ((((val) + (mult) - 1) / (mult)) * (mult))
#define ROUND_DOWN(val, mult) (((val) / (mult)) * (mult))

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) < (y) ? (y) : (x))

#define DELETE_IFACE(p) ((p).vtbl->del((p).self))
