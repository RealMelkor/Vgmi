/* See LICENSE file for copyright and license details. */
#define SB_IGNORE
#include "sandbox.h"
#include <string.h>
#include <errno.h>
#if defined(__linux__) && !defined(__MUSL__)
#include <dlfcn.h>
#include <stdlib.h>
void* libgcc_s = NULL;
#endif
#ifdef __linux__
#include <sys/prctl.h>
#endif

#ifndef NO_SANDBOX
#ifdef sun
#include <priv.h>
int init_privs(const char **privs);
#endif
#endif

#ifndef DISABLE_XDG
#if defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__linux__) || defined(sun)

int xdg_pipe[2] = {-1, -1};
int xdg_open(char*);

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <termbox.h>

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
#endif // #ifndef DISABLE_XDG

#ifndef NO_SANDBOX

#ifdef __FreeBSD__

#include <stdio.h>
#include <sys/capsicum.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#define WITH_CASPER
#include <sys/nv.h>
#include <libcasper.h>
#include <casper/cap_net.h>
#include <capsicum_helpers.h>

cap_channel_t *_capnet;
cap_net_limit_t* limit;

int sandbox_connect(int s, const struct sockaddr *name, socklen_t namelen) {
        return cap_connect(_capnet, s, name, namelen);
}

int sandbox_getaddrinfo(const char *hostname, const char *servname, 
			const struct addrinfo *hints, struct addrinfo **res) {
        return cap_getaddrinfo(_capnet, hostname, servname, hints, res);
}

#include "cert.h"
int sandbox_init() {
#ifndef DISABLE_XDG
	if (xdg_init()) {
		printf("xdg failure\n");
		return -1;
	}
#endif
	if (getconfigfd() < 0) {
                printf("Unable to open/create config folder\n");
                return -1;
	}
	if (chdir("/var/empty")) {
		printf("chdir failure\n");
		return -1;
	}

	cap_rights_t rights;
	cap_rights_init(&rights, CAP_WRITE, CAP_LOOKUP, CAP_READ,
			CAP_SEEK, CAP_CREATE, CAP_FCNTL, CAP_FTRUNCATE);
        if (cap_rights_limit(config_fd, &rights)) {
                printf("cap_rights_limit failed\n");
                return -1;
        }

        cap_channel_t *capcas;
        capcas = cap_init();
        if (capcas == NULL) {
                printf("cap_init failed\n");
                return -1;
        }
        caph_cache_catpages();
        if (caph_enter()) {
                printf("cap_enter failed\n");
                return -1;
        }
        _capnet = cap_service_open(capcas, "system.net");
        cap_close(capcas);
        if (_capnet == NULL) {
                printf("failed to open system.net service\n");
                return -1;
        }
        limit = cap_net_limit_init(_capnet,
                                   CAPNET_NAME2ADDR |
				   CAPNET_CONNECTDNS |
				   CAPNET_CONNECT);
        if (limit == NULL) {
                printf("Unable to create limits.\n");
                return -1;
        }
        int families[] = {AF_INET, AF_INET6};
        cap_net_limit_name2addr_family(limit, families, 2);
        if (cap_net_limit(limit) < 0) {
                printf("Unable to apply limits.\n");
                return -1;
        }
	return 0;
}

int sandbox_close() {
#ifndef DISABLE_XDG
	xdg_close();
#endif
	return 0;
}

int makefd_readonly(int fd) {
	cap_rights_t rights;
        cap_rights_init(&rights, CAP_SEEK, CAP_READ);
        if (cap_rights_limit(fd, &rights))
                return -1;
	return 0;
}

int make_readonly(FILE* f) {
	return makefd_readonly(fileno(f));
}

int makefd_writeonly(int fd) {
	cap_rights_t rights;
        cap_rights_init(&rights, CAP_WRITE);
        if (cap_rights_limit(fd, &rights))
                return -1;
	return 0;
}

int makefd_writeseek(int fd) {
	cap_rights_t rights;
        cap_rights_init(&rights, CAP_WRITE, CAP_SEEK);
        if (cap_rights_limit(fd, &rights))
                return -1;
	return 0;
}

int make_writeonly(FILE* f) {
	return makefd_writeonly(fileno(f));
}

