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
*/


#include "fsc2_module.h"
#include <rulbus.h>


/* Include configuration information for the device */

#include "rb8509.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

#define TEST_VOLTS 3.578

static struct {
	int handle;
	int channel;
	int nchan;
	int gain;
	bool gain_is_set;
	int trig_mode;
	bool trig_mode_is_set;
} rb8509, rb8509_stored;


int rb8509_init_hook( void );
int rb8509_test_hook( void );
int rb8509_exp_hook( void );
int rb8509_end_of_exp_hook( void );

Var *daq_name( Var *v );
Var *daq_get_voltage( Var *v );
Var *daq_trigger_mode( Var *v );
Var *daq_gain( Var *v );

static int rb8509_translate_channel( long channel );


/*------------------------------------------------------*
 *------------------------------------------------------*/

int rb8509_init_hook( void )
{
	need_RULBUS = SET;

	rb8509.handle = -1;
	rb8509.channel = 0;
	rb8509.nchan = -1;
	rb8509.gain = RULBUS_ADC12_GAIN_1;
	rb8509.gain_is_set = SET;
	rb8509.trig_mode = RULBUS_ADC12_INT_TRIG;
	rb8509.trig_mode_is_set = SET;

	return 1;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int rb8509_test_hook( void )
{
	rb8509_stored = rb8509;
	return 1;
}

/*------------------------------------------------------*
 *------------------------------------------------------*/

int rb8509_exp_hook( void )
{
	rb8509 = rb8509_stored;
	rb8509.channel = 0;


	/* Open the card */

	if ( ( rb8509.handle = rulbus_card_open( RULBUS_CARD_NAME ) )
		 														 != RULBUS_OK )
	{
		print( FATAL, "Initialization of card faled: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	/* Find out how many channels the card has */

	if ( ( rb8509.nchan = rulbus_adc12_num_channels( rb8509.handle ) ) < 0 )
	{
		print( FATAL, "Initialization of card faled: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	/* Check if during the test run an invalid channel was used */

	if ( rb8509.nchan < rb8509_stored.nchan )
	{
		print( FATAL, "During the test run CH%d was used but the card has "
			   "only %d channels.\n", rb8509_stored.nchan, rb8509.nchan );
		THROW( EXCEPTION );
	}

	/* If necessary set the gain (card switches to a gain of 1 on
	   initialization) */

	if ( rb8509.gain_is_set && rb8509.gain != RULBUS_ADC12_GAIN_1 &&
		 rulbus_adc12_set_gain( rb8509.handle, rb8509.gain ) != RULBUS_OK )
	{
		print( FATAL, "Initialization of card faled: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	/* If necessary se the trigger mode (card switches to internal trigger
	   mode on initialization) */

	if ( rb8509.trig_mode_is_set &&
		 rb8509.trig_mode != RULBUS_ADC12_INT_TRIG &&
		 rulbus_adc12_set_gain( rb8509.handle, rb8509.trig_mode ) 
		 														 != RULBUS_OK )
	{
		print( FATAL, "Initialization of card faled: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

int rb8509_end_of_exp_hook( void )
{
	if ( rb8509.handle >= 0 )
	{
		rulbus_card_close( rb8509.handle );
		rb8509.handle = -1;
	}

	return 1;
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var *daq_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------*
 * Function returns the converted voltage from one of the channels
 *-----------------------------------------------------------------*/

Var *daq_get_voltage( Var *v )
{
	int channel;
	double volts;
	int retval = 0;


	channel = rb8509_translate_channel(
								  get_strict_long( v, "ADC channel number" ) );

	if ( FSC2_MODE == TEST )
	{
		rb8509.nchan = channel + 1;
		return vars_push( FLOAT_VAR, TEST_VOLTS / rb8509.gain );
	}

	if ( channel >= rb8509.nchan )
	{
		print( FATAL, "Invalid channel CH%d, maximum channels number "
			   "is CH%d\n", channel, rb8509.nchan - 1 );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* If necessary switch the current channel */

	if ( channel != rb8509.channel &&
		 rulbus_adc12_set_channel( rb8509.handle, channel ) != RULBUS_OK )
	{
		print( FATAL, "Communication failure: %s.\n", rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	rb8509.channel = channel;

	/* In internal trigger mode just trigger a conversion and return the
	   value */

	if ( rb8509.trig_mode == RULBUS_ADC12_INT_TRIG )
	{
		if ( rulbus_adc12_convert( rb8509.handle, &volts ) != RULBUS_OK )
		{
			print( FATAL, "Communication failure: %s.\n", rulbus_strerror( ) );
			THROW( EXCEPTION );
		}

		return vars_push( FLOAT_VAR, volts );
	}
			
	/* In internal trigger mode continuously check if a conversion has
	   happened and while doing so give the user a chance to bail out */

	while ( ! retval )
	{
		retval = rulbus_adc12_check_convert( rb8509.handle, &volts );
		if ( retval < 0 )
		{
			print( FATAL, "Communication failure: %s.\n", rulbus_strerror( ) );
			THROW( EXCEPTION );
		}

		stop_on_user_request( );
	} 

	return vars_push( FLOAT_VAR, volts );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var *daq_trigger_mode( Var *v )
{
	if ( v == NULL )
		return vars_push( INT_VAR, rb8509.trig_mode );

	vars_check( v, STR_VAR );

	if ( ! strcasecmp( v->val.sptr, "EXT" ) ||
		 ! strcasecmp( v->val.sptr, "EXTERNAL" ) )
		rb8509.trig_mode = RULBUS_ADC12_EXT_TRIG;
	else if ( ! strcasecmp( v->val.sptr, "INT" ) ||
			  ! strcasecmp( v->val.sptr, "INTERNAL" ) )
		rb8509.trig_mode = RULBUS_ADC12_INT_TRIG;
	else
	{
		print( FATAL, "Invalid trigger mode argument.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	rb8509.trig_mode_is_set = SET;

	if ( FSC2_MODE == EXPERIMENT &&
		 rulbus_adc12_set_trigger_mode( rb8509.handle, rb8509.trig_mode )
																 != RULBUS_OK )
	{
		print( FATAL, "Communication failure: %s.\n", rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, rb8509.trig_mode );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

Var *daq_gain( Var *v )
{
	int gain;


	if ( v == NULL )
		return vars_push( INT_VAR, rb8509.gain );

	gain = get_long( v, "ADC gain" );

	if ( gain != RULBUS_ADC12_GAIN_1 && gain != RULBUS_ADC12_GAIN_2 &&
		 gain != RULBUS_ADC12_GAIN_4 && gain != RULBUS_ADC12_GAIN_8 )
	{
		print( FATAL, "Invalid gain factor, only %d, %d, %d and %d are "
			   "possible.\n", RULBUS_ADC12_GAIN_1, RULBUS_ADC12_GAIN_2,
			   RULBUS_ADC12_GAIN_4, RULBUS_ADC12_GAIN_8 );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	rb8509.gain = gain;
	rb8509.gain_is_set = SET;

	if ( FSC2_MODE == EXPERIMENT &&
		 rulbus_adc12_set_gain( rb8509.handle, rb8509.gain ) != RULBUS_OK )
	{
		print( FATAL, "Communication failure: %s.\n", rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	return vars_push( INT_VAR, rb8509.gain );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static int rb8509_translate_channel( long channel )
{
	switch ( channel )
	{
		case CHANNEL_CH0 :
			return 0;

		case CHANNEL_CH1 :
			return 1;

		case CHANNEL_CH2 :
			return 2;

		case CHANNEL_CH3 :
			return 3;

		case CHANNEL_CH4 :
			return 4;

		case CHANNEL_CH5 :
			return 5;

		case CHANNEL_CH6 :
			return 6;

		case CHANNEL_CH7 :
			return 7;

		case CHANNEL_CH8 :
			return 8;

		case CHANNEL_CH9 :
			return 9;

		case CHANNEL_CH10 :
			return 10;

		case CHANNEL_CH11 :
			return 11;

		case CHANNEL_CH12 :
			return 12;

		case CHANNEL_CH13 :
			return 13;

		case CHANNEL_CH14 :
			return 14;

		case CHANNEL_CH15 :
			return 15;

		default :
			print( FATAL, "Invalid channel number.\n" );
			THROW( EXCEPTION );
	}

	return -1;            /* we'll never get here */
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */

