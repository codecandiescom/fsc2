/*
  $Id$
*/


#include "fsc2.h"


/* Here are all the directly exported functions (i.e. exported either implicit
   as a hook functions or via the Functions data base) */

int hfs9000_init_hook( void );
int hfs9000_test_hook( void );
int hfs9000_end_of_test_hook( void );
int hfs9000_exp_hook( void );
int hfs9000_end_of_exp_hook( void );
void hfs9000_exit_hook( void );


Var *pulser_update( Var *v );
Var *pulser_shift( Var *v );
Var *pulser_increment( Var *v );
Var *pulser_pulse_reset( Var *v );
Var *pulser_lock_keyboard( Var *v );


/* Definitions needed for the pulser */

#define Ticks long              // for times in units of the pulsers time base
#define Ticks_max l_max
#define Ticks_min l_min


/* name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "HFS9000"


#define MIN_TIMEBASE            1.6e-9     // minimum pulser time base: 1.6 ns
#define MAX_TIMEBASE            2.0e-5     // maximum pulser time base: 20 us

#define MAX_CHANNELS            4          // number of channels


# define MIN_POD_HIGH_VOLTAGE  -1.5
# define MAX_POD_HIGH_VOLTAGE   5.5

# define MIN_POD_LOW_VOLTAGE   -2.0
# define MAX_POD_LOW_VOLTAGE    5.0

#define MAX_POD_VOLTAGE_SWING   5.5
#define MIN_POD_VOLTAGE_SWING   0.5

#define VOLTAGE_RESOLUTION      0.01

#define MAX_TRIG_IN_LEVEL       4.7
#define MIN_TRIG_IN_LEVEL      -4.7

#define MAX_PULSER_BITS       65536        // maximum number of bits in channel


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
	int self;                    // the functions number
	bool is_used;                // set if the function has been declared in
	                             // the ASSIGNMENTS section
	bool is_needed;              // set if the function has been assigned
                                 // pulses
	struct _C_ *channel;         // channel assigned to function
	bool need_constant;

	int num_pulses;              // number of pulses assigned to the function
	int num_active_pulses;       // number of pulses currenty in use
	struct _p_ **pulses;         // list of pulse pointers

	long max_seq_len;            // maximum length of the pulse sequence

	bool is_inverted;            // if set polarity is inverted

	Ticks delay;                 // delay for the function/channel combination
	bool is_delay;

	double high_level;           // high and low voltage levels for the pod(s)
	double low_level;            // associated with the fucntion

	bool is_high_level;
	bool is_low_level;

} FUNCTION;


typedef struct _C_ {
	int self;
	FUNCTION *function;
	bool needs_update;
} CHANNEL;


typedef struct {
	int device;              // GPIB number of the device

	double timebase;         // time base of the digitizer
	bool is_timebase;

	int trig_in_mode;        //	EXTERNAL or INTERNAL
	int trig_in_slope;       //	only in EXTERNAL mode
	double trig_in_level;    //	only in EXTERNAL mode
	Ticks repeat_time;       //	only in INTERNAL mode

	bool is_cw_mode;         // set for cw mode

	bool is_trig_in_mode;
	bool is_trig_in_slope;
	bool is_trig_in_level;
	bool is_repeat_time;

	Ticks neg_delay;
	bool is_neg_delay;       // if any of the functions has a negative delay

	long max_seq_len;        // maximum length of all pulse sequences

	FUNCTION function[ PULSER_CHANNEL_NUM_FUNC ];
	CHANNEL channel[ MAX_CHANNELS ];

	int needed_channels;     // number of channels that are going to be needed
	                         // in the experiment

	bool needs_update;       // set if pulse properties have been changed in
                             // test run or experiment
	bool is_running;         // set if the pulser is in run mode

	Ticks mem_size;          // size of the complete sequence, i.e. including
	                         // the memory needed for padding
} HFS9000;


typedef struct _p_ {

	long num;                // (positive) number of the pulse 

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

	CHANNEL *channel;

	bool needs_update;       // set if the pulses properties have been changed
                             // in test run or experiment
} PULSE;


#if defined( HFS9000_MAIN )

bool hfs9000_is_needed = UNSET;
HFS9000 hfs9000;
PULSE *hfs9000_Pulses = NULL;
bool hfs9000_IN_SETUP = UNSET;

#else

extern bool hfs9000_is_needed;
extern HFS9000 hfs9000;
extern PULSE *hfs9000_Pulses;
extern bool hfs9000_IN_SETUP;

#endif



/* Here follow the functions from hfs9000_gen.c */

bool hfs9000_store_timebase( double timebase );
bool hfs9000_assign_channel_to_function( int function, long channel );
bool hfs9000_invert_function( int function );
bool hfs9000_set_function_delay( int function, double delay );
bool hfs9000_set_function_high_level( int function, double voltage );
bool hfs9000_set_function_low_level( int function, double voltage );
bool hfs9000_set_trigger_mode( int mode );
bool hfs9000_set_trig_in_level( double voltage );
bool hfs9000_set_trig_in_slope( int slope );
bool hfs9000_set_repeat_time( double time );

/* These are the functions from hfs9000_pulse.c */

bool hfs9000_new_pulse( long pnum );
bool hfs9000_set_pulse_function( long pnum, int function );
bool hfs9000_set_pulse_position( long pnum, double time );
bool hfs9000_set_pulse_length( long pnum, double time );
bool hfs9000_set_pulse_position_change( long pnum, double time );
bool hfs9000_set_pulse_length_change( long pnum, double time );

bool hfs9000_get_pulse_function( long pnum, int *function );
bool hfs9000_get_pulse_position( long pnum, double *time );
bool hfs9000_get_pulse_length( long pnum, double *time );
bool hfs9000_get_pulse_position_change( long pnum, double *time );
bool hfs9000_get_pulse_length_change( long pnum, double *time );

bool hfs9000_change_pulse_position( long pnum, double time );
bool hfs9000_change_pulse_length( long pnum, double time );
bool hfs9000_change_pulse_position_change( long pnum, double time );
bool hfs9000_change_pulse_length_change( long pnum, double time );

/* Here come the functions from hfs9000_util.c */

Ticks hfs9000_double2ticks( double time );
double hfs9000_ticks2double( Ticks ticks );
void hfs9000_check_pod_level_diff( double high, double low );
PULSE *hfs9000_get_pulse( long pnum );
const char *hfs9000_ptime( double time );
const char *hfs9000_pticks( Ticks ticks );
int hfs9000_start_compare( const void *A, const void *B );
Ticks hfs9000_get_max_seq_len( void );
void hfs9000_calc_padding( void );
void hfs9000_set( bool *arena, Ticks start, Ticks len, Ticks offset );
int hfs9000_diff( bool *old, bool *new, Ticks *start, Ticks *length );
