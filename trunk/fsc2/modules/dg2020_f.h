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
Var *pulser_shift( Var *v );
Var *pulser_increment( Var *v );
Var *pulser_reset( Var *v );



/* Definitions needed for the pulser */

#define Ticks long              // for times in units of the pulsers time base


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
	int num_active_pulses;       // number of pulses currenty in use
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

	bool needs_update;       // set if pulse properties have been changed in
                             // test run or experiment
	bool is_running;         // set if the pulser is in run mode

} DG2020;


typedef struct _p_ {

	long num;                // number of the pulse (pulses used fro realize
                             // phase cycling have negative, normal pulses
	                         // positive numbers

	bool is_active;          // set if the pulse is really used
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

	CHANNEL *channel;        // channels the pulse belongs to

	struct _p_ *for_pulse;   // only for phase cycling pulses: the pulse the
	                         // phase cycling pulse is used for

	bool needs_update;       // set if the pulses properties have been changed
                             // in test run or experiment

} PULSE;


/* Here the global variables of the module are declared */


#if defined (DG2020_MAIN )

bool dg2020_is_needed = UNSET;
DG2020 dg2020;
PULSE *Pulses = NULL;

#else

extern bool dg2020_is_needed;
extern DG2020 dg2020;
extern PULSE *Pulses;

#endif



/* Here follow the functions from dg2020_gen.c */

bool set_timebase( double timebase );
bool assign_function( int function, long pod );
bool assign_channel_to_function( int function, long channel );
bool invert_function( int function );
bool set_delay_function( int function, double delay );
bool set_function_high_level( int function, double voltage );
bool set_function_low_level( int function, double voltage );
bool set_trigger_mode( int mode );
bool set_trig_in_level( double voltage );
bool set_trig_in_slope( int slope );
bool set_repeat_time( double time );

bool set_phase_reference( int phase, int function );
bool setup_phase( int func, PHS phs );


/* These are the functions from dg2020_pulse.c */

bool new_pulse( long pnum );
bool set_pulse_function( long pnum, int function );
bool set_pulse_position( long pnum, double time );
bool set_pulse_length( long pnum, double time );
bool set_pulse_position_change( long pnum, double time );
bool set_pulse_length_change( long pnum, double time );
bool set_pulse_phase_cycle( long pnum, int cycle );

bool get_pulse_function( long pnum, int *function );
bool get_pulse_position( long pnum, double *time );
bool get_pulse_length( long pnum, double *time );
bool get_pulse_position_change( long pnum, double *time );
bool get_pulse_length_change( long pnum, double *time );
bool get_pulse_phase_cycle( long pnum, int *cycle );

bool change_pulse_position( long pnum, double time );
bool change_pulse_length( long pnum, double time );
bool change_pulse_position_change( long pnum, double time );
bool change_pulse_length_change( long pnum, double time );


/* Here come the functions from dg2020_util.c */

Ticks double2ticks( double time );
double ticks2double( Ticks ticks );
void check_pod_level_diff( double high, double low );
PULSE *get_pulse( long pnum );
const char *ptime( double time );
const char *pticks( Ticks ticks );
CHANNEL *get_next_free_channel( void );
int start_compare( const void *A, const void *B );


/* The functions from dg2020_init.c */

void init_setup( void );
void basic_pulse_check( void );
void basic_functions_check( void );
void distribute_channels( void );
void pulse_start_setup( void );
void create_phase_pulses( int func );
PULSE *new_phase_pulse( FUNCTION *f, PULSE *p, int pos, int pod );


/* Functions from dg2020_run.c */

void do_checks( void );
void do_update( void );


/* Finally the functions from dg2020_gpib.c */

bool dg2020_init( const char *name );
bool dg2020_run( bool flag );
bool dg2020_set_timebase( void );
bool dg2020_update_data( void );
