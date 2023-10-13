#ifdef __OpenBSD__
#include "macro.h"
#include "sandbox.h"
#include "storage.h"
#include "error.h"

int sandbox_init() {

	char path[2048];
	int ret;

	if ((ret = storage_path(V(path)))) return ret;
	if (unveil(path, "rwc")) return ERROR_SANDBOX_FAILURE;
	if (unveil(NULL, NULL)) return ERROR_SANDBOX_FAILURE;

	return 0;
}
#else
typedef int hide_warning;
#endif
