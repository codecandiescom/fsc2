#if ! defined PULSE_HEADER
#define PULSE_HEADER

#include "fsc2.h"


#define MAX_PULSE_NUM  400



typedef Pulse_ {

	int num;                  /* number of pulse */

	int func;                 /* function of pulse */

	long pos;                 /* position of pulse in ns */
	long len;                 /* length of pulse in ns */
	long dpos;                /* position increment in ns */
	long dlen;                /* length increment in ns */
	long maxlen;              /* maximum length in ns */

	Phase_Sequence *phase;    /* phase sequence for pulse */

	Pulse_ * rp;              /* list of replacement pulses */
	int n_rp;                 /* number of replacement pulses */

	Pulse_ *next;             /* pointer to next pulse in list */
	Pulse_ *prev;             /* pointer to previous pulse in list */

} Pulse;


#endif  /* ! PULSE_HEADER */

