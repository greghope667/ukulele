#pragma once

#include "page.h"
#include <stdint.h>

typedef uint64_t page_map_entry_t;

#define MMU_REG_PRESENT					0x01ULL  // P
#define MMU_REG_WRITE					0x02ULL  // R/W
#define MMU_REG_USER					0x04ULL  // U/S
#define MMU_REG_WRITE_THROUGH			0x08ULL  // PWT
#define MMU_REG_CACHE_DISABLE			0x10ULL  // PCD
#define MMU_REG_ACCESSED				0x20ULL  // A
#define MMU_REG_DIRTY					0x40ULL  // D
#define MMU_REG_PAGE_SIZE				0x80ULL  // PS
#define MMU_REG_NO_EXECUTE			(1ULL << 63) // XD

// PS = 1
#define MMU_REG_PHYS_ADDRESS_MASK	0x000ffffffffff000ULL

// PS = 0
#define MMU_REG_VIRT_MASK				0777
#define MMU_REG_VIRT_SHIFT_PML4			39
#define MMU_REG_VIRT_SHIFT_PML3			30 // directory pointer
#define MMU_REG_VIRT_SHIFT_PML2			21 // directory
#define MMU_REG_VIRT_SHIFT_PML1			12 // table

#define MMU_REG_PAGE_MAP_ENTRY_COUNT	512

struct page_map_table {
	page_map_entry_t entry[MMU_REG_PAGE_MAP_ENTRY_COUNT];
} PAGE_ALIGNED;

REQUIRE_PAGE_SIZED(struct page_map_table)

enum mmu_page_map_level {
	MMU_PML4,
	MMU_PML3,
	MMU_PML2,
	MMU_PML1,
};
