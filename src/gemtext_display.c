/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdint.h>
#define GEMTEXT_INTERNAL
#include "gemtext.h"
#include "termbox.h"
#include "error.h"
#include "macro.h"

int gemtext_display(struct gemtext text, int from, int to, int selected) {
	int y;
	from--;
	for (y = from; y < (ssize_t)text.length && y < from + to; y++) {
		size_t i;
		int x;
		x = OFFSETX;
		if (y < 0) continue;
		for (i = 0; i < text.lines[y].length; i++) {
			struct gemtext_cell cell = text.lines[y].cells[i];
			int color = cell.color;
			if (selected && cell.link == selected) {
				color = TB_RED;
			}
			tb_set_cell(x, y - from, cell.codepoint,
					color, TB_DEFAULT);
			x += cell.width;
		}
	}
	return 0;
}
