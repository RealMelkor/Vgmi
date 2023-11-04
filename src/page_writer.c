#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "macro.h"
#include "wcwidth.h"
#define PAGE_INTERNAL
#include "page.h"
#include "request.h"
#include "parser.h"

int writecell(struct termwriter *termwriter, struct page_cell cell) {
	size_t i, x, nextspace;
	if (cell.special == PAGE_BLANK) {
		return write(termwriter->fd, P(cell)) != sizeof(cell);
	}
	if (!cell.special && termwriter->pos < LENGTH(termwriter->cells)) {
		termwriter->cells[termwriter->pos] = cell;
		termwriter->pos++;
		return 0;
	}
	nextspace = 0;
	x = 0;
	for (i = 0; i < termwriter->pos; i++) {
		if (i >= nextspace) {
			size_t j, nextspace_x;
			nextspace_x = x;
			for (j = i + 1; j < termwriter->pos; j++) {
				nextspace_x += termwriter->cells[j].width;
				if (WHITESPACE(termwriter->cells[j].codepoint))
					break;
			}
			if (nextspace_x - x <= termwriter->width &&
					nextspace_x >= termwriter->width) {
				struct page_cell cell = {0};
				cell.special = PAGE_NEWLINE;
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
			struct page_cell cell = {0};
			cell.special = PAGE_NEWLINE;
			write(termwriter->fd, P(cell));
			x = 0;
		}
		if (write(termwriter->fd, P(termwriter->cells[i])) !=
				sizeof(cell))
			return -1;
	}
	termwriter->pos = 0;
	return -(write(termwriter->fd, P(cell)) != sizeof(cell));
}

int writenewline(struct termwriter *termwriter) {
	struct page_cell newline = {0};
	newline.special = PAGE_NEWLINE;
	return writecell(termwriter, newline);
}

int writeto(struct termwriter *termwriter, const char *str,
			int color, int link) {
	const char *ptr = str;
	while (*ptr) {
		struct page_cell cell = {0};
		cell.codepoint = *ptr;
		cell.link = link;
		cell.width = mk_wcwidth(cell.codepoint);
		cell.color = color;
		if (writecell(termwriter, cell)) return -1;
		ptr++;
	}
	return 0;
}

int prevent_deadlock(size_t pos, size_t *initial_pos, int fd) {
	if (pos - *initial_pos > BLOCK_SIZE / 2) {
		struct page_cell cell = {0};
		cell.special = PAGE_RESET;
		if (write(fd, P(cell)) != sizeof(cell)) return -1;
		*initial_pos = pos;
	}
	return 0;
}
