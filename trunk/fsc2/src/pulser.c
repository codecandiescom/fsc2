/*
  $Id$

  This file is a kind of interface between the parser and the real pulser
  functions to make it easier to use different types of pulsers. Instead of
  the parser calling the pulser functions directly (which might even not
  exist) one of the following functions is called that can not only find out
  if the corresponding pulser function exists, but can also do some
  preliminary error checking.  */


#include "fsc2.h"


/*---------------------------------------------------------------*/
/* Function clears the complete pulser structure, that has to be */
/* set up by the init_hook( ) function of the pulser driver      */
/*---------------------------------------------------------------*/

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
	pulser_struct.set_function_delay = NULL;
	pulser_struct.set_function_high_level = NULL;
	pulser_struct.set_function_low_level = NULL;
	pulser_struct.set_timebase = NULL;
	pulser_struct.set_trigger_mode = NULL;
	pulser_struct.set_repeat_time = NULL;
	pulser_struct.set_trig_in_level = NULL;
	pulser_struct.set_trig_in_slope = NULL;
	pulser_struct.set_trig_in_impedance = NULL;
	pulser_struct.set_phase_reference = NULL;
	pulser_struct.set_grace_period = NULL;
	pulser_struct.set_pulse_function = NULL;
	pulser_struct.set_pulse_position = NULL;
	pulser_struct.set_pulse_length = NULL;
	pulser_struct.set_pulse_position_change = NULL;
	pulser_struct.set_pulse_length_change = NULL;
	pulser_struct.set_pulse_phase_cycle = NULL;
	pulser_struct.get_pulse_function = NULL;
	pulser_struct.get_pulse_position = NULL;
	pulser_struct.get_pulse_length = NULL;
	pulser_struct.get_pulse_position_change = NULL;
	pulser_struct.get_pulse_length_change = NULL;
	pulser_struct.get_pulse_phase_cycle = NULL;

	pulser_struct.phase_setup_prep = NULL;
	pulser_struct.phase_setup = NULL;
	pulser_struct.set_phase_switch_delay = NULL;

}


/*------------------------------------------------------------------------*/
/* Extracts the pulse number from a pulse name, i.e. a string of the form */
/* /^P(ULSE)?_?[0-9]+$/i (in Perl speak)                                  */
/*------------------------------------------------------------------------*/

long p_num( char *txt )
{
	while ( txt != NULL && ! isdigit( *txt ) )
		txt++;

	assert( txt != NULL );          /* Paranoia ? */

	return T_atol( txt );
}


/*-----------------------------------------------------------------------*/
/* This function is called at the start of each pulser specific function */
/* to avoid using a pulser function if there's no pulser driver          */
/*-----------------------------------------------------------------------*/

