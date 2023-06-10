#pragma once

#define PAGE_SIZE 4096
#define REQUIRE_PAGE_SIZED(x) \
	_Static_assert(sizeof(x) == PAGE_SIZE, "Type must fit single page");

#define PAGE_ALIGNED  __attribute ((aligned(4096)))

#define require_page_aligned(p)								\
	if ((uint64_t)p & 0xfff)								\
		panic("Address %p not aligned %s:%i\n",				\
			  (void*)p, __FILE__, __LINE__)					\

typedef struct {
	char _[PAGE_SIZE];
} PAGE_ALIGNED raw_page;
