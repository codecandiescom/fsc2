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

  This file is a kind of interface between the parser and the real pulser
  functions to make it easier to use different types of pulsers. Instead of
  the parser calling the pulser functions directly (which might even not
  exist) one of the following functions is called that can not only find out
  if the corresponding pulser function exists, but can also do some
  preliminary error checking.
*/


#include "fsc2.h"
#include "pulser.h"


Pulser_Struct *pulser_struct = NULL;
long Cur_Pulser = -1;

const char *Function_Names[ PULSER_CHANNEL_NUM_FUNC ] =
			{ "MW",
			  "PULSE_SHAPE",
              "PHASE_1",
              "PHASE_2",
              "TWT",
              "TWT_GATE",
              "DEFENSE",
              "DETECTION",
			  "DETECTION_GATE",
              "RF",
              "RF_GATE",
			  "OTHER_1",
              "OTHER_2",
              "OTHER_3",
              "OTHER_4" };


static void is_pulser_func( void *func, const char *text );
static void is_pulser_driver( void );


static P_List *plist;


/*--------------------------------------------------------------*/
/* Function clears the complete pulser structure that has to be */
/* set up by the init_hook( ) function of the pulser driver     */
/*--------------------------------------------------------------*/

void pulser_struct_init( void )
{
	long i;
	Device *cd = EDL.Device_List;


	if ( EDL.Num_Pulsers == 0 )
		return;

	pulser_struct = PULSER_STRUCT_P T_malloc( EDL.Num_Pulsers
											  * sizeof( Pulser_Struct ) );

	for ( i = 0; i < EDL.Num_Pulsers; i++ )
	{
		pulser_struct[ i ].name                       = NULL;

		while ( ! cd->generic_type ||
				strcasecmp( cd->generic_type, PULSER_GENERIC_TYPE ) )
			cd = cd->next;
		pulser_struct[ i ].device                     = cd;

		pulser_struct[ i ].needs_phase_pulses         = UNSET;
		pulser_struct[ i ].has_pods                   = UNSET;

		pulser_struct[ i ].assign_function            = NULL;
		pulser_struct[ i ].assign_channel_to_function = NULL;
		pulser_struct[ i ].invert_function            = NULL;
		pulser_struct[ i ].set_function_delay         = NULL;
		pulser_struct[ i ].set_function_high_level    = NULL;
		pulser_struct[ i ].set_function_low_level     = NULL;
		pulser_struct[ i ].set_timebase               = NULL;
		pulser_struct[ i ].set_timebase_level         = NULL;
		pulser_struct[ i ].set_trigger_mode           = NULL;
		pulser_struct[ i ].set_repeat_time            = NULL;
		pulser_struct[ i ].set_trig_in_level          = NULL;
		pulser_struct[ i ].set_trig_in_slope          = NULL;
		pulser_struct[ i ].set_trig_in_impedance      = NULL;
		pulser_struct[ i ].set_max_seq_len            = NULL;
		pulser_struct[ i ].set_phase_reference        = NULL;
		pulser_struct[ i ].new_pulse                  = NULL;
		pulser_struct[ i ].set_pulse_function         = NULL;
		pulser_struct[ i ].set_pulse_position         = NULL;
		pulser_struct[ i ].set_pulse_length           = NULL;
		pulser_struct[ i ].set_pulse_position_change  = NULL;
		pulser_struct[ i ].set_pulse_length_change    = NULL;
		pulser_struct[ i ].set_pulse_phase_cycle      = NULL;
		pulser_struct[ i ].get_pulse_function         = NULL;
		pulser_struct[ i ].get_pulse_position         = NULL;
		pulser_struct[ i ].get_pulse_length           = NULL;
		pulser_struct[ i ].get_pulse_position_change  = NULL;
		pulser_struct[ i ].get_pulse_length_change    = NULL;
		pulser_struct[ i ].get_pulse_phase_cycle      = NULL;
		pulser_struct[ i ].ch_to_num                  = NULL;

		pulser_struct[ i ].phase_setup_prep           = NULL;
		pulser_struct[ i ].phase_setup                = NULL;
		pulser_struct[ i ].set_phase_switch_delay     = NULL;
		pulser_struct[ i ].set_grace_period           = NULL;
		pulser_struct[ i ].keep_all_pulses            = NULL;
	}
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

void pulser_cleanup( void )
{
	P_List *cur_p;
	P_List *next_p;


	for ( cur_p = plist; cur_p != NULL; cur_p = next_p )
	{
		next_p = cur_p->next;
		T_free( cur_p );
	}

	plist = NULL;

	pulser_struct = PULSER_STRUCT_P T_free( pulser_struct );
}


/*------------------------------------------------------------*/
/* Extracts the pulse number from a pulse name, i.e. a string */
/* of the form '/^P(ULSE)?_?[0-9]+$/i' (in Perl speak)        */
/*------------------------------------------------------------*/

long p_num( char *txt )
{
	while ( txt != NULL && ! isdigit( *txt ) )
		txt++;

	fsc2_assert( txt != NULL );          /* Paranoia ? */

	return T_atol( txt );
}


/*-----------------------------------------------------------------------*/
/* This function is called at the start of each pulser specific function */
/* to avoid using a pulser function if there's no pulser driver          */
/*-----------------------------------------------------------------------*/

static void is_pulser_driver( void )
{
	if ( EDL.Num_Pulsers == 0 )
	{
		print( FATAL, "No pulser module has been loaded - can't use "
			   "pulser-specific functions.\n" );
		THROW( EXCEPTION );
	}

	fsc2_assert( Cur_Pulser >= 0 && Cur_Pulser < EDL.Num_Pulsers );

	if ( pulser_struct[ Cur_Pulser ].name == NULL )
	{
		print( FATAL, "No driver has been loaded for pulser #%ld - can't "
			   "use pulser-specific functions.\n", Cur_Pulser + 1 );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------------------------*/
/* This function is called to determine if a certain pulser function    */
/* needed by the experiment is supplied by the pulser driver. The first */
/* argument is the functions address, the second a piece of text to be  */
/* inserted in the error message - some function may become disabled    */
/* during the test run and the experiment, so a different error message */
/* is printed depending on the context. For convenience it is also      */
/* tested if there's a driver at all so that not each function has to   */
/* test for this even when the name of the pulser isn't explicitely     */
/* needed).                                                             */
/*----------------------------------------------------------------------*/

static void is_pulser_func( void *func, const char *text )
{
	is_pulser_driver( );

	if ( func == NULL )
	{
		if ( Internals.mode == PREPARATION )
			print( FATAL, "%s: Function for %s doesn't exist or can't be used "
				   "during the experiment.\n",
				   pulser_struct[ Cur_Pulser ].name, text );
		else
			print( FATAL, "%s: Function for %s not found in module.\n",
				   pulser_struct[ Cur_Pulser ].name, text );
		THROW( EXCEPTION );
	}
}


/*-------------------------------------------------------------------------*/
/* This function is called for the assignment of a function for a pod - it */
/* can't be called when there are no pods, in this case the assignment has */
/* to be done via the p_assign_channel() function                          */
/*-------------------------------------------------------------------------*/

void p_assign_pod( long func, Var *v )
{
	long pod;


	CLOBBER_PROTECT( pod );

	is_pulser_driver( );

	fsc2_assert( func >= 0 && func < PULSER_CHANNEL_NUM_FUNC );

	/* Test if there's a function for assigning channels to pods - otherwise
	   the pulser doesn't have pods and we have to quit */

	if ( pulser_struct[ Cur_Pulser ].assign_channel_to_function == NULL )
	{
		print( FATAL, "%s: Pulser has no pods.\n",
			   pulser_struct[ Cur_Pulser ].name );
		THROW( EXCEPTION );
	}

	/* Check the variable and get its value */

	pod = get_long( v, "pod number" );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].assign_function,
					"assigning function to pod" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].assign_function( func, pod );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*------------------------------------------------------------------------*/
/* This function has a double purpose: For pulsers that have pods and     */
/* channels, the pod to channel assignment is done via this function. For */
/* pulsers that have just channels the assignment of a function to a      */
/* channel is done here (instead of p_assign_pod() as for the other type  */
/* of pulsers)                                                            */
/*------------------------------------------------------------------------*/

void p_assign_channel( long func, Var *v )
{
	long channel;


	CLOBBER_PROTECT( channel );

	is_pulser_driver( );

	fsc2_assert( func >= 0 && func < PULSER_CHANNEL_NUM_FUNC );

	/* Check the variable and get its value */

	channel = get_long( v, "channel number" );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		if ( pulser_struct[ Cur_Pulser ].assign_channel_to_function == NULL )
		{
			is_pulser_func( pulser_struct[ Cur_Pulser ].assign_function,
							"assigning function to pod" );
			pulser_struct[ Cur_Pulser ].assign_function( func, channel );
		}
		else
		{
			is_pulser_func(
				pulser_struct[ Cur_Pulser ].assign_channel_to_function,
				"assigning function to channel" );
			pulser_struct[ Cur_Pulser ].assign_channel_to_function( func,
																	channel );
		}
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-------------------------------------------------------------------*/
/* Function for setting a delay (in seconds) for an output connector */
/*-------------------------------------------------------------------*/

void p_set_delay( long func, Var *v )
{
	double delay;


	CLOBBER_PROTECT( delay );

	is_pulser_driver( );

	fsc2_assert( func >= 0 && func < PULSER_CHANNEL_NUM_FUNC );

	/* Check the variable and get its value */

	delay = get_double( v, "delay" );
	vars_pop( v );

	delay = is_mult_ns( delay, "Delay" );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_function_delay,
					"setting a delay" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_function_delay( func, delay );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-------------------------------------------------------------*/
/* Function for inverting the polarity for an output connector */
/*-------------------------------------------------------------*/

void p_inv( long func )
{
	is_pulser_driver( );

	fsc2_assert( func >= 0 && func < PULSER_CHANNEL_NUM_FUNC );

	is_pulser_func( pulser_struct[ Cur_Pulser ].invert_function,
					"inverting a channel" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].invert_function( func );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-----------------------------------------------------*/
/* Function for setting the high voltage trigger level */
/* for one of the output connector                     */
/*-----------------------------------------------------*/

void p_set_v_high( long func, Var *v )
{
	double voltage;


	CLOBBER_PROTECT( voltage );

	is_pulser_driver( );

	fsc2_assert( func >= 0 && func < PULSER_CHANNEL_NUM_FUNC );

	/* Check the variable and get its value */

	voltage = get_double( v, "high voltage level" );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_function_high_level,
					"setting high voltage level" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_function_high_level( func, voltage );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*----------------------------------------------------*/
/* Function for setting the low voltage trigger level */
/* for one of the output connectors                   */
/*----------------------------------------------------*/

void p_set_v_low( long func, Var *v )
{
	double voltage;


	CLOBBER_PROTECT( voltage );

	is_pulser_driver( );

	fsc2_assert( func >= 0 && func < PULSER_CHANNEL_NUM_FUNC );

	/* Check the variable and get its value */

	voltage = get_double( v, "low voltage level" );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_function_low_level,
					"setting low voltage level" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_function_low_level( func, voltage );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-------------------------------------------------*/
/* Function for setting the timebase of the pulser */
/*-------------------------------------------------*/

void p_set_timebase( Var *v )
{
	double timebase;


	CLOBBER_PROTECT( timebase );

	is_pulser_driver( );

	/* Check the variable and get its value */

	timebase = get_double( v, "time base" );
	timebase = is_mult_ns( timebase, "Time base"  );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_timebase,
					"setting the time base" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_timebase( timebase );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-------------------------------------------------*/
/* Function for setting the timebase of the pulser */
/*-------------------------------------------------*/

void p_set_timebase_level( int level_type )
{
	is_pulser_driver( );

	/* Check the variable and get its value */

	fsc2_assert( level_type == TTL_LEVEL || level_type == ECL_LEVEL );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_timebase,
					"setting external clock inpugt level" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_timebase_level( level_type );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-----------------------------------------------------------------*/
/* Function for setting the trigger in mode (EXTERNAL or INTERNAL) */
/*-----------------------------------------------------------------*/

void p_set_trigger_mode( Var *v )
{
	int mode;


	CLOBBER_PROTECT( mode );

	is_pulser_driver( );

	/* Check the variable and get its value */

	mode = ( int ) get_long( v, "trigger mode" );
	vars_pop( v );

	if ( mode != INTERNAL && mode != EXTERNAL )
	{
		print( FATAL, "Invalid trigger mode.\n" );
		THROW( EXCEPTION );
	}

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_trigger_mode,
					"setting the trigger mode" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_trigger_mode( mode );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}

/*------------------------------------------------------------------*/
/* Function for setting the trigger in slope (POSITIVE or NEGATIVE) */
/*------------------------------------------------------------------*/

void p_set_trigger_slope( Var *v )
{
	int slope;


	CLOBBER_PROTECT( slope );

	is_pulser_driver( );

	/* Check the variable and get its value */

	slope = ( int ) get_long( v, "trigger slope" );
	vars_pop( v );

	if ( slope != POSITIVE && slope != NEGATIVE )
	{
		print( FATAL, "Invalid trigger slope specification.\n" );
		THROW( EXCEPTION );
	}

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_trig_in_slope,
					"setting the trigger slope" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_trig_in_slope( slope );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*---------------------------------------------------*/
/* Function for setting the trigger in level voltage */
/*---------------------------------------------------*/

void p_set_trigger_level( Var *v )
{
	double level;


	CLOBBER_PROTECT( level );

	is_pulser_driver( );

	/* Check the variable and get its value */

	level = get_double( v, "trigger level" );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_trig_in_level,
					"setting the trigger level" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_trig_in_level( level );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*----------------------------------------*/
/* Function sets the trigger in impedance */
/*----------------------------------------*/

void p_set_trigger_impedance( Var *v )
{
	int state;


	CLOBBER_PROTECT( state );

	is_pulser_driver( );

	/* Check the variable and get its value */

	state = ( int ) get_strict_long( v, "trigger impedance" );
	vars_pop( v );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_trig_in_impedance,
					"setting the trigger impedance" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_trig_in_impedance( state );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-------------------------------------------------------------------*/
/* Function for setting the (minimum) repeat time for the experiment */
/*-------------------------------------------------------------------*/

void p_set_rep_time( Var *v )
{
	double rep_time;


	CLOBBER_PROTECT( rep_time );

	is_pulser_driver( );

	/* Check the variable and get its value */

	rep_time = get_double( v, "repetition time" );
	vars_pop( v );

	if ( rep_time < 9.9e-10 )
	{
		print( FATAL, "Invalid repeat time: %g ns.\n", rep_time * 1.0e9 );
		THROW( EXCEPTION );
	}

	rep_time = is_mult_ns( rep_time, "Repeat time" );

	/* Finally call the function (if it exists...) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_repeat_time,
					"setting a repeat time" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_repeat_time( rep_time );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*------------------------------------------------------------------------*/
/* Function for setting the (maximum) repeat frequency for the experiment */
/*------------------------------------------------------------------------*/

void p_set_rep_freq( Var *v )
{
	double freq, rep_time;


	CLOBBER_PROTECT( freq );
	CLOBBER_PROTECT( rep_time );

	is_pulser_driver( );

	/* Check the variable and get its value */

	freq = get_double( v, "repetition frequency" );
	vars_pop( v );

	if ( freq > 1.01e9 || freq <= 0.0 )
	{
		print( FATAL, "Invalid repeat frequency: %g GHz.\n", freq * 1.0e0-9 );
		THROW( EXCEPTION );
	}

	if ( 1.0 / freq * 1.0e9 > LONG_MAX )
	{
		print( FATAL, "Repetition frequency of %f Hz is much too low.\n",
			   1.0 / freq );
		THROW( EXCEPTION );
	}

	/* Make sure we get a repeat time that's a multiple of 1 ns */

	rep_time = lrnd( 1.0 / freq * 1.0e9 ) * 1.0e-9;

	/* Finally call the function (if it exists) */

	is_pulser_func( pulser_struct[ Cur_Pulser ].set_repeat_time,
					"setting a repeat frequency" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_repeat_time( rep_time );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*------------------------------------------------------------------------*/
/* 'func' is either the phase functions number (pulser with phase switch) */
/* or the number of the PHASE_SETUP (pulser without phase switch). 'ref'  */
/* is the function the phase stuff is meant for.                          */
/*------------------------------------------------------------------------*/

void p_phase_ref( int func, int ref )
{
	is_pulser_driver( );
    is_pulser_func( pulser_struct[ Cur_Pulser ].set_phase_reference,
                    "setting a function for phase cycling" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		if ( pulser_struct[ Cur_Pulser ].needs_phase_pulses )
		{
#ifndef NDEBUG
			if ( func < 0 || func >= PULSER_CHANNEL_NUM_FUNC )
			{
				eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
			}
#endif

			if ( func != PULSER_CHANNEL_PHASE_1 &&
				 func != PULSER_CHANNEL_PHASE_2 )
			{
				print( FATAL, "Function '%s' can't be used as PHASE "
					   "function.\n", Function_Names[ func ] );
				THROW( EXCEPTION );
			}

			if ( ref == PULSER_CHANNEL_PHASE_1 ||
				 ref == PULSER_CHANNEL_PHASE_2 )
			{
				print( FATAL, "A PHASE function can't be phase cycled.\n" );
				THROW( EXCEPTION );
			}
		}
		else
		{
#ifndef NDEBUG
			if ( ( func != 0 && func != 1 )    ||
				 ref < 0 || ref >= PULSER_CHANNEL_NUM_FUNC )
			{
				eprint( FATAL, UNSET, "Internal error detected at s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
			}
#endif

			if ( ref == PULSER_CHANNEL_PHASE_1 ||
				 ref == PULSER_CHANNEL_PHASE_2 )
			{
				print( FATAL, "Phase functions can't be used with this "
					   "driver.\n" );
				THROW( EXCEPTION );
			}
		}

		pulser_struct[ Cur_Pulser ].set_phase_reference( func, ref );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-----------------------------------*/
/* Function for creating a new pulse */
/*-----------------------------------*/

long p_new( long pnum )
{
	P_List *cur_p;
	P_List *new_plist;


	is_pulser_driver( );
	is_pulser_func( pulser_struct[ Cur_Pulser ].new_pulse,
					"creating a new pulse" );

	/* First check that the pulse does not already exist, the include it
	   into the private list of pulses. */

	for ( cur_p = plist; cur_p != NULL; cur_p = cur_p->next )
		if ( cur_p->num == pnum )
		{
			print( FATAL, "A pulse #%ld has already been defined.\n",
				   pnum );
			THROW( EXCEPTION );
		}

	new_plist = P_LIST_P T_malloc( sizeof( P_List ) );
	new_plist->next    = plist;
	new_plist->num     = pnum;
	new_plist->dev_num = Cur_Pulser;
	plist = new_plist;

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].new_pulse( pnum );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );

	return pnum;
}


/*-------------------------------------------------------*/
/* Function for setting one of the properties of a pulse */
/*-------------------------------------------------------*/

void p_set( long pnum, int type, Var *v )
{
	long func, phase;
	double pos, len, dpos, dlen;
	long dev_num = -1;
	P_List *cur_p;
	long stored_Cur_Pulser;


	CLOBBER_PROTECT( dev_num );

	for ( cur_p = plist; cur_p != NULL; cur_p = cur_p->next )
		if ( cur_p->num == pnum )
		{
			dev_num = cur_p->dev_num;
			break;
		}

	if ( dev_num == -1 )
	{
		print( FATAL, "Referenced pulse #%ld has not been "
			   "defined.\n", pnum );
		THROW( EXCEPTION );
	}

	fsc2_assert( dev_num >= 0 && dev_num < EDL.Num_Pulsers );
	stored_Cur_Pulser = Cur_Pulser;
	Cur_Pulser = dev_num;

	/* Now the correct driver function is called. All switches just check that
	   the variable has the correct type and the driver function exists. */

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		switch ( type )
		{
			case P_FUNC :
				func = get_strict_long( v, "pulse channel function" );
				vars_pop( v );
				if ( func < 0 || func >= PULSER_CHANNEL_NUM_FUNC )
				{
					print( FATAL, "Invalid pulse channel function.\n" );
					THROW( EXCEPTION );
				}

				is_pulser_func( pulser_struct[ Cur_Pulser ].set_pulse_function,
								"setting a pulse function" );
				pulser_struct[ Cur_Pulser ].set_pulse_function( pnum, func );
				break;

			case P_POS :
				pos = get_double( v, "pulse position" );
				vars_pop( v );
				is_pulser_func( pulser_struct[ Cur_Pulser ].set_pulse_position,
								"setting a pulse position" );
				pulser_struct[ Cur_Pulser ].set_pulse_position( pnum, pos );
				break;

			case P_LEN :
				len = get_double( v, "pulse length" );
				vars_pop( v );
				is_pulser_func( pulser_struct[ Cur_Pulser ].set_pulse_length,
								"setting a pulse length" );
				pulser_struct[ Cur_Pulser ].set_pulse_length( pnum, len );
				break;

			case P_DPOS :
				dpos = get_double( v, "pulse position change" );
				vars_pop( v );
				is_pulser_func(
					pulser_struct[ Cur_Pulser ].set_pulse_position_change,
					"setting a pulse position change" );
				pulser_struct[ Cur_Pulser ].set_pulse_position_change( pnum,
																	   dpos );
				break;

			case P_DLEN :
				dlen = get_double( v, "pulse length change" );
				vars_pop( v );
				is_pulser_func(
					pulser_struct[ Cur_Pulser ].set_pulse_length_change,
					"setting a pulse length change" );
				pulser_struct[ Cur_Pulser ].set_pulse_length_change( pnum,
																	 dlen );
				break;

			case P_PHASE :
				phase = get_strict_long( v, "pulse phase cycle" );
				vars_pop( v );
				is_pulser_func(
					pulser_struct[ Cur_Pulser ].set_pulse_phase_cycle,
					"setting a pulse phase cycle" );
				pulser_struct[ Cur_Pulser ].set_pulse_phase_cycle( pnum,
																   phase );
				break;

			default:
				fsc2_assert( 1 == 0 );
				break;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		Cur_Pulser = stored_Cur_Pulser;
		RETHROW( );
	}

	call_pop( );
	Cur_Pulser = stored_Cur_Pulser;
}


/*-----------------------------------------------------------------------*/
/* Function for asking the pulser driver about the properties of a pulse */
/*-----------------------------------------------------------------------*/

Var *p_get( char *txt, int type )
{
	return p_get_by_num( p_num( txt ), type );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *p_get_by_num( long pnum, int type )
{
	int function;
	double ptime;
	long cycle;
	Var *v = NULL;
	long dev_num = -1;
	P_List *cur_p;
	long stored_Cur_Pulser;


	CLOBBER_PROTECT( v );
	CLOBBER_PROTECT( dev_num );

	for ( cur_p = plist; cur_p != NULL; cur_p = cur_p->next )
		if ( cur_p->num == pnum )
		{
			dev_num = cur_p->dev_num;
			break;
		}

	if ( dev_num == -1 )
	{
		print( FATAL, "Referenced pulse #%ld has not been defined.\n", pnum );
		THROW( EXCEPTION );
	}

	fsc2_assert( dev_num >= 0 && dev_num < EDL.Num_Pulsers );
	stored_Cur_Pulser = Cur_Pulser;
	Cur_Pulser = dev_num;

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		switch ( type )
		{
			case P_FUNC :
				is_pulser_func( pulser_struct[ Cur_Pulser ].get_pulse_function,
								"returning the function of a pulse" );
				pulser_struct[ Cur_Pulser ].get_pulse_function( pnum,
																&function );
				v = vars_push( INT_VAR, ( long ) function );
				break;

			case P_POS :
				is_pulser_func( pulser_struct[ Cur_Pulser ].get_pulse_position,
								"returning a pulses position" );
				pulser_struct[ Cur_Pulser ].get_pulse_position( pnum, &ptime );
				v = vars_push( FLOAT_VAR, ptime );
				break;

			case P_LEN :
				is_pulser_func( pulser_struct[ Cur_Pulser ].get_pulse_length,
								"returning a pulses length" );
				pulser_struct[ Cur_Pulser ].get_pulse_length( pnum, &ptime );
				v = vars_push( FLOAT_VAR, ptime );
				break;

			case P_DPOS :
				is_pulser_func(
					pulser_struct[ Cur_Pulser ].get_pulse_position_change,
					"returning a pulses position change" );
				pulser_struct[ Cur_Pulser ].get_pulse_position_change( pnum,
																	  &ptime );
				v = vars_push( FLOAT_VAR, ptime );
				break;

			case P_DLEN :
				is_pulser_func(
						   pulser_struct[ Cur_Pulser ].get_pulse_length_change,
						   "returning a pulses length change" );
				pulser_struct[ Cur_Pulser ].get_pulse_length_change( pnum,
																	  &ptime );
				v = vars_push( FLOAT_VAR, ptime );
				break;

			case P_PHASE :
				is_pulser_func(
							 pulser_struct[ Cur_Pulser ].get_pulse_phase_cycle,
							 "returning a pulses phase cycle" );
				pulser_struct[ dev_num ].get_pulse_phase_cycle( pnum, &cycle );
				v = vars_push( INT_VAR, cycle );
				break;

			default :
				fsc2_assert ( 1 == 0 );
				break;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		Cur_Pulser = stored_Cur_Pulser;
		RETHROW( );
	}

	call_pop( );
	Cur_Pulser = stored_Cur_Pulser;
	return v;
}


/*----------------------------------------------------------------------*/
/* 'function' is the phase function the data are to be used for (i.e. 0 */
/*	means PHASE_1, 1 means PHASE_2, 2 means both)                       */
/* 'type' means the type of phase, see global.h (PHASE_PLUS/MINUS_X/Y)  */
/* 'pod' means if the value is for the first or the second pod channel  */
/* (0: first pod channel, 1: second pod channel, -1: pick the one not   */
/* set yet)                                                             */
/* 'val' means high or low to be set on the pod channel to set the      */
/* requested phase (0: low, !0: high)                                   */
/* The last parameter indicates if this function was invoked with a POD */
/* keyword - this would be at least dubious for pulsers that don't have */
/* pods but just channels.                                              */
/*----------------------------------------------------------------------*/

void p_phs_setup( int func, int type, int pod, long val, bool is_pod )
{
	is_pulser_driver( );

	/* A few sanity checks before we call the pulsers handler function */

	fsc2_assert( type >= 0 && type < NUM_PHASE_TYPES );
	fsc2_assert( func == 0 || func == 1 );       /* phase function correct ? */

	/* Let's check if the pulser supports the function needed */

	is_pulser_func( pulser_struct[ Cur_Pulser ].phase_setup_prep,
					"setting up phase channels" );
	is_pulser_func( pulser_struct[ Cur_Pulser ].phase_setup,
					"setting up phase channels" );

	if ( is_pod && ! pulser_struct[ Cur_Pulser ].has_pods )
	{
		print( FATAL, "%s: Pulser has no pods, just channels.\n",
			   pulser_struct[ Cur_Pulser ].name );
		THROW( EXCEPTION );
	}

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].phase_setup_prep( func, type, pod, val );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-------------------------------------------------------------------*/
/* Function called by the parser at the end of a phase setup command */
/*-------------------------------------------------------------------*/

void p_phs_end( int func )
{
	is_pulser_driver( );

	fsc2_assert( func == 0 || func == 1 );      /* phase function correct ? */

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].phase_setup( func );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*-----------------------------------------------------------*/
/* Checks if a function keyword is PHASE and if it is if the */
/* the pulser driver supports phase switches.                */
/*-----------------------------------------------------------*/

void p_exists_function( int function )
{
	is_pulser_driver( );

	fsc2_assert( function >= 0 && function < PULSER_CHANNEL_NUM_FUNC );

	if ( ( function == PULSER_CHANNEL_PHASE_1 ||
		   function == PULSER_CHANNEL_PHASE_2 ) &&
		 ! pulser_struct[ Cur_Pulser ].needs_phase_pulses )
	{
		print( FATAL, "%s: Pulser driver does not support phase "
			   "switches, so PHASE functions can't be used.\n",
			   pulser_struct[ Cur_Pulser ].name );
		THROW( EXCEPTION );
	}
}


/****************************************************************/
/* The following functions are for deprecated keywords only and */
/* will be removed sometime.                                    */
/****************************************************************/

/*----------------------------------------------------------*/
/* Function for setting the phase switch delay.             */
/* 'func' is the phase function the data are to be used for */
/* (i.e. 0 means PHASE_1, 1 means PHASE_2)                  */
/*----------------------------------------------------------*/

void p_set_psd( int func, Var *v )
{
	double psd;


	fsc2_assert( func == 0 || func == 1 );

	psd = get_double( v, "phase switch delay" );
	vars_pop( v );
	is_pulser_func( pulser_struct[ Cur_Pulser ].set_phase_switch_delay,
					"setting a phase switch delay" );

	print( WARN, "Use of PHASE_SWITCH_DELAY keyword is deprecated, in the "
		   "future please use the function pulser_phase_switch_delay() in "
		   "the PREPARATIONS section.\n" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_phase_switch_delay( func == 0 ?
				 PULSER_CHANNEL_PHASE_1 : PULSER_CHANNEL_PHASE_2, psd );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*----------------------------------------------------------*/
/* Function for setting the grace period following a pulse. */
/*----------------------------------------------------------*/

void p_set_gp( Var *v )
{
	double gp;


	is_pulser_driver( );


	gp = get_double( v, "grace period" );
	vars_pop( v );
	is_pulser_func( pulser_struct[ Cur_Pulser ].set_grace_period,
					"setting a grace period" );

	print( WARN, "Use of GRACE_PERIOD keyword is deprecated, in the future "
		   "please use the function pulser_grace_period() in the PREPARATIONS "
		   "section.\n" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_grace_period( gp );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*----------------------------------------------------------------*/
/* Function for setting a user defined maximum pattern length for */
/* the pulser. This is needed when in the EDL program FOREVER     */
/* loops are used because in this case in the test the maximum    */
/* length can't be determined.                                    */
/*----------------------------------------------------------------*/

void p_set_max_seq_len( Var *v )
{
	double seq_len;


	CLOBBER_PROTECT( seq_len );

	is_pulser_driver( );

	/* Check the variable and get its value */

	seq_len = get_double( v, "maximum pattern length" );
	vars_pop( v );

	seq_len = is_mult_ns( seq_len, "Maximum pattern length" );

	/* Call the appropriate function (if it exists) */

    is_pulser_func( pulser_struct[ Cur_Pulser ].set_max_seq_len,
                    "setting a maximum pattern length" );

	print( WARN, "Use of MAXIMUM_PATTERN_LENGTH keyword is deprecated, in the "
		   "future please use the function pulser_maximum_pattern_length() in "
		   "the PREPARATIONS section.\n" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].set_max_seq_len( seq_len );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*---------------------------------------------------------*/
/* Function gets called when the keyword 'KEEP_ALL_PULSES' */
/* is found in the ASSIGNMENTS section                     */
/*---------------------------------------------------------*/

void keep_all_pulses( void )
{
	is_pulser_driver( );

	is_pulser_func( pulser_struct[ Cur_Pulser ].keep_all_pulses,
					"enforcing to keep all pulses" );

	print( WARN, "Use of KEEP_ALL_PULSES keyword is deprecated, in the "
		   "future please use the function pulser_keep_all_pulses() in "
		   "the PREPARATIONS section.\n" );

	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		pulser_struct[ Cur_Pulser ].keep_all_pulses( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );
}


/*------------------------------------------------------------------*/
/* Function to ask the pulser driver to convert a number associated */
/* with a symbolic channel name to an internaly used channel number */
/*------------------------------------------------------------------*/

long p_ch2num( long channel )
{
	long num;


	is_pulser_driver( );

	is_pulser_func( pulser_struct[ Cur_Pulser ].ch_to_num,
					"converting symbolic channel names to internal channel"
					"numbers" );
	
	TRY
	{
		call_push( NULL, pulser_struct[ Cur_Pulser ].device,
				   pulser_struct[ Cur_Pulser ].name, Cur_Pulser + 1 );
		TRY_SUCCESS;
	}

	TRY
	{
		num = pulser_struct[ Cur_Pulser ].ch_to_num( channel );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		call_pop( );
		RETHROW( );
	}

	call_pop( );

	return num;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
