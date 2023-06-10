#include "tar.h"
#include "kstring.h"
#include <stddef.h>
#include <stdint.h>

struct tar_header*
tar_next_entry (struct tar_header* head)
{
	unsigned bytes = tar_read_octal (head->file_size, sizeof(head->file_size));
	unsigned blocks = (bytes + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;

	return (struct tar_header*) ((uintptr_t)head + blocks * TAR_BLOCK_SIZE);
}

struct mmfile
tar_file_contents (struct tar_header* head)
{
	struct mmfile file = {};

	if (head->typeflag != tar_type_reg)
		return file;

	unsigned bytes = tar_read_octal (head->file_size, sizeof(head->file_size));
	unsigned blocks = (bytes + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;

	file.data = (void*)((uintptr_t)head + TAR_BLOCK_SIZE);
	file.size = bytes;

	return file;
}

int
tar_read_octal (const char* str, size_t len)
{
	len = strnlen (str, len);
	int val = 0;

	while (len --> 0)
		val = val * 8 + (*str - '0');

	return val;
}
