#pragma once

#include <stddef.h>

struct mmfile {
	void* data;
	size_t size;
};

struct tar_header {
	char file_name[100];
	char file_mode[8];
	char owner_id[8];
	char group_id[8];
	char file_size[12];
	char modification_time[12];
	char checksum[8];
	char typeflag;
	char link_name[100];
	char magic[6];
	char version[2];
	char owner_user_name[32];
	char owner_group_name[32];
	char device_major_number[8];
	char device_minor_number[8];
	char filename_prefix[155];
};

#define TAR_BLOCK_SIZE 512

/* Only flags that we actually handle here */
enum tar_typeflag {
	tar_type_reg = '0',
	tar_type_dir = '5',
};

struct tar_header* tar_next_entry (struct tar_header*);
struct mmfile tar_file_contents (struct tar_header*);
int tar_read_octal (const char* str, size_t len);

_Static_assert(sizeof(struct tar_header) == 500, "");
