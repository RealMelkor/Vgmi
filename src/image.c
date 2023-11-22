/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include "image.h"
#ifdef ENABLE_IMAGE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include "sandbox.h"
#include "termbox.h"
#include "error.h"
#include "proc.h"
#include "macro.h"
#include "config.h"

static int _write(int fd, char *data, int length) {
	int i;
	for (i = 0; i < length;) {
		int bytes = write(fd, &data[i], length - i);
		if (bytes < 1) return -1;
		i += bytes;
	}
	return 0;
}

static int _read(int fd, void *ptr, int length) {
	int i;
	char *data = ptr;
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	for (i = 0; i < length;) {
		int bytes;
		if (!poll(&pfd, 1, 3000)) return -1;
		bytes = read(fd, &data[i], length);
		if (bytes < 1) return -1;
		i += bytes;
	}
	return 0;
}

static int __read(int fd, void *ptr, int length) {
	int i;
	char *data = ptr;
	for (i = 0; i < length;) {
		int bytes;
		bytes = read(fd, &data[i], length);
		if (bytes < 1) return -1;
		i += bytes;
	}
	return 0;
}

static int empty_pipe(int fd) {
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	while (poll(&pfd, 1, 0)) {
		uint8_t byte;
		if (read(fd, &byte, 1) != 1) return -1;
	}
	return 0;
}

void image_parser(int in, int out) {

	char *data;
	size_t length;
	uint8_t byte;

	data = malloc(config.imageParserScratchPad);
	if (!data) return;
	memset(data, 0, config.imageParserScratchPad);
	image_memory_set(data, config.imageParserScratchPad);

	length = config.imageParserScratchPad;
	data = malloc(length);
	if (!data) return;
	memset(data, 0, length);

	sandbox_set_name("vgmi [image]");
	sandbox_isolate();

	byte = 0;
	write(out, &byte, 1);

	while (1) {

		int x, y, size;
		void *img;

		img = NULL;
		size = y = x = 0;

		if (read(in, P(size)) != sizeof(size)) break;
		if ((unsigned)size > length || size == 0) goto fail;
		write(out, P(size));
		if (__read(in, data, size)) goto fail;
		img = image_load(data, size, &x, &y);
		if (!img) goto fail;
		write(out, P(x));
		write(out, P(y));
		if (_write(out, img, x * y * 3)) break;
		continue;
fail:
		size = -1;
		write(out, P(size));
	}
}

#define OK(X) if (X) return NULL
int image_fd_out = -1;
int image_fd_in = -1;
void *image_parse(void *data, int len, int *x, int *y) {
	int _x, _y, size;
	void *img;
	if (image_fd_out == -1 || image_fd_in == -1) return NULL;
	empty_pipe(image_fd_in);
	write(image_fd_out, P(len));
	OK(_read(image_fd_in, P(size)));
	OK(size != len);
	OK(_write(image_fd_out, data, len));
	OK(_read(image_fd_in, P(_x)));
	OK(_x == -1);
	OK(_read(image_fd_in, P(_y)));
	size = _x * _y * 3;
	OK(size < 1 || size > config.imageParserScratchPad);
	img = malloc(size);
	if (!img || _read(image_fd_in, img, size)) {
		free(img);
		return NULL;
	}
	*x = _x;
	*y = _y;
	return img;
}

int image_init() {
	if (!config.enableImage) return 0;
	return proc_fork("--image", &image_fd_in, &image_fd_out);
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
