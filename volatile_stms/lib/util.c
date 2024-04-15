#include <stdlib.h>
#include <string.h>
#include "util.h"

int util_is_zeroed(const void *addr, size_t len)
{
	const char *a = addr;

	if (len == 0)
		return 1;

	if (a[0] == 0 && memcmp(a, a + 1, len - 1) == 0)
		return 1;

	return 0;
}