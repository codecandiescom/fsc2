/*
  $Id$
*/


#include "fsc2.h"


#define Ticks long


#define MIN_TIMEBASE   5.0e-9     /* 5 ns */
#define MAX_TIMEBASE   0.1        /* 0.1 s */

#define MAX_PODS           12
#define MAX_CHANNELS       36


# define MIN_POD_HIGH_VOLTAGE  -2.0   /* Data for P3420 (Variable Ouput Pod) */
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

#define NO_PULSE           -1     /* marks unused pulse numbers */
#define NO_CHANNEL         -1     /* marks unused channels */




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


static Ticks double2ticks( double time );
static double ticks2double( Ticks ticks );
static void check_pod_level_diff( double high, double low );



static bool dg2020_is_needed;


typedef struct {
	int self;

	bool is_used;

	struct POD *pod;
	bool is_inverted;

	int num_channels;
	int channel[ MAX_CHANNELS ];

	Ticks delay;
	bool is_delay;

	double high_level;
	double low_level;
	bool is_high_level;
	bool is_low_level;
} FUNCTION;


typedef struct {
	FUNCTION *function;
} POD;


typedef struct {
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

} DG2020;



static DG2020 dg2020;


int dg2020_init_hook( void )
{
	int i;


	/* The very first thing we have to do in the init_hook is to set up the
	   global structure for pulsers. */

	/* First we test that the name entry in the pulser structure is NULL,
	   otherwise we've got to assume, that another pulser driver has already
	   been installed. */

	if ( pulser_struct.name != NULL )
	{
		eprint( FATAL, "%s:%ld: While loading driver for DG2020 found that "
				"driver for pulser %s is already installed.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	pulser_struct.name = get_string_copy( "DG2020" );

	pulser_struct.set_timebase = set_timebase;

	pulser_struct.assign_function = assign_function;
	pulser_struct.assign_channel_to_function = assign_channel_to_function;
	pulser_struct.invert_function = invert_function;
	pulser_struct.set_delay_function = set_delay_function;
	pulser_struct.set_function_high_level = set_function_high_level;
	pulser_struct.set_function_low_level = set_function_low_level;

	pulser_struct.set_trigger_mode = set_trigger_mode;
	pulser_struct.set_repeat_time = set_repeat_time;
	pulser_struct.set_trig_in_level = set_trig_in_level;
	pulser_struct.set_trig_in_slope = set_trig_in_slope;

	pulser_struct.set_pulse_function = NULL;
	pulser_struct.set_pulse_position = NULL;
	pulser_struct.set_pulse_length = NULL;
	pulser_struct.set_pulse_position_change = NULL;
	pulser_struct.set_pulse_length_change = NULL;
	pulser_struct.set_pulse_maxlen = NULL;
	pulser_struct.set_pulse_replacements = NULL;

	pulser_struct.get_pulse_function = NULL;
	pulser_struct.get_pulse_position = NULL;
	pulser_struct.get_pulse_length = NULL;
	pulser_struct.get_pulse_position_change = NULL;
	pulser_struct.get_pulse_length_change = NULL;
	pulser_struct.get_pulse_maxlen = NULL;


	dg2020.is_timebase = UNSET;

	dg2020.is_trig_in_mode = UNSET;
	dg2020.is_trig_in_slope = UNSET;
	dg2020.is_trig_in_level = UNSET;
	dg2020.is_repeat_time = UNSET;
	
	for ( i = 0; i < MAX_PODS; i++ )
	{
		dg2020.pod[ i ].function = NULL;
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		dg2020.function[ i ].self = i;
		dg2020.function[ i ].is_used = UNSET;
		dg2020.function[ i ].pod = NULL;
		dg2020.function[ i ].num_channels = 0;
		dg2020.function[ i ].is_inverted = UNSET;
		dg2020.function[ i ].delay = 0;
		dg2020.function[ i ].is_delay = UNSET;
		dg2020.function[ i ].is_high_level = UNSET;
		dg2020.function[ i ].is_low_level = UNSET;
	}

	for ( i = 0; i < MAX_CHANNELS; i++ )
		dg2020.channel[ i ].function = NULL;

	dg2020_is_needed = SET;
	return 1;
}


int dg2020_test_hook( void )
{
/*
	if ( Plist == NULL )
	{
		dg2020_is_needed = UNSET;
		eprint( WARN, "DG2020 loaded but no pulses defined.\n" );
		return 1;
	}
*/
	return 1;
}

int dg2020_exp_hook( void )
{
	if ( ! dg2020_is_needed )
		return 1;

	return 1;
}

void dg2020_exit_hook( void )
{
	if ( ! dg2020_is_needed )
		return;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


static bool set_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid time base %f s, valid range  "
				"is %f-%f s.\n", Fname, Lc, timebase,
				MIN_TIMEBASE, MAX_TIMEBASE );
		THROW( EXCEPTION );
	}

	dg2020.is_timebase = SET;
	dg2020.timebase = timebase;
	return OK;
}


