#pragma once

#include "types.h"

struct pmm; // fwd

/*
 * These are general memory mode flags, which get converted to
 * architecture specific flags internally.
 */
enum mmu_flags {
	MEMORY_USER = (1 << 0),
	MEMORY_EXEC = (1 << 1),
	MEMORY_WRITE = (1 << 2),
	MEMORY_CACHE_WRITE_THROUGH = (1 << 3),
};

/*
 * How deep through the page tables we are looking.
 * Depth 0 is top-level. For x86_64 this is the PML4 (possibly PML5).
 * 1 .. n are intermediate levels, largest value of n depends on the machine.
 *
 * 'memory' refers to an allocated page of ram, i.e. a real physical address.
 * 'invalid' (returned by functions) indicates an address is not mapped.
 */
enum mmu_page_map_depth {
	PAGE_MAP_DEPTH_TOP = 0,

	// Implementation specific number of entries (4 for x86_64)
	PAGE_MAP_DEPTH_0 = PAGE_MAP_DEPTH_TOP,
	PAGE_MAP_DEPTH_1,
	PAGE_MAP_DEPTH_2,
	PAGE_MAP_DEPTH_3,

	PAGE_MAP_DEPTH_BOTTOM = PAGE_MAP_DEPTH_3,

	PAGE_MAP_DEPTH_MEMORY,
	PAGE_MAP_DEPTH_INVALID,
};

/* A single part (one page) of the whole page map structure. */
struct mmu_page_map_part {
	physical_t page;
	enum mmu_page_map_depth depth;
};

/*
 * Using mmu_top_page as an argument means the current top-level page map
 * (for x86, read the value from CR3)
 */
static const struct mmu_page_map_part mmu_top_page = {};

/*
 * Set the memory pool from which we allocate pages to make the page map.
 *
 * We don't support using multiple seperate pools for seperate allocations
 * as that is really complex for little gain.
 */
void mmu_initialise (struct pmm* pmm);

void mmu_assign (struct mmu_page_map_part top,
		 enum mmu_flags flags,
		 void* address,
		 size_t size,
		 physical_t page);

void mmu_remove (struct mmu_page_map_part top,
		 void* address,
		 size_t size);

struct mmu_page_map_part mmu_lookup (
	struct mmu_page_map_part top,
	void* address
);
