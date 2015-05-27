#include <string.h>

static char * get_filename(const char *url)
{
	return strdup(strrchr(url, '/') + 1);
}