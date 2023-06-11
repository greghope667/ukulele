#pragma once

#include "allocator.h"

struct pmm;

struct kernel_heap {
	struct pmm* pmm;
};

struct allocator heap_allocator (struct kernel_heap* storage);