#elif __OpenBSD__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cert.h"

extern char home_path[1024];
extern char config_path[1024];

int sandbox_init() {
#ifndef DISABLE_XDG
	if (xdg_init()) {
		printf("xdg failure\n");
		return -1;
	}
#endif
#ifndef HIDE_HOME
	if (gethomefd() < 1) {
		printf("Failed to get home folder\n");
		return -1;
	}
#endif
	if (getconfigfd() < 0) {
		printf("Failed to get cache folder\n");
		return -1;
	}
	if (getdownloadfd() < 0) {
		printf("Failed to get download folder\n");
		return -1;
	}
	if (
#ifndef HIDE_HOME
	    unveil(home_path, "r") ||
#endif
	    unveil(config_path, "rwc") || 
	    unveil(download_path, "rwc") || 
	    unveil("/etc/resolv.conf", "r") ||
	    unveil(NULL, NULL)) {
		printf("Failed to unveil\n");
		return -1;
	}
	if (pledge("stdio rpath wpath cpath inet dns tty", NULL)) {
		printf("Failed to pledge\n");
		return -1;
	}
	return 0;
}

int sandbox_close() {
#ifndef DISABLE_XDG
	xdg_close();
#endif
	return 0;
}

#elif __linux__

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/unistd.h>
#if ENABLE_LANDLOCK || (!defined(DISABLE_LANDLOCK) && \
    __has_include(<linux/landlock.h>))
	#include <linux/landlock.h>
	#define ENABLE_LANDLOCK
#endif
#include "cert.h"

// --------------
#define SC_ALLOW_(nr)                                            \
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, nr, 0, 1),   \
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

#define SC_ALLOW(nr)						\
	BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_##nr, 0, 1),	\
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)

#define SC_ALLOW_ARG(_nr, _arg, _val)					\
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, (_nr), 0, 6),		\
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,				\
	    offsetof(struct seccomp_data, args[(_arg)]) + SC_ARG_LO),	\
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K,				\
	    ((_val) & 0xffffffff), 0, 3),				\
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,				\
	    offsetof(struct seccomp_data, args[(_arg)]) + SC_ARG_HI),	\
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K,				\
	    (((uint32_t)((uint64_t)(_val) >> 32)) & 0xffffffff), 0, 1),	\
	BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),			\
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,				\
	    offsetof(struct seccomp_data, nr))
// --------------

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
#ifdef __NR_dup2
        SC_ALLOW(dup2), // required by getaddrinfo
#endif
        SC_ALLOW(pipe2), // tb_init
	SC_ALLOW(recvmsg), // getaddrinfo_a
	SC_ALLOW(getsockname), // getaddrinfo_a
        SC_ALLOW(fstat), // older glibc and musl
#ifdef __NR_stat
        SC_ALLOW(stat), // older glibc
#endif
#ifdef __NR_pipe
        SC_ALLOW(pipe), // older glibc and musl
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
	SC_ALLOW(rt_sigreturn), // resizing
	SC_ALLOW(mprotect),
	SC_ALLOW(pread64),
	SC_ALLOW(uname), // getaddrinfo
	SC_ALLOW(ppoll), // getaddrinfo
	SC_ALLOW(bind), // getaddrinfo
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
	SC_ALLOW(rseq), // pthread_create
	SC_ALLOW(set_robust_list), // pthread_create
	SC_ALLOW(munmap), // pthread_create
	SC_ALLOW(madvise), // thread exit
	SC_ALLOW(mremap), // realloc
#ifdef __NR_select
	SC_ALLOW(select), // on old version of linux
#endif
	SC_ALLOW(membarrier),
	SC_ALLOW(sendmmsg),
#ifdef __NR_pselect6
	SC_ALLOW(pselect6),
#endif
	BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
};

#ifdef ENABLE_LANDLOCK
static inline int landlock_create_ruleset(
	const struct landlock_ruleset_attr *attr, size_t size, uint32_t flags)
{
	return syscall(__NR_landlock_create_ruleset, attr, size, flags);
}

static inline int landlock_add_rule(int ruleset_fd,
				    enum landlock_rule_type type,
				    const void *attr, uint32_t flags)
{
	return syscall(__NR_landlock_add_rule, ruleset_fd, type, attr, flags);
}

