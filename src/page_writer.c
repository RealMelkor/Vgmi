/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "macro.h"
#include "wcwidth.h"
#define PAGE_INTERNAL
#include "page.h"
#include "request.h"
#include "parser.h"

static int newline(struct termwriter *termwriter) {
	struct page_cell cell = {0};
	cell.special = PAGE_NEWLINE;
	termwriter->sent++;
	termwriter->last_x = 0;
	return write(termwriter->fd, P(cell)) != sizeof(cell);
}

int writecell(struct termwriter *termwriter, struct page_cell cell,
		size_t pos) {

	size_t i, x, nextspace;
	size_t last_reset; /* bytes since last reset cell */

	/* flush last cells before sending EOF */
	if (cell.special == PAGE_EOF) {
		writenewline(termwriter, termwriter->pos);
		write(termwriter->fd, P(cell));
		return 0;
	}

	last_reset = termwriter->sent - termwriter->last_reset;
	if (termwriter->sent && last_reset >= PARSER_CHUNK / 2) {
		struct page_cell cell = {0};
		cell.special = PAGE_RESET;
		cell.codepoint = pos;
		cell.link = termwriter->sent++;
		termwriter->last_reset = termwriter->sent;
		write(termwriter->fd, P(cell));
	}
	
	if (cell.codepoint == '\n') {
		memset(&cell, 0, sizeof(cell));
		cell.special = PAGE_NEWLINE;
	}

	if (cell.special == PAGE_BLANK) {
		termwriter->sent++;
		return write(termwriter->fd, P(cell)) != sizeof(cell);
	}
	if (!cell.special && termwriter->pos < LENGTH(termwriter->cells)) {
		termwriter->cells[termwriter->pos] = cell;
		termwriter->pos++;
		return 0;
	}
	nextspace = 0;
	x = termwriter->last_x;
	for (i = 0; i < termwriter->pos; i++) {
		if (i >= nextspace) {
			size_t j, nextspace_x;
			nextspace_x = x;
			for (j = i + 1; j < termwriter->pos; j++) {
				nextspace_x += termwriter->cells[j].width;
				if (termwriter->cells[j].codepoint == '-') {
					j++;
					break;
				}
				if (WHITESPACE(termwriter->cells[j].codepoint))
					break;
			}
			if (nextspace_x - x <= termwriter->width &&
					nextspace_x >= termwriter->width) {
				newline(termwriter);
				x = 0;
				nextspace = i;
				if (WHITESPACE(termwriter->cells[j].codepoint))
					continue;
			}
			nextspace = j;
		}
		if (termwriter->cells[i].codepoint == '\t') {
			termwriter->cells[i].width = TAB_SIZE - (x % TAB_SIZE);
			termwriter->cells[i].codepoint = ' ';
		}
		x += termwriter->cells[i].width;
		termwriter->sent++;
		if (write(termwriter->fd, P(termwriter->cells[i])) !=
				sizeof(cell))
			return -1;
		if (x >= termwriter->width) {
			newline(termwriter);
			x = 0;
		}
	}
	if (cell.special == PAGE_NEWLINE) x = 0;
	else x += cell.width;
	termwriter->last_x = x;
	termwriter->pos = 0;
	termwriter->sent++;
	return -(write(termwriter->fd, P(cell)) != sizeof(cell));
}

int writenewline(struct termwriter *termwriter, size_t pos) {
	struct page_cell newline = {0};
	newline.special = PAGE_NEWLINE;
	return writecell(termwriter, newline, pos);
}

int writeto(struct termwriter *termwriter, const char *str,
			int color, int link, size_t pos) {
	const char *ptr = str;
	while (*ptr) {
		struct page_cell cell = {0};
		cell.codepoint = *ptr;
		cell.link = link;
		cell.width = mk_wcwidth(cell.codepoint);
		cell.color = color;
		if (writecell(termwriter, cell, pos)) return -1;
		ptr++;
	}
	return 0;
}
