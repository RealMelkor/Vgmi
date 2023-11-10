/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "macro.h"
#include "page.h"
#include "request.h"
#include "parser.h"

int parse_file(const char *path) {

	size_t length;
	int in, out, i;

	in = open(path, O_RDONLY);
	if (in < 0) return -1;
	length = lseek(in, 0, SEEK_END);
	close(in);

	out = open("/dev/null", O_WRONLY);
	if (out < 0) return -1;

	/* test range of different width */
	for (i = 0; i < 1024; i++) {
		in = open(path, O_RDONLY);
		if (in < 0) {
			close(out);
			return -1;
		}
		parse_gemtext(in, length, i, out);
		close(in);
	}

	close(out);
	return 0;
}

/* warning: doesn't detect deadlock */
int fuzzing(int iterations) {

	int i;
	int fd, randfd;

	fd = open("/dev/null", O_WRONLY);
	if (fd < 0) return -1;

	randfd = open("/dev/random", O_RDONLY);
	if (randfd < 0) return -1;

	for (i = 0; i < iterations; i++) {
		int width;
		size_t length, j;
		read(randfd, P(j));
		width = j % 40 + 5;
		read(randfd, P(j));
		length = j % 10000;
		parse_gemtext(randfd, length, width, fd);
		parse_binary(randfd, length, width, fd);
		parse_plain(randfd, length, width, fd);
		if (i % 100 == 0) {
			printf("%d/%d (%d%%)\n", i, iterations,
					i * 100 / iterations);
		}
	}
	return 0;
}
#else
typedef int hide_warning;
#endif
