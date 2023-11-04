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
#define PAGE_INTERNAL
#include "page.h"
#include "request.h"
#define PARSER_INTERNAL
#include "parser.h"
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

/* TODO: verify that writecell and writeto return 0 */
int gemtext_parse_link(int in, size_t *pos, size_t length, int *links,
			struct termwriter *termwriter, uint32_t *ch) {

	char buf[32];
	struct page_cell cell = {0}, cells[MAX_URL] = {0};
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

		struct page_cell cell = {0};

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
				struct page_cell header_cell = {0};
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
				struct page_cell prev_cell = {0};
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
				struct page_cell prev_cell = {0};
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

int gemtext_prerender(int in, size_t length, int width, int out) {

	int color, links, preformatted;
	size_t pos;
	uint32_t last;
	struct termwriter termwriter = {0};
	if (width < 10) width = 10;

	if (skip_meta(in, &pos, length)) return ERROR_INVALID_DATA;

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

		writenewline(&termwriter);
	}

	{
		struct page_cell eof = {0};
		eof.special = GEMTEXT_EOF;
		writecell(&termwriter, eof);
	}

	return 0;
}
