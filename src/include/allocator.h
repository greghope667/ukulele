#pragma once
/*
 * Allocator interface - API for type-erased allocators.
 *
 * All allocation functions take an explicit allocator - we have no global
 * malloc like in standard C. This makes global state modification/usage more
 * explicit in the kernel.
 *
 * The other major change from standard C is that memory allocations use blocks
 * of (ptr, size). This enables the allocators to be a bit easier to code fancy
 * custom allocation strategies. Let's see how this goes, might be more trouble
 * than it's worth.
 *
 *
 * Usage notes:
 *  - The wrapper functions MUST be used, do not directly call through the
 *    vtable.
 *
 *  - Zero-size allocations are always null pointers, kalloc(0) returns NULL
 *
 *  - Alignment/size restrictions are per-allocator requirements.
 *
 * Implementing allocators:
 *  - realloc does not need to be implemented, a fall back strategy of alloc +
 *    memcpy is provided if vtbl->realloc == NULL. Only implement this if
 *    reallocation can be done efficiently (e.g. virtual address reassignment).
 *
 *  - Arenas can set vtbl->free == NULL so frees never occur
 *
 *  - 'Edge cases' are already handled by wrappers sensibly:
 * 	malloc(0), free(0), realloc(0, sz), realloc(p, 0)
 */

#include <stddef.h>

/* Allocator Usage */
struct blk {
	void* ptr;
	size_t size;
};

struct allocator;
typedef struct allocator allocator_t;

struct blk kalloc   (allocator_t, size_t size);
struct blk krealloc (allocator_t, struct blk mem, size_t size);
void       kfree    (allocator_t, struct blk mem);

/* Allocator Implementation */
struct allocator_vtbl {
	struct blk (*alloc)   (void* self, size_t);
	struct blk (*realloc) (void* self, struct blk, size_t);
	void       (*free)    (void* self, struct blk);
	void       (*del)     (void* self);
};

typedef struct allocator {
	void* self;
	const struct allocator_vtbl* vtbl;
} allocator_t;
