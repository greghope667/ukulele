#pragma once
/*
 * A basic arena allocator, allocating directly from the physical memory
 * manager, returning HHDM addresses. To be used early, before we have a better
 * allocator set up
 */

#include "allocator.h"

struct pmm; // fwd

allocator_t make_arena_pmm_allocator (struct pmm* pmm, int p2align);

