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

#define GEMTEXT_NEWLINE	(uint8_t)1
#define GEMTEXT_EOF	(uint8_t)2
#define GEMTEXT_BLANK	(uint8_t)3
#define BLOCK_SIZE 4096

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
		(codepoint < ' ' && codepoint != '\n' && codepoint != '\t'));
}

int readnext(int fd, uint32_t *ch, size_t *pos) {

	char buf[64];
	int len;

	if (read(fd, buf, 1) != 1) return -1;
	len = utf8_char_length(*buf);
	if (len > 1 && read(fd, &buf[1], len - 1) != len - 1) return -1;
	if (len > 0 && pos) *pos += len;
	utf8_char_to_unicode(ch, buf);
	return 0;
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

int writeto(int out, const char *str, int color, int link) {
	const char *ptr = str;
	while (*ptr) {
		struct gemtext_cell cell = {0};
		cell.codepoint = *ptr;
		cell.link = link;
		cell.width = mk_wcwidth(cell.codepoint);
		cell.color = color;
		if (write(out, P(cell)) != sizeof(cell)) return -1;
		ptr++;
	}
	return 0;
}

static int send_blank(int out, int count) {
	int i;
	struct gemtext_cell cell = {0};
	cell.special = GEMTEXT_BLANK;
	for (i = 0; i < count; i ++) {
		if (write(out, P(cell)) != sizeof(cell)) return -1;
	}
	return 0;
}

static int prevent_deadlock(size_t pos, size_t *initial_pos, int fd) {
	if (pos - *initial_pos > BLOCK_SIZE / 2) {
		if (send_blank(fd, BLOCK_SIZE / 2)) return -1;
		*initial_pos = pos;
	}
	return 0;
}

int gemtext_parse_link(int in, size_t *pos, size_t length,
			int *links, int out, int *x, uint32_t *ch) {

	char buf[32];
	struct gemtext_cell cell = {0}, cells[MAX_URL] = {0};
	size_t initial_pos = *pos;
	int i, j, rewrite;

	/* readnext till finding not a space */ 
	while (*pos < length) {
		prevent_deadlock(*pos, &initial_pos, out);
		if (readnext(in, &cell.codepoint, pos)) return -1;
		if (!WHITESPACE(cell.codepoint)) break;
	}

	/* if ch is newline, abort since not a valid link */
	if (cell.codepoint == '\n') {
		writeto(out, "=>", colorFromLine(LINE_TEXT), 0);
		*ch = '\n';
		return 0;
	}
	/* readnext till find a space or newline */ 
	cells[0] = cell;
	for (i = 1; i < MAX_URL && *pos < length; i++) {
		prevent_deadlock(*pos, &initial_pos, out);
		if (readnext(in, &cells[i].codepoint, pos)) return -1;
		if (!renderable(cells[i].codepoint)) {
			i--;
			continue;
		}
		if (SEPARATOR(cells[i].codepoint)) break;
	}

	if (i == MAX_URL) { /* invalid url */
		*ch = 0;
		/* read remaining bytes of the invalid url */
		while (*ch != '\n' && *pos < length)
			if (readnext(in, ch, pos)) return -1;
		return 0;
	}

	/* if finding space ignore what was read */
	rewrite = (cells[i].codepoint == '\n' || *pos >= length) ? i : 0;

	*ch = rewrite ? '\n' : 0;
	for (j = 0; !rewrite && j < MAX_URL && *pos < length; j++) {
		prevent_deadlock(*pos, &initial_pos, out);
		if (readnext(in, ch, pos)) return -1;
		if (!WHITESPACE(*ch)) {
			if (*ch == '\n') rewrite = i;
			break;
		}
	}

	(*links)++;

	j = snprintf(V(buf), "[%d]", *links);
	writeto(out, buf, colorFromLine(LINE_LINK), *links);
	(*x) += j;

	cell.color = colorFromLine(LINE_TEXT);
	cell.width = 1;
	cell.link = 0;
	cell.codepoint = ' ';
	for (; j < 6; j++) {
		write(out, P(cell));
		(*x)++;
	}

	if (ch && !rewrite) {
		cell.codepoint = *ch;
		cell.width = mk_wcwidth(*ch);
		write(out, P(cell));
		(*x) += cell.width;
	}

	/* if finding newline reprint what was read */
	for (j = 0; j < rewrite; j++) {
		cells[j].color = colorFromLine(LINE_TEXT);
		cells[j].width = mk_wcwidth(cells[j].codepoint);
		cells[j].link = 0;
		cells[j].special = 0;
		write(out, P(cells[j]));
		(*x)++;
	}

	return 0;
}

#define PARSE_LINE_COMPLETED 0
#define PARSE_LINE_SKIP 1
#define PARSE_LINE_BROKEN 2
int gemtext_parse_line(int in, size_t *pos, size_t length, int *_color,
			int width, int *links, int *preformatted, int out,
			uint32_t *last) {

	int x, ret;
	int color = *_color, header = 0, link = 0, preformat = 0, start = 1;

	if (*preformatted) color = colorFromLine(LINE_PREFORMATTED);
	if (width < 7) width = 7;

	x = 0;
	ret = PARSE_LINE_COMPLETED;
	while (*pos < length) {

		struct gemtext_cell cell = {0};

		if (*last) {
			cell.codepoint = *last;
			*last = 0;
		} else if (readnext(in, &cell.codepoint, pos)) return -1;
		if (cell.codepoint == '\n') break;
		cell.width = mk_wcwidth(cell.codepoint);
		if (cell.width >= width) cell.width = 1;
		if (cell.codepoint == '\t') {
			cell.codepoint = ' ';
			cell.width = TAB_SIZE - (x % TAB_SIZE);
		}
		if (!renderable(cell.codepoint)) continue;
		
		x += cell.width;
		if (x > width) {
			ret = PARSE_LINE_BROKEN;
			*last = cell.codepoint;
			break;
		}

		/* TODO: reimplement proper line breaking */
		/*if (!link_tmp && next_separator < data) {
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
		*/

		if (header) {
			if (header < 3 && cell.codepoint == '#') {
				color = nextHeader(color);
				header++;
			} else {
				struct gemtext_cell header_cell = {0};
				header_cell.codepoint = '#';
				header_cell.width = 1;
				header_cell.color = colorFromLine(color);
				while (header) {
					write(out, P(header_cell));
					header--;
				}
			}
		}
		if (link) {
			if (cell.codepoint == '>') {
				uint32_t ch;
				int n;
				link = 0;
				n = gemtext_parse_link(in, pos, length, links,
							out, &x, &ch);
				if (n) return n;
				if (ch == '\n') break;
				continue;
			} else {
				struct gemtext_cell prev_cell = {0};
				prev_cell.codepoint = '=';
				prev_cell.width = 1;
				prev_cell.color = color;
				write(out, P(prev_cell));
				link = 0;
			}
		}
		if (preformat) {
			if (cell.codepoint == '`') {
				if (preformat < 3) {
					preformat++;
					continue;
				}
				*preformatted = !*preformatted;
				color = colorFromLine(*preformatted ?
						LINE_PREFORMATTED : LINE_TEXT);
				/* TODO: if next character is \n skip line */
				continue;
			} else {
				struct gemtext_cell prev_cell = {0};
				prev_cell.codepoint = '`';
				prev_cell.width = 1;
				prev_cell.color = colorFromLine(color);
				while (preformat) {
					write(out, P(prev_cell));
					preformat--;
				}
			}
		}

		/* start of the line */
		if ((cell.codepoint == '`' || !*preformatted) && start) {
			switch (cell.codepoint) {
			case '=':
				link = 1;
				break;
			case '`':
				preformat = 1;
				break;
			case '#':
				color = nextHeader(color);
				header = 1;
				break;
			case '>':
				color = LINE_BLOCKQUOTE;
				break;
			case '*':
				color = LINE_LIST;
				break;
			}
		}
		start = 0;
		if (!header && !link && !preformat) {
			cell.color = colorFromLine(color);
			write(out, P(cell));
		}
	}
	*_color = color;

	return ret;
}

int gemtext_parse(int in, size_t length, int width, int out) {

	int color, links, preformatted;
	size_t pos;
	uint32_t last;

	/* read till \r\n if not found return return ERROR_INVALID_DATA; */
	{
		uint32_t prev = 0;
		int found = 0;
		for (pos = 0; pos < length; ) {
			uint32_t ch;
			if (readnext(in, &ch, &pos)) return -1;
			if (ch == '\n' && prev == '\r') {
				found = 1;
				break;
			}
			prev = ch;
		}
		if (!found) return ERROR_INVALID_DATA;
	}

	color = LINE_TEXT;
	preformatted = links = 0;
	last = 0;
	while (pos < length) {
		int ret = gemtext_parse_line(in, &pos, length, &color,
				width - OFFSETX, &links, &preformatted, out,
				&last);
		if (ret == -1) return -1;
		if (ret == PARSE_LINE_SKIP) continue;
		if (ret == PARSE_LINE_COMPLETED) {
			color = LINE_TEXT;
			last = 0;
		}

		{
			struct gemtext_cell newline = {0};
			newline.special = GEMTEXT_NEWLINE;
			write(out, P(newline));
		}
	}

	{
		struct gemtext_cell eof = {0};
		eof.special = GEMTEXT_EOF;
		write(out, P(eof));
	}

	return 0;
}

int gemtext_update(int in, int out, const char *data, size_t length,
			struct gemtext *gemtext) {

	int ret;
	size_t i, pos, cells;
	struct gemtext_line line = {0};

	/* clean up previous */
	for (i = 0; i < gemtext->length; i++) {
		free(gemtext->lines[i].cells);
	}
	free(gemtext->lines);

	gemtext->length = 0;
	gemtext->lines = NULL;

	cells = pos = ret = 0;
	while (1) {

		struct gemtext_cell cell;
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
		if (cell.special == GEMTEXT_EOF) break;
		if (cell.special == GEMTEXT_BLANK) continue;
		if (cell.special == GEMTEXT_NEWLINE) {
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