static bool assign_function( int function, long pod )
{
	if ( pod < 0 || pod > MAX_PODS )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid pod number: %ld, valid pod "
				"number are %d-%d.\n", Fname, Lc, pod, 0, MAX_PODS - 1 );
		THROW( EXCEPTION );
	}

	if ( dg2020.pod[ pod ].function != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Pod number %ld has already been "
				"assigned to function %s.\n", Fname, Lc, pod,
				Function_Names[ dg2020.pod[ pod ].function->self ] );
		THROW( EXCEPTION );
	}

	dg2020.function[ function ].is_used = SET;
	dg2020.pod[ pod ].function = &dg2020.function[ function ];
	return OK;
}


static bool assign_channel_to_function( int function, long channel )
{
	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid channel number: %ld, valid "
				"range is 0-%d.\n", Fname, Lc, channel, MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	if ( dg2020.channel[ channel ].function != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Channel %ld is already used for "
				"function %s.\n", Fname, Lc,
				Function_Names[ dg2020.channel[ channel ].function->self ] );
		THROW( EXCEPTION );
	}

	dg2020.channel[ channel ].function = &dg2020.function[ function ];

	dg2020.function[ function ].is_used = SET;
	dg2020.function[ function ].channel[ 
		dg2020.function[ function ].num_channels++ ] = channel;

	return OK;
}


static bool invert_function( int function )
{
	dg2020.function[ function ].is_used = SET;
	dg2020.function[ function ].is_inverted = SET;
	return OK;
}


static bool set_delay_function( int function, double delay )
{
	if ( dg2020.function[ function ].is_delay )
	{
		eprint( FATAL, "%s:%ld: DG2020: Delay for function %s has already "
				"been set.\n", Fname, Lc, Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	dg2020.function[ function ].delay = double2ticks( delay );
	dg2020.function[ function ].is_used = SET;

	return OK;
}


static bool set_function_high_level( int function, double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_HIGH_VOLTAGE || voltage > MAX_POD_HIGH_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid high level of %f V for "
				"function %s, valid range is %f V to %f V.\n", Fname, Lc,
				voltage, Function_Names[ function ], MIN_POD_HIGH_VOLTAGE,
				MAX_POD_HIGH_VOLTAGE );
		THROW( EXCEPTION );
	}
				
	if ( dg2020.function[ function ].is_low_level )
		check_pod_level_diff( voltage, dg2020.function[ function ].low_level );

	dg2020.function[ function ].high_level = voltage;
	dg2020.function[ function ].is_high_level = SET;
	dg2020.function[ function ].is_used = SET;

	return OK;
}


static bool set_function_low_level( int function, double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_LOW_VOLTAGE || voltage > MAX_POD_LOW_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid low level of %f V for "
				"function %s, valid range is %f V to %f V.\n", Fname, Lc,
				voltage, Function_Names[ function ], MIN_POD_LOW_VOLTAGE,
				MAX_POD_LOW_VOLTAGE );
		THROW( EXCEPTION );
	}
				
	if ( dg2020.function[ function ].is_high_level )
		check_pod_level_diff( dg2020.function[ function ].high_level,
							  voltage );

	dg2020.function[ function ].low_level = voltage;
	dg2020.function[ function ].is_low_level = SET;
	dg2020.function[ function ].is_used = SET;

	return OK;
}


