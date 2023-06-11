#include <string.h>
#include <limits.h>
#include <limine.h>

#include "fb.h"
#include "serial.h"
#include "kstdio.h"
#include "mmu_reg.h"

void _hcf(void);
void kernel_main(void);

// Cookies for debug output
static __ssize_t
serial_cookie_write (void* cookie, const char* buf, size_t nbytes)
{
	int port = *(int*)cookie;
	for (size_t c=nbytes; c --> 0; )
		serial_write (port, *buf++);
	return nbytes;
}

static int stdout_serial = SERIAL_PORT_1;

static cookie_io_functions_t serial_io = {
	.writer = serial_cookie_write,
};

struct fb_cookie {
	struct framebuffer_config* config;
	int x;
	int y;
} fb_cookie;

static __ssize_t
fb_cookie_write (void* cookie, const char* buf, size_t nbytes)
{
	struct fb_cookie* fbc = cookie;
	for ( size_t c=nbytes; c--> 0; ) {
		char print = *buf++;
		framebuffer_write_at(*fbc->config, print,
							 (fb_point_t){fbc->x, fbc->y});
		fbc->x += 8;
		if (print == '\n' || fbc->x > fbc->config->width) {
			fbc->x = 0;
			fbc->y += 8;
			if (fbc->y > fbc->config->height) {
				fbc->y = 0;
			}
		}
	}
	return nbytes;
}

static cookie_io_functions_t fb_io = {
	.writer = fb_cookie_write,
};

// Bootloader requests
struct limine_bootloader_info_request blinfo = {
	.id = LIMINE_BOOTLOADER_INFO_REQUEST,
};

struct limine_kernel_address_request kainfo = {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST,
};

struct limine_memmap_request mmapinfo = {
	.id = LIMINE_MEMMAP_REQUEST,
};

struct limine_framebuffer_request fbinfo = {
	.id = LIMINE_FRAMEBUFFER_REQUEST,
};

struct limine_rsdp_request rsdpinfo = {
	.id = LIMINE_RSDP_REQUEST,
};

struct limine_hhdm_request hhdminfo = {
	.id = LIMINE_HHDM_REQUEST,
};

struct limine_kernel_file_request kfdinfo = {
	.id = LIMINE_KERNEL_FILE_REQUEST,
};

struct RSDPDescriptor {
 char Signature[8];
 uint8_t Checksum;
 char OEMID[6];
 uint8_t Revision;
 uint32_t RsdtAddress;
} __attribute__ ((packed));

struct RSDPDescriptor20 {
 struct RSDPDescriptor firstPart;

 uint32_t Length;
 uint64_t XsdtAddress;
 uint8_t ExtendedChecksum;
 uint8_t reserved[3];
} __attribute__ ((packed));

const char* limine_mmap_typename[] = {
	"Usable",
	"Reserved",
	"Acpi Reclaimable",
	"Acpi NVS",
	"Bad Memory",
	"Bootloader Reclaimable",
	"Kernel and Modules",
	"Framebuffer",
};

#include "pmm.h"

static raw_page rwpmmpg;
static struct pmm* pmm = (void*)&rwpmmpg;

static void
print_meminfo()
{
	printf ("Memmap resp: %p rev: %zi count: %zu\n",
			mmapinfo.response,
			mmapinfo.response->revision,
			mmapinfo.response->entry_count);

	int entries = mmapinfo.response->entry_count;

	size_t free_memory = 0;
	size_t reclaimable = 0;

	size_t largest_addr;
	size_t largest_size = 0;

	struct pmm* pmm = pmm_new (&rwpmmpg);

	for ( int i=0; i<entries; i++ ) {
		struct limine_memmap_entry* mem = mmapinfo.response->entries[i];
		printf ("\t%016zx+%16zx : %s\n",
				mem->base, mem->length,
				limine_mmap_typename[mem->type]);

		if (mem->type == LIMINE_MEMMAP_USABLE) {
			free_memory += mem->length;
			if (mem->length > largest_size) {
				largest_size = mem->length;
				largest_addr = mem->base;
			}
			pmm_add (pmm, mem->base, mem->length);
		}

		if (mem->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE)
			reclaimable += mem->length;

	}

	printf ("\n\tfree %zu reclaim %zu\n", free_memory, reclaimable);
	printf ("\tlargest %zu at %zx\n", largest_size, largest_addr);

	struct pmm_stat stat = pmm_count_pages (pmm);

	printf ("\tPMM total: %zu free: %zu used: %zu\n",
			stat.total, stat.free, stat.used);
}

static void
xxd (const void* address, int lines, int columns)
{
	const char* p = address;

	static const char hex[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
	};

	for ( int i=0; i<lines; i++ ) {
		printf ("%p: ", p);
		for ( int j=0; j<columns; j++ ) {
			uint8_t c = p[j];
			putchar (hex[c >> 4]);
			putchar (hex[c & 0xf]);
			if (j%2)
				putchar (' ');
		}
		putchar (' ');
		for ( int j=0; j<columns; j++ ) {
			uint8_t c = p[j];
			if (c >= ' ' && c < 128)
				putchar (c);
			else
				putchar ('.');
		}
		putchar ('\n');
		p += columns;
	}
}

#define REG_READ(r) \
static uint64_t reg_ ## r () { \
	uint64_t ret;\
	asm ("mov\t%%" # r ", %0" : "=r" (ret));\
	return ret;\
}

