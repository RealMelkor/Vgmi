/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "macro.h"
#include "client.h"
#define GEMTEXT_INTERNAL
#define GEMTEXT_INTERNAL_FUNCTIONS
#include "gemtext.h"
#include "termbox.h"
#include "wcwidth.h"
#include "error.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "utf8.h"

#define TAB_SIZE 4

#define GEMTEXT_NEWLINE (uint32_t)-1
#define GEMTEXT_EOF (uint32_t)-2

enum {
	LINE_TEXT,
	LINE_HEADER,
	LINE_SUBHEADER,
	LINE_SUBSUBHEADER,
	LINE_BLOCKQUOTE,
	LINE_LIST,
	LINE_LINK,
	LINE_PREFORMATTED
};

static int colorFromLine(int line) {
	switch (line) {
	case LINE_TEXT: return TB_DEFAULT;
	case LINE_HEADER: return TB_RED;
	case LINE_SUBHEADER: return TB_GREEN;
	case LINE_SUBSUBHEADER: return TB_YELLOW;
	case LINE_BLOCKQUOTE: return TB_MAGENTA;
	case LINE_LIST: return TB_CYAN;
	case LINE_LINK: return TB_BLUE;
	case LINE_PREFORMATTED: return TB_WHITE;
	default: return TB_DEFAULT;
	}
}

static int nextHeader(int header) {
	switch (header) {
	case LINE_TEXT: return LINE_HEADER;
	case LINE_HEADER: return LINE_SUBHEADER;
	case LINE_SUBHEADER: return LINE_SUBSUBHEADER;
	default: return header;
	}
}

static int renderable(uint32_t codepoint) {
	return !(codepoint == 0xFEFF ||
		(codepoint < ' ' && codepoint != '\n'));
}

int gemtext_free(struct gemtext gemtext) {
	size_t i;
	for (i = 0; i < gemtext.length; i++) {
		free(gemtext.lines[i].cells);
	}
	free(gemtext.lines);
	for (i = 0; i < gemtext.links_count; i++) {
		free(gemtext.links[i]);
	}
	free(gemtext.links);
	return 0;
}

#define PARSE_LINE_COMPLETED 0
#define PARSE_LINE_SKIP 1
#define PARSE_LINE_BROKEN 2
int gemtext_parse_line(const char **ptr, size_t length, int *_color, int width,
			int *links, int *preformatted, int fd) {

	int x, ret;
	int color = *_color;
	const char *data = *ptr;
	const char *end = *ptr + length;
	const char *link_tmp = NULL;
	char link[64];
	const char *next_separator = NULL;

	if (*preformatted) color = colorFromLine(LINE_PREFORMATTED);
	if (width < 7) width = 7;

	x = 0;
	ret = PARSE_LINE_COMPLETED;
	while (link_tmp || data < end) {

		struct gemtext_cell cell = {0};
		int len;

		if (!*data && link_tmp) {
			data = link_tmp;
			link_tmp = NULL;
			color = colorFromLine(LINE_TEXT);
			while (data < end && WHITESPACE(*data)) data++;
			continue;
		}

		len = utf8_char_to_unicode(&cell.codepoint, data);
		if (cell.codepoint == '\n') break;
		cell.width = mk_wcwidth(cell.codepoint);
		if (cell.width >= width) cell.width = 1;
		if (cell.codepoint == '\t') {
			cell.codepoint = ' ';
			cell.width = TAB_SIZE - (x % TAB_SIZE);
		}
		if (!renderable(cell.codepoint)) {
			data += len;
			continue;
		}
		if (link_tmp) {
			cell.link = (*links);
		}
		x += cell.width;
		if (x > width) {
			ret = PARSE_LINE_BROKEN;
			break;
		}
		if (!link_tmp && next_separator < data) {
			int nextX = x;
			const char *ptr = data;
			while (ptr < end && !SEPARATOR(*ptr)) {
				uint32_t ch;
				int w;
				ptr += utf8_char_to_unicode(&ch, ptr);
				w = mk_wcwidth(ch);
				if (w >= width) w = 1;
				nextX += w;
			}
			if (nextX - x < width && nextX > width) {
				ret = PARSE_LINE_BROKEN;
				break;
			}
			next_separator = ptr;
		}
		if (*ptr == data && cell.codepoint == '`' &&
				data[1] == '`' && data[2] == '`') {
			*preformatted = !*preformatted;
			if (*preformatted) {
				color = colorFromLine(LINE_PREFORMATTED);
			} else {
				color = colorFromLine(LINE_TEXT);
			}
			data = data + 3;
			if (*data == '\n') {
				ret = PARSE_LINE_SKIP;
				data++;
				break;
			}
			continue;
		}
		if (!*preformatted && *ptr == data) { /* start of the line */
			switch (cell.codepoint) {
			case '=':
				if (data[1] != '>') break;
				data += 2;
				while (data < end && WHITESPACE(*data))
					utf8_next(&data);
				if (*data == '\n') {
					data = *ptr;
					break;
				}
				while (data < end && !SEPARATOR(*data))
					utf8_next(&data);
				while (data < end && WHITESPACE(*data)) {
					if (*data == '\n') break;
					utf8_next(&data);
				}
				/* link without label */
				if (data >= end || *data == '\n') {
					data = *ptr + 2;
					while (data < end && WHITESPACE(*data))
						utf8_next(&data);
					tb_shutdown();
				}
				len = 0;
				color = colorFromLine(LINE_LINK);
				link_tmp = data;
				data = link;
				(*links)++;
				{
					int i, j;
					i = snprintf(V(link), "[%d]", *links);
					if (i < 6) j = 6;
					else j = i + 1;
					for (; i < j; i++)
						link[i] = ' ';
					link[i] = '\0';
				}
				continue;
			case '>':
				color = colorFromLine(LINE_BLOCKQUOTE);
				break;
			case '*':
				color = colorFromLine(LINE_LIST);
				break;
			case '#':
				color = LINE_HEADER;
				while (*(++data) == '#' && data < end)
					color = nextHeader(color);
				color = colorFromLine(color);
				data = *ptr;
				break;
			}
		}
		data += len;
		cell.color = color;
		write(fd, P(cell));
	}
	(*ptr) = data;
	*_color = color;

	return ret;
}

