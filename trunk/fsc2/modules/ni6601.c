/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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
*/


#include "fsc2_module.h"


/* Include the header file for the library for the card */

#include <ni6601.h>


/* Include configuration information for the device */

#include "ni6601.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



/* Exported functions */

int ni6601_init_hook( void );
int ni6601_exp_hook( void );
int ni6601_end_of_exp_hook( void );
void ni6601_exit_hook( void );


Var *counter_start_continuous_counter( Var *v );
Var *counter_start_timed_counter( Var *v );
Var *counter_intermediate_count( Var *v );
Var *counter_final_count( Var *v );
Var *counter_stop_counter( Var *v );
Var *counter_single_pulse( Var *c );
Var *counter_continuous_pulses( Var *v );


static int ni6601_counter_number( long ch );
static int ni6601_source_number( long ch );
static const char *ni6601_ptime( double p_time );
static double ni6601_time_check( double duration, const char *text );


#define COUNTER_IS_BUSY 1

static int states[ 4 ];

/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int ni6601_init_hook( void )
{
	int i;


	for ( i = 0; i < 4; i++ )
		states[ i ] = 0;

	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int ni6601_exp_hook( void )
{
	int s;


	/* Ask for state of the one of the counters of the board to find out
	   if we can access the board */

	switch ( ni6601_is_counter_armed( BOARD_NUMBER, NI6601_COUNTER_0, &s ) )
	{
		case 0 :             /* everything ok */
			break;

		case NI6601_ERR_IBN :
			print( FATAL, "Invalid board number.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_NSB :
			print( FATAL, "Driver for board not loaded.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_NDF :
			print( FATAL, "Device file for board missing or inaccessible.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_BBS :
			print( FATAL, "Board already in use by another program.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_INT :
			print( FATAL, "Internal error in baord driver or library.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Unrecognized error when trying to access the "
				   "board.\n" );
			THROW( EXCEPTION );
	}

	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int ni6601_end_of_exp_hook( void )
{
	ni6601_close( BOARD_NUMBER );
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void ni6601_exit_hook( void )
{
	/* This shouldn't be necessary, I just want to make 100% sure that
	   the device file for the board is really closed */

	ni6601_close( BOARD_NUMBER );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_start_continuous_counter( Var *v )
{
	int	counter;
	int	source;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( ( v = vars_pop( v ) ) == NULL )
		source = NI6601_DEFAULT_SOURCE;
	else
	{
		source = ni6601_source_number( get_strict_long( v,
														"source channel" ) );
		too_many_arguments( v );
	}

	if ( FSC2_MODE == EXPERIMENT )
		switch( ni6601_start_counter( BOARD_NUMBER, counter, source ) )
		{
			case 0 :
				break;

			case NI6601_ERR_CBS :
				print( FATAL, "Counter CH%d is already running.\n", counter );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Can't start counter.\n" );
				THROW( EXCEPTION );
		}
	else if ( FSC2_MODE == TEST )
	{
		if ( states[ counter ] == COUNTER_IS_BUSY )
		{
			print( FATAL, "Counter CH%d is already running.\n", counter );
			THROW( EXCEPTION );
		}
		states[ counter ] = COUNTER_IS_BUSY;
	}

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_start_timed_counter( Var *v )
{
	int counter;
	int source;
	double interval;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing time interval for counting.\n" );
		THROW( EXCEPTION );
	}

	interval = ni6601_time_check( get_double( v, "counting time interval" ),
								  "counting time interval" );

	if ( ( v = vars_pop( v ) ) == NULL )
		source = NI6601_DEFAULT_SOURCE;
	else
	{
		source = ni6601_source_number( get_strict_long( v,
														"source channel" ) );
		too_many_arguments( v );
	}

	if ( FSC2_MODE == EXPERIMENT )
		switch ( ni6601_start_gated_counter( BOARD_NUMBER, counter, source,
											 interval ) )
		{
			case 0 :
				break;

			case NI6601_ERR_CBS :
				print( FATAL, "Counter CH%d is already running.\n", counter );
				THROW( EXCEPTION );

			case NI6601_ERR_NCB :
				print( FATAL, "Required neighbouring counter CH%d is already "
					   "running.\n", counter & 1 ? counter - 1 : counter + 1 );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Can't start counter.\n" );
				THROW( EXCEPTION );
		}
	else if ( FSC2_MODE == TEST &&
			  states[ counter ] == COUNTER_IS_BUSY )
	{
		print( FATAL, "Counter CH%d is already running.\n", counter );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_intermediate_count( Var *v )
{
	int counter;
	unsigned long count;
	int state;
	static long dummy_count = 0;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ni6601_get_count( BOARD_NUMBER, counter, 0, &count, &state ) < 0 )
		{
			print( FATAL, "Can't get counter value.\n" );
			THROW( EXCEPTION );
		}

		if ( count > LONG_MAX )
		{
			print( SEVERE, "Counter value too large.\n" );
			count = LONG_MAX;
		}
	}
	else
		count = ++dummy_count;

	return vars_push( INT_VAR, count );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_final_count( Var *v )
{
	int counter;
	unsigned long count;
	int state;
	static long dummy_count = 0;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( FSC2_MODE == EXPERIMENT )
		switch ( ni6601_get_count( BOARD_NUMBER, counter, 1, &count, &state ) )
		{
			case 0 :
				if ( count > LONG_MAX )
				{
					print( SEVERE, "Counter value too large.\n" );
					count = LONG_MAX;
				}
				break;

			case NI6601_ERR_WFC :
				print( FATAL, "Counter CH%d is running in continuous mode.\n",
					   counter );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Can't get counter value.\n" );
				THROW( EXCEPTION );
		}
	else
		count = ++dummy_count;

	return vars_push( INT_VAR, count );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_stop_counter( Var *v )
{
	int counter;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ni6601_stop_counter( BOARD_NUMBER, counter ) < 0 )
			print( SEVERE, "Failed to stop counter CH%d.\n", counter );
	}
	else if ( FSC2_MODE == TEST )
		states[ counter ] = 0;

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_single_pulse( Var *v )
{
	int counter;
	double duration;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );
	duration = ni6601_time_check( get_double( v->next, "pulse length" ),
								  "pulse length" );

	if ( FSC2_MODE == EXPERIMENT )
		switch ( ni6601_generate_single_pulse( BOARD_NUMBER, counter,
											   duration ) )
		{
			case 0 :
				break;

			case NI6601_ERR_CBS :
				print( FATAL, "Counter CH%d requested for pulse is already "
					   "running.\n", counter );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Can't create the pulse.\n" );
				THROW( EXCEPTION );
		}
	else if ( FSC2_MODE == TEST &&
			  states[ counter ] == COUNTER_IS_BUSY )
	{
		print( FATAL, "Counter CH%d requested for pulse is already "
			   "running.\n", counter );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *counter_continuous_pulses( Var *v )
{
	int counter;
	double len_hi, len_low;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing pulse length.\n" );
		THROW( EXCEPTION );
	}

	len_hi = ni6601_time_check( get_double( v,
											v->next != NULL ?
											"length of high phase of pulse" :
											"pulse length" ),
								v->next != NULL ?
								"length of high phase of pulse" :
								"pulse length" );

	if ( ( v = vars_pop( v ) ) != NULL )
		len_low = ni6601_time_check( get_double( v,
											  "length of low phase of pulse" ),
									 "length of low phase of pulse" );
	else
		len_low = len_hi;

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		switch ( ni6601_generate_continuous_pulses( BOARD_NUMBER, counter,
													len_hi, len_low ) )
		{
			case 0 :
				break;

			case NI6601_ERR_CBS :
				print( FATAL, "Counter CH%d requested for continnuous pulses "
					   "is already running.\n", counter );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Can't create continuous pulses.\n" );
				THROW( EXCEPTION );
		}
	else if ( FSC2_MODE == TEST &&
			  states[ counter ] == COUNTER_IS_BUSY )
	{
		print( FATAL, "Counter CH%d requested for continnuous pulses is "
			   "already running.\n", counter );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}



/*---------------------------------------------------------------*/
/* Converts a channel number as we get it passed from the parser */
/* into a real counter number.                                   */
/*---------------------------------------------------------------*/

static int ni6601_counter_number( long ch )
{
	switch ( ch )
	{
		case CHANNEL_CH0 :
			return NI6601_COUNTER_0;

		case CHANNEL_CH1 :
			return NI6601_COUNTER_1;

		case CHANNEL_CH2 :
			return NI6601_COUNTER_2;

		case CHANNEL_CH3 :
			return NI6601_COUNTER_3;
	}

	if ( ch > CHANNEL_INVALID && ch < NUM_CHANNEL_NAMES )
	{
		print( FATAL, "There's no counter channel named %s.\n",
			   Channel_Names[ ch ] );
		THROW( EXCEPTION );
	}

	print( FATAL, "Invalid counter channel number %ld.\n", ch );
	THROW( EXCEPTION );

	return -1;
}


/*---------------------------------------------------------------*/
/* Converts a channel number as we get it passed from the parser */
/* into a real source channel number.                            */
/*---------------------------------------------------------------*/

static int ni6601_source_number( long ch )
{
	switch ( ch )
	{
		case CHANNEL_DEFAULT_SOURCE :
			return NI6601_DEFAULT_SOURCE;

		case CHANNEL_SOURCE_0 :
			return NI6601_SOURCE_0;

		case CHANNEL_SOURCE_1 :
			return NI6601_SOURCE_1;

		case CHANNEL_SOURCE_2 :
			return NI6601_SOURCE_2;

		case CHANNEL_SOURCE_3 :
			return NI6601_SOURCE_3;

		case CHANNEL_NEXT_GATE :
			return NI6601_NEXT_GATE;

		case CHANNEL_TIMEBASE_1 :
			return NI6601_TIMEBASE_1;

		case CHANNEL_TIMEBASE_2 :
			return NI6601_TIMEBASE_2;
	}

	if ( ch > CHANNEL_INVALID && ch < NUM_CHANNEL_NAMES )
	{
		print( FATAL, "There's no source channel named %s.\n",
			   Channel_Names[ ch ] );
		THROW( EXCEPTION );
	}

	print( FATAL, "Invalid source channel number %ld.\n", ch );
	THROW( EXCEPTION );

	return -1;
}

/*----------------------------------------------------*/
/*----------------------------------------------------*/

static const char *ni6601_ptime( double p_time )
{
	static char buffer[ 128 ];

	if ( fabs( p_time ) >= 1.0 )
		sprintf( buffer, "%g s", p_time );
	else if ( fabs( p_time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * p_time );
	else if ( fabs( p_time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * p_time );
	else
		sprintf( buffer, "%g ns", 1.e9 * p_time );

	return buffer;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static double ni6601_time_check( double duration, const char *text )
{
	unsigned long ticks;


	if ( 1.01 * duration < 2 * NI6601_TIME_RESOLUTION ||
		 duration / NI6601_TIME_RESOLUTION - 1 >= ULONG_MAX + 0.5 )
	{
		static char *mint = NULL;
		static char *maxt = NULL;

		maxt = NULL;
		TRY
		{
			mint = T_strdup( ni6601_ptime( 2 * NI6601_TIME_RESOLUTION ) );
			maxt = T_strdup( ni6601_ptime( ( ULONG_MAX + 1 )
										   * NI6601_TIME_RESOLUTION ) );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( mint )
				mint = CHAR_P T_free( mint );
			if ( maxt )
				maxt = CHAR_P T_free( maxt );
			RETHROW( );
		}

		print( FATAL, "Invalid %s of %s, valid values are in the range "
			   "between %s and %s.\n", text, ni6601_ptime( duration ),
			   mint, maxt );
		mint = CHAR_P T_free( mint );
		maxt = CHAR_P T_free( maxt );
		THROW( EXCEPTION );
	}

	ticks = ( unsigned long ) floor( duration / NI6601_TIME_RESOLUTION + 0.5 );
	if ( fabs( ticks * NI6601_TIME_RESOLUTION - duration ) >
		 													1.0e-2 * duration )
	{
		static  char *oldv = NULL;

		TRY
		{
			oldv = T_strdup( ni6601_ptime( duration ) );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( oldv )
				oldv = CHAR_P T_free( oldv );
			RETHROW( );
		}

		print( WARN, "Adjusting %s from %s to %s.\n", text, oldv,
			   ni6601_ptime( ticks * NI6601_TIME_RESOLUTION ) );
		oldv = CHAR_P T_free( oldv );
	}

	return ticks * NI6601_TIME_RESOLUTION;
}