void is_pulser_driver( void )
{
	if ( pulser_struct.name == NULL )
	{
		eprint( FATAL, "%s:%ld: No pulser driver has been loaded - can't use "
				"pulser-specific functions.", Fname, Lc );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------------------------*/
/* This function is called to determine if a certain pulser function    */
/* needed by the experiment is supplied by the pulser driver. The first */
/* argument is the functions address, the second a piece of text to be  */
/* inserted in the error message (for convenience it is also tested if  */
/* there's a driver at all so that not each function has to test for    */
/* this even when the name of the pulser isn't explicitely needed).     */
/*----------------------------------------------------------------------*/

void is_pulser_func( void *func, const char *text )
{
	is_pulser_driver( );

	if ( func == NULL )
	{
		eprint( FATAL, "%s:%ld: Function for %s is missing in driver "
				"for pulser %s.", Fname, Lc, text, pulser_struct.name );
		THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------------------*/
/* Function tests if the time (in seconds) it gets passed is a reasonable    */
/* integer multiple of 1 ns and tries to reduce rounding errors. If the time */
/* is more than 10 ps off from a multiple of a nanosecond an error message   */
/* is output, using the piece of text passed to the function as the second   */
/* argument.                                                                 */
/*---------------------------------------------------------------------------*/

double is_mult_ns( double val, const char * text )
{
	val *= 1.e9;
	if ( fabs( val - lround( val ) ) > 1.e-2 )
	{
		eprint( FATAL, "%s:%ld: %s has to be an integer multiple of 1 ns.",
				Fname, Lc, text );
		THROW( EXCEPTION );
	}

	return lround( val ) * 1.e-9;
}


/*-------------------------------------------------------------------------*/ 
/* This function is called for the assignment of a function for a pod - it */
/* can't be called when there are no pods, in this case the assignment has */
/* to be done via the p_assign_channel() function                          */
/*-------------------------------------------------------------------------*/ 

void p_assign_pod( long func, Var *v )
{
	long pod;


	is_pulser_driver( );

	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* Test if there's a function for assigning channels to pods - otherwise
	   the pulser doesn't have pods and we have to quit */

	if ( pulser_struct.assign_channel_to_function == NULL )
	{
		eprint( FATAL, "%s:%ld: Sorry, pulser %s has no pods.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
		pod = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as pod number.",
				Fname, Lc );
		pod = ( long ) v->val.dval;
	}

	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.assign_function,
					"assigning function to pod" );
	( *pulser_struct.assign_function )( func, pod );
}


/*------------------------------------------------------------------------*/
/* This function has a double purpose: For pulsers that have pods and     */
/* channels, the pod to channel assignment is done via this function. For */
/* pulsers, that have just channels, the assignment of a function to a    */
/* channel is done here (instead of p_assign_pod() as for the other type  */
/* of pulsers)                                                            */
/*------------------------------------------------------------------------*/

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
		eprint( WARN, "%s:%ld: Float variable used as channel number.",
				Fname, Lc );
		channel = ( long ) v->val.dval;
	}

	vars_pop( v );

	/* finally call the function (if it exists...) */

	if ( pulser_struct.assign_channel_to_function == NULL )
	{
		is_pulser_func( pulser_struct.assign_function,
						"assigning function to pod" );
		( *pulser_struct.assign_function )( func, channel );
	}
	else
	{
		is_pulser_func( pulser_struct.assign_channel_to_function,
						"assigning function to channel" );
		( *pulser_struct.assign_channel_to_function )( func, channel );
	}
}


/*-------------------------------------------------------------------*/
/* Function for setting a delay (in seconds) for an output connector */
/*-------------------------------------------------------------------*/

void p_set_delay( long func, Var *v )
{
	double delay;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	delay = ( ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval );
	vars_pop( v );

	delay = is_mult_ns( delay, "Delay" );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_function_delay, "setting a delay" );
	( *pulser_struct.set_function_delay )( func, delay );
}


/*-------------------------------------------------------------*/
/* Function for inverting the polarity for an output connector */
/*-------------------------------------------------------------*/

void p_inv( long func )
{
	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	is_pulser_func( pulser_struct.invert_function, "inverting a channel" );
	( *pulser_struct.invert_function )( func );
}


/*-----------------------------------------------------*/
/* Function for setting the high voltage trigger level */
/* for one of the output connector                     */
/*-----------------------------------------------------*/

void p_set_v_high( long func, Var *v )
{
	double voltage;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	voltage = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;
	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_function_high_level,
					"setting high voltage level" );
	( *pulser_struct.set_function_high_level )( func, voltage );
}


/*----------------------------------------------------*/
/* Function for setting the low voltage trigger level */
/* for one of the output connectors                   */
/*----------------------------------------------------*/

void p_set_v_low( long func, Var *v )
{
	double voltage;


	assert( func >= PULSER_CHANNEL_FUNC_MIN &&
			func <= PULSER_CHANNEL_FUNC_MAX );

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	voltage = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;
	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_function_low_level,
					"setting low voltage level" );
	( *pulser_struct.set_function_low_level )( func, voltage );
}


/*-------------------------------------------------*/
/* Function for setting the timebase of the pulser */
/*-------------------------------------------------*/

