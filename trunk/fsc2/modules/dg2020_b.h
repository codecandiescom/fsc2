/*
 $Id$
*/


#include "fsc2.h"


/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int dg2020_b_init_hook( void );
int dg2020_b_test_hook( void );
int dg2020_b_end_of_test_hook( void );
int dg2020_b_exp_hook( void );
int dg2020_b_end_of_exp_hook( void );
void dg2020_b_exit_hook( void );


Var *pulser_update( Var *v );
Var *pulser_shift( Var *v );
Var *pulser_increment( Var *v );
Var *pulser_next_phase( Var *v );
Var *pulser_phase_reset( Var *v );
Var *pulser_pulse_reset( Var *v );



/* Definitions needed for the pulser */

#define Ticks long              // for times in units of the pulsers time base
#define Ticks_max l_max
#define Ticks_min l_min


/* name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "DG2020"


#define MIN_TIMEBASE            5.0e-9     // minimum pulser time base: 5 ns
#define MAX_TIMEBASE            0.1        // maximum pulser time base: 0.1 s

#define MAX_PODS                12         // number of pods
#define MAX_CHANNELS            36         // number of channels


# define MIN_POD_HIGH_VOLTAGE  -2.0     // data for P3420 (Variable Output Pod)
# define MAX_POD_HIGH_VOLTAGE   7.0

# define MIN_POD_LOW_VOLTAGE   -3.0
# define MAX_POD_LOW_VOLTAGE    6.0

#define MAX_POD_VOLTAGE_SWING   9.0
#define MIN_POD_VOLTAGE_SWING   0.5

#define VOLTAGE_RESOLUTION      0.1

#define MAX_TRIG_IN_LEVEL       5.0
#define MIN_TRIG_IN_LEVEL      -5.0


#define MIN_BLOCK_SIZE     64     // minimum length of a block
#define MAX_BLOCK_REPEATS  65536  // maximum of repeats of block in sequence
#define MAX_PULSER_BITS    65536  // maximum number of bits in channel


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

typedef struct _F_ {

	int self;                    // the functions number

	bool is_used;                // set if the function has been declared in
	                             // the ASSIGNMENTS section
	bool is_needed;              // set if the function has been assigned
                                 // pulses

	struct _P_ *pod[ MAX_PODS_PER_FUNC ];   // pods assigned to the function
	int num_pods;

	int num_channels;            // number of channels assigned to function
	int num_needed_channels;     // number of channels really needed
	struct _C_ *channel[ MAX_CHANNELS ];

	int num_pulses;              // number of pulses assigned to the function
	int num_active_pulses;       // number of pulses currenty in use
	struct _p_ **pulses;         // list of pulse pointers

	bool needs_phases;           // set if phase cycling is needed
	struct _PHS_ *phase_setup;
	int next_phase;

	long max_seq_len;            // maximum length of the pulse sequence

	bool is_inverted;            // if set polarity is inverted

	Ticks delay;                 // delay for the function/pod combination
	bool is_delay;

	double high_level;           // high and low voltage levels for the pod(s)
	double low_level;            // associated with the fucntion

	bool is_high_level;
	bool is_low_level;

} FUNCTION;


typedef struct _P_ {
	int self;                    // pod number
	FUNCTION *function;          // the function the pod is assigned to
} POD;


typedef struct _C_ {
	int self;
	FUNCTION *function;
} CHANNEL;


typedef struct {
	bool is_used;
	char blk_name[ 9 ];
	Ticks start;
	long repeat;
} BLOCK;


typedef struct _PHS_ {
	bool is_set[ PHASE_CW - PHASE_PLUS_X + 1 ];
	bool is_needed[ PHASE_CW - PHASE_PLUS_X + 1 ];
	POD *pod[ PHASE_CW - PHASE_PLUS_X + 1 ];
	FUNCTION *function;
} PHASE_SETUP;


typedef struct {
	int device;              // GPIB number of the device

	double timebase;         // time base of the digitizer
	bool is_timebase;

	int trig_in_mode;        //	EXTERNAL or INTERNAL
	int trig_in_slope;       //	only in EXTERNAL mode
	double trig_in_level;    //	only in EXTERNAL mode
	int trig_in_impedance;   //	only in EXTERNAL mode
	Ticks repeat_time;       //	only in INTERNAL mode

	bool is_trig_in_mode;
	bool is_trig_in_slope;
	bool is_trig_in_level;
	bool is_trig_in_impedance;
	bool is_repeat_time;

	Ticks neg_delay;
	bool is_neg_delay;       // if any of the functions has a negative delay

	long max_seq_len;        // maximum length of all pulse sequences

	FUNCTION function[ PULSER_CHANNEL_NUM_FUNC ];
	POD pod[ MAX_PODS ];
	CHANNEL channel[ MAX_CHANNELS ];

	int needed_channels;     // number of channels that are going to be needed
	                         // for the experiment

	bool needs_update;       // set if pulse properties have been changed in
                             // test run or experiment
	bool is_running;         // set if the pulser is in run mode

	BLOCK block[ 2 ];        // blocks needed for padding

	Ticks mem_size;          // size of the complete sequence, i.e. including
	                         // the memory needed for padding
} DG2020;


typedef struct _p_ {

	long num;                // number of the pulse (pulses used fro realize
                             // phase cycling have negative, normal pulses
	                         // positive numbers

	bool is_active;          // set if the pulse is really used
	bool was_active;
	bool has_been_active;    // used to find useless pulses

	struct _p_ *next;
	struct _p_ *prev;

	FUNCTION *function;      // function the pulse is associated with

	Ticks pos;               // current position, length, position change and
	Ticks len;               // length change of pulse (in units of the pulsers
	Ticks dpos;              // time base)
	Ticks dlen;

	Phase_Sequence *pc;      // the pulse sequence to be used for the pulse
	                         // (or NULL if none is to be used)

	bool is_function;        // flags that are set when the corresponding
	bool is_pos;             // property has been set
	bool is_len;
	bool is_dpos;
	bool is_dlen;

	Ticks initial_pos;       // position, length, position change and length
	Ticks initial_len;       // change at the start of the experiment
	Ticks initial_dpos;
	Ticks initial_dlen;

	bool initial_is_pos;     // property has initially been set
	bool initial_is_len;
	bool initial_is_dpos;
	bool initial_is_dlen;

	Ticks old_pos;           // position and length of pulse before a change
	Ticks old_len;           // is applied

	bool is_old_pos;
	bool is_old_len;

	CHANNEL *channel;        // channel the pulse belongs to - only needed
	                         // for phase pulses

	struct _p_ *for_pulse;   // only for phase cycling pulses: the pulse the
	                         // phase cycling pulse is used for

	bool needs_update;       // set if the pulses properties have been changed
                             // in test run or experiment

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



/* Here follow the functions from dg2020_gen.c */

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
bool dg2020_set_repeat_time( double time );
bool dg2020_set_phase_reference( int phase, int function );
bool dg2020_phase_setup_prep( int func, int type, int pod, long val,
							  long protocol );
