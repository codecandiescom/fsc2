/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "ep385.conf"


/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int ep385_init_hook( void );
int ep385_test_hook( void );
int ep385_end_of_test_hook( void );
int ep385_exp_hook( void );
int ep385_end_of_exp_hook( void );
void ep385_exit_hook( void );


Var *pulser_name( Var *v );
Var *pulser_state( Var *v );
Var *pulser_channel_state( Var *v );
Var *pulser_cw_mode( Var *v );
Var *pulser_update( Var *v );
Var *pulser_shift( Var *v );
Var *pulser_increment( Var *v );
Var *pulser_pulse_reset( Var *v );
Var *pulser_next_phase( Var *v );
Var *pulser_phase_reset( Var *v );
Var *pulser_lock_keyboard( Var *v );


/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min


#define FIXED_TIMEBASE         8.0e-9   /* fixed pulser time base: 8 ns */

#define MAX_CHANNELS            8       /* number of channels */

#define CHANNEL_OFFSET         14       /* channel numbers at the front panel
										   range from 14 to 21 */


#define MAX_PULSER_BITS       32767     /* maximum number of bits in channel */

#define BITS_PER_MEMORY_BLOCK  2048

#define MAX_PULSES_PER_CHANNEL   59

/* The repetition time can only be set in multiples of 102.4 us for this
   pulser and the shortest repetition time is times this value while the
   longest possible one is the 65535 times it. The repetition time set this
   way is the time from the end of the pulse sequence until the start of the
   next sequence. To calculate the correct repetition rate you will have to
   add the time it takes for a pulse sequence, which, in turn is a multiple
   of 16.384 us (2048 * 8 ns), the time it takes to output a single memory
   block. */

#define REPEAT_RATE_UNITS      1.024e-4
#define MIN_REPEAT_RATE_MULT   10
#define MAX_REPEAT_RATE_MULT   65535



#define START ( ( bool ) 1 )
#define STOP  ( ( bool ) 0 )


/* A pulse is acive if it has a position and a length set and if the length
   is non-zero */

#define IS_ACTIVE( p )    ( ( p )->is_pos && ( p )->is_len && ( p )->len > 0 )

/* A pulse needs to be updated if either its activity state changed, its
   position changed or its length changed */

#define NEEDS_UPDATE( p ) ( ( ( p )->is_active ^ ( p )->was_active ) || \
                            ( ( p )->is_old_pos &&                      \
							  ( p )->old_pos != ( p )->pos )         || \
                            ( ( p )->is_old_len &&                      \
							  ( p )->old_len != ( p )->len ) )


/* typedefs of structures needed in the module */


typedef struct _F_ {
	int self;                  /* the functions number */
	bool is_used;              /* set if the function has been declared in
								  the ASSIGNMENTS section */
	bool is_needed;            /* set if the function has been assigned
								  pulses */
	int num_channels;
	struct _C_ *channel[ MAX_CHANNELS ];
							   /* channels assigned to function */

	int num_pulses;            /* number of pulses assigned to the function */
	struct _p_ **pulses;       /* list of pulse pointers */

	struct _p_ ***pl;          /* array of currently active pulse lists */
	struct _p_ ***pm;          /* (channel x phases) pulse list matrix */

	struct _PHS_ *phase_setup;
	int next_phase;
	int pc_len;                 /* length of the phase cycle */

	long max_seq_len;          /* maximum length of the pulse sequence */

	Ticks delay;               /* delay for the function/channel combination */
	bool is_delay;
} FUNCTION;


typedef struct {
	Ticks pos;
	Ticks len;
	struct _p_ *pulse;
} PULSE_PARAMS;


typedef struct _C_ {
	int self;
	FUNCTION *function;
	bool needs_update;
	bool state;

	int num_pulses;
	int num_active_pulses;
	int old_num_active_pulses;
	PULSE_PARAMS *pulse_params;
	PULSE_PARAMS *old_pulse_params;
} CHANNEL;


typedef struct _PHS_ {
	bool is_defined;
	bool is_set[ PHASE_CW - PHASE_PLUS_X + 1 ];
	bool is_needed[ PHASE_CW - PHASE_PLUS_X + 1 ];
	CHANNEL *channels[ PHASE_CW - PHASE_PLUS_X + 1 ];
	FUNCTION *function;
} PHASE_SETUP;


typedef struct {
	int device;              /* GPIB number of the device */

	double timebase;         /* time base of the digitizer */
	bool is_timebase;
	bool timebase_mode;      /* INTERNAL (default) or EXTERNAL */

	int trig_in_mode;        /* EXTERNAL or INTERNAL */
	bool is_trig_in_mode;

	Ticks repeat_time;       /* only in INTERNAL mode */
	bool is_repeat_time;

	bool is_cw_mode;

	bool keep_all;           /* keep even unused pulses ? */

	Ticks neg_delay;
	bool is_neg_delay;       /* if any of the functions has a negative delay */

	long max_seq_len;        /* maximum length of all pulse sequences */
	bool is_max_seq_len;

	FUNCTION function[ PULSER_CHANNEL_NUM_FUNC ];
	CHANNEL channel[ MAX_CHANNELS ];

	int needed_channels;     /* number of channels that are going to be needed
								in the experiment */

	bool needs_update;       /* set if pulse properties have been changed in
							    test run or experiment */
	bool is_running;         /* set if the pulser is in run mode */
	bool has_been_running;
} EP385;


