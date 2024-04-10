/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef __linux__
#define _DEFAULT_SOURCE
#include <syscall.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "error.h"
#include "proc.h"

char **argv_ptr = NULL;

void proc_argv(char **argv) {
	argv_ptr = argv;
}

int proc_fork(char *arg, int *fd_in, int *fd_out) {

	int in[2], out[2];
	uint8_t byte;
	pid_t pid;

	if (!argv_ptr) return ERROR_NULL_ARGUMENT;

	if (pipe(in) || pipe(out) || (pid = vfork()) < 0) return ERROR_ERRNO;
	if (!pid) {
		char **argv;
		close(in[0]);
		close(out[1]);

		argv = malloc(sizeof(char*) * 3);
		if (!argv) exit(0);
		argv[0] = argv_ptr[0];
		argv[1] = arg;
		argv[2] = NULL;

		close(STDOUT_FILENO);
		dup(in[1]);
		close(STDIN_FILENO);
		dup(out[0]);

		execvp(argv_ptr[0], argv);
		exit(0);
	}

	close(in[1]);
	close(out[0]);

	if (read(in[0], &byte, 1) != 1 || byte) return ERROR_SANDBOX_FAILURE;
	if (fd_out) *fd_out = out[1];
	if (fd_in) *fd_in = in[0];
	return 0;
}

void proc_exit(void) {
#ifdef __linux__
	syscall(SYS_exit, EXIT_SUCCESS);
#else
	exit(0);
#endif
}
