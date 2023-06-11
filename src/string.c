#include "kstring.h"

#define WEAK __attribute__((weak))

WEAK size_t
strlen (const char* str)
{
	size_t len = 0;
	while (*str++)
		len++;
	return len;
}

WEAK size_t
strnlen (const char* str, size_t n)
{
	size_t len = 0;
	while (n --> 0 && *str++)
		len++;
	return len;
}

WEAK char*
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

WEAK void*
memcpy (void* restrict dst, const void* restrict src, size_t count)
{
	char* d = dst;
	const char* s = src;
	while (count --> 0)
		*d++ = *s++;
	return dst;
}

WEAK void*
memset (void* dst, int ch, size_t count)
{
	char* d = dst;
	while (count --> 0)
		*d++ = ch;
	return dst;
}

WEAK void*
memmove (void* dst, const void* src, size_t count)
{
	char* d = dst;
	char* s = (char*)src;

	if (d <= s)
		return memcpy (dst, src, count);
	if (s + count <= d)
		return memcpy (dst, src, count);

	// s < d < s + count

	size_t offset = d - s;
	size_t overlap = count - offset;

	memcpy (d + offset, s + offset, overlap);
	return memcpy (d, s, offset);
}
