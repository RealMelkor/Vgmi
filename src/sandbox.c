#include "sandbox.h"

#ifdef NO_SANDBOX
int sandbox_init() {
	return 0;
}
#endif