static inline int landlock_restrict_self(int ruleset_fd, __u32 flags)
{
	return syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}

int landlock_unveil(int landlock_fd, int fd, int perms)
{
	struct landlock_path_beneath_attr attr = {
		.allowed_access = perms,
		.parent_fd = fd
	};

	int ret = landlock_add_rule(landlock_fd, LANDLOCK_RULE_PATH_BENEATH,
				    &attr, 0);
	int err = errno;
	close(attr.parent_fd);
	errno = err;
	return ret;
}

#include <fcntl.h>
int landlock_unveil_path(int landlock_fd, const char* path, int perms) {
	int fd = open(path, 0);
	if (fd < 0) return -1;
	int ret = landlock_unveil(landlock_fd, fd, perms);
	return ret;
}

int landlock_init() {
	struct landlock_ruleset_attr attr = {
		.handled_access_fs =	LANDLOCK_ACCESS_FS_EXECUTE |
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
					LANDLOCK_ACCESS_FS_MAKE_SYM,
	};
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

extern char config_path[1024];
extern char download_path[1024];
#endif

int sandbox_init() {
#ifndef DISABLE_XDG
	if (xdg_init()) {
		printf("xdg failure\n");
		return -1;
	}
#endif
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		printf("PR_SET_NO_NEW_PRIVS failed\n");
		return -1;
	}

	int dfd = getdownloadfd();
	int cfd = getconfigfd();
	if (cfd < 0 || dfd < 0) {
		printf("Failed to access folders\n");
		return -1;
	}
#ifdef ENABLE_LANDLOCK
	int llfd = landlock_init();
	if (llfd < 0) {
		printf("[WARNING] Failed to initialize landlock : %s\n",
		       strerror(errno));
		printf("[WARNING] The filesystem won't " \
		       "be hidden from the program\n");
		goto skip_landlock;
	}
	int home = 0;
	#ifndef HIDE_HOME
#include <pwd.h>
	struct passwd *pw = getpwuid(geteuid());
        if (!pw) {
		printf("failed to get home folder: %s\n", strerror(errno));
		return -1;
	}
	home = landlock_unveil_path(llfd, pw->pw_dir,
					LANDLOCK_ACCESS_FS_READ_FILE);
	#endif
	int cfg = landlock_unveil_path(llfd, config_path,
					LANDLOCK_ACCESS_FS_READ_FILE |
					LANDLOCK_ACCESS_FS_WRITE_FILE |
					LANDLOCK_ACCESS_FS_MAKE_REG);
	int dl = landlock_unveil_path(llfd, download_path,
					LANDLOCK_ACCESS_FS_WRITE_FILE |
					LANDLOCK_ACCESS_FS_MAKE_REG);
	int hosts = landlock_unveil_path(llfd, "/etc/hosts",
					LANDLOCK_ACCESS_FS_READ_FILE);
	int run = landlock_unveil_path(llfd, "/run",
					LANDLOCK_ACCESS_FS_READ_FILE);

	int resolv = landlock_unveil_path(llfd, "/etc/resolv.conf",
					LANDLOCK_ACCESS_FS_READ_FILE);

	if (dl || cfg || hosts || run || home || resolv) {
		printf("landlock, failed to unveil : %s\n", strerror(errno));
		return -1;
	}

#ifndef __MUSL__
	// with glibc, load dynamic library before restricting process
	libgcc_s = dlopen("/lib64/libgcc_s.so.1", RTLD_LAZY);
	if (!libgcc_s)
		printf("failed to load libgcc_s.so.1, " \
		       "unexpected behaviors may occur\n");
#endif
	if (landlock_apply(llfd)) {
		printf("landlock, failed to restrict process : %s\n",
		       strerror(errno));
		return -1;
	}
skip_landlock:;
#endif
        struct sock_fprog prog = {
		.len = (unsigned short)(sizeof(filter) / sizeof (filter[0])),
		.filter = filter,
        };
	if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0)) {
		printf("Failed to enable seccomp\n");
		return -1;
	}
	return 0;
}

int sandbox_close() {
#if defined(__linux__) && !defined(__MUSL__)
	if (libgcc_s)
		dlclose(libgcc_s);
#endif
#ifndef DISABLE_XDG
	xdg_close();
#endif
	return 0;
}

