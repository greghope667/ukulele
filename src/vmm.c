#include "mmu_reg.h"
#include <limine.h>
#include <stdio.h>

static struct page_map_table pml4 = {};
static struct page_map_table pml3_hhdm = {};
static struct page_map_table pml3_kernel = {};
static struct page_map_table pml2_kernel = {};
static struct page_map_table pml1_kernel = {};

extern struct limine_kernel_address_request kainfo;

static uint64_t
kernel_virt_to_phys (void* virt)
{
	return ((uint64_t)virt) - kainfo.response->virtual_base + kainfo.response->physical_base;
}

static void
mmu_map (struct page_map_table* pmlx,
		 enum mmu_page_map_level level,
		 void* virtual,
		 uint64_t phys,
		 uint64_t bits)
{
	int index;
	bits |= MMU_REG_PRESENT;

	switch (level) {
		case MMU_PML1:
			index = ((uint64_t)virtual >> MMU_REG_VIRT_SHIFT_PML1)
					& MMU_REG_VIRT_MASK;
			break;

		case MMU_PML2:
			index = ((uint64_t)virtual >> MMU_REG_VIRT_SHIFT_PML2)
					& MMU_REG_VIRT_MASK;
			break;

		case MMU_PML3:
			index = ((uint64_t)virtual >> MMU_REG_VIRT_SHIFT_PML3)
					& MMU_REG_VIRT_MASK;
			break;

		case MMU_PML4:
			index = ((uint64_t)virtual >> MMU_REG_VIRT_SHIFT_PML4)
					& MMU_REG_VIRT_MASK;
			break;
	}
	pmlx->entry[index] = bits | (MMU_REG_PHYS_ADDRESS_MASK & phys);
}

static void
mmu_allocate (struct page_map_table* pmlx,
			  enum mmu_page_map_level level,
			  void* virtual,
			  uint64_t phys)
{
	uint64_t bits = MMU_REG_WRITE;
	if (level != MMU_PML1)
		bits |= MMU_REG_PAGE_SIZE;
	mmu_map (pmlx, level, virtual, phys, bits);
}

static void
mmu_point (struct page_map_table* src,
		   enum mmu_page_map_level slevel,
		   struct page_map_table* dst,
		   void* virtual)
{
	uint64_t phys = kernel_virt_to_phys (dst);
	uint64_t bits = MMU_REG_WRITE;
	mmu_map (src, slevel, virtual, phys, bits);
}

extern uint64_t hhdm_offset;

static void
setup_map_hhdm ()
{
	char* virt = (void*)hhdm_offset;
	uint64_t phys = 0;

	mmu_point (&pml4, MMU_PML4, &pml3_hhdm, virt);

	for (int i=0; i<4; i++) {
		mmu_allocate (&pml3_hhdm, MMU_PML3, virt, phys);
		virt += (1 << 30);
		phys += (1 << 30);
	}
}

extern void __start_exe;
extern void __end_exe;
extern void __start_data;
extern void __end_data;

static void
setup_map_kernel ()
{
	char* virt = (void*)kainfo.response->virtual_base;
	uint64_t phys = kainfo.response->physical_base;

	mmu_point (&pml4, MMU_PML4, &pml3_kernel, virt);
	mmu_point (&pml3_kernel, MMU_PML3, &pml2_kernel, virt);
	mmu_point (&pml2_kernel, MMU_PML2, &pml1_kernel, virt);

	uint64_t bits = 0;
	int length = (&__end_exe - &__start_exe) >> 12;
	printf ("\ttext length = %i\n", length);

	for (int i=0; i<length; i++) {
		mmu_map (&pml1_kernel, MMU_PML1, virt, phys, bits);
		virt += 4096;
		phys += 4096;
	}

	bits |= MMU_REG_NO_EXECUTE;
	length = (&__start_data - &__end_exe) >> 12;
	printf ("\trodata length = %i\n", length);

	for (int i=0; i<length; i++) {
		mmu_map (&pml1_kernel, MMU_PML1, virt, phys, bits);
		virt += 4096;
		phys += 4096;
	}

	bits |= MMU_REG_WRITE;
	length = (&__end_data - &__start_data) >> 12;
	printf ("\tdata/bss length = %i\n", length);

	for (int i=0; i<length; i++) {
		mmu_map (&pml1_kernel, MMU_PML1, virt, phys, bits);
		virt += 4096;
		phys += 4096;
	}
}

static void
setup_paging()
{
	printf ("Configuring paging...\n");
	setup_map_hhdm ();
	setup_map_kernel ();

	uint64_t pml4phys = kernel_virt_to_phys (&pml4);
	asm ("mov\t%0, %%cr3": : "r" (pml4phys));
	printf ("\tdone\n");
}