void p_set_timebase( Var *v )
{
	double timebase;

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	timebase = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;
	vars_pop( v );

	timebase = is_mult_ns( timebase, "Time base"  );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_timebase,"setting the timebase" );
	( *pulser_struct.set_timebase )( timebase );
}


/*-----------------------------------------------------------------*/
/* Function for setting the trigger in mode (EXTERNAL or INTERNAL) */
/*-----------------------------------------------------------------*/

void p_set_trigger_mode( Var *v )
{
	int mode;

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    mode = ( int ) v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as trigger mode.",
				Fname, Lc );
		mode = ( int ) v->val.dval;
	}

	vars_pop( v );

	if ( mode != INTERNAL && mode != EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: Invalid trigger mode specification.",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_trigger_mode,
					"setting the trigger mode" );
	( *pulser_struct.set_trigger_mode )( mode );
}

/*------------------------------------------------------------------*/
/* Function for setting the trigger in slope (POSITIVE or NEGATIVE) */
/*------------------------------------------------------------------*/

void p_set_trigger_slope( Var *v )
{
	int slope;

	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == INT_VAR )
	    slope = ( int ) v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: Float variable used as trigger slope.",
				Fname, Lc );
		slope = ( int ) v->val.dval;
	}

	vars_pop( v );

	if ( slope != POSITIVE && slope != NEGATIVE )
	{
		eprint( FATAL, "%s:%ld: Invalid trigger slope specification.", 
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_trig_in_slope,
					"setting the trigger slope" );
	( *pulser_struct.set_trig_in_slope )( slope );
}


/*---------------------------------------------------*/
/* Function for setting the trigger in level voltage */
/*---------------------------------------------------*/

void p_set_trigger_level( Var *v )
{
	double level;


	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	level = ( v->type == INT_VAR ) ? ( double ) v->val.lval : v->val.dval;
	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_trig_in_level,
					"setting the trigger level" );
	( *pulser_struct.set_trig_in_level )( level );
}


/*----------------------------------------*/
/* Function sets the trigger in impedance */
/*----------------------------------------*/

void p_set_trigger_impedance( Var *v )
{
	int state;


	/* check the variable and get its value */

	vars_check( v, INT_VAR );
	state = ( int ) v->val.lval;
	vars_pop( v );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_trig_in_impedance,
					"setting the trigger impedance" );
	( *pulser_struct.set_trig_in_impedance )( state );
}


/*-------------------------------------------------------------------*/
/* Function for setting the (minimum) repeat time for the experiment */
/*-------------------------------------------------------------------*/

void p_set_rep_time( Var *v )
{
	double time;


	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	time = ( v->type == INT_VAR ) ? ( double ) v->val.lval :  v->val.dval;
	vars_pop( v );

	if ( time < 9.9e-10 )
	{
		eprint( FATAL, "%s:%ld: Invalid repeat time: %g s.",
				Fname, Lc, time );
		THROW( EXCEPTION );
	}

	time = is_mult_ns( time, "Repeat time" );

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_repeat_time, "setting a repeat time" );
	( *pulser_struct.set_repeat_time )( time );
}


/*------------------------------------------------------------------------*/
/* Function for setting the (maximum) repeat frequency for the experiment */
/*------------------------------------------------------------------------*/

void p_set_rep_freq( Var *v )
{
	double freq, time;


	/* check the variable and get its value */

	vars_check( v, INT_VAR | FLOAT_VAR );
	freq = ( v->type == INT_VAR ) ? ( double ) v->val.lval :  v->val.dval;
	vars_pop( v );

	if ( freq > 1.01e9 || freq <= 0.0 )
	{
		eprint( FATAL, "%s:%ld: Invalid repeat frequency: %g s.",
				Fname, Lc, freq );
		THROW( EXCEPTION );
	}

	/* make sure we get a repeat time that's a multiple of 1 ns */

	time = 1.0 / freq;
	time = lround( time * 1.e9 ) * 1.e-9;

	/* finally call the function (if it exists...) */

	is_pulser_func( pulser_struct.set_repeat_time,
					"setting a repeat frequency" );
	( *pulser_struct.set_repeat_time )( time );
}