#elif sun

#include <stdio.h>
#include <string.h>
#include <priv.h>
#include <errno.h>
#include <strings.h>
#include <fcntl.h>
#include "cert.h"
#include "gemini.h"

int init_privs(const char **privs) {
	priv_set_t *pset;
	if ((pset = priv_allocset()) == NULL) {
		printf("priv_allocset: %s", strerror(errno));
		return -1;
	}
	priv_emptyset(pset);
	for (int i = 0; privs[i]; i++) {
		if (priv_addset(pset, privs[i]) != 0) {
			printf("priv_addset: %s", strerror(errno));
			return -1;
		}
	}
	if (setppriv(PRIV_SET, PRIV_PERMITTED, pset) != 0 ||
	    setppriv(PRIV_SET, PRIV_LIMIT, pset) != 0 ||
	    setppriv(PRIV_SET, PRIV_INHERITABLE, pset) != 0) {
		printf("setppriv: %s", strerror(errno));
		return -1;
	}
	priv_freeset(pset);
	return 0;
}

/*

   Illumos sandbox components

   fork,exec,read,write : xdg
   write : bookmark, known_host, certs_write, download
   read : certs_read

*/

// write request
unsigned int WR_BOOKMARKS = 0xFFFFFFFF;
unsigned int WR_KNOWNHOSTS = 0xEEEEEEEE;
unsigned int WR_KNOWNHOST_ADD = 0xEEEEFFFF;
unsigned int WR_DOWNLOAD = 0xDDDDDDDD;
unsigned int WR_CERTIFICATE = 0xCCCCCCCC;
unsigned int WR_END = 0xBBBBBBBB;

int wr_pair[2];

int sandbox_download(struct gmi_tab* tab, const char* path) {
	if (send(wr_pair[1], &WR_DOWNLOAD, sizeof(WR_DOWNLOAD), 0) !=
	    sizeof(WR_DOWNLOAD)) {
sandbox_error:
		tab->show_error = -1;
		snprintf(tab->error, sizeof(tab->error),
			 "Sandbox error");
		return -1;
	}
	int path_len = strlen(path);
	if (send(wr_pair[1], path, path_len, 0) != path_len)
		goto sandbox_error;
	int res;
	if (recv(wr_pair[1], &res, sizeof(res), 0) != sizeof(res))
		goto sandbox_error;
	if (res) {
		tab->show_error = -1;
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to write file : %s",
			 strerror(res));
		return -1;
	}
	return 0;
}

int sandbox_dl_length(size_t length) {
	return send(wr_pair[1], &length, sizeof(length), 0);
}

int sandbox_cert_create(char* host, char* error, int errlen) {
	if (send(wr_pair[1], &WR_CERTIFICATE, sizeof(WR_CERTIFICATE), 0) !=
	    sizeof(WR_CERTIFICATE)) {
sandbox_error:
		snprintf(error, errlen, "Sandbox error : %s", strerror(errno));
		return -1;
	}
	int len = strlen(host);
	if (send(wr_pair[1], host, len, 0) != len)
		goto sandbox_error;
	int err;
	if (recv(wr_pair[1], &err, sizeof(len), 0) != sizeof(err))
		goto sandbox_error;
	if (err) { // error
		len = recv(wr_pair[1], error, errlen - 1, 0);
		if (len < 0) len = 0;
		error[len] = '\0';
	}
	return err;
}

