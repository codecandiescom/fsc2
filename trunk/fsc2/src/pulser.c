/*
  $Id$
*/


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
	pulser_struct.set_delay_function = NULL;
	pulser_struct.set_function_high_level = NULL;
	pulser_struct.set_function_low_level = NULL;
	pulser_struct.set_timebase = NULL;
	pulser_struct.set_trigger_mode = NULL;
	pulser_struct.set_repeat_time = NULL;
	pulser_struct.set_trig_in_level = NULL;
	pulser_struct.set_trig_in_slope = NULL;
	pulser_struct.set_phase_reference = NULL;
	pulser_struct.set_pulse_function = NULL;
	pulser_struct.set_pulse_position = NULL;
	pulser_struct.set_pulse_length = NULL;
	pulser_struct.set_pulse_position_change = NULL;
	pulser_struct.set_pulse_length_change = NULL;
	pulser_struct.set_pulse_phase_cycle = NULL;
	pulser_struct.set_pulse_maxlen = NULL;
	pulser_struct.set_pulse_replacements = NULL;
	pulser_struct.get_pulse_function = NULL;
	pulser_struct.get_pulse_position = NULL;
	pulser_struct.get_pulse_length = NULL;
	pulser_struct.get_pulse_position_change = NULL;
	pulser_struct.get_pulse_length_change = NULL;
	pulser_struct.get_pulse_phase_cycle = NULL;
	pulser_struct.get_pulse_maxlen = NULL;
}


/*------------------------------------------------------------------------*/
/* Extracts the pulse number from a pulse name, i.e. a string of the form */
/* /^P(ULSE)?_?[0-9]+$/i (in Perl speak)                                  */
/*------------------------------------------------------------------------*/

long p_num( char *txt )
{
	long num;


	while ( txt != NULL && ! isdigit( *txt ) )
		txt++;

	assert( txt != NULL );          /* Paranoia ? */

	num = strtol( txt, NULL, 10 );
	if ( errno == ERANGE )
	{
		eprint( FATAL, "%s:%ld: Pulse number (%s) out of range.\n",
				Fname, Lc, txt );
		THROW( EXCEPTION );
	}

	return num;
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
				"pulser-specific functions.\n", Fname, Lc );
		THROW( EXCEPTION );
	}
}


/*-----------------------------------------------------------------------*/
/* This function is called to determine if a certain pulser function     */
/* needed by the experiment is supplied by the pulser driver. The first  */
/* argument is the functions address, the second a snippet of text to be */
/* inserted in the error message (for convenience it is also tested if   */
/* there's a driver at all so tha not each function has to test for this */
/* even when the name of the pulser isn't explicitely needed).           */
/*-----------------------------------------------------------------------*/

void is_pulser_func( void *func, const char *text )
{
	is_pulser_driver( );

	if ( func == NULL )
	{
		eprint( FATAL, "%s:%ld: Function for %s is missing in driver "
				"for pulser %s.\n", Fname, Lc, text, pulser_struct.name );
		THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------------------*/
/* Function tests if the time (in seconds) it gets passed is a reasonable    */
/* integer multiple of 1 ns and tries to reduce rounding errors. If the time */
/* more than 10 ps off from a ns an error message is output, using the text  */
/* snippet passed to the function as the second argument.                    */
/*---------------------------------------------------------------------------*/

double is_mult_ns( double val, const char * text )
{
	val *= 1.e9;
	if ( fabs( val - lround( val ) ) > 1.e-2 )
	{
		eprint( FATAL, "%s:%ld: %s has to be an integer multiple of 1 ns\n",
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
		eprint( WARN, "%s:%ld: Float variable used as channel number.\n",
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

	is_pulser_func( pulser_struct.set_delay_function, "setting a delay" );
	( *pulser_struct.set_delay_function )( func, delay );
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
		eprint( FATAL, "%s:%ld: Invalid repeat time: %g s\n",
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
		eprint( FATAL, "%s:%ld: Invalid repeat frequency: %g s\n",
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
/* Function for setting the trigger in level voltage */
/*---------------------------------------------------*/

void p_phase_ref( long func, int ref )
{
	is_pulser_func( pulser_struct.set_phase_reference,
					"setting a function for phase cycling" );

	if ( func != PULSER_CHANNEL_PHASE_1 && func != PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: A reference function can only be set for the "
				"PHASE functions.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( ref == PULSER_CHANNEL_PHASE_1 || ref == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: A PHASE function can't be phase cycled.\n",
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
	long np;                                 /* number of replacement pulses */
	long *pl;                                /* list  of replacement pulses */
	Var *first, *cur;
	long i;


	/* Now the correct driver function is called. All but the last switch just
	   check that the variable has the correct type and that the driver
	   function exists. Only the switch for setting replacement pulses has to
	   be different: here we first need to count the number of variables on
	   the stack, create an array of the pulse numbers and than call the
	   appropriate function. */

	switch ( type )
	{
		case P_FUNC :
			if ( v->type != INT_VAR ||
				 v->val.lval < PULSER_CHANNEL_FUNC_MIN ||
				 v->val.lval > PULSER_CHANNEL_FUNC_MAX )
			{
				eprint( FATAL, "%s:%ld: Invalid function.\n", Fname, Lc );
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
			( *pulser_struct.set_pulse_phase_cycle )( pnum, 
													  ( int ) v->val.lval );
			vars_pop( v );
			break;

		case P_MAXLEN :
			vars_check( v, INT_VAR | FLOAT_VAR );
			is_pulser_func( pulser_struct.set_pulse_maxlen,
							"setting a maximum length for a pulse" );
			( *pulser_struct.set_pulse_maxlen )( pnum, VALUE( v ) );
			vars_pop( v );
			break;

		case P_REPL :
			is_pulser_func( pulser_struct.set_pulse_replacements,
							"setting replacement pulses" );

			/* check and count the variables on the stack (there's always 
			   going to be at least one) */

			vars_check( v, INT_VAR );
			for ( first = v, np = 1; v->next != NULL; np++, v = v->next )
				vars_check( v, INT_VAR );

			/* create the array of replacment pulse numbers, while doing so
			   get rid of stack variables that aren't needed any longer */

			pl = T_malloc( np * sizeof( long ) );
			for ( i = 0, v = first; i < np; i++ )
			{
				pl[ i ] = v->val.lval;
				cur = v;
				v = v->next;
				vars_pop( cur );
			}

			( *pulser_struct.set_pulse_replacements )( pnum, np, pl );

			T_free( pl );
			break;
	}
}


/*-----------------------------------------------------------------------*/
/* Function for asking the pulser driver about the properties of a pulse */
/*-----------------------------------------------------------------------*/

Var *p_get( char *txt, int type )
{
	long pnum = p_num( txt );               /* determine pulse number */
	int function;
	double time;
	int cycle;
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

		case P_MAXLEN :
			is_pulser_func( pulser_struct.get_pulse_maxlen,
							"returning a pulses maximum length" );
			( *pulser_struct.get_pulse_maxlen )( pnum, &time );
			v = vars_push( FLOAT_VAR, time );
			break;

		default :
			assert ( 1 == 0 );
	}

	return v;
}
