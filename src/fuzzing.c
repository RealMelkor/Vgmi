/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef DEBUG
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "macro.h"
#include "gemtext.h"

int fuzzing(int iterations) {

	int i;
	char buf[4096];
	const int pad = 16;
	int fd, randfd;

	fd = open("/dev/null", O_WRONLY);
	if (fd < 0) return -1;

	randfd = open("/dev/random", O_RDONLY);
	if (randfd < 0) return -1;

	for (i = 0; i < iterations; i++) {
		int j, width, length;
		read(randfd, P(j));
		width = j % 40 + 5;
		read(randfd, P(j));
		length = j % sizeof(buf);
		if (length < pad) length += pad;
		if (length != read(randfd, buf, length)) return -1;
		gemtext_parse(buf, length - pad, width, fd);
		if (i % 100000 == 0)
			printf("%d/%d (%d%%)\n", i, iterations,
					i * 100 / iterations);
	}
	return 0;
}
#else
typedef int hide_warning;
#endif
