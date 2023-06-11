#pragma once

#include "page.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pmm;

/* Creates an empty physical page allocator.
 * Uses a single page to store top-level information. */
struct pmm* pmm_new (void* control_page);


/* Add a block of contiguous memory to the allocator.
 * Only regular memory (NOT MMIO) should be added */
void pmm_add (struct pmm* pmm, uint64_t physical_start, size_t size);


/* Request a physical 4K page from the allocator
 * Returns physical addr on success, 0 on failure */
uint64_t pmm_allocate_page (struct pmm* pmm);


/* Free a single page previously allocated by pmm_allocate_page */
void pmm_free_page (struct pmm* pmm, uint64_t physical);


/* Returns usage info for the allocator */
struct pmm_stat {
	size_t free;
	size_t total;
	size_t used;
};
struct pmm_stat pmm_count_pages (struct pmm* pmm);


/* Conversion from physical address to a valid virtual (r/w) address
 * Constant over kernel lifetime - only written at boot */
extern uint64_t global_hhdm_offset;
#define HHDM_POINTER(page) ((void*) ((char*)(page) + global_hhdm_offset))
#define HHDM_PHYSICAL(page) ((uint64_t)(page) - global_hhdm_offset)
