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
		char buf[8];

		read(in, &byte, 1);
		snprintf(V(buf), "%02X ", byte);

		writeto(&termwriter, buf, 0, 0, i);
	}

	{
		struct page_cell cell = {0};
		cell.special = PAGE_EOF;
		writecell(&termwriter, cell, i);
	}
	return 0;
}
