/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "macro.h"
#include "wcwidth.h"
#define PAGE_INTERNAL
#include "page.h"
#include "request.h"
#define PARSER_INTERNAL
#include "parser.h"

int parse_binary(int in, size_t length, int width, int out) {

	size_t i;
	struct termwriter termwriter = {0};

	if (width < 10) width = 10;

	termwriter.width = width - OFFSETX;
	termwriter.fd = out;
	termwriter.pos = 0;

	for (i = 0; i < length; i++) {
		uint8_t byte;
		struct page_cell cell = {0};
		char buf[8];

		cell.width = 1;

		read(in, &byte, 1);
		snprintf(V(buf), "%02X", byte);

		cell.codepoint = buf[0];
		writecell(&termwriter, cell);
		cell.codepoint = buf[1];
		writecell(&termwriter, cell);
		cell.codepoint = ' ';
		writecell(&termwriter, cell);
	}

	{
		struct page_cell cell = {0};
		cell.special = PAGE_EOF;
		writecell(&termwriter, cell);
	}
	return 0;
}
