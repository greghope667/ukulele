#include "kstdio.h"
#include "kstring.h"
#include "kerrno.h"

#include "types.h"
#include <stdbool.h>

struct FILE {
	void* cookie;
	cookie_io_functions_t io_functions;
	bool eof;
	bool error;
};

FILE* stdout = NULL;
FILE* stderr = NULL;

#define MAX_OPEN_STREAMS 8

FILE streams[MAX_OPEN_STREAMS] = {};

size_t
fread ( void* restrict buffer, size_t size, size_t count, FILE* restrict stream)
{
	errno = 0;

	if (! stream) {
		errno = ENOSTR;
		return 0;
	}

	void* cookie = stream->cookie;
	cookie_read_function_t* read = stream->io_functions.reader;

	if (! read) {
		errno = EOPNOTSUPP;
		return 0;
	}

	size_t nbytes = size * count;
	if (nbytes == 0)
		return 0;

	ssize_t n = read (cookie, buffer, nbytes);

	if (n < 0) {
		stream->error = true;
		errno = EIO;
		return 0;
	}

	if (n == 0)
		stream->eof = true;

	return n;
}

size_t fwrite ( const void* restrict buffer, size_t size, size_t count, FILE* restrict stream)
{
	errno = 0;

	if (! stream) {
		errno = ENOSTR;
		return 0;
	}

	void* cookie = stream->cookie;
	cookie_write_function_t* write = stream->io_functions.writer;

	if (! write) {
		errno = EOPNOTSUPP;
		return 0;
	}

	size_t nbytes = size * count;
	if (nbytes == 0)
		return 0;

	ssize_t n = write (cookie, buffer, nbytes);

	if (n <= 0) {
		stream->error = true;
		errno = EIO;
		return 0;
	}

	return n;
}

FILE*
fopencookie (void* cookie, const char* opentype, cookie_io_functions_t io)
{
	(void) opentype;

	for ( int i=0; i<MAX_OPEN_STREAMS; i++ ) {
		FILE* f = streams + i;
		if (f->cookie == NULL) {
			FILE new = {
				.cookie = cookie,
				.io_functions = io,
			};

			*f = new;

			errno = 0;
			return f;
		}
	}

	errno = ENOSR;
	return NULL;
}

int
fprintf (FILE* stream, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vfprintf (stream, fmt, args);
	va_end(args);
	return ret;
}

int
printf (const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vfprintf (stdout, fmt, args);
	va_end(args);
	return ret;
}

int
fputs (const char* str, FILE* stream)
{
	size_t len = strlen (str);
	return fwrite (str, 1, len, stream) ?: EOF;
}

int
fputc (int c, FILE* stream)
{
	unsigned char u = c;
	return fwrite (&u, 1, 1, stream) ? c : EOF;
}

int
puts (const char* str)
{
	int ret = fputs (str, stdout);
	if (ret == EOF)
		return EOF;

	if (fputc ('\n', stdout) == EOF)
		return EOF;

	return 1 + ret;
}

void
clearerr (FILE* stream)
{
	if (stream) {
		stream->eof = false;
		stream->error = false;
	}
}

int
ferror (FILE* stream)
{
	return stream ? stream->error : true;
}

int
feof (FILE* stream)
{
	return stream ? stream->eof : true;
}

void
perror (const char* str)
{
	const char* err = strerror (errno);
	if (str)
		fprintf (stderr, "%s: %s\n", str, err);
	else
		fprintf (stderr, "%s\n", err);
}
