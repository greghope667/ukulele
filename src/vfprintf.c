#include "kstdio.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

#define setjmp __builtin_setjmp
#define longjmp __builtin_longjmp
#define jmp_buf intptr_t*
#define ERROR(n) longjmp((void**)err, 1)

struct fmt_specifier {
	int precision; // e.g. %.6s
	signed char width; // e.g. %4u
	signed char length; // e.g. %lli, %zi
	bool zero_pad; // e.g. %02i
	bool has_precision;
};

static int
digit_count_base_10 (long long unsigned value)
{
	if (value == 0)
		return 1;

	int count = 0;
	while (value > 0) {
		value /= 10;
		count += 1;
	}
	return count;
}

static int
digit_count_base_16 (long long unsigned value)
{
	if (value == 0)
		return 1;

	int count = 0;
	while (value > 0) {
		value /= 16;
		count += 1;
	}
	return count;
}

static int
fwrite_e (const char* c, int len, FILE* stream, jmp_buf err)
{
	int n = fwrite(c, 1, len, stream);
	if (n < len)
		ERROR(1);
	return n;
}

static int
fputc_e (int c, FILE* stream, jmp_buf err)
{
	if (fputc (c, stream) == EOF)
		ERROR(1);
	return 1;
}

static int
print_num_10 (FILE* stream, long long unsigned value, jmp_buf err)
{
	char buffer[32] = {};

	int length = digit_count_base_10 (value);
	for ( int i=0; i<length; i++ ) {
		int digit = value % 10;
		value /= 10;
		buffer[length - 1 - i] = '0' + digit;
	}

	return fwrite_e (buffer, length, stream, err);
}

static int
print_num_16 (FILE* stream, long long unsigned value, jmp_buf err)
{
	char buffer[32] = {};

	static const char hex[16] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
	};

	int length = digit_count_base_16 (value);
	for ( int i=0; i<length; i++ ) {
		int digit = value % 16;
		value /= 16;
		buffer[length - 1 - i] = hex[digit];
	}

	return fwrite_e (buffer, length, stream, err);
}

static int
print_unsigned (struct fmt_specifier afmt, FILE* stream, va_list args, jmp_buf err)
{
	long long unsigned value = 0;
	int printed = 0;

	switch (afmt.length) {
		case -2: case -1: case 0:
			value = va_arg (args, unsigned);
			break;
		case 1:
			value = va_arg (args, long unsigned);
			break;
		case 2:
			value = va_arg (args, long long unsigned);
			break;
		default:
			ERROR(2);
	}

	if (afmt.width > 0) {
		int digits = digit_count_base_10 (value);
		char pad = afmt.zero_pad ? '0' : ' ';
		while (afmt.width > digits) {
			afmt.width--;
			printed++;
			fputc_e (pad, stream, err);
		}
	}

	return printed + print_num_10 (stream, value, err);
}

static int
print_signed (struct fmt_specifier afmt, FILE* stream, va_list args, jmp_buf err)
{
	long long value = 0;
	int printed = 0;

	switch (afmt.length) {
		case -2: case -1: case 0:
			value = va_arg (args, int);
			break;
		case 1:
			value = va_arg (args, long);
			break;
		case 2:
			value = va_arg (args, long long);
			break;
		default:
			ERROR(2);
	}

	if (value < 0) {
		fputc_e ('-', stream, err);
		value = - value;
		printed++;
	}

	if (afmt.width > 0) {
		afmt.width -= printed;

		int digits = digit_count_base_10 (value);
		char pad = afmt.zero_pad ? '0' : ' ';
		while (afmt.width > digits) {
			afmt.width--;
			printed++;
			fputc_e (pad, stream, err);
		}
	}

	return printed + print_num_10 (stream, value, err);
}

static int
print_string (struct fmt_specifier afmt, FILE* stream, va_list args, jmp_buf err)
{
	const char* str = va_arg (args, const char*);
	int printed = 0;
	int max = afmt.has_precision ? afmt.precision : INT_MAX;
	int chars = strnlen (str, max);

	if (afmt.width > 0) {
		while (afmt.width > chars) {
			afmt.width--;
			afmt.precision--;
			printed++;
			fputc_e (' ', stream, err);
		}
	}

	if (afmt.has_precision && chars > afmt.precision) {
		chars = afmt.precision;
	}
	return printed + fwrite_e (str, chars, stream, err);
}

static int
print_hex (struct fmt_specifier afmt, FILE* stream, va_list args, jmp_buf err)
{
	long long unsigned value = 0;
	int printed = 0;

	switch (afmt.length) {
		case -2: case -1: case 0:
			value = va_arg (args, unsigned);
			break;
		case 1:
			value = va_arg (args, long unsigned);
			break;
		case 2:
			value = va_arg (args, long long unsigned);
			break;
		default:
			ERROR(2);
	}

	if (afmt.width > 0) {
		int digits = digit_count_base_16 (value);
		char pad = afmt.zero_pad ? '0' : ' ';
		while (afmt.width > digits) {
			afmt.width--;
			printed++;
			fputc_e (pad, stream, err);
		}
	}

	return printed + print_num_16 (stream, value, err);
}

static int
print_char (struct fmt_specifier afmt, FILE* stream, va_list args, jmp_buf err)
{
	(void) afmt;
	char c = va_arg (args, int);
	return fputc_e (c, stream, err);
}


static int
parse_format_str (const char** fmt, FILE* stream, va_list args, jmp_buf err)
{
	struct fmt_specifier afmt = {};
	enum { FLAG, WIDTH, PRECISION } state = FLAG;

	for (;;) {
		char c = *((*fmt)++);
		switch (c) {
		case 'd': case 'i':
			return print_signed (afmt, stream, args, err);
		case 'u':
			return print_unsigned (afmt, stream, args, err);
		case 'x':
			return print_hex (afmt, stream, args, err);
		case 'c':
			return print_char (afmt, stream, args, err);
		case 's':
			return print_string (afmt, stream, args, err);
		case 'p':
			afmt.width = 16;
			afmt.length = 2;
			afmt.zero_pad = true;
			return print_hex (afmt, stream, args, err);
		case '%':
			return fputc_e ('%', stream, err);

		case 'h':
			afmt.length--;
			break;
		case 'l':
			afmt.length++;
			break;

		case 'z': case 'j': case 't':
			afmt.length = 1;
			break;

		case '0':
			switch (state) {
				case FLAG:
					afmt.zero_pad = true;
					state = WIDTH;
					break;
				case WIDTH:
					afmt.width *= 10;
					break;
				case PRECISION:
					afmt.precision *= 10;
					break;
			}
			break;

		case '1' ... '9':
			switch (state) {
				case FLAG:
					state = WIDTH;
				case WIDTH:
					afmt.width = afmt.width * 10 + (c - '0');
					break;
				case PRECISION:
					afmt.precision = afmt.precision * 10 + (c - '0');
					break;
			}
			break;

		case '.':
			state = PRECISION;
			afmt.has_precision = true;
			break;

		case 'f':
		case 'e':
		case 'g':
		case 'a':
			// Float - not supported
			va_arg (args, double);
			return fputc_e ('?', stream, err);

		default:
			ERROR(2);
		}
	}
}

int
vfprintf (FILE* stream, const char* fmt, va_list args)
{
	intptr_t err[6];

	if (setjmp ((void**)err) != 0) {
		return EOF;
	}

	char c;
	int written = 0;

	while ((c = *fmt++)) {
		if (c != '%') {
			fputc_e (c, stream, err);
			written += 1;
		} else {
			written += parse_format_str(&fmt, stream, args, err);
		}
	}

	return written;
}
