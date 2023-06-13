#include "allocator.h"
#include "macros.h"
#include "panic.h"
#include "kstring.h"

static struct blk
doalloc (allocator_t alloc, size_t size)
{
	return NOT_NULL (alloc.vtbl->alloc) ((void*)alloc.self, size);
}

static void
dofree (allocator_t alloc, struct blk blk)
{
	if (alloc.vtbl->free)
		alloc.vtbl->free (alloc.self, blk);
}


struct blk
kalloc (allocator_t alloc, size_t size)
{
	if (size == 0)
		return (struct blk) {};

	return doalloc (alloc, size);
}

struct blk
krealloc (allocator_t alloc, struct blk mem, size_t size)
{
	if (size == 0) {
		kfree (alloc, mem);
		return (struct blk) {};
	}

	if (mem.ptr == 0)
		return doalloc (alloc, size);

	if (alloc.vtbl->realloc)
		return alloc.vtbl->realloc (alloc.self, mem, size);

	struct blk blk = doalloc (alloc, size);
	if (blk.ptr == 0)
		return (struct blk) {};

	memcpy (blk.ptr, mem.ptr, mem.size);
	dofree (alloc, mem);

	return blk;
}

void
kfree (allocator_t alloc, struct blk mem)
{
	if (mem.ptr)
		dofree (alloc, mem);
}
