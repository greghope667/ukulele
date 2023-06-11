#include "page.h"
#include "vaddress_space.h"
#include "panic.h"

#include "pmm.h"
#include <string.h>
#include "kstdio.h"

#define REGIONS_PER_BLOCK 100

/* vaddress_space manages a virtual address space
 * Uses a linked list of address_region for each used region of memory
 *
 * Storage for linked_list backed by storage_blocks
 */

struct address_region {
	struct address_region* next;
	//struct address_region* prev;
	void* begin;
	void* end;
};

struct storage_block {
	struct storage_block* next;
	int entries;
	struct address_region entry[REGIONS_PER_BLOCK];
} PAGE_ALIGNED;

REQUIRE_PAGE_SIZED(struct storage_block);

struct vaddress_space {
	struct storage_block* storage;
	struct pmm* pmm;

	struct address_region* allocated;
	struct address_region* unused;

	void* space_begin;
	void* space_end;
};

inline static void*
new_page (struct pmm* pmm)
{
	uint64_t phys = pmm_allocate_page (pmm);
	assert (phys, "Vaddress allocation failed");
	return HHDM_POINTER (phys);
}

struct vaddress_space*
vaddress_space_new (struct pmm* pmm, void* begin, void* end)
{
	// Yes, this wastes a page of ram.
	// We don't yet have a sup-page allocator :(
	struct vaddress_space* vaddr = new_page (pmm);
	*vaddr = (struct vaddress_space) {
		.pmm = pmm,
		.space_begin = begin,
		.space_end = end,
	};

	return vaddr;
}

static struct address_region*
find_free_node (struct vaddress_space* vaddr)
{
	struct address_region* node = vaddr->unused;
	struct storage_block* blk = vaddr->storage;

	if (node) {
		// Pop from free list
		vaddr->unused = node->next;
		goto found;
	}

	for ( ; blk; blk = blk->next ) {
		// Find block with unused entries
		if (blk->entries < REGIONS_PER_BLOCK) {
			node = &blk->entry[blk->entries];
			blk->entries++;
			goto found;
		}
	}

	// No space, allocate
	blk = new_page (vaddr->pmm);

	blk->next = vaddr->storage;
	vaddr->storage = blk;

	blk->entries = 1;
	node = &blk->entry[0];
found:
	return memset (node, 0, sizeof(*node));
}

void*
vaddress_allocate (struct vaddress_space* vaddr, size_t size)
{
	struct address_region* next = vaddr->allocated;

	void* free_begin = vaddr->space_begin;
	void* free_end = next ? next->begin : vaddr->space_end;
	size_t space = free_end - free_begin;

	if ( space >= size ) {
		struct address_region* region = find_free_node (vaddr);
		// Push head
		region->begin = free_begin;
		region->end = free_begin + size;
		region->next = vaddr->allocated;
		vaddr->allocated = region;
		return free_begin;
	}

	struct address_region* allocated = vaddr->allocated;

	while ( allocated ) {
		next = allocated->next;
		free_begin = allocated->end;
		free_end = next ? next->begin : vaddr->space_end;
		space = free_end - free_begin;
		if ( space >= size ) {
			// Extend
			allocated->end += size;
			return free_begin;
		}
		allocated = next;
	}

	return NULL;
}

void
vaddress_free (struct vaddress_space* vaddr, void* address, size_t size)
{
	// This is a mess...
	struct address_region** prev = &vaddr->allocated;
	struct address_region* allocated = *prev;
	void* address_end = address + size;

	while ( allocated ) {
		struct address_region* next = allocated->next;

		if (address_end < allocated->begin) {
			return;
		}

		else if (allocated->end < address) {
			prev = &allocated->next;
		}

		else if (address <= allocated->begin && allocated->end <= address_end) {
			// Delete whole region
			*prev = next;
			allocated->next = vaddr->unused;
			vaddr->unused = allocated;
		}

		else if (address <= allocated->begin) {
			// Shift block start
			allocated->end = address_end;
			prev = &allocated->next;
		}

		else if (allocated->end <= address_end) {
			// Shift block end
			allocated->end = address;
			prev = &allocated->next;
		}

		else {
			// Split block
			struct address_region* region = find_free_node (vaddr);
			region->end = allocated->end;
			region->begin = address_end;
			region->next = next;
			allocated->end = address;
			allocated->next = region;

			prev = &region->next;
		}

		allocated = next;
	}
}


void
vaddress_print (struct vaddress_space* vaddr)
{
	struct address_region* region;
	struct storage_block* blk;

	printf ("vaddr %p-%p\n", vaddr->space_begin, vaddr->space_end);

	int regions = 0;
	int blocks = 0;
	int regions_used = 0;
	int regions_unused = 0;

	for ( blk = vaddr->storage; blk; blk = blk->next ) {
		blocks++;
		regions += blk->entries;
	}

	printf ("\tstorage blocks: %i nodes: %i\n", blocks, regions);

	for ( region = vaddr->unused; region; region = region->next) {
		regions_unused++;
	}

	for ( region = vaddr->allocated; region; region = region->next) {
		regions_used++;
		printf ("\t%p-%p\n", region->begin, region->end);
	}

	printf ("\tnodes used: %i unused: %i\n", regions_used, regions_unused);

	assert (regions == regions_used + regions_unused, "Region lost from vaddr!");
}
