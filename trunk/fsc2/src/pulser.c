/*
  $Id$
*/


#include "fsc2.h"


static void is_driver( void );
static void is_func( void *func, const char *text );
static double is_mult_ns( double val, const char * text );



/* Function clears the complete pulser structure, that has to be set up
   by the init_hook( ) function of the pulser driver */

void pulser_struct_init( void )
{
	if ( pulser_struct.name != NULL )
	{
		T_free( pulser_struct.name );
		pulser_struct.name = 0;
	}

	pulser_struct.assign_function = NULL;
	pulser_struct.assign_channel_to_function = NULL;
	pulser_struct.invert_function = NULL;
	pulser_struct.set_delay_function = NULL;
	pulser_struct.set_function_high_level = NULL;
	pulser_struct.set_function_low_level = NULL;
	pulser_struct.set_timebase = NULL;
	pulser_struct.set_trigger_mode = NULL;
	pulser_struct.set_repeat_time = NULL;
	pulser_struct.set_trig_in_level = NULL;
	pulser_struct.set_trig_in_slope = NULL;
	pulser_struct.set_pulse_function = NULL;
	pulser_struct.set_pulse_position = NULL;
	pulser_struct.set_pulse_length = NULL;
	pulser_struct.set_pulse_position_change = NULL;
	pulser_struct.set_pulse_length_change = NULL;
	pulser_struct.set_pulse_maxlen = NULL;
	pulser_struct.set_pulse_replacements = NULL;
}


/* 
   This function is going to be called at the start of each pulser specific
   function to avoid using a pulser function if there's no pulser driver
*/

static void is_driver( void )
{
	if ( pulser_struct.name == NULL )
	{
		eprint( FATAL, "%s:%ld: No pulser driver has been loaded - can't use "
				"pulser-specific functions.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
}


/*
   This function is called to determine if a certain pulser function needed by
   the experiment is suppled by the pulser driver. The first argument is the
   functions address, the second a snippet of text to be inserted in the error
   message (for convenience it is also tested if there's a driver at all so
   tha not each function has to test for this even when the name of the pulser
   isn't explicitely needed).
*/

static void is_func( void *func, const char *text )
{
	is_driver( );

	if ( func == NULL )
	{
		eprint( FATAL, "%s:%ld: Function for %s is missing in driver "
				"for pulser %s.\n", Fname, Lc, text, pulser_struct.name );
		THROW( EXCEPTION );
	}
}

static double is_mult_ns( double val, const char * text )
{
	val *= 1.e9;
	if ( fabs( val - lround( val ) ) > 1.e-2 )
	{
		eprint( FATAL, "%s:%ld: %s has to be a multiple of 1 ns\n",
				Fname, Lc, text );
		THROW( EXCEPTION );
	}

	return lround( val ) * 1.e-9;
}


/* 
   This function is called for the assignment of a function for a pod - it
   can't be called when there are no pods, in this case the assignment has to
   be done via the p_assign_channel() function
*/

void p_assign_pod( long func, Var *v )
{
	long pod;


	is_driver( );

	/* <PARANOIA=on> */

	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* Test if there's a function for assigning channels to pods - otherwise
	   the pulser doesn't have pods and we have to quit */

	if ( pulser_struct.assign_channel_to_function == NULL )
	{
		eprint( FATAL, "%s:%ld: Sorry, pulser %s has no pods.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		pod = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as pod number.\n",
				Fname, Lc );
		pod = ( long ) v->val.dval;
	}

	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.assign_function, "assigning function to pod" );
	( *pulser_struct.assign_function )( func, pod );
}


/*
   This function has a double purpose: For pulsers that have pods and
   channels, the pod to channel assignment is done via this function. For
   pulsers, that have just channels, the assignment of a function to a channel
   is done here (instead of p_assign_pod() as for the other type of pulsers)
*/

void p_assign_channel( long func, Var *v )
{
	long channel;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		channel = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as channel number.\n",
				Fname, Lc );
		channel = ( long ) v->val.dval;
	}

	vars_pop( v );

	/* finally call the function (if it exists...) */

	if ( pulser_struct.assign_channel_to_function == NULL )
	{
		is_func( pulser_struct.assign_function, "assigning function to pod" );
		( *pulser_struct.assign_function )( func, channel );
	}
	else
	{
		is_func( pulser_struct.assign_channel_to_function,
				 "assigning function to channel" );
		( *pulser_struct.assign_channel_to_function )( func, channel );
	}
}


/*
   Function for setting a delay (in seconds) for a certain connector
*/

