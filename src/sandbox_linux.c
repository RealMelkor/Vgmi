/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#if defined (__linux__) && !defined (DISABLE_SANDBOX)
#include <linux/seccomp.h>
#define _DEFAULT_SOURCE
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "macro.h"
#include "sandbox.h"
#include "storage.h"
#include "error.h"
#include "dns.h"
#include "config.h"

#ifdef ENABLE_SECCOMP_FILTER
#include <linux/filter.h>
#include <stddef.h>

#define SC_ALLOW(nr)						\
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_##nr, 0, 1),	\
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

struct sock_filter filter[] = {
	BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
		 (offsetof(struct seccomp_data, arch))),
	BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
		 (offsetof(struct seccomp_data, nr))),
        SC_ALLOW(readv),
        SC_ALLOW(writev),
#ifdef __NR_open
        SC_ALLOW(open),
#endif
        SC_ALLOW(dup),
#ifdef __NR_dup2
        SC_ALLOW(dup2),
#endif
        SC_ALLOW(pipe2),
	SC_ALLOW(recvmsg),
	SC_ALLOW(getsockname),
        SC_ALLOW(fstat),
#ifdef __NR_stat
        SC_ALLOW(stat),
#endif
#ifdef __NR_pipe
        SC_ALLOW(pipe),
#endif
	SC_ALLOW(setsockopt),
	SC_ALLOW(read),
	SC_ALLOW(write),
	SC_ALLOW(openat),
	SC_ALLOW(close),
	SC_ALLOW(exit),
	SC_ALLOW(ioctl),
	SC_ALLOW(exit_group),
	SC_ALLOW(futex),
	SC_ALLOW(sysinfo),
	SC_ALLOW(brk),
	SC_ALLOW(newfstatat),
	SC_ALLOW(getpid),
	SC_ALLOW(getrandom),
	SC_ALLOW(mmap),
	SC_ALLOW(fcntl),
	SC_ALLOW(lseek),
	SC_ALLOW(rt_sigaction),
	SC_ALLOW(rt_sigprocmask),
	SC_ALLOW(rt_sigreturn),
	SC_ALLOW(mprotect),
#ifdef ENABLE_PKEY
	SC_ALLOW(pkey_mprotect),
	SC_ALLOW(pkey_alloc),
#endif
	SC_ALLOW(pread64),
	SC_ALLOW(uname),
	SC_ALLOW(ppoll),
	SC_ALLOW(bind),
	SC_ALLOW(sendto),
	SC_ALLOW(recvfrom),
	SC_ALLOW(socket),
	SC_ALLOW(socketpair),
	SC_ALLOW(connect),
        SC_ALLOW(getsockopt),
#ifdef __NR_poll
	SC_ALLOW(poll),
#endif
	SC_ALLOW(clone),
#ifdef __NR_clone3
	SC_ALLOW(clone3),
#endif
	SC_ALLOW(clock_nanosleep),
	SC_ALLOW(nanosleep),
	SC_ALLOW(rseq),
	SC_ALLOW(set_robust_list),
	SC_ALLOW(munmap),
	SC_ALLOW(madvise),
	SC_ALLOW(mremap),
#ifdef __NR_select
	SC_ALLOW(select),
#endif
	SC_ALLOW(membarrier),
	SC_ALLOW(sendmmsg),
#ifdef __NR_pselect6
	SC_ALLOW(pselect6),
#endif
#ifdef __NR_getdents
	SC_ALLOW(getdents),
#endif
	SC_ALLOW(getdents64),
#ifdef __NR_unlink
	SC_ALLOW(unlink),
#endif
	SC_ALLOW(unlinkat),
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
};

int enable_seccomp(void) {
	struct sock_fprog prog;
	prog.len = (unsigned short)LENGTH(filter);
	prog.filter = filter;
	return prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0);
}
#endif

#ifdef HAS_LANDLOCK
#include <linux/landlock.h>

static int landlock_create_ruleset(
		const struct landlock_ruleset_attr *attr,
		size_t size, uint32_t flags) {
	return syscall(SYS_landlock_create_ruleset, attr, size, flags);
}

static int landlock_add_rule(int ruleset_fd,
		enum landlock_rule_type type,
		const void *attr, uint32_t flags) {
	return syscall(SYS_landlock_add_rule, ruleset_fd, type, attr, flags);
}

