#pragma once

extern int errno;
char* strerror ( int errnum );

enum {
	ESUCCESS = 0,
	EIO,
	EOPNOTSUPP,
	ENOSTR,
	ENOSR,
};
