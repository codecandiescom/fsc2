#if ! defined PULSE_HEADER
#define PULSE_HEADER

#include "fsc2.h"


typedef struct Pulse_ {

	int num;                  /* number of pulse */

	int func;                 /* function of pulse */

	long pos;                 /* position of pulse in ns */
	long len;                 /* length of pulse in ns */
	long dpos;                /* position increment in ns */
	long dlen;                /* length increment in ns */
	long maxlen;              /* maximum length in ns */

	Phase_Sequence *phase;    /* phase sequence for pulse */

	struct Pulse_ * rp;       /* list of replacement pulses */
	int n_rp;                 /* number of replacement pulses */

	struct Pulse_ *next;      /* pointer to next pulse in list */
	struct Pulse_ *prev;      /* pointer to previous pulse in list */

} Pulse;



Pulse *pulse_new( int num );
Pulse *pulse_find( int num );
void pulse_set_func( Pulse *p, long func );
void pulse_set_start( Pulse *p, Var *v );
void pulse_set_len( Pulse *p, Var *v );
void pulse_set_dpos( Pulse *p, Var *v );
void pulse_set_dlen( Pulse *p, Var *v );
void pulse_set_maxlen( Pulse *p, Var *v );


#endif  /* ! PULSE_HEADER */

