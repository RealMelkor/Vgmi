/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include <stdint.h>
#define PAGE_INTERNAL
#include "page.h"
#include "utf8.h"

void page_search(struct page *page, const char *search) {
	size_t y, x, length;
	uint32_t field[1024] = {0}, occurrences;
	while (page->results) {
		struct page_search *next = page->results->next;
		free(page->results);
		page->results = next;
	}
	for (length = 0; ; length++) {
		search += utf8_char_to_unicode(&field[length], search);
		if (!field[length]) break;
	}
	if (!length) return;
	occurrences = 0;
	for (y = 0; y < page->length; y++) {
		size_t i = 0;
		for (x = 0; x < page->lines[y].length; x++) {
			struct page_cell *cell = &page->lines[y].cells[x];
			if (cell->codepoint == field[i]) i++;
			else i = 0;
			cell->selected = 0;
			if (i == length) {
				occurrences++;
				for (; i > 0; i--) {
					cell->selected = occurrences;
					cell--;
				}
			}
		}
	}
	page->occurrences = occurrences;
}

int page_selection_line(struct page page) {
	size_t y, x;
	unsigned int selected = page.selected;
	if (selected < 1 && selected >= page.occurrences) return 0;
	for (y = 0; y < page.length; y++) {
		for (x = 0; x < page.lines[y].length; x++) {
			if (page.lines[y].cells[x].selected == selected)
				return y;
		}
	}
	return 0;
}
