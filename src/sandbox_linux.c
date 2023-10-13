#ifdef __linux__
#if __has_include(<linux/landlock.h>)
#define _DEFAULT_SOURCE
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <linux/landlock.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include "macro.h"
#include "sandbox.h"
#include "storage.h"
#include "error.h"

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

int landlock_unveil(int landlock_fd, int fd, int perms)
{
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

int landlock_init() {
	struct landlock_ruleset_attr attr = {0};
	attr.handled_access_fs =	LANDLOCK_ACCESS_FS_EXECUTE |
					LANDLOCK_ACCESS_FS_READ_FILE |
					LANDLOCK_ACCESS_FS_READ_DIR |
					LANDLOCK_ACCESS_FS_WRITE_FILE |
					LANDLOCK_ACCESS_FS_REMOVE_DIR |
					LANDLOCK_ACCESS_FS_REMOVE_FILE |
					LANDLOCK_ACCESS_FS_MAKE_CHAR |
					LANDLOCK_ACCESS_FS_MAKE_DIR |
					LANDLOCK_ACCESS_FS_MAKE_REG |
					LANDLOCK_ACCESS_FS_MAKE_SOCK |
					LANDLOCK_ACCESS_FS_MAKE_FIFO |
					LANDLOCK_ACCESS_FS_MAKE_BLOCK |
					LANDLOCK_ACCESS_FS_MAKE_SYM;
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

int sandbox_init() {

	char path[2048];
	int ret, fd;

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
		return ERROR_SANDBOX_FAILURE;

	if ((ret = storage_path(V(path)))) return ret;

	fd = landlock_init();
	if (fd < 0) return ERROR_SANDBOX_FAILURE;
	ret = landlock_unveil_path(fd, path,
					LANDLOCK_ACCESS_FS_READ_FILE |
					LANDLOCK_ACCESS_FS_WRITE_FILE |
					LANDLOCK_ACCESS_FS_MAKE_REG);
	ret |= landlock_unveil_path(fd, "/etc/hosts",
					LANDLOCK_ACCESS_FS_READ_FILE);
	ret |= landlock_unveil_path(fd, "/etc/resolv.conf",
					LANDLOCK_ACCESS_FS_READ_FILE);

	if (ret) return ERROR_SANDBOX_FAILURE;

	if (landlock_apply(fd)) return ERROR_SANDBOX_FAILURE;

	return 0;
}
#else
#endif
typedef int hide_warning;
#endif
