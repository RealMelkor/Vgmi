/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include "image.h"
#ifdef ENABLE_IMAGE
#include <stdint.h>
#include "sandbox.h"
#include "termbox.h"
#include "error.h"

#define IMG_MEMORY 1024 * 1024 * 8

int image_init() {
	void *memory = malloc(IMG_MEMORY);
	if (!memory) return ERROR_MEMORY_FAILURE;
	image_memory_set(memory, IMG_MEMORY);
	return 0;
}

static int color_abs(int c, int x, int i) {
	return abs(c -= i > 215 ? i * 10 - 2152:x * 40 + !!x * 55);
}

static int color(uint8_t r, uint8_t g, uint8_t b) {
	int m = 0, t = 0, i = 240, l = 240;
	while (i--) {
		t = color_abs(r, i / 36, i) + color_abs(g, i / 6 % 6, i) +
			color_abs(b, i % 6, i);
		if (t < l) {
			l = t;
			m = i;
		} 
	}
	i = m + 16;
	return i;
}

int average_pixel(uint8_t* data, int x, int y, int w, int h, float ratio) {
	int pixels = 0;
	int r = 0, g = 0, b = 0, i, j;
	float precision = 2;
	float rp = ratio / precision;
	for (i = x - rp; i < x + rp; i++) {
		if (i >= w || i < 0) continue;
		for (j = y - rp; j < y + rp; j++) {
			if (j >= h || j < 0) continue;
			r += data[(i + j * w) * 3];
			g += data[(i + j * w) * 3 + 1];
			b += data[(i + j * w) * 3 + 2];
			pixels++;
		}
	}
	if (pixels == 0) return TB_DEFAULT;
	return color(r / pixels, g / pixels, b / pixels);
}

int image_display(unsigned char* data, int w, int h, int offsety) {
	int _x = 0, _y = 0;
	double x, y;
	double w_ratio = (double)w/(tb_width());
	double h_ratio = (double)h/((tb_height() - offsety - 2) * 2);

	if (w_ratio < h_ratio)
		w_ratio = h_ratio;
	else
		h_ratio = w_ratio;

	for (x = 0; x < (double)w; x += w_ratio) {
		for (y = 0; y < (double)h; y += h_ratio) {
			int fg = average_pixel(data, x, y + 1, w, h, h_ratio);
			int bg = average_pixel(data, x, y, w, h, h_ratio);
			tb_set_cell(_x, _y + offsety, 0x2584, fg, bg);
			y += h_ratio;
			_y++;
		}
		_y = 0;
		_x++;
	}
	return 0;
}
#else
typedef int hide_warning;
#endif
