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
#include <sys/socket.h>
#include "sandbox.h"
#include "termbox.h"
#include "error.h"
#include "macro.h"

#define IMG_MEMORY 1024 * 1024 * 16

void image_parser(int fd, char *data, size_t length) {
	while (1) {

		int x, y, size;
		void *img;

		img = NULL;
		size = y = x = 0;

		if (read(fd, P(size)) != sizeof(size)) break;
		if ((unsigned)size > length || size == 0) goto fail;
		write(fd, P(size));
		if (read(fd, data, size) != size) goto fail;
		img = image_load(data, size, &x, &y);
		if (!img) goto fail;
		write(fd, P(x));
		write(fd, P(y));
		write(fd, img, x * y * 3);
		continue;
fail:
		size = -1;
		write(fd, P(size));
	}
}

int image_fd = -1;
void *image_parse(void *data, int len, int *x, int *y) {
	int _x, _y, size;
	void *img;
	if (image_fd == -1) return NULL;
	write(image_fd, P(len));
	read(image_fd, P(size));
	if (size == -1) return NULL;
	write(image_fd, data, len);
	read(image_fd, P(_x));
	if (_x == -1) return NULL;
	read(image_fd, P(_y));
	size = _x * _y * 3;
	img = malloc(size);
	if (!img) {
		uint8_t byte;
		int i;
		for (i = 0; i < size; i++) read(image_fd, &byte, 1);
		return NULL;
	}
	if (read(image_fd, img, size) != size) {
		free(img);
		return NULL;
	}
	*x = _x;
	*y = _y;
	return img;
}

int image_init() {

	int fd[2];
	uint8_t byte;
	void *memory;

	socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
	if (fork()) {
		close(fd[0]);
		byte = -1;
		read(fd[1], &byte, 1);
		if (byte) return ERROR_MEMORY_FAILURE;
		image_fd = fd[1];
		return 0;
	}
	close(fd[1]);
	byte = 0;
	write(fd[0], &byte, 1);
	memory = malloc(IMG_MEMORY);
	if (!memory) goto exit;
	memset(memory, 0, IMG_MEMORY);
	image_memory_set(memory, IMG_MEMORY);
	memory = malloc(IMG_MEMORY);
	if (!memory) goto exit;
	memset(memory, 0, IMG_MEMORY);
	sandbox_set_name("vgmi [image]");
	sandbox_isolate();
	image_parser(fd[0], memory, IMG_MEMORY);
exit:
	close(fd[0]);
	exit(0);
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
