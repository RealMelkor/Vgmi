#define NO_TTY_MOUSE
#if defined(__linux__) && defined(ENABLE_GPM) && __has_include(<gpm.h>)
#define HAS_GPM
extern int mouse_gpm_fd;
#undef NO_TTY_MOUSE
#endif

int mouse_init(void);
struct tb_event;
int mouse_event(struct tb_event *ev);
