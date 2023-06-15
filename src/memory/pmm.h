#pragma once

#include "types.h"

struct pmm;
typedef struct pmm* pmm_t;

/* Creates an empty physical page allocator.
 * Uses a single page to store top-level information. */
pmm_t pmm_new (void* control_page);


/* Add a block of contiguous memory to the allocator.
 * Only regular memory (NOT MMIO) should be added */
void pmm_add (pmm_t, physical_t start, size_t size);


/* Request a physical 4K page from the allocator
 * Returns physical addr on success, 0 on failure */
physical_t pmm_allocate_page (pmm_t);


/* Free a single page previously allocated by pmm_allocate_page */
void pmm_free_page (pmm_t, physical_t phys);


/* Returns usage info for the allocator */
struct pmm_stat {
	size_t free;
	size_t total;
	size_t used;
	size_t overhead;
};
struct pmm_stat pmm_count_pages (pmm_t);


/* Conversion from physical address to a valid virtual (r/w) address
 * Constant over kernel lifetime - only written at boot */
extern uint64_t global_hhdm_offset;

#define HHDM_POINTER(page) ((void*) ((uintptr_t)(page) + global_hhdm_offset))
#define HHDM_PHYSICAL(page) ((uintptr_t)(page) - global_hhdm_offset)