static bool set_trigger_mode( int mode )
{
	assert( mode == INTERNAL || mode == EXTERNAL );

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode != mode )
	{
		eprint( FATAL, "%s:%ld: DG2020: Trigger mode has already been set to "
				"%s.\n", Fname, Lc,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
					"a trigger slope is incompatible.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
					"a trigger level is incompatible.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( dg2020.is_repeat_time )
		{
			eprint( FATAL, "%s:%ld: DG2020: External trigger mode and setting "
					"a repeat time or frequency is incompatible.\n",
					Fname, Lc );
			THROW( EXCEPTION );
		}
	}

	dg2020.trig_in_mode = mode;
	dg2020.is_trig_in_mode = SET;

	return OK;
}


static bool set_trig_in_level( double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( dg2020.is_trig_in_level && dg2020.trig_in_level != voltage )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different level of %f V for the "
				"trigger has already been set.\n", Fname, Lc,
				dg2020.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
				"a trigger level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time as well as "
				"a trigger level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( voltage > MAX_TRIG_IN_LEVEL || voltage < MIN_TRIG_IN_LEVEL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Inalid level for trigger of %f V, "
				"valid range is %f V to %f V.\n", Fname, Lc, MIN_TRIG_IN_LEVEL,
				MAX_TRIG_IN_LEVEL );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_level = voltage;
	dg2020.is_trig_in_level = SET;

	return OK;
}


static bool set_trig_in_slope( int slope )
{
	assert( slope == POSITIVE || slope == NEGATIVE );

	if ( dg2020.is_trig_in_slope && dg2020.trig_in_slope != slope )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different trigger slope (%s) "
				"has already been set.\n", Fname, Lc,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
				"a trigger slope is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time as well as "
				"a trigger slope is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_slope = slope;
	dg2020.is_trig_in_slope = SET;

	return OK;
}


static bool set_repeat_time( double time )
{
	if ( dg2020.is_repeat_time && dg2020.repeat_time != double2ticks( time ) )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different repeat time/frequency of "
				"%f s / %f Hz has already been set.\n", Fname, Lc,
				ticks2double( dg2020.repeat_time ),
				1.0 / ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time/frequency and "
				"trigger mode to internal is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_slope || dg2020.is_trig_in_level )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time/frequency and "
				"a trigger slope or level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	dg2020.repeat_time = double2ticks( time );
	dg2020.is_repeat_time = SET;

	return OK;
}










/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. a integer multiple of the time base                        */
/*-----------------------------------------------------------------*/

static Ticks double2ticks( double time )
{
	double ticks;

	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: DG2020: Can't set delay, no time base has "
				"been set yet.\n",Fname, Lc );
		THROW( EXCEPTION );
	}

	ticks = time / dg2020.timebase;

	if ( fabs( ticks - lround( ticks ) ) > 1.0e-2 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Specified time %f is not a integer "
				"multiple of the pulsers time base.\n", Fname, Lc, time );
		THROW( EXCEPTION );
	}

	return ( Ticks ) lround( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

static double ticks2double( Ticks ticks )
{
	assert( dg2020.is_timebase );
	return ( double ) ( dg2020.timebase * ticks );
}


/*----------------------------------------------------------------------*/
/* Checks if the difference of the levels specified for a pod connector */
/* are within the valid limits.                                         */
/*----------------------------------------------------------------------*/

static void check_pod_level_diff( double high, double low )
{
	if ( low > high )
	{
		eprint( FATAL, "%s:%ld: DG2020: Low voltage level is above high "
				"level, instead use keyword INVERT to invert the polarity.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING )
	{
		eprint( FATAL, "%s:%ld: DG2020: Diference between high and low "
				"voltage is too big, maximum is %f V.\n", Fname, Lc,
				MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING )
	{
		eprint( FATAL, "%s:%ld: DG2020: Diference between high and low "
				"voltage is too small, minimum is %f V.\n", Fname, Lc,
				MIN_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}
}