/*---------------------------------------------------*/
/*---------------------------------------------------*/

void p_phase_ref( long func, int ref )
{
	is_pulser_func( pulser_struct.set_phase_reference,
					"setting a function for phase cycling" );

	if ( func != PULSER_CHANNEL_PHASE_1 && func != PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: A reference function can only be set for the "
				"PHASE functions.", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( ref == PULSER_CHANNEL_PHASE_1 || ref == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: A PHASE function can't be phase cycled.",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	( *pulser_struct.set_phase_reference )( ( int ) func, ref );
}


/*-----------------------------------*/
/* Function for creating a new pulse */
/*-----------------------------------*/

long p_new( long pnum )
{
	is_pulser_func( pulser_struct.new_pulse, "creating a new pulse" );
	( *pulser_struct.new_pulse )( pnum );
	return pnum;
}


/*-------------------------------------------------------*/
/* Function for setting one of the properties of a pulse */
/*-------------------------------------------------------*/

void p_set( long pnum, int type, Var *v )
{
	/* Now the correct driver function is called. All switches just check that
	   the variable has the correct type and that the driver function
	   exists. */

	switch ( type )
	{
		case P_FUNC :
			if ( v->type != INT_VAR ||
				 v->val.lval < PULSER_CHANNEL_FUNC_MIN ||
				 v->val.lval > PULSER_CHANNEL_FUNC_MAX )
			{
				eprint( FATAL, "%s:%ld: Invalid function.", Fname, Lc );
				THROW( EXCEPTION );
			}
			is_pulser_func( pulser_struct.set_pulse_function,
							"setting a pulse function" );
			( *pulser_struct.set_pulse_function )( pnum, ( int ) v->val.lval );
			vars_pop( v );
			break;

		case P_POS :
			vars_check( v, INT_VAR | FLOAT_VAR );
			is_pulser_func( pulser_struct.set_pulse_position,
							"setting a pulse position" );
			( *pulser_struct.set_pulse_position )( pnum, VALUE( v ) );
			vars_pop( v );
			break;

		case P_LEN :
			vars_check( v, INT_VAR | FLOAT_VAR );
			is_pulser_func( pulser_struct.set_pulse_length,
							"setting a pulse length" );
			( *pulser_struct.set_pulse_length )( pnum, VALUE( v ) );
			vars_pop( v );
			break;

		case P_DPOS :
			vars_check( v, INT_VAR | FLOAT_VAR );
			is_pulser_func( pulser_struct.set_pulse_position_change,
							"setting a pulse position change" );
			( *pulser_struct.set_pulse_position_change )( pnum, VALUE( v ) );
			vars_pop( v );
			break;

		case P_DLEN :
			vars_check( v, INT_VAR | FLOAT_VAR );
			is_pulser_func( pulser_struct.set_pulse_length_change,
							"setting a pulse length change" );
			( *pulser_struct.set_pulse_length_change )( pnum, VALUE( v ) );
			vars_pop( v );
			break;

		case P_PHASE :
			vars_check( v, INT_VAR );
			is_pulser_func( pulser_struct.set_pulse_phase_cycle,
							"setting a pulse phase cycle" );
			( *pulser_struct.set_pulse_phase_cycle )( pnum, v->val.lval );
			vars_pop( v );
			break;

		default:
			assert( 1 == 0 );
	}
}


/*-----------------------------------------------------------------------*/
/* Function for asking the pulser driver about the properties of a pulse */
/*-----------------------------------------------------------------------*/

Var *p_get( char *txt, int type )
{
	return p_get_by_num( p_num( txt ), type );
}


Var *p_get_by_num( long pnum, int type )
{
	int function;
	double time;
	long cycle;
	Var *v;


	switch ( type )
	{
		case P_FUNC :
			is_pulser_func( pulser_struct.get_pulse_function,
							"returning a pulses function" );
			( *pulser_struct.get_pulse_function )( pnum, &function );
			v = vars_push( INT_VAR, ( long ) function );
			break;

		case P_POS :
			is_pulser_func( pulser_struct.get_pulse_position,
							"returning a pulses position" );
			( *pulser_struct.get_pulse_position )( pnum, &time );
			v = vars_push( FLOAT_VAR, time );
			break;

		case P_LEN :
			is_pulser_func( pulser_struct.get_pulse_length,
							"returning a pulses length" );
			( *pulser_struct.get_pulse_length )( pnum, &time );
			v = vars_push( FLOAT_VAR, time );
			break;

		case P_DPOS :
			is_pulser_func( pulser_struct.get_pulse_position_change,
							"returning a pulses position change" );
			( *pulser_struct.get_pulse_position_change )( pnum, &time );
			v = vars_push( FLOAT_VAR, time );
			break;

		case P_DLEN :
			is_pulser_func( pulser_struct.get_pulse_length_change,
							"returning a pulses length change" );
			( *pulser_struct.get_pulse_length_change )( pnum, &time );
			v = vars_push( FLOAT_VAR, time );
			break;

		case P_PHASE :
			is_pulser_func( pulser_struct.get_pulse_phase_cycle,
							"returning a pulses phase cycle" );
			( *pulser_struct.get_pulse_phase_cycle )( pnum, &cycle );
			v = vars_push( INT_VAR, cycle );
			break;

		default :
			assert ( 1 == 0 );
	}

	return v;
}


/*----------------------------------------------------------------------------
  'function' is the phase function the data are to be used for (i.e. 0 means
    PHASE_1, 1 means PHASE_2, 2 means both)
  'type' means the type of phase, see global.h (PHASE_PLUS/MINUX_X/Y)
  'pod' means if the value is for the first or the second pod channel
    (0: first pod channel, 1: second pod channel, -1: pick the one not set yet)
  'val' means high or low to be set on the pod channel to set the requested
    phase(0: low, !0: high)
  'protocol' characterizes the way the phase setup is declared in the input.
-----------------------------------------------------------------------------*/

void p_phs_setup( int func, int type, int pod, long val, long protocol )
{
	/* A few sanity checks before we call the pulsers handler function */

	assert( type >= PHASE_TYPES_MIN && type <= PHASE_TYPES_MAX );
	assert( func >= 0 && func <= 2 );        /* phase function correct ? */

	/* Let's check if the pulser supports the function needed */

	is_pulser_func( pulser_struct.phase_setup_prep,
					"setting up phase channels" );
	is_pulser_func( pulser_struct.phase_setup, "setting up phase channels" );

	( *pulser_struct.phase_setup_prep )( func, type, pod, val, protocol );	
}


/*
  We reached the end of a phase setup command...
*/

void p_phs_end( int func )
{
	assert( func >= 0 && func <= 2 );        /* phase function correct ? */

	( *pulser_struct.phase_setup )( func );
}


/*
  Function for setting the phase switch delay.
  'func' is the phase function the data are to be used for (i.e. 0 means
  PHASE_1, 1 means PHASE_2, 2 means both)
*/

void p_set_psd( int func, Var *v )
{
	assert( func >= 0 && func <= 2 );

	vars_check( v, INT_VAR | FLOAT_VAR );
	is_pulser_func( pulser_struct.set_phase_switch_delay,
					"setting a phase switch delay" );

	if ( func == 0 || func == 2 )
		( *pulser_struct.set_phase_switch_delay )( PULSER_CHANNEL_PHASE_1,
												   VALUE( v ) );
	if ( func == 1 || func == 2 )
		( *pulser_struct.set_phase_switch_delay )( PULSER_CHANNEL_PHASE_2,
												   VALUE( v ) );

	vars_pop( v );
}


/*
  Function for setting the grace period following a pulse.
*/

void p_set_gp( Var *v )
{
	vars_check( v, INT_VAR | FLOAT_VAR );
	is_pulser_func( pulser_struct.set_grace_period,
					"setting a grace period" );

	( *pulser_struct.set_grace_period )( VALUE( v ) );

	vars_pop( v );
}
