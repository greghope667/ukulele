#pragma once

#include <stddef.h>

struct vaddress_space;
struct pmm;

/* Create a new address space structure */
struct vaddress_space* vaddress_space_new (struct pmm*, void* begin, void* end);
void vaddress_space_free (struct vaddress_space*);

void* vaddress_allocate (struct vaddress_space*, size_t size);
void vaddress_free (struct vaddress_space*, void* address, size_t size);

void vaddress_print (struct vaddress_space*);
