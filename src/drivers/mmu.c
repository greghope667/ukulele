#include "mmu.h"
#include "mmu_reg.h"
#include "page.h"
#include "panic.h"
#include "macros.h"

#include "memory/pmm.h"
#include "libk/kstring.h"
#include "libk/kstdio.h"

#define LOWER_HALF_MAX		0x0000800000000000
#define HIGHER_HALF_MIN		0xffff800000000000

static pmm_t global_mmu_pmm;

void
mmu_initialise (pmm_t pmm)
{
	global_mmu_pmm = pmm;
}

static struct mmu_page_map_part
get_current_page_map_top ()
{
	physical_t addr;
	asm ("mov\t%%cr3,%0" : "=r"(addr));
	return (struct mmu_page_map_part){addr, PAGE_MAP_DEPTH_TOP};
}

static const int depth_shift_size[] = {
	MMU_REG_VIRT_SHIFT_PML4,
	MMU_REG_VIRT_SHIFT_PML3,
	MMU_REG_VIRT_SHIFT_PML2,
	MMU_REG_VIRT_SHIFT_PML1,
};

static int
extract_page_map_index (struct mmu_page_map_part top, void* address)
{
	uintptr_t addr = (uintptr_t)address;
	int shift = depth_shift_size[top.depth];
	return (addr >> shift) & MMU_REG_VIRT_MASK;
}

static page_map_entry_t
convert_flags (enum mmu_flags flags)
{
	page_map_entry_t f = 0;
	if (flags & MEMORY_USER)
		f |= MMU_REG_USER;
	if (flags & MEMORY_CACHE_WRITE_THROUGH)
		f |= MMU_REG_WRITE_THROUGH;
	if (flags & MEMORY_WRITE)
		f |= MMU_REG_WRITE;
	if (! (flags & MEMORY_EXEC))
		f |= MMU_REG_NO_EXECUTE;
	return f;
}

const page_map_entry_t PM_PERMS = MMU_REG_WRITE |  MMU_REG_PRESENT;

static physical_t
allocate ()
{
	physical_t page = pmm_allocate_page (global_mmu_pmm);
	assert (page, "Failed to allocate page table");
	memset (HHDM_POINTER (page), 0, PAGE_SIZE);
	return page;
}

static void
assign_partial (struct mmu_page_map_part page_map,
		uintptr_t start,
		uintptr_t end,
		physical_t phys,
		page_map_entry_t flags)
{
	const int depth = page_map.depth;
	const int shift = depth_shift_size[depth];
	const uintptr_t base = ((start >> shift) & (~(uintptr_t)MMU_REG_VIRT_MASK)) << shift;

	const uintptr_t p2 = 1ULL << shift;
	const int start_idx = (ROUND_DOWN_P2 (start, p2) - base) >> shift;
	const int end_idx = (ROUND_UP_P2 (end, p2) - base) >> shift;

	for (size_t i=start_idx; i<end_idx; i++) {
		struct mmu_page_map_table* table = HHDM_POINTER (page_map.page);
		page_map_entry_t entry = table->entry[i];
		physical_t target = phys + ((i - start_idx) << shift);

		if (depth == PAGE_MAP_DEPTH_BOTTOM) {
			table->entry[i] = target | flags;
			continue;
		}

		if (entry & MMU_REG_PRESENT) {
			//page_map_entry_t next = entry & MMU_REG_PHYS_ADDRESS_MASK;
			//entry = next | flags;
			//table->entry[i] = entry;
		} else {
			page_map_entry_t next = allocate ();
			entry = next | PM_PERMS;
			table->entry[i] = entry;
		}

		uintptr_t blk_start = (i << shift) + base;
		uintptr_t blk_end = ((i+1) << shift) + base;
		blk_start = MAX (start, blk_start);
		blk_end = MIN (end, blk_end);

		struct mmu_page_map_part child = {
			.page = entry & MMU_REG_PHYS_ADDRESS_MASK,
			.depth = depth+1
		};

		assign_partial (child, blk_start, blk_end,
				target, flags);
	}
}

void
mmu_assign (
	struct mmu_page_map_part top,
	enum mmu_flags flags,
	void* address,
	size_t size,
	physical_t page)
{
	if (top.page == 0)
		top = get_current_page_map_top();

	require_page_aligned (top.page);
	require_page_aligned (address);
	require_page_aligned (size);
	require_page_aligned (page);
	assert (top.depth >= PAGE_MAP_DEPTH_TOP
		&& top.depth <= PAGE_MAP_DEPTH_BOTTOM,
		"Invalid page_map_depth");

	uintptr_t start = (uintptr_t)address;
	uintptr_t end = (uintptr_t)address + size;
	start = ROUND_DOWN_P2 (start, PAGE_SIZE);
	end = ROUND_UP_P2 (end, PAGE_SIZE);

	page_map_entry_t set_flags = convert_flags (flags) | MMU_REG_PRESENT;

	if (start < LOWER_HALF_MAX)
		assign_partial (top, start, MIN (end, LOWER_HALF_MAX), page, set_flags);
	if (end > HIGHER_HALF_MIN)
		assign_partial (top, MAX (start, HIGHER_HALF_MIN), end, page, set_flags);
}

