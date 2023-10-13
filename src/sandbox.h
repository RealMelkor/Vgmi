#define SANDBOX_INFO "The program is not sandbox"
#define NO_SANDBOX

#ifdef __OpenBSD__
#undef NO_SANDBOX
#undef SANDBOX_INFO
#define SANDBOX_INFO "Sandboxed using pledge(2) and unveil(2)"
#endif

int sandbox_init();
