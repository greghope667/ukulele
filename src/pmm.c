#include "pmm.h"
#include "kstdio.h"
#include "page.h"
#include "panic.h"
#include <string.h>

/*
 * Physical memory manager / allocator
 *
 * Usage:
 * 	- create a struct pmm using statically allocated storage
 * 	- add physical, ram-backed memory ranges to pmm
 * 	- allocate + free physical pages
 * 	- freeing is a no-op; just overrite by calling new() again
 *
 * Structures (each box = 1 page):
 *
 *    pmm
 *  +=====+
 *  |     |             +==========+====+====+====+====
 *  |  0  | -- ctrl --> | ctrl_blk | usable pages ...
 *  |     |             +==========+====+====+====+====
 *  |     |             +==========+====+====+====+====
 *  |  1  | -- ctrl --> | ctrl_blk | usable pages ...
 *  |     |             +==========+====+====+====+====
 *  | ... |
 *  +=====+
 *
 * ctrl_blk is a bitset of free pages
 */

/* Below this many pages, skip actually adding block to allocator.
 * Save resources for later blocks which may be larger */
#define MIN_BLOCK_SIZE_PAGES 		64

#define PMM_ENTRIES					168
#define PMM_CTRL_ENTRIES			512

#define MAX_BLOCK_SIZE_PAGES 	(PMM_CTRL_ENTRIES * 64)
#define MAX_BLOCK_SIZE_BYTES 	(MAX_BLOCK_SIZE_PAGES * PAGE_SIZE)
#define MIN_BLOCK_SIZE_BYTES 	(MIN_BLOCK_SIZE_PAGES * PAGE_SIZE)

/* Manages a contiguous block of physical memory
 * 4KiB control block, each set bit represents a free page,
 * so we have 4K * 8 = 32K pages, 4K * 32K = 128M bytes */

struct pmm_control_block {
	uint64_t entry[PMM_CTRL_ENTRIES];
} PAGE_ALIGNED;

struct pmm_ctrl_ptr {
	uint64_t physical_start;
	struct pmm_control_block* ctrl;
	uint16_t max_pages;
	uint16_t free_pages;
	bool active;
};

/* Manages 168 control blocks, 168 * 128M = 21 G max */
struct pmm {
	struct pmm_ctrl_ptr entry[PMM_ENTRIES];
} PAGE_ALIGNED;

REQUIRE_PAGE_SIZED(struct pmm_control_block)
REQUIRE_PAGE_SIZED(struct pmm)


struct pmm*
pmm_new (void* control_page)
{
	require_page_aligned (control_page);
	return memset (control_page, 0, PAGE_SIZE);
}

static void
pmm_ctrl_initialise (struct pmm_control_block* ctrl, uint16_t pages)
{
	require_page_aligned (ctrl);
	memset (ctrl, 0, PAGE_SIZE);

	int full_entries = pages / 64;
	int extra_bits = pages % 64;

	for ( int i=0; i<full_entries; i++ )
		ctrl->entry[i] = (uint64_t)-1;

	if (extra_bits)
		ctrl->entry[full_entries] = ((uint64_t)-1) >> (64 - extra_bits);
}

static void
pmm_setup_entry (struct pmm_ctrl_ptr* p, uint64_t physical_start, size_t size)
{
	// First page becomes control block
	struct pmm_control_block* ctrl = HHDM_POINTER (physical_start);
	physical_start += PAGE_SIZE;
	size -= PAGE_SIZE;

	size_t pages = size / PAGE_SIZE;
	pmm_ctrl_initialise (ctrl, pages);

	*p = (struct pmm_ctrl_ptr) {
		.ctrl = ctrl,
		.physical_start = physical_start,
		.max_pages = pages,
		.free_pages = pages,
		.active = true,
	};
}

void
pmm_add (struct pmm* pmm, uint64_t physical_start, size_t size)
{
	require_page_aligned (physical_start);

	size = (size / PAGE_SIZE) * PAGE_SIZE;

	size_t remaining;
	if (size > MAX_BLOCK_SIZE_BYTES) {
		// Split block
		remaining = size - MAX_BLOCK_SIZE_BYTES;
		size = MAX_BLOCK_SIZE_BYTES;
	} else if (size < MIN_BLOCK_SIZE_BYTES) {
		eprintf ("Warning: Memory block %zx+%zx not allocated (%s)\n",
				 physical_start, size, "too small");
		return;
	} else {
		remaining = 0;
	}

	for ( int i=0; i<PMM_ENTRIES; i++ ) {
		if (!pmm->entry[i].active) {
			pmm_setup_entry (&pmm->entry[i], physical_start, size);

			if (remaining) // Tail recurse
				return pmm_add (pmm, physical_start + size, remaining);

			return;
		}
	}

	eprintf ("Warning: Memory block %zx+%zx not allocated (%s)\n",
			 physical_start, size, "out of resources");
	return;
}

static uint16_t
pmm_ctrl_alloc (struct pmm_control_block* blk)
{
	for ( int i=0; i<PMM_CTRL_ENTRIES; i++ ) {
		if (blk->entry[i] == 0)
			continue;

		int bit = __builtin_ctzl (blk->entry[i]);
		blk->entry[i] &= ~(1ULL << bit);
		return 64 * i + bit;
	}

	panic ("%s:%i Block %p should have space\n", __FILE__, __LINE__, blk);
}

uint64_t
pmm_allocate_page (struct pmm* pmm)
{
	for ( int i=0; i<PMM_ENTRIES; i++ ) {
		struct pmm_ctrl_ptr* p = &pmm->entry[i];
		if (p->active && p->free_pages) {
			int idx = pmm_ctrl_alloc (p->ctrl);
			p->free_pages--;
			return p->physical_start + PAGE_SIZE * idx;
		}
	}

	eprintf ("Warning: Physical allocation failure %p\n", pmm);
	return 0;
}

void
pmm_free_page (struct pmm* pmm, uint64_t physical)
{
	if (physical == 0)
		return;

	for ( int i=0; i<PMM_ENTRIES; i++ ) {
		struct pmm_ctrl_ptr* p = &pmm->entry[i];
		if (p->active && (physical >= p->physical_start)
			&& (physical < (p->physical_start + PAGE_SIZE * p->max_pages)))
		{
			int pageindex = (physical - p->physical_start) / PAGE_SIZE;
			int entry = pageindex / 64;
			int bit = pageindex % 64;
			p->ctrl->entry[entry] |= (1ULL << bit);
			p->free_pages++;

			return;
		}
	}

	panic ("%s:%i %p Bad free (block %zx)\n",
		   __FILE__, __LINE__, pmm, physical);
}

struct pmm_stat
pmm_count_pages (struct pmm* pmm)
{
	struct pmm_stat stat = {};

	for ( int i=0; i<PMM_ENTRIES; i++ ) {
		struct pmm_ctrl_ptr* p = &pmm->entry[i];
		if (p->active) {
			stat.free += p->free_pages;
			stat.used += (p->max_pages - p->free_pages);
			stat.total += p->max_pages;
		}
	}

	return stat;
}
