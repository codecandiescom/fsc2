/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#if ! defined DG2020_B_HEADER
#define DG2020_B_HEADER

#include "fsc2_module.h"


/* Include configuration information for the device */

#include "dg2020_b.conf"


/* Uncomment the next line for debugging of data sent via GPIB bus */

/*
#define DG2020_B_GPIB_DEBUG 1
*/

/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int dg2020_b_init_hook( void );
int dg2020_b_test_hook( void );
int dg2020_b_end_of_test_hook( void );
int dg2020_b_exp_hook( void );
int dg2020_b_end_of_exp_hook( void );
void dg2020_b_exit_hook( void );


Var *pulser_name( Var *v );
Var *pulser_automatic_shape_pulses( Var *v );
Var *pulser_automatic_twt_pulses( Var *v );
Var *pulser_show_pulses( Var *v );
Var *pulser_dump_pulses( Var *v );
Var *pulser_shape_to_defense_minimum_distance( Var *v );
Var *pulser_defense_to_shape_minimum_distance( Var *v );
Var *pulser_minimum_twt_pulse_distance( Var *v );
Var *pulser_keep_all_pulses( Var *v );
Var *pulser_maximum_pattern_length( Var *v );
Var *pulser_state( Var *v );
Var *pulser_channel_state( Var *v );
Var *pulser_update( Var *v );
Var *pulser_shift( Var *v );
Var *pulser_increment( Var *v );
Var *pulser_next_phase( Var *v );
Var *pulser_phase_reset( Var *v );
Var *pulser_pulse_reset( Var *v );
Var *pulser_reset( Var *v );
Var *pulser_reset( Var *v );
Var *pulser_lock_keyboard( Var *v );
Var *pulser_command( Var *v );


/* Definitions needed for the pulser */

#define Ticks long              /* times in units of the pulsers time base */
#define Ticks_max l_max
#define Ticks_min l_min

#define TICKS_MAX LONG_MAX
#define TICKS_MIN LONG_MIN

#define Ticksrnd lrnd

#define MIN_TIMEBASE            5.0e-9   /* minimum pulser time base: 5 ns */
#define MAX_TIMEBASE            0.1      /* maximum pulser time base: 0.1 s */

#define MAX_PODS                12       /* number of pods */
#define MAX_CHANNELS            36       /* number of channels */


# define MIN_POD_HIGH_VOLTAGE  -2.0     /* for P3420 (Variable Output Pod) */
# define MAX_POD_HIGH_VOLTAGE   7.0

# define MIN_POD_LOW_VOLTAGE   -3.0
# define MAX_POD_LOW_VOLTAGE    6.0

#define MAX_POD_VOLTAGE_SWING   9.0
#define MIN_POD_VOLTAGE_SWING   0.5

#define VOLTAGE_RESOLUTION      0.1

#define MAX_TRIG_IN_LEVEL       5.0
#define MIN_TRIG_IN_LEVEL      -5.0


#define MIN_BLOCK_SIZE     64     /* minimum length of a block */
#define MAX_BLOCK_REPEATS  65536  /* maximum of repeats of block in sequence */
#define MAX_PULSER_BITS    65536  /* maximum number of bits in channel */


#define START ( ( bool ) 1 )
#define STOP  ( ( bool ) 0 )

#define MAX_PODS_PER_FUNC       5


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

typedef struct {
	Ticks pos;
	Ticks len;
	struct PULSE *pulse;
} PULSE_PARAMS;


typedef struct _F_ {
	int self;                   /* the functions number */
	const char *name;           /* name of function */
	bool is_used;               /* set if the function has been declared in
								   the ASSIGNMENTS section */

	struct POD *pod[ MAX_PODS_PER_FUNC ];   /* list of pointers to pods 
											   assigned to the function */
	int num_pods;

	int num_channels;           /* number of channels assigned to function */
	int num_needed_channels;    /* number of channels really needed */
	struct CHANNEL *channel[ MAX_CHANNELS ];  /* list of pointers to channels
											 assigned to the function */
	bool need_constant;

	int num_pulses;             /* number of pulses assigned to the function */
	int num_active_pulses;      /* number of pulses currenty in use */
	struct PULSE **pulses;      /* list of pulse pointers */

	struct PHASE_SETUP *phase_setup;
	int next_phase;
	int pc_len;                 /* length of the phase cycle */

	long max_seq_len;           /* maximum length of the pulse sequence */

	bool is_inverted;           /* if set polarity is inverted */

	Ticks delay;                /* delay for the function/pod combination */
	bool is_delay;

	double high_level;          /* high and low voltage levels of the pod(s) */
	double low_level;           /* associated with the function */

	bool is_high_level;         /* set when high or low volatge level was */
	bool is_low_level;          /* specified for the function */

	bool *pm;
	struct CHANNEL **pcm;       /* phase matrix */

	PULSE_PARAMS *pulse_params;
	int num_params;

	bool uses_auto_shape_pulses;     /* set when shape pulses are to be set */
	Ticks left_shape_padding;        /* left side padding for shape pulses */
	Ticks right_shape_padding;       /* right  side padding for shape pulses */
	Ticks min_left_shape_padding;    /* minimum left and right shape padding */
	Ticks min_right_shape_padding;   /* that could be set (might be lower than
										what was requested) */

	bool uses_auto_twt_pulses;       /* set when TWT pulses are to be set */
	Ticks left_twt_padding;          /* left side padding for TWT pulses */
	Ticks right_twt_padding;         /* right side padding for TWT pulses */
	Ticks min_left_twt_padding;      /* minimum left and right TWT padding */
	Ticks min_right_twt_padding;     /* that could be set (might be lower than
										what was requested) */

	long max_duty_warning;   /* number of times TWT duty cycle was exceeded */

} FUNCTION;


