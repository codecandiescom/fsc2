/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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
	int has_ext_trigger;
	bool trig_mode_is_set;
} rb8509, rb8509_stored;


int rb8509_init_hook(       void );
int rb8509_test_hook(       void );
int rb8509_exp_hook(        void );
int rb8509_end_of_exp_hook( void );

Var_T *daq_name(         Var_T * v );
Var_T *daq_get_voltage(  Var_T * v );
Var_T *daq_trigger_mode( Var_T * v );
Var_T *daq_gain(         Var_T * v );


static int rb8509_translate_channel( long channel );

static void rb8509_comm_failure( void );


/*--------------------------------------------------------------*
 * Function called immediately after the module has been loaded
 *--------------------------------------------------------------*/

int rb8509_init_hook( void )
{
	RULBUS_CARD_INFO card_info;


	Need_RULBUS = SET;

	if ( rulbus_get_card_info( RULBUS_CARD_NAME, &card_info ) != RULBUS_OK )
	{
		print( FATAL, "Failed to get RULBUS configuration: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	rb8509.handle = -1;
	rb8509.channel = 0;
	rb8509.nchan = card_info.num_channels;
	rb8509.gain = RULBUS_RB8509_ADC12_GAIN_1;
	rb8509.gain_is_set = SET;
	rb8509.has_ext_trigger = card_info.has_ext_trigger;
	rb8509.trig_mode = RULBUS_RB8509_ADC12_INT_TRIG;
	rb8509.trig_mode_is_set = SET;

	return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int rb8509_test_hook( void )
{
	rb8509_stored = rb8509;
	return 1;
}

/*------------------------------------------------*
 * Function called at the start of the experiment
 *------------------------------------------------*/

int rb8509_exp_hook( void )
{
	int ret;


	rb8509 = rb8509_stored;
	rb8509.channel = 0;

	/* Open the card */

	raise_permissions( );
	rb8509.handle = rulbus_card_open( RULBUS_CARD_NAME );
	lower_permissions( );

	if ( rb8509.handle < 0 )
	{
		print( FATAL, "Initialization of card failed: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	/* If necessary set the gain (card switches to a gain of 1 on
	   initialization) */

	if ( rb8509.gain_is_set && rb8509.gain != RULBUS_RB8509_ADC12_GAIN_1 )
	{
		raise_permissions( );
		ret = rulbus_rb8509_adc12_set_gain( rb8509.handle, rb8509.gain );
		lower_permissions( );

		if ( ret != RULBUS_OK )
		{
			print( FATAL, "Initialization of card failed: %s.\n",
				   rulbus_strerror( ) );
			THROW( EXCEPTION );
		}
	}

	/* If necessary set the trigger mode (card switches to internal trigger
	   mode on initialization) */

	if ( rb8509.trig_mode_is_set &&
		 rb8509.trig_mode != RULBUS_RB8509_ADC12_INT_TRIG )
	{
		raise_permissions( );
		ret = rulbus_rb8509_adc12_set_trigger_mode( rb8509.handle,
													rb8509.trig_mode );
		lower_permissions( );

		if ( ret != RULBUS_OK )
		{
			print( FATAL, "Initialization of card failed: %s.\n",
				   rulbus_strerror( ) );
			THROW( EXCEPTION );
		}
	}

	return 1;
}


/*----------------------------------------------*
 * Function called at the end of the experiment
 *----------------------------------------------*/

int rb8509_end_of_exp_hook( void )
{
	if ( rb8509.handle >= 0 )
	{
		raise_permissions( );
		rulbus_card_close( rb8509.handle );
		lower_permissions( );

		rb8509.handle = -1;
	}

	return 1;
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *daq_name( Var_T * v  UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------*
 * Function returns the converted voltage from one of the channels
 *-----------------------------------------------------------------*/

Var_T *daq_get_voltage( Var_T * v )
{
	int channel;
	double volts;
	int retval = 0;


	if ( v == NULL )
	{
		print( FATAL, "Missing channel argument\n" );
		THROW( EXCEPTION );
	}

	channel = rb8509_translate_channel(
								  get_strict_long( v, "ADC channel number" ) );


	if ( channel >= rb8509.nchan )
	{
		print( FATAL, "Invalid channel CH%d, maximum channels number "
			   "is CH%d\n", channel, rb8509.nchan - 1 );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, TEST_VOLTS / rb8509.gain );

	too_many_arguments( v );

	/* If necessary switch the current channel */

	if ( channel != rb8509.channel &&
		 rulbus_rb8509_adc12_set_channel( rb8509.handle,
										  channel ) != RULBUS_OK )
		rb8509_comm_failure( );

	rb8509.channel = channel;

	/* In internal trigger mode just trigger a conversion and return the
	   value */

	if ( rb8509.trig_mode == RULBUS_RB8509_ADC12_INT_TRIG )
	{
		raise_permissions( );
		retval = rulbus_rb8509_adc12_convert( rb8509.handle, &volts );
		lower_permissions( );

		if ( retval != RULBUS_OK )
			rb8509_comm_failure( );

		return vars_push( FLOAT_VAR, volts );
	}
			
	/* In internal trigger mode continuously check if a conversion has
	   happened and while doing so give the user a chance to bail out */

	while ( ! retval )
	{
		raise_permissions( );
		retval = rulbus_rb8509_adc12_check_convert( rb8509.handle, &volts );
		lower_permissions( );

		if ( retval < 0 )
			rb8509_comm_failure( );

		stop_on_user_request( );
	} 

	return vars_push( FLOAT_VAR, volts );
}


/*-------------------------------------------------------------------*
 * Function for setting or quering the trigger mode - note that some
 * versions of the card do not support external triggerin, but this
 * can only be determined when the experiment is started
 *-------------------------------------------------------------------*/

Var_T *daq_trigger_mode( Var_T * v )
{
	int ret;


	if ( v == NULL )
		return vars_push( INT_VAR, rb8509.trig_mode );

	vars_check( v, STR_VAR );

	if ( ! strcasecmp( v->val.sptr, "EXT" ) ||
		 ! strcasecmp( v->val.sptr, "EXTERNAL" ) )
	{
		if ( ! rb8509.has_ext_trigger )
		{
			print( FATAL, "The ADC card does not have an exernal trigger "
				   "input\n" );
			THROW( EXCEPTION );
		}

		rb8509.trig_mode = RULBUS_RB8509_ADC12_EXT_TRIG;
	}
	else if ( ! strcasecmp( v->val.sptr, "INT" ) ||
			  ! strcasecmp( v->val.sptr, "INTERNAL" ) )
		rb8509.trig_mode = RULBUS_RB8509_ADC12_INT_TRIG;
	else
	{
		print( FATAL, "Invalid trigger mode argument.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	rb8509.trig_mode_is_set = SET;

	if ( FSC2_MODE == EXPERIMENT )
	{
		raise_permissions( );
		ret = rulbus_rb8509_adc12_set_trigger_mode( rb8509.handle,
													rb8509.trig_mode );
		lower_permissions( );
		 
		if ( ret != RULBUS_OK )
			rb8509_comm_failure( );
	}

	return vars_push( INT_VAR, rb8509.trig_mode );
}


/*---------------------------------------------------------*
 * Function for setting or quering the pre-amplifier gain,
 * argument must be either 1, 2, 4 or 8
 *---------------------------------------------------------*/

Var_T *daq_gain( Var_T * v )
{
	int gain;
	int ret;


	if ( v == NULL )
		return vars_push( INT_VAR, rb8509.gain );

	gain = get_long( v, "ADC gain" );

	if ( gain != RULBUS_RB8509_ADC12_GAIN_1 &&
		 gain != RULBUS_RB8509_ADC12_GAIN_2 &&
		 gain != RULBUS_RB8509_ADC12_GAIN_4 &&
		 gain != RULBUS_RB8509_ADC12_GAIN_8 )
	{
		print( FATAL, "Invalid gain factor, only %d, %d, %d and %d are "
			   "possible.\n", RULBUS_RB8509_ADC12_GAIN_1,
			   RULBUS_RB8509_ADC12_GAIN_2, RULBUS_RB8509_ADC12_GAIN_4,
			   RULBUS_RB8509_ADC12_GAIN_8 );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	rb8509.gain = gain;
	rb8509.gain_is_set = SET;

	if ( FSC2_MODE == EXPERIMENT )
	{
		raise_permissions( );
		ret = rulbus_rb8509_adc12_set_gain( rb8509.handle, rb8509.gain );
		lower_permissions( );

		if ( ret != RULBUS_OK )
			rb8509_comm_failure( );
	}

	return vars_push( INT_VAR, rb8509.gain );
}


/*----------------------------------------------------------------*
 * Internally used function for converting a channel number as we
 * get it from the EDL script into a "real" channel number
 *----------------------------------------------------------------*/

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

		default :
			print( FATAL, "Invalid channel number.\n" );
			THROW( EXCEPTION );
	}

	return -1;            /* we'll never get here */
}


/*---------------------------------------------------*
 * Called on failures of communication with the card
 *---------------------------------------------------*/

static void rb8509_comm_failure( void )
{
	print( FATAL, "Communication failure: %s.\n", rulbus_strerror( ) );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