bool dg2020_phase_setup( int func );
bool dg2020_set_phase_switch_delay( int func, double time );
bool dg2020_set_grace_period( double time );


/* These are the functions from dg2020_pulse.c */

bool dg2020_new_pulse( long pnum );
bool dg2020_set_pulse_function( long pnum, int function );
bool dg2020_set_pulse_position( long pnum, double time );
bool dg2020_set_pulse_length( long pnum, double time );
bool dg2020_set_pulse_position_change( long pnum, double time );
bool dg2020_set_pulse_length_change( long pnum, double time );
bool dg2020_set_pulse_phase_cycle( long pnum, long cycle );

bool dg2020_get_pulse_function( long pnum, int *function );
bool dg2020_get_pulse_position( long pnum, double *time );
bool dg2020_get_pulse_length( long pnum, double *time );
bool dg2020_get_pulse_position_change( long pnum, double *time );
bool dg2020_get_pulse_length_change( long pnum, double *time );
bool dg2020_get_pulse_phase_cycle( long pnum, long *cycle );

bool dg2020_change_pulse_position( long pnum, double time );
bool dg2020_change_pulse_length( long pnum, double time );
bool dg2020_change_pulse_position_change( long pnum, double time );
bool dg2020_change_pulse_length_change( long pnum, double time );


