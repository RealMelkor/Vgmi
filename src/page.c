/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#define PAGE_INTERNAL
#include "page.h"
#include "termbox.h"
#include "error.h"
#include "macro.h"

int page_display(struct page text, int from, int to, int selected) {
	int y;
	from--;
	for (y = from; y < (ssize_t)text.length && y < from + to; y++) {
		size_t i;
		int x;
		x = OFFSETX;
		if (y < 0) continue;
		for (i = 0; i < text.lines[y].length; i++) {
			struct page_cell cell = text.lines[y].cells[i];
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

int page_update(int in, int out, const char *data, size_t length,
			struct page *page) {

	int ret;
	size_t i, pos, cells;
	struct page_line line = {0};

	/* clean up previous */
	for (i = 0; i < page->length; i++) {
		free(page->lines[i].cells);
	}
	free(page->lines);

	page->length = 0;
	page->lines = NULL;

	line.cells =
		malloc((page->width + 1) * sizeof(struct page_cell));
	if (!line.cells) return ERROR_MEMORY_FAILURE;

	cells = pos = ret = 0;
	while (!ret) {

		struct page_cell cell;
		void *tmp;
		int len;

		if (pos < length && cells >= pos / 2) {
			int bytes = BLOCK_SIZE;
			if (pos + bytes > length) bytes = length - pos;
			write(out, &data[pos], bytes);
			pos += bytes;
		}

		len = read(in, P(cell));
		cells++;
		if (len != sizeof(cell)) {
			ret = -1;
			break;
		}

		switch (cell.special) {
		case PAGE_EOF: break;
		case PAGE_BLANK: break;
		case PAGE_RESET:
			cells = pos / 2;
			break;
		case PAGE_NEWLINE:
		{
			struct page_line *ptr;
			size_t length;

			tmp = realloc(page->lines,
					(page->length + 1) * sizeof(line));
			if (!tmp) {
				free(page->lines);
				ret = ERROR_MEMORY_FAILURE;
				break;
			}
			page->lines = tmp;

			length = sizeof(struct page_cell) * line.length;
			ptr = &page->lines[page->length];
			ptr->cells = malloc(length);
			if (!ptr->cells) {
				free(page->lines);
				ret = ERROR_MEMORY_FAILURE;
				break;
			}
			memcpy(ptr->cells, line.cells, length);
			ptr->length = line.length;

			line.length = 0;
			page->length++;
		}
			break;
		default:
			if (line.length >= (size_t)page->width) break;
			line.cells[line.length] = cell;
			line.length++;
			break;
		}
		if (cell.special == PAGE_EOF) break;
	}
	free(line.cells);
	return ret;
}

int page_free(struct page page) {
	size_t i;
	for (i = 0; i < page.length; i++) {
		free(page.lines[i].cells);
	}
	free(page.lines);
	for (i = 0; i < page.links_count; i++) {
		free(page.links[i]);
	}
	free(page.links);
	return 0;
}
