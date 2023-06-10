#pragma once

#include "page.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pmm;

/* Creates a physical page allocator. Uses a single page
 * for storing top-level information */
struct pmm* pmm_initialise (void* control_page);

/* Add a block of contiguous memory to the allocator. */
void pmm_add (struct pmm* pmm, uint64_t physical_start, size_t size);

uint64_t pmm_allocate_page (struct pmm* pmm);
void pmm_free_page (struct pmm* pmm, uint64_t physical);

size_t pmm_count_free_pages (struct pmm* pmm);

extern uint64_t global_hhdm_offset;