typedef struct _p_ {

	long num;                /* (positive) number of the pulse */

	bool is_active;          /* set if the pulse is really used */
	bool was_active;
	bool has_been_active;    /* used to find useless pulses */

	struct _p_ *next;
	struct _p_ *prev;

	FUNCTION *function;      /* function the pulse is associated with */

	Ticks pos;               /* current position, length, position change */
	Ticks len;               /* and length change of pulse (in units of the */
	Ticks dpos;              /* pulsers time base) */
	Ticks dlen;

	Phase_Sequence *pc;      /* the pulse sequence to be used for the pulse
								(or NULL if none is to be used) */

	bool is_function;        /* flags that are set when the corresponding */
	bool is_pos;             /* property has been set */
	bool is_len;
	bool is_dpos;
	bool is_dlen;

	Ticks initial_pos;       /* position, length, position change and length */
	Ticks initial_len;       /* change at the start of the experiment */
	Ticks initial_dpos;
	Ticks initial_dlen;

	bool initial_is_pos;     /* property has initially been set */
	bool initial_is_len;
	bool initial_is_dpos;
	bool initial_is_dlen;

	Ticks old_pos;           /* position and length of pulse before a change */
	Ticks old_len;           /* is applied */

	bool is_old_pos;
	bool is_old_len;

	bool needs_update;       /* set if the pulses properties have been changed
								in test run or experiment */
} PULSE;


#if defined( EP385_MAIN )

bool ep385_is_needed = UNSET;
EP385 ep385;
PULSE *ep385_Pulses = NULL;
bool ep385_IN_SETUP = UNSET;
PHASE_SETUP ep385_phs[ 2 ];

#else

extern bool ep385_is_needed;
extern EP385 ep385;
extern PULSE *ep385_Pulses;
extern bool ep385_IN_SETUP;
extern PHASE_SETUP ep385_phs[ 2 ];

#endif



/* Here follow the functions from ep385_gen.c */

bool ep385_store_timebase( double timebase );
bool ep385_assign_channel_to_function( int function, long channel );
bool ep385_set_function_delay( int function, double delay );
bool ep385_set_trigger_mode( int mode );
bool ep385_set_repeat_time( double rep_time );
bool ep385_set_max_seq_len( double seq_len );
bool ep385_set_phase_reference( int phase, int function );
bool ep385_phase_setup_prep( int func, int type, int dummy, long channel );
bool ep385_phase_setup( int func );
bool ep385_keep_all( void );


/* These are the functions from ep385_pulse.c */

bool ep385_new_pulse( long pnum );
bool ep385_set_pulse_function( long pnum, int function );
bool ep385_set_pulse_position( long pnum, double p_time );
bool ep385_set_pulse_length( long pnum, double p_time );
bool ep385_set_pulse_position_change( long pnum, double p_time );
bool ep385_set_pulse_length_change( long pnum, double p_time );
bool ep385_set_pulse_phase_cycle( long pnum, long cycle );

bool ep385_get_pulse_function( long pnum, int *function );
bool ep385_get_pulse_position( long pnum, double *p_time );
bool ep385_get_pulse_length( long pnum, double *p_time );
bool ep385_get_pulse_position_change( long pnum, double *p_time );
bool ep385_get_pulse_length_change( long pnum, double *p_time );
bool ep385_get_pulse_phase_cycle( long pnum, long *cycle );

bool ep385_change_pulse_position( long pnum, double p_time );
bool ep385_change_pulse_length( long pnum, double tp_ime );
bool ep385_change_pulse_position_change( long pnum, double p_time );
bool ep385_change_pulse_length_change( long pnum, double p_time );


/* Functions from ep385_init.c */

void ep385_init_setup( void );
void ep385_pulse_start_setup( void );


/* Functions from ep385_util.c */

Ticks ep385_double2ticks( double p_time );
double ep385_ticks2double( Ticks ticks );
PULSE *ep385_get_pulse( long pnum );
const char *ep385_ptime( double p_time );
const char *ep385_pticks( Ticks ticks );
int ep385_pulse_compare( const void *A, const void *B );


/* Functions fron ep385_run.c */

bool ep385_do_update( void );
void ep385_do_checks( FUNCTION *f );
void ep385_set_pulses( FUNCTION *f );
void ep385_full_reset( void );
void ep385_cw_setup( void );


/* Functions from ep385_gpib.c */

bool ep385_init( const char *name );
bool ep385_run( bool state );
bool ep385_set_channels( void );
bool ep385_set_channel_state( int channel, bool flag );


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