static int landlock_restrict_self(int ruleset_fd, __u32 flags) {
	return syscall(SYS_landlock_restrict_self, ruleset_fd, flags);
}

int landlock_unveil(int landlock_fd, int fd, int perms) {
	int ret, err;
	struct landlock_path_beneath_attr attr = {0};
	attr.allowed_access = perms;
	attr.parent_fd = fd;

	ret = landlock_add_rule(landlock_fd, LANDLOCK_RULE_PATH_BENEATH,
					&attr, 0);
	err = errno;
	close(attr.parent_fd);
	errno = err;
	return ret;
}

int landlock_unveil_path(int landlock_fd, const char* path, int perms) {
	int fd = open(path, 0);
	if (fd < 0) return -1;
	return landlock_unveil(landlock_fd, fd, perms);
}

int landlock_init(void) {
	struct landlock_ruleset_attr attr = {0};
	attr.handled_access_fs =	LANDLOCK_ACCESS_FS_READ_FILE |
					LANDLOCK_ACCESS_FS_READ_DIR |
					LANDLOCK_ACCESS_FS_WRITE_FILE |
					LANDLOCK_ACCESS_FS_MAKE_REG;
	return landlock_create_ruleset(&attr, sizeof(attr), 0);
}

int landlock_apply(int fd)
{
	int ret = landlock_restrict_self(fd, 0);
	int err = errno;
	close(fd);
	errno = err;
	return ret;
}
#endif

int sandbox_init(void) {

	char path[2048], download_path[2048];
#ifdef HAS_LANDLOCK
	int fd;
#endif
	int ret;
	struct rlimit limit;

	if (!config.enableSandbox) return 0;

	/* prevents from creating large file */
	limit.rlim_max = limit.rlim_cur = config.maximumBodyLength;
	if (setrlimit(RLIMIT_FSIZE, &limit))
		return ERROR_SANDBOX_FAILURE;

	if (prctl(PR_SET_DUMPABLE, 0))
		return ERROR_SANDBOX_FAILURE;
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
		return ERROR_SANDBOX_FAILURE;

	if ((ret = storage_path(V(path)))) return ret;
	if ((ret = storage_download_path(V(download_path)))) return ret;

#ifdef HAS_LANDLOCK
	if (config.enableLandlock) {
		/* restrict filesystem access */
		fd = landlock_init();
		if (fd < 0) return ERROR_LANDLOCK_FAILURE;
		ret = landlock_unveil_path(fd, path,
						LANDLOCK_ACCESS_FS_READ_FILE |
						LANDLOCK_ACCESS_FS_WRITE_FILE |
						LANDLOCK_ACCESS_FS_MAKE_REG);
		ret |= landlock_unveil_path(fd, download_path,
						LANDLOCK_ACCESS_FS_WRITE_FILE |
						LANDLOCK_ACCESS_FS_MAKE_REG);
		ret |= landlock_unveil_path(fd, "/etc/hosts",
						LANDLOCK_ACCESS_FS_READ_FILE);
		ret |= landlock_unveil_path(fd, "/etc/resolv.conf",
						LANDLOCK_ACCESS_FS_READ_FILE);
		if (ret) return ERROR_LANDLOCK_FAILURE;

		if (landlock_apply(fd)) return ERROR_LANDLOCK_FAILURE;
	}
#endif

#ifdef ENABLE_SECCOMP_FILTER
	if (!config.enableLandlock) {
		/* if landlock is not enabled and getaddrinfo is not called
		 * before enabling seccomp, getaddrinfo will not resolve */
		void *ptr;
		dns_getip("geminiprotocol.net", &ptr);
	}
	if (enable_seccomp()) return ERROR_SANDBOX_FAILURE;
#endif

	return 0;
}

#include <stdlib.h>
int sandbox_isolate(void) {
	if (!config.enableSandbox) return 0;
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
		return ERROR_SANDBOX_FAILURE;
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT))
		return ERROR_SANDBOX_FAILURE;
	return 0;
}

int sandbox_set_name(const char *name) {
	if (prctl(PR_SET_NAME, name))
		return ERROR_SANDBOX_FAILURE;
	return 0;
}
#else
typedef int hide_warning;
#endif