REG_READ(cr0)
REG_READ(cr2)
REG_READ(cr3)
REG_READ(cr4)
REG_READ(cr8)

uint64_t global_hhdm_offset;

static void
print_limine_info ()
{
	printf ("Bootload info resp: %p rev: %zi name: %s version: %s\n",
			blinfo.response,
			blinfo.response->revision,
			blinfo.response->name,
			blinfo.response->version);

	printf ("Kernel address info resp: %p rev: %zi phys: %zx virt %zx\n",
			kainfo.response,
			kainfo.response->revision,
			kainfo.response->physical_base,
			kainfo.response->virtual_base);

	print_meminfo();

	printf ("Framebuffers resp: %p rev: %zi\n",
			fbinfo.response, fbinfo.response->revision);

	int entries = fbinfo.response->framebuffer_count;
	for ( int i=0; i<entries; i++ ) {
		struct limine_framebuffer fb = *fbinfo.response->framebuffers[i];
		printf ("\t%p:%zux%zu pitch %zu bpp %hu\n",
				fb.address, fb.width, fb.height, fb.pitch, fb.bpp);
	}

	printf ("HHDM resp: %p rev: %zu: offset: %zx\n",
			hhdminfo.response, hhdminfo.response->revision,
			hhdminfo.response->offset);

	printf ("Kernel file resp: %p rev: %zu: ptr: %p\n",
			kfdinfo.response, kfdinfo.response->revision,
			kfdinfo.response->kernel_file);

	struct limine_file* file = kfdinfo.response->kernel_file;

	printf ("\taddress: %p size: %zu path: %s cmdline: %s\n",
			file->address, file->size, file->path, file->cmdline);

	printf ("RSDP resp: %p rev %zu: addr %p\n",
			rsdpinfo.response, rsdpinfo.response->revision,
			rsdpinfo.response->address);

	struct RSDPDescriptor* response = rsdpinfo.response->address;
	unsigned checksum = 0;
	for ( int i=0; i < sizeof(*response) ; i++)
		checksum += ((char*)response)[i];
	printf ("\tsig: %.8s oem: %.6s rev: %u rsdt: %x check: %i\n",
			response->Signature,
			response->OEMID,
			response->Revision,
			response->RsdtAddress,
			checksum
	);

	printf ("cr0: %lx\n", reg_cr0 ());
	printf ("cr2: %lx\n", reg_cr2 ());
	printf ("cr3: %lx\n", reg_cr3 ());
	printf ("cr4: %lx\n", reg_cr4 ());
	printf ("cr8: %lx\n", reg_cr8 ());
}

static void
clear_map_lower_half ()
{
	struct page_map_table* pml4 = (void*)(reg_cr3 () + hhdminfo.response->offset);
	for (int i=0; i<256; i++) {
		pml4->entry[i] = 0;
	}
}

#include "vaddress_space.h"
static void
test_vaddr()
{
	struct pmm* pmm = (struct pmm*) (&rwpmmpg);
	struct vaddress_space* vaddr = vaddress_space_new (pmm, (void*)0x600, (void*)0x6000);

	void* p, * q, * r;
	vaddress_print (vaddr);
	p = vaddress_allocate (vaddr, 0x100);
	vaddress_print (vaddr);
	vaddress_free (vaddr, p, 0x100);
	vaddress_print (vaddr);
	p = vaddress_allocate (vaddr, 0x100);
	vaddress_print (vaddr);
	q = vaddress_allocate (vaddr, 0x80);
	vaddress_print (vaddr);
	r = vaddress_allocate (vaddr, 0x200);
	vaddress_print (vaddr);
	vaddress_free (vaddr, p + 0x20, 0x30);
	vaddress_print (vaddr);
	vaddress_free (vaddr, p + 0x80, r - q + 0x20);
	vaddress_print (vaddr);
	vaddress_free (vaddr, 0, 0x5000);
	vaddress_print (vaddr);
}

void
kernel_main(void)
{
	global_hhdm_offset = hhdminfo.response->offset;

	bool use_serial = strstr (kfdinfo.response->kernel_file->cmdline, "serial");
	bool do_fractal = strstr (kfdinfo.response->kernel_file->cmdline, "fractal");

	struct framebuffer_config fb = {
		.address = fbinfo.response->framebuffers[0]->address,
		.width = fbinfo.response->framebuffers[0]->width,
		.height = fbinfo.response->framebuffers[0]->height,
		.pitch = fbinfo.response->framebuffers[0]->pitch,
	};

	fb_cookie.config = &fb;

	if (use_serial && serial_detect (SERIAL_PORT_1)) {
		stdout = fopencookie (&stdout_serial, "w", serial_io);
	} else {
		stdout = fopencookie (&fb_cookie, "w", fb_io);
	}

	stderr = stdout;

	printf ("=== SYSTEM BOOT ===\n");

	for ( int port = SERIAL_PORT_1; port <= SERIAL_PORT_4; port++ ) {
		if (serial_detect (port))
			printf ("Detected serial port: %i\n", port + 1);
	}

	print_limine_info ();
	clear_map_lower_half ();
	test_vaddr ();


	if (do_fractal)
		framebuffer_dofractals (fb);

	printf ("=== SYSTEM SHUTDOWN ===\n");
}
