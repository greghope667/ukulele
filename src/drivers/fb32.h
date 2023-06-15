#pragma once

typedef struct framebuffer_point {
	int x;
	int y;
} fb_point_t;

typedef struct framebuffer_config {
	void* address;
	int width;
	int height;
	int pitch;
} fb_config_t;

void framebuffer_write_at (fb_config_t, char c, fb_point_t p);
void framebuffer_draw_line (fb_config_t, fb_point_t, fb_point_t);
void framebuffer_dofractals (fb_config_t);
