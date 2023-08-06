#ifndef	_lookup
#define	_lookup

/* Module lookup */

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/message.h>

#ifndef	mig_external
#define mig_external extern
#endif

#include "lookup_types.h"

/* Routine _lookup_link */
mig_external kern_return_t _lookup_link (
	port_t server,
	lookup_name name,
	int *procno);

/* Routine _lookup_all */
mig_external kern_return_t _lookup_all (
	port_t server,
	int proc,
	inline_data indata,
	unsigned int indataCnt,
	ooline_data *outdata,
	unsigned int *outdataCnt);

/* Routine _lookup_one */
mig_external kern_return_t _lookup_one (
	port_t server,
	int proc,
	inline_data indata,
	unsigned int indataCnt,
	inline_data outdata,
	unsigned int *outdataCnt);

/* Routine _lookup_ooall */
mig_external kern_return_t _lookup_ooall (
	port_t server,
	int proc,
	ooline_data indata,
	unsigned int indataCnt,
	ooline_data *outdata,
	unsigned int *outdataCnt);

#endif	/* _lookup */
