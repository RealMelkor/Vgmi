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
#define GEMTEXT_RESET	(uint8_t)4
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

struct termwriter {
	struct gemtext_cell cells[1024]; /* maximum cells for a line */
	size_t pos;
	size_t width;
	int fd;
};

/* manage lines breaking */
static int writecell(struct termwriter *termwriter, struct gemtext_cell cell) {
	size_t i, x, nextspace, nextspace_x;
	if (cell.special == GEMTEXT_BLANK) {
		return write(termwriter->fd, P(cell)) != sizeof(cell);
	}
	if (!cell.special && termwriter->pos < LENGTH(termwriter->cells)) {
		termwriter->cells[termwriter->pos] = cell;
		termwriter->pos++;
		return 0;
	}
	nextspace = 0;
	nextspace_x = 0;
	x = 0;
	for (i = 0; i < termwriter->pos; i++) {
		if (i >= nextspace) {
			size_t j;
			nextspace_x = x;
			for (j = i + 1; j < termwriter->pos; j++) {
				nextspace_x += termwriter->cells[j].width;
				if (WHITESPACE(termwriter->cells[j].codepoint))
					break;
			}
			if (nextspace_x >= termwriter->width) {
				struct gemtext_cell cell = {0};
				cell.special = GEMTEXT_NEWLINE;
				write(termwriter->fd, P(cell));
				x = 0;
				nextspace = i;
				continue;
			}
			nextspace = j;
		}
		if (termwriter->cells[i].codepoint == '\t') {
			termwriter->cells[i].width = TAB_SIZE - (x % TAB_SIZE);
			termwriter->cells[i].codepoint = ' ';
		}
		x += termwriter->cells[i].width;
		if (x > termwriter->width) {
			struct gemtext_cell cell = {0};
			cell.special = GEMTEXT_NEWLINE;
			write(termwriter->fd, P(cell));
			x = 0;
		}
		if (write(termwriter->fd, P(termwriter->cells[i])) !=
				sizeof(cell))
			return -1;
	}
	termwriter->pos = 0;
	return write(termwriter->fd, P(cell)) != sizeof(cell);
}

static int writeto(struct termwriter *termwriter, const char *str,
			int color, int link) {
	const char *ptr = str;
	while (*ptr) {
		struct gemtext_cell cell = {0};
		cell.codepoint = *ptr;
		cell.link = link;
		cell.width = mk_wcwidth(cell.codepoint);
		cell.color = color;
		if (writecell(termwriter, cell)) return -1;
		ptr++;
	}
	return 0;
}

static int prevent_deadlock(size_t pos, size_t *initial_pos, int fd) {
	if (pos - *initial_pos > BLOCK_SIZE / 2) {
		struct gemtext_cell cell = {0};
		cell.special = GEMTEXT_RESET;
		if (write(fd, P(cell)) != sizeof(cell)) return -1;
		*initial_pos = pos;
	}
	return 0;
}

/* TODO: verify that writecell and writeto return 0 */
int gemtext_parse_link(int in, size_t *pos, size_t length, int *links,
			struct termwriter *termwriter, uint32_t *ch) {

	char buf[32];
	struct gemtext_cell cell = {0}, cells[MAX_URL] = {0};
	size_t initial_pos = *pos;
	int i, j, rewrite;

	/* readnext till finding not a space */ 
	while (*pos < length) {
		prevent_deadlock(*pos, &initial_pos, termwriter->fd);
		if (readnext(in, &cell.codepoint, pos)) return -1;
		if (!WHITESPACE(cell.codepoint)) break;
	}

	/* if ch is newline, abort since not a valid link */
	if (cell.codepoint == '\n') {
		writeto(termwriter, "=>", colorFromLine(LINE_TEXT), 0);
		*ch = '\n';
		return 0;
	}
	/* readnext till find a space or newline */ 
	cells[0] = cell;
	for (i = 1; i < MAX_URL && *pos < length; i++) {
		prevent_deadlock(*pos, &initial_pos, termwriter->fd);
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
		prevent_deadlock(*pos, &initial_pos, termwriter->fd);
		if (readnext(in, ch, pos)) return -1;
		if (!WHITESPACE(*ch)) {
			if (*ch == '\n') rewrite = i;
			break;
		}
	}

	(*links)++;

	j = snprintf(V(buf), "[%d]", *links);
	writeto(termwriter, buf, colorFromLine(LINE_LINK), *links);

	cell.color = colorFromLine(LINE_TEXT);
	cell.width = 1;
	cell.link = 0;
	cell.codepoint = ' ';
	for (; j < 6; j++) {
		writecell(termwriter, cell);
	}

	if (ch && !rewrite) {
		cell.codepoint = *ch;
		cell.width = mk_wcwidth(*ch);
		writecell(termwriter, cell);
	}

	/* if finding newline reprint what was read */
	for (j = 0; j < rewrite; j++) {
		cells[j].color = colorFromLine(LINE_TEXT);
		cells[j].width = mk_wcwidth(cells[j].codepoint);
		cells[j].link = 0;
		cells[j].special = 0;
		writecell(termwriter, cells[j]);
	}

	return 0;
}

