/*
  $Id$
*/

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

	int set_flags;
	bool is_active;           /* set while pulse is in use */

	struct {                  /* structure for storing starting values */
		long pos;
		long len;
		long dpos;
		long dlen;
		bool is_active;
	} store;

	Phase_Sequence *phase;    /* phase sequence for pulse */


	int *nrp;                 /* list of numbers of replacement pulses */
	struct Pulse_ **rp;       /* list of replacement pulses */
	int n_rp;                 /* number of replacement pulses */

	struct Pulse_ *next;      /* pointer to next pulse in list */
	struct Pulse_ *prev;      /* pointer to previous pulse in list */

} Pulse;


enum {
	P_FUNC    = ( 1 << 0 ),
	P_POS     = ( 1 << 1 ),
	P_LEN     = ( 1 << 2 ),
	P_DPOS    = ( 1 << 3 ),
	P_DLEN    = ( 1 << 4 ),
	P_MAXLEN  = ( 1 << 5 ),
	P_REPL    = ( 1 << 6 )
};


int    ps2n( char *txt );
Pulse *pulse_new( char *txt );
Pulse *pulse_find( int num );
Var   *pulse_get_prop( char *txt, int type );
void   pulse_set( Pulse *p, int type, Var *v );
void   basic_pulse_check( void );
void   save_restore_pulses( bool flag );
void   delete_pulses( void );


#endif  /* ! PULSE_HEADER */

