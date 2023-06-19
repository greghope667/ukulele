#include "mmu.h"
#include "mmu_reg.h"
#include "page.h"
#include "panic.h"
#include "macros.h"

#include "memory/pmm.h"
#include "libk/kstring.h"
#include "libk/kstdio.h"

/*
 * MMU driver implementation
 *
 * This main bulk of this file is split into two halves.
 * The top half is code for iterating through the parts of the page tables.
 * We use a callback based api which makes extending new features much easier.
 * There's one main function `apply_nodes` that does most of the iteration, and
 * various node_callback_ and leaf_callback_ functions that modify the tree.
 *
 * The bottom half of this file is the public api. Most functions here should
 * just call `apply_nodes_entry` with appropriate callbacks in place.
 */

static pmm_t global_mmu_pmm;

void
mmu_initialise (pmm_t pmm)
{
	global_mmu_pmm = pmm;
}

bool
mmu_is_canonical_address (uintptr_t address)
{
	address &= MMU_HIGHER_HALF_MIN;
	return address == 0 || address == MMU_HIGHER_HALF_MIN;
}

/* First, a few helper functions */

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

/* Default permissions for high-level page tables */
static const page_map_entry_t PM_PERMS =
	MMU_REG_USER | MMU_REG_WRITE | MMU_REG_PRESENT;

/* We don't currently handle the allocation failure case for page tables,
 * This is fine for now - page tables only occupy a tiny part of ram, I have
 * no desire to implement overcommit for a while
 */
static physical_t
allocate ()
{
	physical_t page = pmm_allocate_page (global_mmu_pmm);
	assert (page, "Failed to allocate page table");
	memset (HHDM_POINTER (page), 0, PAGE_SIZE);
	return page;
}

/* Top half - Iteration through nodes */

/* Which part of the page table tree we are working on.
 * Note start, end are virtual addresses but we do enough pointer maths
 * that uintptr_t is more convenient. This convention is followed through
 * most of this file - uintptr_t for virtual and physical_t for physical
 * addresses
 */
struct node_command_loc {
	struct mmu_page_map_part page;
	uintptr_t start;
	uintptr_t end;
};

typedef void (*node_callback)(
	struct node_command_loc loc,
	page_map_entry_t* entry,
	void* ctx
);

static void
apply_nodes (
	struct node_command_loc loc,
	node_callback visit,
	void* ctx
){
	const int depth = loc.page.depth;
	const int shift = depth_shift_size[depth];
	const uintptr_t p2 = 1ULL << shift;

	const uintptr_t base = ROUND_DOWN_P2 (
		loc.start, MMU_REG_PAGE_MAP_ENTRY_COUNT << shift);

	const int start_idx = (ROUND_DOWN_P2 (loc.start, p2) - base) >> shift;
	const int end_idx = (ROUND_UP_P2 (loc.end, p2) - base) >> shift;

	for (int i=start_idx; i<end_idx; i++) {
		struct mmu_page_map_table* table = HHDM_POINTER (loc.page.page);
		page_map_entry_t entry = table->entry[i];

		uintptr_t blk_start = (((size_t)i) << shift) + base;
		uintptr_t blk_end = (((size_t)(i+1)) << shift) + base;
		blk_start = MAX (loc.start, blk_start);
		blk_end = MIN (loc.end, blk_end);

		struct node_command_loc child = {
			.page = {
				.page = entry & MMU_REG_PHYS_ADDRESS_MASK,
				.depth = depth+1
			},
			.start = blk_start,
			.end = blk_end,
		};

		visit (child, &table->entry[i], ctx);
	}
}

static void
node_callback_reserve (
	struct node_command_loc loc,
	page_map_entry_t* entry,
	void* ctx
){
	if (loc.page.depth == PAGE_MAP_DEPTH_BOTTOM)
		return;

	if (!(*entry & MMU_REG_PRESENT)) {
		page_map_entry_t next = allocate ();
		*entry = next | PM_PERMS;
		loc.page.page = next;
	}

	apply_nodes (loc, node_callback_reserve, NULL);
}

static void
node_callback_gc (
	struct node_command_loc loc,
	page_map_entry_t* entry,
	void* ctx
){
	if (!(*entry & MMU_REG_PRESENT))
		return;

	if (loc.page.depth < PAGE_MAP_DEPTH_BOTTOM)
		apply_nodes (loc, node_callback_gc, NULL);

	struct mmu_page_map_table* table = HHDM_POINTER (loc.page.page);

	for (int i=0; i<MMU_REG_PAGE_MAP_ENTRY_COUNT; i++) {
		page_map_entry_t entry = table->entry[i];
		if (entry & MMU_REG_PRESENT)
			goto not_empty;
	}
	*entry = 0;
	pmm_free_page (global_mmu_pmm, loc.page.page);

not_empty:;
}

