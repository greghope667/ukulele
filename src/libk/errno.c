#include "kerrno.h"

int errno = 0;

static const char* messages[] = {
	[ESUCCESS] = "Success",
	[EIO] = "Input/output error",
	[EOPNOTSUPP] = "Operation not supported",
	[ENOSTR] = "Device not a stream",
	[ENOSR] = "Out of stream resources",
};

#define ERROR_COUNT (sizeof(messages) / sizeof(messages[0]))
static const char* unknown = "Unknown error";

static const char*
cstrerror (unsigned errnum)
{
	if (errnum >= ERROR_COUNT)
		return unknown;
	else
		return messages[errnum] ?: unknown;
}

char*
strerror (int errnum)
{
	return (char*) cstrerror (errnum);
}