void p_set_delay( long func, Var *v )
{
	double delay;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		delay = ( double ) v->val.lval;
	else
		delay = v->val.dval;

	vars_pop( v );

	/* check that the delay value is reasonable */

	if ( delay < 0 )
	{
		eprint( FATAL, "%s:%ld: Invalid delay: %f\n", Fname, Lc, delay );
		THROW( EXCEPTION );
	}

	delay = is_mult_ns( delay, "Delay" );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_delay_function, "setting a delay" );
	( *pulser_struct.set_delay_function )( func, delay );
}


/*
   Function for inverting the polarity for a certain connector
*/

void p_inv( long func )
{
	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	is_func( pulser_struct.invert_function, "inverting a channel" );
	( *pulser_struct.invert_function )( func );
}


/*
   Function for setting the high voltage trigger level for a certain connector
*/

void p_set_v_high( long func, Var *v )
{
	double voltage;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		voltage = ( double ) v->val.lval;
	else
		voltage = v->val.dval;

	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_function_high_level,
			 "setting high voltage level" );
	( *pulser_struct.set_function_high_level )( func, voltage );
}


/*
   Function for setting the high voltage trigger level for a certain connector
*/

void p_set_v_low( long func, Var *v )
{
	double voltage;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		voltage = ( double ) v->val.lval;
	else
		voltage = v->val.dval;

	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_function_low_level,
			 "setting low voltage level" );
	( *pulser_struct.set_function_low_level )( func, voltage );
}


/*
   Function for setting the timebase of the pulser
*/

void p_set_timebase( Var *v )
{
	double timebase;

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    timebase = ( double ) v->val.lval;
	else
		timebase = v->val.dval;

	vars_pop( v );

	/* check that the timebase has a reasonable value */

	if ( timebase < 0.99e-9 )
	{
		eprint( FATAL, "%s:%ld: Invalid timebase: %f\n", Fname, Lc, timebase );
		THROW( EXCEPTION );
	}

	timebase = is_mult_ns( timebase, "Time base"  );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_timebase,"setting the timebase" );
	( *pulser_struct.set_timebase )( timebase );
}


/*
   Function for setting the trigger in mode (EXTERNAL or INTERNAL)
*/

void p_set_trigger_mode( Var *v)
{
	int mode;

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    mode = ( int ) v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as trigger mode.\n",
				Fname, Lc );
		mode = ( int ) v->val.dval;
	}

	vars_pop( v );

	if ( mode != INTERNAL && mode != EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: Invalid trigger mode specification.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_trigger_mode, "setting the trigger mode" );
	( *pulser_struct.set_trigger_mode )( mode );
}

/*
   Function for setting the trigger in slope (POSITIVE or NEGATIVE)
*/

void p_set_trigger_slope( Var *v )
{
	int slope;

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    slope = ( int ) v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as trigger slope.\n",
				Fname, Lc );
		slope = ( int ) v->val.dval;
	}

	vars_pop( v );

	if ( slope != POSITIVE && slope != NEGATIVE )
	{
		eprint( FATAL, "%s:%ld: Invalid trigger slope specification.\n", 
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_trig_in_slope, "setting the trigger slope" );
	( *pulser_struct.set_trig_in_slope )( slope );
}


/*
   Function for setting the trigger in level voltage
*/

void p_set_trigger_level( Var *v )
{
	double level;


	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    level = ( double ) v->val.lval;
	else
		level = v->val.dval;

	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_trig_in_level, "setting the trigger level" );
	( *pulser_struct.set_trig_in_level )( level );
}


/*
   Function for setting the (minimum) repeat time for the experiment
*/

void p_set_rep_time( Var *v )
{
	double time;


	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    time = ( double ) v->val.lval;
	else
		time = v->val.dval;

	vars_pop( v );

	if ( time < 0.99e-9 )
	{
		eprint( FATAL, "%s:%ld: Invalid repeat time: %f s\n",
				Fname, Lc, time );
		THROW( EXCEPTION );
	}

	time = is_mult_ns( time, "Repeat time" );

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_repeat_time, "setting a repeat time" );
	( *pulser_struct.set_repeat_time )( time );
}


/*
   Function for setting the (maximum) repeat frequency for the experiment
*/

void p_set_rep_freq( Var *v )
{
	double freq, time;


	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    freq = ( double ) v->val.lval;
	else
		freq = v->val.dval;

	vars_pop( v );

	if ( freq > 1.01e9 || freq <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Invalid repeat frequency: %f s\n",
				Fname, Lc, freq );
		THROW( EXCEPTION );
	}

	/* make sure we get a repeat time that's a multiple of 1 ns */

	time = 1.0 / freq;
	time = lround( time * 1.e9 ) * 1.e-9;

	/* finally call the function (if it exists...) */

	is_func( pulser_struct.set_repeat_time, "setting a repeat frequency" );
	( *pulser_struct.set_repeat_time )( time );
}