typedef void (*leaf_callback)(
	uintptr_t virt_addr,
	page_map_entry_t* entry,
	void* ctx
);

struct node_callback_leaf_ctx {
	leaf_callback callback;
	void* ctx;
};

static void
node_callback_leaf (
	struct node_command_loc loc,
	page_map_entry_t* entry,
	void* ctx
){
	if (loc.page.depth < PAGE_MAP_DEPTH_BOTTOM) {
		apply_nodes (loc, node_callback_leaf, ctx);
		return;
	}

	struct node_callback_leaf_ctx* visit = ctx;
	visit->callback (loc.start, entry, visit->ctx);
}

struct leaf_callback_assign_ctx {
	physical_t p_base;
	uintptr_t v_base;
	page_map_entry_t flags;
};

static void
leaf_callback_assign_linear (
	uintptr_t virt_addr,
	page_map_entry_t* entry,
	void* ctx
){
	struct leaf_callback_assign_ctx* data = ctx;
	physical_t addr = virt_addr - data->v_base + data->p_base;
	*entry = addr & data->flags;
}

static void
leaf_callback_clear (
	uintptr_t virt_addr,
	page_map_entry_t* entry,
	void* ctx
){
	*entry = 0;
}

/* Second part - Public Api. Shoudn't be much logic here */

/* Wrapper round apply_nodes that does some basic argument checking first */
static void
apply_nodes_entry (
	struct node_command_loc loc,
	node_callback visit,
	void* ctx
){
	if (loc.page.page == 0)
		loc.page = get_current_page_map_top();

	/* Here's quite a few checks...
	 * Some of these could be handled by rounding off to page sizes */
	require_page_aligned (loc.page.page);
	require_page_aligned (loc.start);
	require_page_aligned (loc.end);
	assert (loc.page.depth >= PAGE_MAP_DEPTH_TOP
		&& loc.page.depth <= PAGE_MAP_DEPTH_BOTTOM,
		"Invalid page_map_depth");
	assert (loc.start < loc.end, "Negative MMU allocation");
	assert (mmu_is_canonical_address (loc.start),
		"Non-canonical address");
	assert ((loc.start & MMU_HIGHER_HALF_MIN)
		== (loc.end & MMU_HIGHER_HALF_MIN),
		"Start and end not in same half");

	return apply_nodes (loc, visit, ctx);
}

void
mmu_assign (
	struct mmu_page_map_part top,
	enum mmu_flags flags,
	void* address,
	size_t size,
	physical_t page
){
	page_map_entry_t set_flags = convert_flags (flags) | MMU_REG_PRESENT;
	struct node_command_loc loc = {
		.page = top,
		.start = (uintptr_t)address,
		.end = (uintptr_t)address + size,
	};

	struct leaf_callback_assign_ctx leaves ={
		.flags = convert_flags (flags),
		.p_base = page,
		.v_base = (uintptr_t)address,
	};

	struct node_callback_leaf_ctx nodes = {
		.callback = leaf_callback_assign_linear,
		.ctx = &leaves,
	};

	apply_nodes_entry (loc, node_callback_reserve, NULL);
	apply_nodes_entry (loc, node_callback_leaf, &nodes);
}

void
mmu_assign_1 (
	struct mmu_page_map_part top,
	enum mmu_flags flags,
	physical_t page,
	void* address
){
	mmu_assign (top, flags, address, PAGE_SIZE, page);
}


void
mmu_remove (
	struct mmu_page_map_part top,
	void* address,
	size_t size
){
	struct node_command_loc loc = {
		.page = top,
		.start = (uintptr_t)address,
		.end = (uintptr_t)address + size,
	};

	struct node_callback_leaf_ctx nodes = {
		.callback = leaf_callback_clear,
	};

	apply_nodes_entry (loc, node_callback_leaf, &nodes);
	apply_nodes_entry (loc, node_callback_gc, NULL);
}

void
mmu_remove_1 (struct mmu_page_map_part top, void* address)
{
	mmu_remove (top, address, PAGE_SIZE);
}

static int
extract_page_map_index (struct mmu_page_map_part top, void* address)
{
	uintptr_t addr = (uintptr_t)address;
	int shift = depth_shift_size[top.depth];
	return (addr >> shift) & MMU_REG_VIRT_MASK;
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
