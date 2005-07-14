/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
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


Var_T *counter_name( Var_T *v );
Var_T *counter_start_continuous_counter( Var_T *v );
Var_T *counter_start_timed_counter( Var_T *v );
Var_T *counter_intermediate_count( Var_T *v );
Var_T *counter_timed_count( Var_T *v );
Var_T *counter_final_count( Var_T *v );
Var_T *counter_stop_counter( Var_T *v );
Var_T *counter_single_pulse( Var_T *c );
Var_T *counter_continuous_pulses( Var_T *v );
Var_T *counter_dio_read( Var_T *v );
Var_T *counter_dio_write( Var_T *v );


static int ni6601_counter_number( long ch );
static int ni6601_source_number( long ch );
static const char *ni6601_ptime( double p_time );
static double ni6601_time_check( double duration, const char *text );


#define COUNTER_IS_BUSY 1

static int states[ 4 ];
static double jiffy_len;


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int ni6601_init_hook( void )
{
	int i;


	for ( i = 0; i < 4; i++ )
		states[ i ] = 0;

	/* Get the number of clock ticks per second */

	jiffy_len = 1.0 / ( double ) sysconf( _SC_CLK_TCK );

	return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int ni6601_exp_hook( void )
{
	int s;


	/* Ask for state of the one of the counters of the board to find out
	   if we can access the board */

	switch ( ni6601_is_counter_armed( BOARD_NUMBER, NI6601_COUNTER_0, &s ) )
	{
		case NI6601_OK :
			break;

		case NI6601_ERR_NSB :
			print( FATAL, "Invalid board number.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_NDV :
			print( FATAL, "Driver for board not loaded.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_ACS :
			print( FATAL, "No permissions to open device file for board.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_DFM :
			print( FATAL, "Device file for board missing.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_DFP :
			print( FATAL, "Unspecified error when opening device file for "
				   "board.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_BBS :
			print( FATAL, "Board already in use by another program.\n" );
			THROW( EXCEPTION );

		case NI6601_ERR_INT :
			print( FATAL, "Internal error in board driver or library.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Unrecognized error when trying to access the "
				   "board.\n" );
			THROW( EXCEPTION );
	}

	return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int ni6601_end_of_exp_hook( void )
{
	ni6601_close( BOARD_NUMBER );
	return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *counter_name( UNUSED_ARG Var_T *v )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *counter_start_continuous_counter( Var_T *v )
{
	int	counter;
	int	source;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

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
			case NI6601_OK :
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


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *counter_start_timed_counter( Var_T *v )
{
	int counter;
	int source;
	double interval;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

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
		switch ( ni6601_start_gated_counter( BOARD_NUMBER, counter, interval,
											 source ) )
		{
			case NI6601_OK :
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


/*-----------------------------------------------------------------*
 * This function is a combination of counter_start_timed_counter()
 * and counter_final_count() - it starts a timed count and waits
 * for the count interval finish and then fetches the count.
 *-----------------------------------------------------------------*/

Var_T *counter_timed_count( Var_T *v )
{
	int counter;
	int source;
	double interval;
	unsigned long count;
	int state;
	static long dummy_count = 0;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

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
		switch ( ni6601_start_gated_counter( BOARD_NUMBER, counter, interval,
											 source ) )
		{
			case NI6601_OK :
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

	if ( FSC2_MODE == EXPERIMENT )
	{
		/* For longer intervals (i.e. longer than the clock rate of the
		   machine) sleep instead of waiting (either sleeping or even
		   polling) in the kernel for the count interval to finish (as we
		   would do when calling ni6601_get_count() immediately with the
		   third argument being set to 1). If the user presses the "Stop"
		   button while we were sleeping stop the counter and return the
		   current count without waiting any longer. */

		if ( ( interval -= jiffy_len ) > 0.0 )
		{
			fsc2_usleep( ( unsigned long ) ( interval * 1.0e6 ), SET );
			if ( check_user_request( ) )
				counter_stop_counter( vars_push( INT_VAR, counter ) );
		}

	try_counter_again:

		/* Now ask the board for the value - make the kernel poll for the
		   result because, once we get here, the result should be available
		   within less than the clock rate. This helps not slowing down the
		   program too much when really short counting intervals are used. */

		switch ( ni6601_get_count( BOARD_NUMBER, counter, 1, 1,
								   &count, &state ) )
		{
			case NI6601_OK :
				if ( count > LONG_MAX )
				{
					print( SEVERE, "Counter value too large.\n" );
					count = LONG_MAX;
				}
				break;

			case NI6601_ERR_WFC :
				print( FATAL, "Can't get final count, counter CH%d is "
					   "running in continuous mode.\n", counter );
				THROW( EXCEPTION );

			case NI6601_ERR_ITR :
				goto try_counter_again;

			default :
				print( FATAL, "Can't get counter value.\n" );
				THROW( EXCEPTION );
		}
	}
	else
		count = ++dummy_count;

	return vars_push( INT_VAR, ( long ) count );
}


/*------------------------------------------------------*
 * Gets a count even while the counter is still running
 *------------------------------------------------------*/

Var_T *counter_intermediate_count( Var_T *v )
{
	int counter;
	unsigned long count;
	int state;
	static long dummy_count = 0;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ni6601_get_count( BOARD_NUMBER, counter, 0, 0,
							   &count, &state ) < 0 )
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


/*------------------------------------------------------------------*
 * Get a count after waiting until the counter is finished counting
 *------------------------------------------------------------------*/

Var_T *counter_final_count( Var_T *v )
{
	int counter;
	unsigned long count;
	int state;
	static long dummy_count = 0;


	counter = ni6601_counter_number( get_strict_long( v, "counter channel" ) );

	if ( FSC2_MODE == EXPERIMENT )
	{

	try_counter_again:

		switch ( ni6601_get_count( BOARD_NUMBER, counter, 1, 0,
								   &count, &state ) )
		{
			case NI6601_OK :
				if ( count > LONG_MAX )
				{
					print( SEVERE, "Counter value too large.\n" );
					count = LONG_MAX;
				}
				break;

			case NI6601_ERR_WFC :
				print( FATAL, "Can't get final count, counter CH%d is "
					   "running in continuous mode.\n", counter );
				THROW( EXCEPTION );

			case NI6601_ERR_ITR :
				goto try_counter_again;

			default :
				print( FATAL, "Can't get counter value.\n" );
				THROW( EXCEPTION );
		}
	}
	else
		count = ++dummy_count;

	return vars_push( INT_VAR, ( long ) count );
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *counter_stop_counter( Var_T *v )
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


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *counter_single_pulse( Var_T *v )
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
			case NI6601_OK :
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


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Var_T *counter_continuous_pulses( Var_T *v )
{
	int counter;
	double len_hi, len_low;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

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
			case NI6601_OK :
				break;

			case NI6601_ERR_CBS :
				print( FATAL, "Counter CH%d requested for continuous pulses "
					   "is already running.\n", counter );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Can't create continuous pulses.\n" );
				THROW( EXCEPTION );
		}
	else if ( FSC2_MODE == TEST &&
			  states[ counter ] == COUNTER_IS_BUSY )
	{
		print( FATAL, "Counter CH%d requested for continuous pulses is "
			   "already running.\n", counter );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *counter_dio_read( Var_T *v )
{
	long mask;
	unsigned char bits = 0;


	if ( v == NULL )
		mask = 0xFF;
	else
	{
		mask = get_strict_long( v, "DIO bit mask" );
		if ( mask < 0 || mask > 0xFF )
		{
			print( FATAL, "Invalid mask of 0x%X, valid range is [0-255].\n",
				   mask );
			THROW( EXCEPTION );
		}
	}

	if ( FSC2_MODE == EXPERIMENT &&
		 ni6601_dio_read( BOARD_NUMBER, &bits,
						  ( unsigned char ) ( mask & 0xFF ) ) < 0 )
		{
			print( FATAL, "Can't read data from DIO.\n" );
			THROW( EXCEPTION );
		}

	return vars_push( INT_VAR, ( long ) bits );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *counter_dio_write( Var_T *v )
{
	long bits;
	long mask;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	bits = get_strict_long( v, "DIO value" );

	if ( bits < 0 || bits > 0xFF )
	{
		print( FATAL, "Invalid value of %ld, valid range is [0-255].\n",
			   bits );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
		mask = 0xFF;
	else
	{
		mask = get_strict_long( v, "DIO bit mask" );
		if ( mask < 0 || mask > 0xFF )
		{
			print( FATAL, "Invalid mask of 0x%X, valid range is [0-255].\n",
				   mask );
			THROW( EXCEPTION );
		}

		too_many_arguments( v );
	}

	if ( FSC2_MODE == EXPERIMENT &&
		 ni6601_dio_write( BOARD_NUMBER, ( unsigned char ) ( bits & 0xFF ),
						   ( unsigned char ) ( mask & 0xFF ) ) < 0 )
	{
		print( FATAL, "Can't write value to DIO.\n" );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, 1 );
}


/*---------------------------------------------------------------*
 * Converts a channel number as we get it passed from the parser
 * into a real counter number.
 *---------------------------------------------------------------*/

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


/*---------------------------------------------------------------*
 * Converts a channel number as we get it passed from the parser
 * into a real source channel number.
 *---------------------------------------------------------------*/

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

/*----------------------------------------------------*
 *----------------------------------------------------*/

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


/*----------------------------------------------------*
 *----------------------------------------------------*/

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
			maxt = T_strdup( ni6601_ptime( ( ULONG_MAX + 1.0 )
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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