/* Here come the functions from dg2020_util.c */

Ticks dg2020_double2ticks( double time );
double dg2020_ticks2double( Ticks ticks );
void dg2020_check_pod_level_diff( double high, double low );
PULSE *dg2020_get_pulse( long pnum );
const char *dg2020_ptime( double time );
const char *dg2020_pticks( Ticks ticks );
CHANNEL *dg2020_get_next_free_channel( void );
int dg2020_start_compare( const void *A, const void *B );
bool dg2020_find_phase_pulse( PULSE *p, PULSE ***pl, int *num );
Ticks dg2020_get_max_seq_len( void );
void dg2020_calc_padding( void );
bool dg2020_prep_cmd( char **cmd, int channel, Ticks address, Ticks length );
void dg2020_set( bool *arena, Ticks start, Ticks len, Ticks offset );
int dg2020_diff( bool *old, bool *new, Ticks *start, Ticks *length );


/* The functions from dg2020_init.c */

void dg2020_init_setup( void );
void dg2020_distribute_channels( void );
void dg2020_pulse_start_setup( void );
void dg2020_create_phase_pulses( FUNCTION *f );
PULSE *dg2020_new_phase_pulse( FUNCTION *f, PULSE *p, int nth,
							   int pos, int pod );
void dg2020_set_phase_pulse_pos_and_len( FUNCTION *f, PULSE *np,
										 PULSE *p, int nth );


/* Functions from dg2020_run.c */

void dg2020_do_update( void );
void dg2020_reorganize_pulses( bool flag );
void dg2020_do_checks( FUNCTION *f );
void dg2020_full_reset( void );
PULSE *dg2020_delete_pulse( PULSE *p );
void dg2020_reorganize_phases( FUNCTION *f, bool flag );
void dg2020_recalc_phase_pulse( FUNCTION *f, PULSE *phase_p,
								PULSE *p, int nth, bool flag );
void dg2020_finalize_phase_pulses( int func );
void dg2020_set_pulses( FUNCTION *f );
void dg2020_set_phase_pulses( FUNCTION *f );
void dg2020_commit( FUNCTION * f, bool flag );
void dg2020_commit_phases( FUNCTION * f, bool flag );
void dg2020_clear_padding_block( FUNCTION *f );


/* Finally the functions from dg2020_gpib.c */

bool dg2020_init( const char *name );
bool dg2020_run( bool flag );
bool dg2020_set_timebase( double timebase );
bool dg2020_set_memory_size( long mem_size );
bool dg2020_channel_assign(  int channel, int pod );
bool dg2020_update_data( void );
bool dg2020_make_blocks( int num_blocks, BLOCK *block );
bool dg2020_make_seq( int num_blocks, BLOCK *block );
bool pulser_set_channel( int channel, Ticks address,
						 Ticks length, char *pattern );
bool dg2020_set_constant( int channel, Ticks address,
						  Ticks length, int state );
bool dg2020_set_pod_high_level( int pod, double voltage );
bool dg2020_set_pod_low_level( int pod, double voltage );
bool dg2020_set_trigger_in_level( double voltage );
bool dg2020_set_trigger_in_slope( int slope );
bool dg2020_set_trigger_in_impedance( int state );
void dg2020_gpib_failure( void );
