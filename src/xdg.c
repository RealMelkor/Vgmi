/* See LICENSE file for copyright and license details. */
#include <sys/wait.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#ifndef NO_SANDBOX
#ifdef sun
#include <priv.h>
int init_privs(const char **privs);
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "cert.h"
#include "str.h"

const char startby[] = "XDG_DOWNLOAD_DIR=\"$HOME";

#ifndef DISABLE_XDG
#if defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__linux__) || defined(sun)

int xdg_pipe[2] = {-1, -1};
int xdg_open(const char*);

int xdg_request(char* str) {
        int len = strnlen(str, 1024)+1;
        return write(xdg_pipe[1], str, len) != len;
}

void xdg_listener() {
        char buf[4096];
        while (1) {
                int len = read(xdg_pipe[0], buf, sizeof(buf));
                if (len <= 0)
                        break;
                xdg_open(buf);
        }
}

int xdg_init() {
	if (gethomefd() < 0) {
		return -1;
	}

        if (pipe(xdg_pipe)) {
                printf("pipe failed\n");
                return -1;
        }
        int pid = fork();
        if (pid != 0) {
                close(xdg_pipe[0]);
                return 0;
        }
        close(xdg_pipe[1]);

	chdir(download_path);

#ifndef NO_SANDBOX
#ifdef __OpenBSD__
        if (unveil("/bin/sh", "x") ||
            unveil("/usr/bin/which", "x") ||
            unveil("/usr/local/bin/xdg-open", "x") ||
            unveil(NULL, NULL)) {
                close(xdg_pipe[1]);
                exit(-1);
        }
        if (pledge("stdio rpath exec proc", NULL)) {
                close(xdg_pipe[1]);
                exit(-1);
        }
#endif
#ifdef sun
        const char* privs[] = {PRIV_PROC_FORK, PRIV_PROC_EXEC,
                               PRIV_FILE_READ, PRIV_FILE_WRITE, NULL};
        if (init_privs(privs)) {
                exit(-1);
        }
#endif
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
        setproctitle("vgmi.xdg");
#endif
#ifdef __linux__
        prctl(PR_SET_NAME, "vgmi [xdg]", 0, 0, 0);
#endif
        xdg_listener();
        exit(0);
}

void xdg_close() {
        if (xdg_pipe[0] > -1)
                close(xdg_pipe[0]);
        if (xdg_pipe[1] > -1)
                close(xdg_pipe[1]);
}

#endif
#endif

#define SB_IGNORE
#include "gemini.h"
int xdg_open(const char* str) {
        if (!client.xdg) return 0;
	char buf[4096];
	snprintf(buf, sizeof(buf), "xdg-open %s>/dev/null 2>&1", str);
	if (fork() == 0) {
		setsid();
		char* argv[] = {"/bin/sh", "-c", buf, NULL};
		execvp(argv[0], argv);
		exit(0);
	}
        return 0;
}

int xdg_path(char* path, size_t len) {
	int home = gethomefd();
	if (home < 0) return -1;
	int fd = openat(home, ".config/user-dirs.dirs", O_RDONLY);
	if (fd < 0) return -1;
	char line[1024];
	int found = -1;
	for (size_t i = 0; found; i++) {
		if (i >= sizeof(line)) break;
		if (read(fd, &line[i], 1) != 1) break;
		if (line[i] != '\n') continue;
		line[i] = '\0';
		if (sizeof(startby) >= i ||
		    strncmp(line, startby, sizeof(startby) - 1)) {
			i = -1;
			continue;
		}
		int offset = strlcpy(path, home_path, len);
		for (size_t j = 0; j < len - 1; j++) {
			path[offset + j] = line[sizeof(startby) - 1 + j];
			if (path[offset + j] == '"') {
				found = 0;
				path[offset + j] = 0;
				break;
			}
		}
	}
	close(fd);
	return found;
}
