/*
 $Id$
*/


#include "fsc2.h"


#define Ticks long


#define MIN_TIMEBASE            5.0e-9     /* 5 ns */
#define MAX_TIMEBASE            0.1        /* 0.1 s */

#define MAX_PODS                12
#define MAX_CHANNELS            36


# define MIN_POD_HIGH_VOLTAGE  -2.0  /* Data for P3420 (Variable Output Pod) */
# define MAX_POD_HIGH_VOLTAGE   7.0

# define MIN_POD_LOW_VOLTAGE   -3.0
# define MAX_POD_LOW_VOLTAGE    6.0

#define MAX_POD_VOLTAGE_SWING   9.0
#define MIN_POD_VOLTAGE_SWING   0.5

#define VOLTAGE_RESOLUTION      0.1

#define MAX_TRIG_IN_LEVEL       5.0
#define MIN_TRIG_IN_LEVEL      -5.0



#define MIN_BLOCK_SIZE     64     /* minimum length of block */
#define MAX_BLOCK_REPEATS  65536  /* maximum of repeats of block in sequence */
#define MAX_PULSER_BITS    65536  /* maximum number of bits in channel */



typedef struct _F_ {
	int self;                    // the functions number

	bool is_used;                // set if the function has been declared in
	                             // the ASSIGNMENTS section
	bool is_needed;              // set if the function has been assigned
                                 // pulses

	struct _P_ *pod;             // points to the pod assigned to the function
	struct _P_ *pod2;            // points to the second pod assigned to the 
	                             // function (phase functions only)

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
	double timebase;
	bool is_timebase;

	int trig_in_mode;        /* EXTERNAL or INTERNAL */
	int trig_in_slope;       /* only in EXTERNAL mode */
	double trig_in_level;    /* only in EXTERNAL mode */
	Ticks repeat_time;       /* only in INTERNAL mode */

	bool is_trig_in_mode;
	bool is_trig_in_slope;
	bool is_trig_in_level;
	bool is_repeat_time;

	FUNCTION function[ PULSER_CHANNEL_NUM_FUNC ];
	POD pod[ MAX_PODS ];
	CHANNEL channel[ MAX_CHANNELS ];

	int needed_channels;
} DG2020;


typedef struct _p_ {

	long num;

	struct _p_ *next;
	struct _p_ *prev;

	FUNCTION *function;
	Ticks pos;
	Ticks len;
	Ticks dpos;
	Ticks dlen;
	Phase_Sequence *pc;
	Ticks maxlen;
	long num_repl;
	long *repl_list;

	bool is_function;
	bool is_pos;
	bool is_len;
	bool is_dpos;
	bool is_dlen;
	bool is_maxlen;

	Ticks initial_pos;
	Ticks initial_len;
	Ticks initial_dpos;
	Ticks initial_dlen;

} PULSE;



int dg2020_init_hook( void );
int dg2020_test_hook( void );
int dg2020_exp_hook( void );
void dg2020_exit_hook( void );


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
