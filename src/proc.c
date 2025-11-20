/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#ifdef __linux__
#define _DEFAULT_SOURCE
#include <syscall.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>
#include "error.h"
#include "proc.h"

char **argv_ptr = NULL;

void proc_argv(char **argv) {
	argv_ptr = argv;
}

extern char **environ;

int spawn(char *program, int wait, int closefds, ...) {

	int err, n, i;
	pid_t pid;
	char **argv;
	posix_spawn_file_actions_t action;
	va_list args;

	va_start(args, closefds);
	for (n = 0; va_arg(args, char*); n++) ;
	va_end(args);

	argv = malloc(sizeof(char*) * (n + 2));
	if (!argv) return -1;
	argv[0] = program;

	va_start(args, closefds);
	for (i = 0; i < n; i++)
		argv[1 + i] = va_arg(args, char*);
	va_end(args);

	argv[1 + i] = NULL;

	posix_spawn_file_actions_init(&action);
	if (closefds) {
		posix_spawn_file_actions_addclose(&action, STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&action, STDIN_FILENO);
		posix_spawn_file_actions_addclose(&action, STDERR_FILENO);
	}

	err = posix_spawnp(&pid, argv[0], &action, NULL, argv, environ);

	posix_spawn_file_actions_destroy(&action);
	free(argv);

	if (!err && wait) waitpid(pid, NULL, 0);

	return err;
}

int proc_fork(char *arg, int *fd_in, int *fd_out) {

	int in[2], out[2], err;
	uint8_t byte;
	pid_t pid;
	char **argv;
	posix_spawn_file_actions_t action;

	if (!argv_ptr) return ERROR_NULL_ARGUMENT;
	argv = malloc(sizeof(char*) * 3);
	if (!argv) return ERROR_MEMORY_FAILURE;
	if (pipe(in) || pipe(out)) {
		free(argv);
		return ERROR_ERRNO;
	}

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, STDOUT_FILENO);
	posix_spawn_file_actions_adddup2(&action, in[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&action, in[0]);
	posix_spawn_file_actions_addclose(&action, in[1]);
	posix_spawn_file_actions_addclose(&action, STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&action, out[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&action, out[0]);
	posix_spawn_file_actions_addclose(&action, out[1]);

	argv[0] = argv_ptr[0];
	argv[1] = arg;
	argv[2] = NULL;

	err = posix_spawnp(&pid, argv[0], &action, NULL, argv, environ);

	close(in[1]);
	close(out[0]);

	free(argv);
	posix_spawn_file_actions_destroy(&action);

	if (err != 0) {
		close(out[1]);
		close(in[0]);
		return ERROR_ERRNO;
	}
	if (read(in[0], &byte, 1) != 1 || byte) {
		close(out[1]);
		close(in[0]);
		return ERROR_SANDBOX_FAILURE;
	}
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