#define PARSE_LINE_COMPLETED 0
#define PARSE_LINE_SKIP 1
#define PARSE_LINE_BROKEN 2
int gemtext_parse_line(int in, size_t *pos, size_t length, int *_color,
			int *links, int *preformatted,
			struct termwriter *termwriter, uint32_t *last) {

	int ret;
	int color = *_color, header = 0, link = 0, preformat = 0, start = 1;

	if (*preformatted) color = colorFromLine(LINE_PREFORMATTED);

	ret = PARSE_LINE_COMPLETED;
	while (*pos < length) {

		struct gemtext_cell cell = {0};

		if (*last) {
			cell.codepoint = *last;
			*last = 0;
		} else if (readnext(in, &cell.codepoint, pos)) return -1;
		if (preformat == -1) {
			if (cell.codepoint == '\n') {
				ret = PARSE_LINE_SKIP;
				*last = 0;
				break;
			}
			preformat = 0;
		}
		if (cell.codepoint == '\n') break;
		cell.width = mk_wcwidth(cell.codepoint);
		if (cell.width >= termwriter->width) cell.width = 1;
		if (!renderable(cell.codepoint)) continue;
		
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
					writecell(termwriter, header_cell);
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
							termwriter, &ch);
				if (n) return n;
				if (ch == '\n') break;
				continue;
			} else {
				struct gemtext_cell prev_cell = {0};
				prev_cell.codepoint = '=';
				prev_cell.width = 1;
				prev_cell.color = color;
				writecell(termwriter, prev_cell);
				link = 0;
			}
		}
		if (preformat > 0) {
			if (cell.codepoint == '`') {
				if (preformat < 2) {
					preformat++;
					continue;
				}
				*preformatted = !*preformatted;
				color = colorFromLine(*preformatted ?
						LINE_PREFORMATTED : LINE_TEXT);
				preformat = -1;
				continue;
			} else {
				struct gemtext_cell prev_cell = {0};
				prev_cell.codepoint = '`';
				prev_cell.width = 1;
				prev_cell.color = colorFromLine(color);
				while (preformat) {
					writecell(termwriter, prev_cell);
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
			writecell(termwriter, cell);
		}
	}
	*_color = color;

	return ret;
}

int gemtext_parse(int in, size_t length, int width, int out) {

	int color, links, preformatted;
	size_t pos;
	uint32_t last;
	struct termwriter termwriter = {0};
	if (width < 10) width = 10;

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

	termwriter.width = width - OFFSETX;
	termwriter.fd = out;
	termwriter.pos = 0;

	color = LINE_TEXT;
	preformatted = links = 0;
	last = 0;
	while (pos < length) {
		int ret = gemtext_parse_line(in, &pos, length, &color,
				&links, &preformatted, &termwriter, &last);
		if (ret == -1) return -1;
		if (ret == PARSE_LINE_SKIP) continue;
		if (ret == PARSE_LINE_COMPLETED) {
			color = LINE_TEXT;
			last = 0;
		}

		{
			struct gemtext_cell newline = {0};
			newline.special = GEMTEXT_NEWLINE;
			writecell(&termwriter, newline);
		}
	}

	{
		struct gemtext_cell eof = {0};
		eof.special = GEMTEXT_EOF;
		writecell(&termwriter, eof);
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

	line.cells =
		malloc((gemtext->width + 1) * sizeof(struct gemtext_cell));
	if (!line.cells) return ERROR_MEMORY_FAILURE;

	cells = pos = ret = 0;
	while (!ret) {

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

		switch (cell.special) {
		case GEMTEXT_EOF: break;
		case GEMTEXT_BLANK: break;
		case GEMTEXT_RESET:
			cells = pos / 2;
			break;
		case GEMTEXT_NEWLINE:
		{
			struct gemtext_line *ptr;
			size_t length;

			tmp = realloc(gemtext->lines,
					(gemtext->length + 1) * sizeof(line));
			if (!tmp) {
				free(gemtext->lines);
				ret = ERROR_MEMORY_FAILURE;
				break;
			}
			gemtext->lines = tmp;

			length = sizeof(struct gemtext_cell) * line.length;
			ptr = &gemtext->lines[gemtext->length];
			ptr->cells = malloc(length);
			if (!ptr->cells) {
				free(gemtext->lines);
				ret = ERROR_MEMORY_FAILURE;
				break;
			}
			memcpy(ptr->cells, line.cells, length);
			ptr->length = line.length;

			line.length = 0;
			gemtext->length++;
		}
			break;
		default:
			if (line.length >= (size_t)gemtext->width) break;
			line.cells[line.length] = cell;
			line.length++;
			break;
		}
		if (cell.special == GEMTEXT_EOF) break;
	}
	free(line.cells);
	return ret;
}
