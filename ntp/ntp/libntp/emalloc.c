/*
 * emalloc - return new memory obtained from the system.  Belch if none.
 */
#include "ntp_types.h"
#include "ntp_malloc.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

void *
emalloc(
	u_int size
	)
{
	char *mem;

	if ((mem = (char *)malloc(size)) == 0) {
		msyslog(LOG_ERR, "No more memory!");
		exit(1);
	}
	return mem;
}