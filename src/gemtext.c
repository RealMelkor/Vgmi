/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "client.h"
#define GEMTEXT_INTERNAL
#include "gemtext.h"
#include "termbox.h"
#include "wcwidth.h"
#include "error.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "utf8.h"
#include "macro.h"

#define TAB_SIZE 4

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

static int whitespace(char c) {
	return c == ' ' || c == '\t';
}

static int separator(char c) {
	return whitespace(c) || c == '\n';
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

int renderable(uint32_t codepoint) {
	if (codepoint == 0xFEFF) return 0;
	if (codepoint < ' ' && !whitespace(codepoint)) return 0;
	return 1;
}

int gemtext_parse_line(const char **ptr, size_t length, int color, int width,
			struct gemtext_line *line, int *links,
			int *preformatted) {

	int x;
	const char *data = *ptr;
	const char *end = *ptr + length;
	const char *link_tmp = NULL;
	char link[64];
	const char *next_separator = NULL;
	if (*preformatted) color = colorFromLine(LINE_PREFORMATTED);

	line->length = 0;
	line->cells = NULL;

	x = 0;
	while (link_tmp || data < end) {

		struct gemtext_cell cell = {0};
		void *tmp;
		int len;

		if (!*data && link_tmp) {
			data = link_tmp;
			link_tmp = NULL;
			color = colorFromLine(LINE_TEXT);
			while (whitespace(*data)) data++;
			continue;
		}

		len = utf8_char_to_unicode(&cell.codepoint, data);
		if (cell.codepoint == '\n') break;
		cell.width = mk_wcwidth(cell.codepoint);
		if (cell.codepoint == '\t') {
			cell.codepoint = ' ';
			cell.width = TAB_SIZE - x % TAB_SIZE;
		}
		if (!renderable(cell.codepoint)) {
			data += len;
			continue;
		}
		if (link_tmp) {
			cell.link = (*links);
		}
		x += cell.width;
		if (x > width) break;
		if (!link_tmp && next_separator < data) {
			int nextX = x;
			const char *ptr = data;
			while (ptr < end && !separator(*ptr)) {
				uint32_t ch;
				ptr += utf8_char_to_unicode(&ch, ptr);
				nextX += mk_wcwidth(ch);
			}
			if (nextX - x < width && nextX > width) break;
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
				line->length = -1;
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
				while (whitespace(*(utf8_next(&data)))) ;
				while (data < end && !separator(*data))
					utf8_next(&data);
				/* link without label */
				if (data >= end || *data == '\n') {
					data = *ptr + 2;
					while (whitespace(*(data)))
						utf8_next(&data);
				} else data++;
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
		tmp = realloc(line->cells, sizeof(struct gemtext_cell) *
				(line->length + 1));
		if (!tmp) {
			free(line->cells);
			return ERROR_MEMORY_FAILURE;
		}
		line->cells = tmp;
		line->cells[line->length++] = cell;
	}
	(*ptr) = data;

	return 0;
}

int gemtext_parse(const char *data, size_t length,
		int width, struct gemtext *gemtext) {

	const char *end = data + length;
	int color, links, preformatted;
	size_t i;

	/* clean up previous */
	for (i = 0; i < gemtext->length; i++) {
		free(gemtext->lines[i].cells);
	}
	free(gemtext->lines);

	gemtext->length = 0;
	gemtext->lines = NULL;
	gemtext->width = width;

	data = strnstr(data, "\r\n", length);
	if (!data) return ERROR_INVALID_DATA;
	data += 2;

	color = LINE_TEXT;
	preformatted = links = 0;
	while (data < end) {
		void *tmp;
		struct gemtext_line line;
		if (gemtext_parse_line(&data, end - data, color,
					width - OFFSETX, &line,
					&links, &preformatted)) {
			return ERROR_MEMORY_FAILURE;
		}
		if (line.length == (size_t)-1) continue;
		if (*data == '\n' || line.length == 0) {
			color = LINE_TEXT;
			data++;
		} else color = line.cells[line.length - 1].color;
		tmp = realloc(gemtext->lines, sizeof(struct gemtext_line) *
				(gemtext->length + 1));
		if (!tmp) {
			free(gemtext->lines);
			return ERROR_MEMORY_FAILURE;
		}
		gemtext->lines = tmp;
		gemtext->lines[gemtext->length++] = line;
	}
	return 0;
}

static int format_link(const char *link, size_t length,
			char *out, size_t out_length) {
	int i = 0, j = 0;
	uint32_t prev = 0;
	while (link[i]) {
		uint32_t ch;
		int len;
		len = utf8_char_to_unicode(&ch, &link[i]);
		if ((prev == '/' || prev == 0) && ch == '.') {
			if (link[i + len] == '/') {
				j -= 1;
				i += len;
				continue;
			} else if (link[i + len] == '.' &&
					link[i + len + 1] == '/'){
				j -= 2;
				if (j < 0) j = 0;
				while (out[j] != '/' && j)
					j = utf8_previous(out, j);
				i += len + 1;
				continue;
			}
		}
		if (i + len >= (ssize_t)length ||
				j + len >= (ssize_t)out_length) {
			out[j] = '\0';
			break;
		}
		if (ch < ' ') ch = '\0';
		memcpy(&out[j], &link[i], len);
		i += len;
		j += len;
		prev = ch;
	}
	out[j] = '\0';
	return j;
}

int gemtext_links(const char *data, size_t length,
			char ***out, size_t *out_length) {

	char **links = NULL;
	int count = 0;
	int newline = 1;
	size_t i;

	i = strnstr(data, "\r\n", length) - data;

	for (i = 0; i < length; ) {

		uint32_t ch;
		size_t j;

		j = utf8_char_to_unicode(&ch, &data[i]);
		if (j < 1) j = 1;
		i += j;

		if (newline && ch == '=' && data[i] == '>') {
			char url[MAX_URL], buf[MAX_URL], **tmp;
			i++;
			j = 0;
			while (whitespace(data[i]) && i < length)
				i += utf8_char_length(data[i]);
			while (!separator(data[i]) && i < length &&
					j < sizeof(url)) {
				uint32_t ch;
				int len;
				len = utf8_char_to_unicode(&ch, &data[i]);
				if (ch < ' ') ch = '\0';
				memcpy(&url[j], &ch, len);
				j += len;
				i += len;
			}
			url[j] = '\0';
			tmp = realloc(links, sizeof(char*) * (count + 1));
			if (!tmp) {
				free(links);
				return ERROR_MEMORY_FAILURE;
			}
			j = format_link(V(url), V(buf));
			links = tmp;
			links[count] = malloc(j + 1);
			if (!links[count]) {
				free(links);
				return ERROR_MEMORY_FAILURE;
			}
			strlcpy(links[count], buf, j + 1);
			count++;
		}

		newline = ch == '\n';
	}
	if (out_length) *out_length = count;
	if (out) *out = links;
	return 0;
}
