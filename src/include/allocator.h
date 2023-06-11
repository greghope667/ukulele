#pragma once

#include <stddef.h>

typedef void* (*alloc_fn) (void* allocator, size_t);
typedef void* (*realloc_fn) (void* allocator, void*, size_t);
typedef void (*free_fn) (void* allocator, void*);

struct page_allocator_functions {
	alloc_fn alloc;
	realloc_fn realloc;
	free_fn free;
};

struct allocator {
	void* context;
	struct page_allocator_functions* vtbl;
};

void* kalloc (struct allocator* a, size_t size);
void* krealloc (struct allocator* a, void* mem, size_t size);
void kfree (struct allocator* a, void* mem);
