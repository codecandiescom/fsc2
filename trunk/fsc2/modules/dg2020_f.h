/*
 $Id$
*/


#include "fsc2.h"


/* Here are all the directy exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int dg2020_init_hook( void );
int dg2020_test_hook( void );
int dg2020_exp_hook( void );
int dg2020_end_of_exp_hook( void );
void dg2020_exit_hook( void );


Var *pulser_start( Var *v );



/* Definitions needed for the pulser*/

#define Ticks long              // for times in units of the pulsers time base


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


#define START ( ( bool ) 0 )
#define STOP  ( ( bool ) 1 )



/* typedefs of structures needed in the module */

typedef struct _F_ {

	int self;                    // the functions number

	bool is_used;                // set if the function has been declared in
	                             // the ASSIGNMENTS section
	bool is_needed;              // set if the function has been assigned
                                 // pulses

	struct _P_ *pod;             // points to the pod assigned to the function
	struct _P_ *pod2;            // points to the second pod assigned to the 
	                             // function (phase functions only)

	PHS phs;                     // phase functions only: how to translate
	                             // phases to pod outputs
	bool is_phs;

	int num_channels;            // number of channels assigned to function
	int num_needed_channels;     // number of channels really needed
	struct _C_ *channel[ MAX_CHANNELS ];

	int num_pulses;              // number of pulses assigned to the function
	struct _p_ **pulses;         // list of pulse pointers

	struct _F_ *phase_func;      // for phase functions here's stored which
	                             // function it's going to take care of while
	                             // for normal functions it's a pointer to the
	                             // phase function responsible for it.

	bool is_inverted;            // if set polarity is inverted

	Ticks delay;                 // delay for the function/pod combination
	bool is_delay;

	double high_level;           // high and low voltage levels for the pod(s)
	double low_level;            // associated with the fucntion

	bool is_high_level;
	bool is_low_level;

} FUNCTION;


typedef struct _P_ {
	FUNCTION *function;
} POD;


typedef struct _C_ {
	FUNCTION *function;
} CHANNEL;


typedef struct
{
	int device;              // GPIB number of the device

	double timebase;         // time base of the digitizer
	bool is_timebase;

	int trig_in_mode;        //	EXTERNAL or INTERNAL
	int trig_in_slope;       //	only in EXTERNAL mode
	double trig_in_level;    //	only in EXTERNAL mode
	Ticks repeat_time;       //	only in INTERNAL mode

	bool is_trig_in_mode;
	bool is_trig_in_slope;
	bool is_trig_in_level;
	bool is_repeat_time;

	FUNCTION function[ PULSER_CHANNEL_NUM_FUNC ];
	POD pod[ MAX_PODS ];
	CHANNEL channel[ MAX_CHANNELS ];

	int needed_channels;     // number of channels that are going to be needed
	                         // for the experiment

	bool need_update;        // set if pulse properties have been changed in
                             // test run or experiment
	bool is_running;         // set if the pulser is in run mode

} DG2020;


typedef struct _p_ {

	long num;                // number of the pulse (pulses used fro realize
                             // phase cycling have negative, normal pulses
	                         // positive numbers

	struct _p_ *next;
	struct _p_ *prev;

	FUNCTION *function;      // function the pulse is associated with

	Ticks pos;               // current position, length, position change and
	Ticks len;               // length change of pulse (in units of the pulsers
	Ticks dpos;              // time base)
	Ticks dlen;

	Phase_Sequence *pc;      // the pulse sequence to be used for the pulse
	                         // (or NULL if none is to be used)
	Ticks maxlen;            // maximum length of the pulse (only to be used
                             // together with replacement pulses)
	long num_repl;           // number of replacement pulses
	long *repl_list;         // array of the numbers of the replacement pulses

	bool is_a_repl;          // set if the pulse is a replacement pulse

	bool is_function;        // flags that are set when the corresponding
	bool is_pos;             // property has been set
	bool is_len;
	bool is_dpos;
	bool is_dlen;
	bool is_maxlen;

	Ticks initial_pos;       // position, length, position change and length
	Ticks initial_len;       // change at the start of the experiment
	Ticks initial_dpos;
	Ticks initial_dlen;

	Ticks old_pos;           // position and length of pulse before a change
	Ticks old_len;           // is applied

	CHANNEL **channel;       // list of channels the pulse belongs to

	struct _p_ *for_pulse;   // only for phase cycling pulses: the pulse the
	                         // phase cycling pulse is used for

	bool need_update;        // set if the pulses properties have been changed
                             // in test run or experiment

} PULSE;


/* All the remaining declarations are internal to the module */

/* The following functions are indirectly exported via the pulser structure
   as defined in pulser.h */

static bool set_timebase( double timebase );
static bool assign_function( int function, long pod );
static bool assign_channel_to_function( int function, long channel );
static bool invert_function( int function );
static bool set_delay_function( int function, double delay );
static bool set_function_high_level( int function, double voltage );
static bool set_function_low_level( int function, double voltage );
static bool set_trigger_mode( int mode );
static bool set_trig_in_level( double voltage );
static bool set_trig_in_slope( int slope );
static bool set_repeat_time( double time );

static bool set_phase_reference( int phase, int function );

static bool new_pulse( long pnum );
static bool set_pulse_function( long pnum, int function );
static bool set_pulse_position( long pnum, double time );
static bool set_pulse_length( long pnum, double time );
static bool set_pulse_position_change( long pnum, double time );
static bool set_pulse_length_change( long pnum, double time );
static bool set_pulse_phase_cycle( long pnum, int cycle );
static bool set_pulse_maxlen( long pnum, double time );
static bool set_pulse_replacements( long pnum, long num_repl, 
									long *repl_list );

static bool get_pulse_function( long pnum, int *function );
static bool get_pulse_position( long pnum, double *time );
static bool get_pulse_length( long pnum, double *time );
static bool get_pulse_position_change( long pnum, double *time );
static bool get_pulse_length_change( long pnum, double *time );
static bool get_pulse_phase_cycle( long pnum, int *cycle );
static bool get_pulse_maxlen( long pnum, double *time );

static bool setup_phase( int func, PHS phs );


/* All the remaining functions are exclusively used internally */

static Ticks double2ticks( double time );
static double ticks2double( Ticks ticks );
static void check_pod_level_diff( double high, double low );
static PULSE *get_pulse( long pnum );
static const char *ptime( double time );
static const char *pticks( Ticks ticks );
static void check_consistency( void );

static void basic_pulse_check( void );
static void basic_functions_check( void );
static void distribute_channels( void );
static CHANNEL *get_next_free_channel( void );
static void pulse_start_setup( void );
static int start_compare( const void *A, const void *B );
void create_phase_pulses( int func );
PULSE *new_phase_pulse( FUNCTION *f, PULSE *p, int pos, int pod );

static bool change_pulse_position( long pnum, double time );
static bool change_pulse_length( long pnum, double time );
static bool change_pulse_position_change( long pnum, double time );
static bool change_pulse_length_change( long pnum, double time );

static void do_checks( void );
static void do_update( void );


/* All the functions starting with dg2020 are functions that really access the
   pulser, i.e. they can only be used after the GPIB bus has been initialized
*/

static bool dg2020_init( const char *name );
static bool dg2020_run( bool flag );
static bool dg2020_set_timebase( void );
static bool dg2020_update_data( void );
