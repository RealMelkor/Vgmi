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

	if (!argv_ptr) return -1;

	if (pipe(in)) return ERROR_ERRNO;
	if (pipe(out)) return ERROR_ERRNO;
	pid = vfork();
	if (pid < 0) return ERROR_ERRNO;
	if (!pid) {
		char **argv;
		close(in[0]);
		close(out[1]);

		argv = malloc(sizeof(char*) * 3);
		if (!argv) goto fail;
		argv[0] = argv_ptr[0];
		argv[1] = arg;
		argv[2] = NULL;

		close(STDOUT_FILENO);
		dup(in[1]);
		close(STDIN_FILENO);
		dup(out[0]);

		execv(argv_ptr[0], argv);
		printf("execv failed\n");
fail:
		close(in[0]);
		close(out[1]);
		exit(0);
	}

	close(in[1]);
	close(out[0]);

	byte = -1;
	read(in[0], &byte, 1);
	if (byte) return ERROR_SANDBOX_FAILURE;
	*fd_out = out[1];
	*fd_in = in[0];
	return 0;
}

void proc_exit() {
#ifdef __linux__
	syscall(SYS_exit, EXIT_SUCCESS);
#else
	exit(0);
#endif
}
