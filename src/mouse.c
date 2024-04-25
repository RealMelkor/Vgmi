#include "mouse.h"
#ifdef NO_TTY_MOUSE

int mouse_init(void) {
	return 0;
}

#else
typedef int hide_warning;
#endif
