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

#include <me6x00.h>


/* Include configuration information for the device */

#include "me6000.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define MAX_NUMBER_OF_DACS   16

#define MIN_VOLTS    -10.0
#define MAX_VOLTS     10.0


/* Exported functions */

int me6000_init_hook( void );
int me6000_test_hook( void );
int me6000_exp_hook( void );
int me6000_end_of_exp_hook( void );
void me6000_exit_hook( void );

Var *dac_name( Var *v );
Var *dac_voltage( Var *v );


/* Locally used functions */

static int me6000_channel_number( long ch );


typedef struct {
	int num_dacs;
	struct {
		bool is_used;
		double volts;
	} dac[ MAX_NUMBER_OF_DACS ];
} ME6000;

ME6000 me6000, me6000_stored;


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int me6000_init_hook( void )
{
	int i;


	me6000.num_dacs = MAX_NUMBER_OF_DACS;
	
	for ( i = 0; i < 16; i++ )
	{
		me6000.dac[ i ].is_used = UNSET;
		me6000.dac[ i ].volts = 0.0;
	}

	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int me6000_test_hook( void )
{
	me6000_stored = me6000;
	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int me6000_exp_hook( void )
{
	int num_dacs;
	int i;


	/* Restore state from before the start of the test run */

	me6000 = me6000_stored;

	/* Try to get the number of DACs the boards has, this is also a test to
	   see if it can be opened and accessed */

	switch ( me6x00_num_dacs( BOARD_NUMBER, &num_dacs ) )
	{
		case 0 :                    /* everything is fine */
			break;

		case ME6X00_ERR_IBN :
			print( FATAL, "Invalid board number.\n" );
			THROW( EXCEPTION );

		case ME6X00_ERR_NSB :
			print( FATAL, "Driver for board not loaded.\n" );
			THROW( EXCEPTION );

		case ME6X00_ERR_NDF :
			print( FATAL, "Device file for board missing or inaccessible.\n" );
			THROW( EXCEPTION );

		case ME6X00_ERR_BSY :
			print( FATAL, "Board already in use by another program.\n" );
			THROW( EXCEPTION );

		case ME6X00_ERR_INT :
			print( FATAL, "Internal error in driver for board.\n" );
			THROW( EXCEPTION );

		default :
			print( FATAL, "Unrecognized error when trying to access the "
				   "board.\n" );
			THROW( EXCEPTION );
	}

	/* Now set the start voltage for all DACs that needs it to be set */

	me6000.num_dacs = num_dacs;

	for ( i = 0; i < me6000.num_dacs; i++ )
	{
		if ( me6000.dac[ i ].is_used &&
			 me6x00_voltage( BOARD_NUMBER, i, me6000.dac[ i ].volts ) < 0 )
		{
			print( FATAL, "Failed to set voltage for CH%d.\n", i );
			me6x00_close( BOARD_NUMBER );
			THROW( EXCEPTION );
		}

		if ( me6x00_keep_voltage( BOARD_NUMBER, i, 1 ) < 0 )
		{
			print( FATAL, "Failed to initialize board.\n" );
			me6x00_close( BOARD_NUMBER );
			THROW( EXCEPTION );
		}
	}

	/* Check that in the preparations section there wasn't a request for
	   setting a voltage for a DAC that doesn't exist */

	for ( i = me6000.num_dacs; i < MAX_NUMBER_OF_DACS; i++ )
		if ( me6000.dac[ i ].is_used )
		{
			print( FATAL, "Can't set voltage for CH%d, board has only "
				   "%d channels.\n", i, me6000.num_dacs );
			me6x00_close( BOARD_NUMBER );
			THROW( EXCEPTION );
		}

	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int me6000_end_of_exp_hook( void )
{
	me6x00_close( BOARD_NUMBER );
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void me6000_exit_hook( void )
{
	/* This shouldn't be necessary, I just want to make 100% sure that
	   the device file for the board is really closed */

	me6x00_close( BOARD_NUMBER );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *dac_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *dac_voltage( Var *v )
{
	long channel;
	int dac;
	double volts;


	channel = get_strict_long( v, "channel number" );

	dac = me6000_channel_number( channel );

	if ( dac >= me6000.num_dacs )
	{
		print( FATAL, "Can't set voltage for CH%d, board has only "
			   "%d channels.\n", dac, me6000.num_dacs );
		THROW( EXCEPTION );
	}

	v = vars_pop( v );

	if ( v == NULL )
	{
		if ( ! me6000.dac[ dac ].is_used )
		{
			if ( FSC2_MODE == EXPERIMENT )
				print( SEVERE, "Can't determine output voltage, it has never "
					   "been set.\n" );
			else
			{
				print( FATAL, "Can't determine output voltage, it has never "
					   "been set.\n" );
				THROW( EXCEPTION );
			}
		}

		return vars_push( FLOAT_VAR, me6000.dac[ dac ].volts );
	}

	volts = get_double( v, "DAC output voltage" );

	too_many_arguments( v );

	if ( volts < MIN_VOLTS || volts > MAX_VOLTS)
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( SEVERE, "Output voltage of %f too %s, must be between "
				   "%.2f V and %.2f V. Keeping current voltage of %.2f V.\n",
				   volts, volts < MIN_VOLTS ? "low" : "high", MIN_VOLTS,
				   MAX_VOLTS, me6000.dac[ dac ].volts );
			return vars_push( FLOAT_VAR, me6000.dac[ dac ].volts );
		}
		else
		{
			print( FATAL, "Output voltage of %f too %s, must be between "
				   "%.2f V and %.2f V.\n", volts,
				   volts < MIN_VOLTS ? "low" : "high", MIN_VOLTS, MAX_VOLTS );
			THROW( EXCEPTION );
		}
	}
	
	if ( FSC2_MODE == EXPERIMENT && 
		 me6x00_voltage( BOARD_NUMBER, dac, volts ) < 0 )
	{
		print( FATAL, "Failed to set output voltage.\n" );
		THROW( EXCEPTION );
	}

	me6000.dac[ dac ].is_used = SET;
	me6000.dac[ dac ].volts = volts;

	return vars_push( FLOAT_VAR, 0.0 );
}


/*---------------------------------------------------------------*/
/* Converts a channel number as we get it passed from the parser */
/* into a real DAC number.                                       */
/*---------------------------------------------------------------*/

static int me6000_channel_number( long ch )
{
	switch ( ch )
	{
		case CHANNEL_CH0 :
			return ME6X00_DAC00;

		case CHANNEL_CH1 :
			return ME6X00_DAC01;

		case CHANNEL_CH2 :
			return ME6X00_DAC02;

		case CHANNEL_CH3 :
			return ME6X00_DAC03;

		case CHANNEL_CH4 :
			return ME6X00_DAC04;

		case CHANNEL_CH5 :
			return ME6X00_DAC05;

		case CHANNEL_CH6 :
			return ME6X00_DAC06;

		case CHANNEL_CH7 :
			return ME6X00_DAC07;

		case CHANNEL_CH8 :
			return ME6X00_DAC08;

		case CHANNEL_CH9 :
			return ME6X00_DAC09;

		case CHANNEL_CH10 :
			return ME6X00_DAC10;

		case CHANNEL_CH11 :
			return ME6X00_DAC11;

		case CHANNEL_CH12 :
			return ME6X00_DAC12;

		case CHANNEL_CH13 :
			return ME6X00_DAC13;

		case CHANNEL_CH14 :
			return ME6X00_DAC14;

		case CHANNEL_CH15 :
			return ME6X00_DAC15;
	}


	if ( ch > CHANNEL_INVALID && ch < NUM_CHANNEL_NAMES )
	{
		print( FATAL, "There's no DAC channel named %s.\n",
			   Channel_Names[ ch ] );
		THROW( EXCEPTION );
	}

	print( FATAL, "Invalid channel number %ld.\n", ch );
	THROW( EXCEPTION );

	return -1;
}
