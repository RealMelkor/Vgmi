/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include "client.h"
#define PAGE_INTERNAL
#include "page.h"
#include "termbox.h"
#include "error.h"
#include "macro.h"
#include "memory.h"

int page_display(struct page text, int from, struct rect rect, int selected) {
	int y;
	int to = rect.h - rect.y;
	from--;
	for (y = from; y < (ssize_t)text.length && y < from + to; y++) {
		size_t i;
		int x;
		x = rect.x + OFFSETX;
		if (y < 0) continue;
		for (i = 0; i < text.lines[y].length; i++) {
			struct page_cell cell = text.lines[y].cells[i];
			int fg = cell.color, bg = TB_DEFAULT;
			if (selected && cell.link == (uint32_t)selected) {
				fg = TB_RED;
			}
			if (cell.selected) {
				if (cell.selected == text.selected) {
					bg = TB_WHITE;
					if (!fg || fg == TB_WHITE ||
							fg == TB_DEFAULT)
						fg = TB_YELLOW;
				} else bg = TB_YELLOW;
			}
			tb_set_cell(x, rect.y + y - from, cell.codepoint,
					fg, bg);
			x += cell.width;
		}
	}
	return 0;
}

int page_update(int in, int out, const char *data, size_t length,
			struct page *page) {

	int ret;
	size_t i, bytes_read, bytes_sent, cells_read;
	struct page_line line = {0};
	struct pollfd pfd;

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

	cells_read = bytes_sent = bytes_read = ret = 0;
	pfd.fd = in;
	pfd.events = POLLIN;

	while (!ret) {

		struct page_cell cell;
		void *tmp;
		int len;

		if (!poll(&pfd, 1, 0)) {
			int bytes = PARSER_CHUNK;
			if (bytes_sent + bytes > length)
				bytes = length - bytes_sent;
			write(out, &data[bytes_sent], bytes);
			bytes_sent += bytes;
			continue;
		}

		len = read(in, P(cell));
		if (len != sizeof(cell)) {
			ret = -1;
			break;
		}
		cells_read++;

		switch (cell.special) {
		case PAGE_EOF: break;
		case PAGE_BLANK: break;
		case PAGE_RESET:
			bytes_read = cell.codepoint;
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
		free_readonly(page.links[i], strnlen(page.links[i], MAX_URL));
	}
	free(page.links);
	free(page.img);
	return 0;
}
