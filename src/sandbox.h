#define SANDBOX_INFO "The program is not sandboxed"
#define SANDBOX_FILESYSTEM "Unrestricted"
#define SANDBOX_IPC "Unrestricted"
#define SANDBOX_DEVICE "Unrestricted"
#define NO_SANDBOX

#ifdef __linux__
#if __has_include(<linux/landlock.h>)
#undef NO_SANDBOX
#undef SANDBOX_INFO
#ifdef ENABLE_SECCOMP
#define SANDBOX_INFO "Sandboxed using landlock(7) and seccomp(2)"
#else
#define SANDBOX_INFO "Sandboxed using landlock(7)"
#endif
#undef SANDBOX_FILESYSTEM
#define SANDBOX_FILESYSTEM "Restricted"
#undef SANDBOX_IPC
#ifdef ENABLE_SECCOMP
#define SANDBOX_IPC "Restricted"
#else
#define SANDBOX_IPC "Weakly restricted"
#endif
#undef SANDBOX_DEVICE
#define SANDBOX_DEVICE "Restricted"
#endif
#endif

#ifdef __OpenBSD__
#undef NO_SANDBOX
#undef SANDBOX_INFO
#define SANDBOX_INFO "Sandboxed using pledge(2) and unveil(2)"
#undef SANDBOX_FILESYSTEM
#define SANDBOX_FILESYSTEM "Restricted"
#undef SANDBOX_IPC
#define SANDBOX_IPC "Restricted"
#undef SANDBOX_DEVICE
#define SANDBOX_DEVICE "Restricted"
#endif

int sandbox_init();
int sandbox_isolate();
int sandbox_set_name(const char*);