int sandbox_listen() {
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, wr_pair)) {
                printf("socketpair: %s\n", strerror(errno));
                return -1;
        }
        int pid = fork();
        if (pid != 0) {
                close(wr_pair[0]);
                return 0;
        }
        close(wr_pair[1]);
	const char* privs[] = {PRIV_FILE_WRITE, NULL};
	if (init_privs(privs)) exit(-1);
	char buf[1024];
	int fd = -1;
	uint64_t length = 0;
        while (1) {
		int l = fd == -1 ?
			4 : (length > 0 && length < sizeof(buf) ?
			length : sizeof(buf));
                int len = recv(wr_pair[0], buf, l, 0);
                if (len <= 0)
                        break;
		if (fd > -1) goto transfer;
		if (len != 4) continue;
		if (*(uint32_t*)buf == WR_DOWNLOAD) {
			// recv file name
			len = recv(wr_pair[0], buf, sizeof(buf) - 1, 0);
			if (len < 1) {
				len = -1;
				send(wr_pair[0], &len, sizeof(len), 0);
				continue;
			}
			// clean file name from slash
			for (int i = 0; i < len; i++) {
				if (buf[i] == '/')
					buf[i] = '_';
			}
			buf[len] = '\0';
			fd = openat(download_fd, buf,
				    O_CREAT|O_WRONLY|O_EXCL, 0600);
			// send back errno if failed to open file, 0 if sucess
			len = (fd < 0) ? errno : 0;
			send(wr_pair[0], &len, sizeof(len), 0);
			if (fd < 0) // exit here on error
				continue;
			// recv file length
                	len = recv(wr_pair[0], buf, sizeof(length), 0);
			if (len != sizeof(length)) {
				close(fd);
				fd = -1;
			}
			length = *(uint64_t*)buf;
		}
		if (*(uint32_t*)buf == WR_CERTIFICATE) {
			len = recv(wr_pair[0], buf, sizeof(buf) - 1, 0);
			if (len < 1) {
				len = -1;
				send(wr_pair[0], &len, sizeof(len), 0);
				continue;
			}
			buf[len] = '\0';
			char err[1024];
			int ret = cert_create(buf, err, sizeof(err));
			send(wr_pair[0], &ret, sizeof(ret), 0);
			if (ret)
				send(wr_pair[0], err, strlen(err), 0);
		}
		if (*(uint32_t*)buf == WR_BOOKMARKS) {
			fd = openat(config_fd, "bookmarks.txt",
				    O_CREAT|O_WRONLY|O_CLOEXEC|O_TRUNC, 0600);
			if (fd < 0) exit(-1);
		}
		if (*(uint32_t*)buf == WR_KNOWNHOSTS) {
			fd = openat(config_fd, "known_hosts",
				    O_CREAT|O_WRONLY|O_CLOEXEC|O_TRUNC, 0600);
			if (fd < 0) exit(-1);
		}
		if (*(uint32_t*)buf == WR_KNOWNHOST_ADD) {
                        fd = openat(config_fd, "known_hosts",
                                    O_CREAT|O_APPEND|O_WRONLY, 0600);
                        if (fd < 0) exit(-1);
                }
		continue;
transfer:
		write(fd, buf, len);
		if (length) {
			if (length < (unsigned)len) len = length;
			length -= len;
			if (length > 0) continue;
			goto close;
		}
		if (*(uint32_t*)&buf[len - 4] == WR_END) {
			send(wr_pair[0], &WR_END, sizeof(WR_END), 0);
close:
			fsync(fd);
			close(fd);
			fd = -1;
		}
        }
	if (fd > -1) close(fd);
	close(wr_pair[0]);
	exit(-1);
}

int sandbox_init() {
#ifndef DISABLE_XDG
	if (xdg_init()) {
		printf("xdg failure\n");
		return -1;
	}
#endif
	if (getconfigfd() < 0) {
		printf("Failed to get cache folder\n");
		return -1;
	}
	if (getdownloadfd() < 0) {
		printf("Failed to get download folder\n");
		return -1;
	}
	if (cert_load()) {
		printf("Failed to load known host\n");
		return -1;
	}
	if (sandbox_listen()) {
		printf("Failed to initialize sandbox\n");
		return -1;
	}

	struct addrinfo hints, *result;
        bzero(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;

	getaddrinfo("example.com", NULL, &hints, &result);

	const char* privs[] = {PRIV_NET_ACCESS, NULL};
	if (init_privs(privs)) return -1;

	return 0;
}

int sandbox_close() {
#ifndef DISABLE_XDG
	xdg_close();
#endif
	close(wr_pair[1]);
	close(wr_pair[0]);
	return 0;
}

#else
#warning No sandbox found for the current operating system
#define NO_SANDBOX
#endif

#endif // #ifndef NO_SANDBOX

#ifdef NO_SANDBOX
int sandbox_init() {
	return 0;
}

int sandbox_close() {
	return 0;
}
#endif
