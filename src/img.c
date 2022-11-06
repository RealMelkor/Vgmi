/* See LICENSE file for copyright and license details. */
#if defined(TERMINAL_IMG_VIEWER) && defined(__has_include) && \
    !(__has_include(<stb_image.h>))
#warning Unable to build built-in image viewer, stb_image header not found
#endif
#include "img.h"

#ifdef TERMINAL_IMG_VIEWER
#include <termbox.h>
#include "gemini.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

int color_abs(int c, int x, int i) {
	return abs(c -= i > 215?i * 10 - 2152:x * 40 + !!x * 55);
}

int color(uint8_t r, uint8_t g, uint8_t b) {
	int m = 0, t = 0;
	int i = 240, l = 240;
	while (i--) {
		t = color_abs(r, i/36, i) +
		    color_abs(g, i/6%6, i) +
		    color_abs(b, i%6, i);
		if (t < l) {
			l = t;
			m = i;
		} 
	}
	i=m+16;
	return i;
}

int color8(int r, int g, int b) {
	if (r < 20 && g < 20 && b < 20)
		return TB_BLACK;
	if (r > 150 && g > 150 && b > 150 && r + g + b > 490)
		return TB_WHITE;
	if (r > (b + 10) * 2 && g > (b + 10) * 2 && g - r <= 10 * (g > r?1:-1))
		return TB_YELLOW;
	if (b > (r + 10) && g > (r + 10) && b > 80 && g > 80)
		return TB_CYAN;
	if (g > (r + b + 50)/2)
		return TB_GREEN;
	if (r > g + 30 && b > g + 30) // purple
		return TB_MAGENTA;
	if (b > (r + g + 50)/2)
		return TB_BLUE;
	if (r > g + 30 && g > b + 30 && b > 30 && g < b * 2) // brown
		return TB_WHITE;
	if (r > (g + b + 50)/2)
		return TB_RED;
	if (r > 120 && g > 60 && b > 130 && g < r && g < b)
		return TB_MAGENTA;
	if (r + g + b > 380)
		return TB_WHITE;
	return TB_BLACK;
}

int average_pixel(uint8_t* data, int x, int y, int w, int h,
		  float ratio, int c8) {
	int pixels = 0;
	int r = 0, g = 0, b = 0;
	float precision = 2;
	for (int i = x - ratio/precision; i < x + ratio/precision; i++) {
		if (i >= w || i < 0) continue;
		for (int j = y - ratio/precision;
		     j < y + ratio/precision; j++) {
			if (j >= h || j < 0) continue;
			r += data[(i + j * w) * 3];
			g += data[(i + j * w) * 3 + 1];
			b += data[(i + j * w) * 3 + 2];
			pixels++;
		}
	}
	if (pixels == 0) return 0;
	return c8?color8(r/pixels, g/pixels, b/pixels):
		   color(r/pixels, g/pixels, b/pixels);
}

int img_display(uint8_t* data, int w, int h, int offsety) {
	int c8 = !client.c256;

	double w_ratio = (double)w/(tb_width());
	double h_ratio = (double)h/((tb_height() - offsety - 2) * 2);
	if (w_ratio < h_ratio)
		w_ratio = h_ratio;
	else
		h_ratio = w_ratio;
	int _x = 0;
	int _y = 0;
	for (double x = 0; x < (double)w; x += w_ratio) {
		for (double y = 0; y < (double)h; y += h_ratio) {
			int fg = average_pixel(data, x, y + 1, w, h,
					       h_ratio, c8);
			int bg = average_pixel(data, x, y, w, h,
					       h_ratio, c8);
			tb_print(_x, _y+offsety,
				fg,
				bg,
				"▄");
			y += h_ratio;
			_y++;
		}
		_y = 0;
		_x++;
	}
	return 0;
}

#else
typedef int remove_iso_warning;
#endif
