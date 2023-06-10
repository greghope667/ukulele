#include "fb.h"
#include "font/font.h"
#include <stdint.h>
#include <string.h>

static const uint32_t FOREGROUND = -1;
static const uint32_t BACKGROUND = 0;

static inline
uint32_t* pixel32 (fb_config_t fb, int x, int y)
{
	return (uint32_t*) ((char*)fb.address + fb.pitch * y + 4 * x);
}

static inline int
abs (int x)
{
	return x >= 0 ? x : -x;
}

void
framebuffer_draw_line (fb_config_t fb, fb_point_t from, fb_point_t to)
{
	int x0 = from.x;
	int y0 = from.y;
	const int x1 = to.x;
	const int y1 = to.y;

	const int dx = abs (x1 - x0);
	const int dy = - abs (y1 - y0);
	const int sx = x0 < x1 ? 1 : -1;
	const int sy = y0 < y1 ? 1 : -1;

	int err = dx + dy;

	for (;;) {
		*pixel32 (fb, x0, y0) = FOREGROUND;
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;

		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

static unsigned
urand ()
{
	static unsigned r = 0;
	r = (r * 1103515245) + 12345;
	return r;
}

void
framebuffer_dofractals (fb_config_t fb)
{
	for (;;) {
		fb_point_t corners[] = {
			{ 0, 0 },
			{ fb.width, 0 },
			{ fb.width / 2, fb.height },
		};

		fb_point_t curr = {
			urand () % (unsigned) fb.width,
			urand () % (unsigned) fb.height,
		};

		for (;;) {
			*pixel32 (fb, curr.x, curr.y) = -1;

			fb_point_t target = corners[(urand () >> 20) % 3];

			fb_point_t next = {
				(curr.x + target.x) / 2,
				(curr.y + target.y) / 2,
			};
			curr = next;

			for (volatile int i=0; i<1000000; i++);
		}
	}
}

void
framebuffer_write_at(fb_config_t fb, char c, fb_point_t p)
{
	if (c < ' ' || c > '~')
		return;

	char fchar[FONT_CHAR_HEIGHT];
	memcpy(fchar, font_8x8[c - ' '], sizeof(fchar));

	for ( int y=0; y<FONT_CHAR_HEIGHT; y++) {
		char row = fchar[y];
		for ( int x=0; x<FONT_CHAR_WIDTH; x++) {
			uint32_t color = (row & 1) ? FOREGROUND : BACKGROUND;
			*pixel32 (fb, p.x + x, p.y + y) = color;
			row = row >> 1;
		}
	}
}
