#include "arena_allocator.h"
#include "allocator.h"
#include "page.h"
#include "pmm.h"

struct arena_pmm_page_header {
	struct arena_pmm_page_header* next;
};

struct arena_pmm {
	struct arena_pmm_page_header hdr;
	struct arena_pmm_page_header* current_page;
	int current_offset;
	int p2align;
	pmm_t pmm;
};

static const struct allocator_vtbl arena_pmm_vtbl;

static inline unsigned
align_p2 (unsigned val, int p2)
{
	return (val + (1U<<p2) - 1) >> p2 << p2;
}

allocator_t
make_arena_pmm_allocator (pmm_t pmm, int p2align)
{
	physical_t page = pmm_allocate_page (pmm);

	if (page == 0)
		return (allocator_t) {};

	struct arena_pmm* arena = HHDM_POINTER (page);
	*arena = (struct arena_pmm) {
		.current_page = &arena->hdr,
		.current_offset = align_p2 (sizeof(*arena), p2align),
		.p2align = p2align,
		.pmm = pmm,
	};

	return (allocator_t) {
		.self = arena,
		.vtbl = &arena_pmm_vtbl,
	};
}

static struct blk
arena_pmm_alloc (void* self, size_t size)
{
	struct arena_pmm* arena = self;

	size = align_p2 (size, arena->p2align);
	if (size >= PAGE_SIZE)
		return (struct blk) {};

	if (arena->current_offset + size > PAGE_SIZE) {
		// Out of space, get another page
		physical_t page = pmm_allocate_page (arena->pmm);
		if (page == 0)
			return (struct blk) {};

		struct arena_pmm_page_header* hdr = HHDM_POINTER (page);
		arena->current_page->next = hdr;
		arena->current_page = hdr;
		arena->current_offset = align_p2 (sizeof(*hdr), arena->p2align);
	}

	struct blk blk = {
		.ptr = (void*)arena->current_page + arena->current_offset,
		.size = size,
	};
	arena->current_offset += size;
	return blk;
}

static void
arena_pmm_del (void* self)
{
	struct arena_pmm* arena = self;

	struct arena_pmm_page_header* page = &arena->hdr;
	pmm_t pmm = arena->pmm;

	while (page) {
		physical_t phy = HHDM_PHYSICAL (page);
		page = page->next;
		pmm_free_page (pmm, phy);
	}
}

static const struct allocator_vtbl
arena_pmm_vtbl = {
	.alloc = arena_pmm_alloc,
	.del = arena_pmm_del,
};
