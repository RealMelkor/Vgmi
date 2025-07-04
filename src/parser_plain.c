/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdlib.h>
#include <stdint.h>
#include "macro.h"
#include "wcwidth.h"
#include "termbox.h"
#define PAGE_INTERNAL
#include "page.h"
#include "request.h"
#define PARSER_INTERNAL
#include "parser.h"

int parse_plain(int in, size_t length, int width, int out) {

	size_t i;
	struct termwriter termwriter = {0};

	if (width < 10) width = 10;

	termwriter.width = width - OFFSETX;
	termwriter.fd = out;
	termwriter.pos = 0;

	for (i = 0; i < length;) {
		uint32_t ch;
		struct page_cell cell = {0};

		if (readnext(in, &ch, &i, length)) return -1;
		if (!renderable(ch)) continue;
		if (ch == '\n') {
			cell.special = PAGE_NEWLINE;
		} else {
			cell.codepoint = ch;
			cell.width = mk_wcwidth(ch);
			cell.color = TB_DEFAULT;
		}
		writecell(&termwriter, cell, i);
	}

	{
		struct page_cell cell = {0};
		cell.special = PAGE_EOF;
		writecell(&termwriter, cell, i);
	}
	return 0;
}
