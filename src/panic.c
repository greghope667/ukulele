#include "panic.h"
#include <stdarg.h>
#include "libk/kstdio.h"

void _hcf (void);

void
panic (const char* fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	fputs ("=== KERNEL PANIC ===\n", stderr);
	vfprintf (stderr, fmt, args);

	for (;;) _hcf ();
}
