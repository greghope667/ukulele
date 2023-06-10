#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

struct FILE;
typedef struct FILE FILE;

extern FILE* stdout;
extern FILE* stderr;

#define EOF ((int)-1)

/* Direct IO */

size_t fread ( void* restrict buffer, size_t size, size_t count, FILE* restrict stream);
size_t fwrite ( const void* restrict buffer, size_t size, size_t count, FILE* restrict stream);

/* Cookie for custom io - idea stolen from GLIBC
 * Note that there's no `standard` io for this kernel,
 * all FILEs must have cookies.
 */

typedef __ssize_t cookie_write_function_t (void* cookie, const char* buf, size_t nbytes);
typedef __ssize_t cookie_read_function_t (void* cookie, char* buf, size_t nbytes);

typedef struct cookie_io_functions {
	cookie_read_function_t* reader;
	cookie_write_function_t* writer;
} cookie_io_functions_t;

FILE* fopencookie (void* cookie, const char* opentype, cookie_io_functions_t io);

/* Formatted IO */

int fprintf (FILE* stream, const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));
int vfprintf (FILE* stream, const char* fmt, va_list args);
int printf (const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));

#define eprintf(...) fprintf (stderr, __VA_ARGS__)

/* Character IO */
int fputc (int c, FILE* stream);
int fputs (const char* str, FILE* stream);
int puts (const char* str);

#define putc fputc
#define putchar(c) fputc (c, stdout)

/* Errors */
void clearerr (FILE* stream);
int feof (FILE* stream);
int ferror (FILE* stream);
void perror (const char* str);