typedef struct POD {
	int self;                   /* pod number */
	FUNCTION *function;         /* the function the pod is assigned to */
} POD;


typedef struct CHANNEL {
	int self;
	FUNCTION *function;
	bool needs_update;
    char *old_d;
    char *new_d;
} CHANNEL;


typedef struct {
	bool is_used;
	char blk_name[ 9 ];
	Ticks start;
	long repeat;
} BLOCK;


typedef struct PHASE_SETUP {
	bool is_defined;
	bool is_set[ PHASE_MINUS_Y - PHASE_PLUS_X + 1 ];
	bool is_needed[ PHASE_MINUS_Y - PHASE_PLUS_X + 1 ];
	POD *pod[ PHASE_MINUS_Y - PHASE_PLUS_X + 1 ];
	FUNCTION *function;
} PHASE_SETUP;


typedef struct {
	int device;              /* GPIB number of the device */

	double timebase;         /* time base of the digitizer */
	bool is_timebase;

	int trig_in_mode;        /* EXTERNAL or INTERNAL */
	int trig_in_slope;       /* only in EXTERNAL mode */
	double trig_in_level;    /* only in EXTERNAL mode */
	int trig_in_impedance;   /* only in EXTERNAL mode */
	Ticks repeat_time;       /* only in INTERNAL mode */

	bool keep_all;           /* keep even unused pulses ? */

	bool is_trig_in_mode;
	bool is_trig_in_slope;
	bool is_trig_in_level;
	bool is_trig_in_impedance;
	bool is_repeat_time;

	Ticks neg_delay;
	bool is_neg_delay;       /* if any of the functions has a negative delay */

	long max_seq_len;        /* maximum length of all pulse sequences */
	bool is_max_seq_len;

	FUNCTION function[ PULSER_CHANNEL_NUM_FUNC ];
	POD pod[ MAX_PODS ];
	CHANNEL channel[ MAX_CHANNELS ];

	int needed_channels;     /* number of channels that are going to be needed
								in the experiment */

	bool needs_update;       /* set if pulse properties have been changed in
								test run or experiment */
	bool is_running;         /* set if the pulser is in run mode */

	BLOCK block[ 2 ];        /* blocks needed for padding */

	Ticks mem_size;          /* size of the complete sequence, i.e. including
								the memory needed for padding */
	Ticks shape_2_defense;
	bool is_shape_2_defense;
	long shape_2_defense_too_near;

	Ticks defense_2_shape;
	bool is_defense_2_shape;
	long defense_2_shape_too_near;

	bool is_confirmation;

	FILE *show_file;
	FILE *dump_file;

	bool do_show_pulses;
	bool do_dump_pulses;

	bool auto_shape_pulses;
	long left_shape_warning;
	long right_shape_warning;

	bool auto_twt_pulses;
	long left_twt_warning;
	long right_twt_warning;

	Ticks minimum_twt_pulse_distance;
	bool is_minimum_twt_pulse_distance;
	long twt_distance_warning;

	PHASE_SETUP *dummy_phase_setup;
	int num_dummy_phase_setups;

} DG2020;


typedef struct PULSE {

	long num;                /* number of the pulse (automatically created
								pulses have negative, normal pulses
								positive numbers */
	bool is_active;          /* set if the pulse is really used */
	bool was_active;
	bool has_been_active;    /* used to find useless pulses */

	struct PULSE *next;
	struct PULSE *prev;

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

	CHANNEL **channel;

	bool needs_update;       /* set if the pulses properties have been
								changed in test run or experiment */

	bool left_shape_warning; /* stores if for pulse the left or right shape */
	bool right_shape_warning;/* padding couldn't be set correctly */
	struct PULSE *sp;        /* for normal pulses reference to related shape
								pulse (if such exists), for shape pulses
								reference to pulse it is associated with */

	bool left_twt_warning;   /* stores if for pulse the left or right TWT */
	bool right_twt_warning;  /* padding couldn't be set correctly */
	struct PULSE *tp;        /* for normal pulses reference to related TWT
								pulse (if such exists), for TWT pulses
								reference to pulse it is associated with */
	PULSE_PARAMS *pp;
	PULSE_PARAMS *old_pp;
} PULSE;


/* Here the global variables of the module are declared */


#if defined( DG2020_B_MAIN )

bool dg2020_is_needed = UNSET;
DG2020 dg2020;
PULSE *dg2020_Pulses = NULL;
bool dg2020_IN_SETUP = UNSET;
PHASE_SETUP dg2020_phs[ 2 ];