static void
remove_full (struct mmu_page_map_part page_map)
{
	int depth = page_map.depth;
	struct mmu_page_map_table* table = HHDM_POINTER (page_map.page);

	for (int i=0; i<MMU_REG_PAGE_MAP_ENTRY_COUNT; i++) {
		page_map_entry_t entry = table->entry[i];

		if (!(entry & MMU_REG_PRESENT))
			continue;

		if (depth < PAGE_MAP_DEPTH_BOTTOM) {
			remove_full ((struct mmu_page_map_part) {
				.page=entry & MMU_REG_PHYS_ADDRESS_MASK,
				.depth=depth + 1,
			});
		}
	}

	pmm_free_page (global_mmu_pmm, page_map.page);
}

static bool
remove_partial (struct mmu_page_map_part page_map, uintptr_t start, uintptr_t end)
{
	const int depth = page_map.depth;
	const int shift = depth_shift_size[depth];
	const uintptr_t base = ((start >> shift) & (~(uintptr_t)MMU_REG_VIRT_MASK)) << shift;

	if (start == base
	    && end >= base + ((uintptr_t)MMU_REG_PAGE_MAP_ENTRY_COUNT << shift))
	{
		remove_full (page_map);
		return true;
	}

	const uintptr_t p2 = 1ULL << shift;
	const int start_idx = (ROUND_DOWN_P2 (start, p2) - base) >> shift;
	const int end_idx = (ROUND_UP_P2 (end, p2) - base) >> shift;

	for (size_t i=start_idx; i<end_idx; i++) {
		struct mmu_page_map_table* table = HHDM_POINTER (page_map.page);
		page_map_entry_t entry = table->entry[i];

		if (!(entry & MMU_REG_PRESENT))
			continue;

		if (depth == PAGE_MAP_DEPTH_BOTTOM) {
			table->entry[i] = 0;
			continue;
		}

		uintptr_t blk_start = (i << shift) + base;
		uintptr_t blk_end = ((i+1) << shift) + base;
		blk_start = MAX (start, blk_start);
		blk_end = MIN (end, blk_end);

		struct mmu_page_map_part child = {
			.page = entry & MMU_REG_PHYS_ADDRESS_MASK,
			.depth = depth+1
		};

		if (remove_partial (child, blk_start, blk_end))
			table->entry[i] = 0;
	}
	return false;
}

void
mmu_remove (
	struct mmu_page_map_part top,
	void* address,
	size_t size)
{
	if (size == 0)
		return;

	if (top.page == 0)
		top = get_current_page_map_top();

	require_page_aligned (top.page);
	require_page_aligned (address);
	require_page_aligned (size);
	assert (top.depth >= PAGE_MAP_DEPTH_TOP
		&& top.depth <= PAGE_MAP_DEPTH_BOTTOM,
		"Invalid page_map_depth");

	uintptr_t start = (uintptr_t)address;
	uintptr_t end = (uintptr_t)address + size;
	start = ROUND_DOWN_P2 (start, PAGE_SIZE);
	end = ROUND_UP_P2 (end, PAGE_SIZE);

	if (start < LOWER_HALF_MAX)
		remove_partial (top, start, MIN (end, LOWER_HALF_MAX));
	if (end > HIGHER_HALF_MIN)
		remove_partial (top, MAX (start, HIGHER_HALF_MIN), end);
}

struct mmu_page_map_part
mmu_lookup_step (struct mmu_page_map_part top, void* address)
{
	if (top.page == 0)
		top = get_current_page_map_top();

	require_page_aligned (top.page);
	assert (top.depth < PAGE_MAP_DEPTH_MEMORY, "In too deep");

	struct mmu_page_map_table* table = HHDM_POINTER (top.page);
	int idx = extract_page_map_index (top, address);
	page_map_entry_t entry = table->entry[idx];

	if (!(entry & MMU_REG_PRESENT))
		return (struct mmu_page_map_part){0, PAGE_MAP_DEPTH_INVALID};

	physical_t phys = entry & MMU_REG_PHYS_ADDRESS_MASK;

	if (entry & MMU_REG_PAGE_SIZE) {
		return (struct mmu_page_map_part){phys, PAGE_MAP_DEPTH_MEMORY};
	}

	return (struct mmu_page_map_part){phys, top.depth + 1};
}