int gemtext_parse(const char *data, size_t length, int width, int fd) {

	const char *end = data + length;
	int color, links, preformatted;

	data = strnstr(data, "\r\n", length);
	if (!data) return ERROR_INVALID_DATA;
	data += 2;

	color = LINE_TEXT;
	preformatted = links = 0;
	while (data < end) {
		int ret = gemtext_parse_line(&data, end - data, &color,
				width - OFFSETX, &links, &preformatted, fd);

		if (ret == PARSE_LINE_SKIP) continue;
		if (ret == PARSE_LINE_COMPLETED) {
			color = LINE_TEXT;
			data++;
		}

		{
			struct gemtext_cell newline = {0};
			newline.codepoint = GEMTEXT_NEWLINE;
			write(fd, P(newline));
		}
	}

	{
		struct gemtext_cell eof = {0};
		eof.codepoint = GEMTEXT_EOF;
		write(fd, P(eof));
	}

	return 0;
}

int gemtext_update(int fd, struct gemtext *gemtext) {

	int ret;
	size_t i;
	struct gemtext_line line = {0};

	/* clean up previous */
	for (i = 0; i < gemtext->length; i++) {
		free(gemtext->lines[i].cells);
	}
	free(gemtext->lines);

	gemtext->length = 0;
	gemtext->lines = NULL;

	ret = 0;
	while (1) {

		struct gemtext_cell cell;
		void *tmp;
		int len;

		len = read(fd, P(cell));
		if (len != sizeof(cell)) {
			ret = -1;
			break;
		}
		if (cell.codepoint == GEMTEXT_EOF) break;
		if (cell.codepoint == GEMTEXT_NEWLINE) {
			tmp = realloc(gemtext->lines,
					(gemtext->length + 1) * sizeof(line));
			if (!tmp) {
				free(gemtext->lines);
				ret = ERROR_MEMORY_FAILURE;
				break;
			}
			gemtext->lines = tmp;
			gemtext->lines[gemtext->length] = line;
			gemtext->length++;
			memset(&line, 0, sizeof(line));
			continue;
		}
		tmp = realloc(line.cells, (line.length + 1) * sizeof(cell));
		if (!tmp) {
			ret = ERROR_MEMORY_FAILURE;
			free(line.cells);
			break;
		}
		line.cells = tmp;
		line.cells[line.length] = cell;
		line.length++;
	}
	return ret;
}