#else

extern bool dg2020_is_needed;
extern DG2020 dg2020;
extern PULSE *dg2020_Pulses;
extern bool dg2020_IN_SETUP;
extern PHASE_SETUP dg2020_phs[ 2 ];

#endif



/* Here follow the functions from dg2020_gen_b.c */

bool dg2020_store_timebase( double timebase );
bool dg2020_assign_function( int function, long pod );
bool dg2020_assign_channel_to_function( int function, long channel );
bool dg2020_invert_function( int function );
bool dg2020_set_function_delay( int function, double delay );
bool dg2020_set_function_high_level( int function, double voltage );
bool dg2020_set_function_low_level( int function, double voltage );
bool dg2020_set_trigger_mode( int mode );
bool dg2020_set_trig_in_level( double voltage );
bool dg2020_set_trig_in_slope( int slope );
bool dg2020_set_trig_in_impedance( int state );
bool dg2020_set_repeat_time( double rep_time );
bool dg2020_set_max_seq_len( double seq_len );
bool dg2020_set_phase_reference( int phase, int function );
bool dg2020_phase_setup_prep( int func, int type, int pod, long val );
bool dg2020_phase_setup( int func );
bool dg2020_keep_all( void );


/* These are the functions from dg2020_pulse_b.c */

bool dg2020_new_pulse( long pnum );
bool dg2020_set_pulse_function( long pnum, int function );
bool dg2020_set_pulse_position( long pnum, double p_time );
bool dg2020_set_pulse_length( long pnum, double p_time );
bool dg2020_set_pulse_position_change( long pnum, double p_time );
bool dg2020_set_pulse_length_change( long pnum, double p_time );
bool dg2020_set_pulse_phase_cycle( long pnum, long cycle );

bool dg2020_get_pulse_function( long pnum, int *function );
bool dg2020_get_pulse_position( long pnum, double *p_time );
bool dg2020_get_pulse_length( long pnum, double *p_time );
bool dg2020_get_pulse_position_change( long pnum, double *p_time );
bool dg2020_get_pulse_length_change( long pnum, double *p_time );
bool dg2020_get_pulse_phase_cycle( long pnum, long *cycle );

bool dg2020_change_pulse_position( long pnum, double p_time );
bool dg2020_change_pulse_length( long pnum, double p_time );
bool dg2020_change_pulse_position_change( long pnum, double p_time );
bool dg2020_change_pulse_length_change( long pnum, double p_time );


/* Here come the functions from dg2020_util_b.c */

Ticks dg2020_double2ticks( double p_time );
double dg2020_ticks2double( Ticks ticks );
void dg2020_check_pod_level_diff( double high, double low );
PULSE *dg2020_get_pulse( long pnum );
const char *dg2020_ptime( double p_time );
const char *dg2020_pticks( Ticks ticks );
CHANNEL *dg2020_get_next_free_channel( void );
int dg2020_start_compare( const void *A, const void *B );
bool dg2020_find_phase_pulse( PULSE *p, PULSE ***pl, int *num );
Ticks dg2020_get_max_seq_len( void );
void dg2020_calc_padding( void );
bool dg2020_prep_cmd( char **cmd, int channel, Ticks address, Ticks length );
void dg2020_set( char *arena, Ticks start, Ticks len );
void dg2020_clear( char *arena, Ticks start, Ticks len );
int dg2020_diff( char *old_p, char *new_p, Ticks *start, Ticks *length );
void dg2020_duty_check( void );
Ticks dg2020_calc_max_length( FUNCTION *f );
void dg2020_show_pulses( void );
void dg2020_dump_pulses( void );
void dg2020_dump_channels( FILE *fp );


/* The functions from dg2020_init_b.c */

void dg2020_init_setup( void );


/* Functions from dg2020_run_b.c */

bool dg2020_do_update( void );
bool dg2020_reorganize_pulses( bool flag );
void dg2020_do_checks( FUNCTION *f );
void dg2020_full_reset( void );
PULSE *dg2020_delete_pulse( PULSE *p, bool warn );
void dg2020_reorganize_phases( FUNCTION *f, bool flag );
void dg2020_recalc_phase_pulse( FUNCTION *f, PULSE *phase_p,
								PULSE *p, int nth, bool flag );
void dg2020_finalize_phase_pulses( int func );
void dg2020_set_pulses( FUNCTION *f );
void dg2020_set_phase_pulses( FUNCTION *f );
void dg2020_commit( FUNCTION * f, bool flag );
void dg2020_commit_phases( FUNCTION * f, bool flag );


/* Finally the functions from dg2020_gpib_b.c */

bool dg2020_init( const char *name );
bool dg2020_run( bool flag );
bool dg2020_channel_assign(  int channel, int pod );
bool dg2020_update_data( void );
bool dg2020_make_blocks( int num_blocks, BLOCK *block );
bool dg2020_make_seq( int num_blocks, BLOCK *block );
bool dg2020_set_constant( int channel, Ticks address,
						  Ticks length, int state );
bool dg2020_lock_state( bool lock );
bool dg2020_command( const char *cmd );


#endif /* ! DG2020_B_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
