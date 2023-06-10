#include "kstring.h"

size_t
strlen (const char* str)
{
	size_t len = 0;
	while (*str++)
		len++;
	return len;
}

size_t
strnlen (const char* str, size_t n)
{
	size_t len = 0;
	while (n --> 0 && *str++)
		len++;
	return len;
}

char*
strstr (const char* haystack, const char* needle)
{
	for (const char* h = haystack; *h; h++) {
		for ( int i=0; ; i++) {
			if (needle[i] == 0)
				return (char*)h;

			if (needle[i] != h[i])
				break;
		}
	}
	return NULL;
}

void*
memcpy (void* restrict dst, const void* src, size_t count)
{
	char* d = dst;
	const char* s = src;
	while (count --> 0)
		*d++ = *s++;
	return dst;
}

void*
memset (void* dst, int ch, size_t count)
{
	char* d = dst;
	while (count --> 0)
		*d++ = ch;
	return dst;
}
