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
#include <rulbus.h>


/* Include configuration information for the device */

#include "rb8510_1.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define TEST_VOLTS 3.578


static struct {
	int handle;
	double volts;
	bool volts_is_set;
	double Vmax;
	double Vmin;
	double dV;
} rb8510, rb8510_stored;


int rb8510_1_init_hook( void );
int rb8510_1_test_hook( void );
int rb8510_1_exp_hook( void );
int rb8510_1_end_of_exp_hook( void );

Var_T *daq_name( Var_T *v );
Var_T *daq_set_voltage( Var_T *v );


/*--------------------------------------------------------------*
 * Function called immediately after the module has been loaded
 *--------------------------------------------------------------*/

int rb8510_1_init_hook( void )
{
	Need_RULBUS = SET;

	rb8510.handle = -1;
	rb8510.volts_is_set = UNSET;

	return 1;
}


/*----------------------------------------------*
 * Function called at the start of the test run
 *----------------------------------------------*/

int rb8510_1_test_hook( void )
{
	rb8510_stored = rb8510;

	if ( rb8510.volts_is_set )
		rb8510.Vmax = rb8510.Vmin = rb8510.volts;
	else
	{
		rb8510.Vmax = - HUGE_VAL;
		rb8510.Vmin = HUGE_VAL;
	}

	return 1;
}

/*------------------------------------------------*
 * Function called at the start of the experiment
 *------------------------------------------------*/

int rb8510_1_exp_hook( void )
{
	rb8510 = rb8510_stored;

	/* Open the card */

	if ( ( rb8510.handle = rulbus_card_open( RULBUS_CARD_NAME ) ) < 0 )
	{
		print( FATAL, "Initialization of card failed: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	/* Find out about minimum and maximum output voltage */

	if ( rulbus_rb8510_dac12_properties( rb8510.handle, &rb8510.Vmax,
										 &rb8510.Vmin,
										 &rb8510.dV ) != RULBUS_OK )
	{
		print( FATAL, "Initialization of card failed: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	/* Check if during the test run an invalid voltage was requested */

	if ( rb8510_stored.Vmax >= rb8510.Vmax + 0.5 * rb8510.dV )
	{
		print( FATAL, "Voltage requested in preparation or test run was "
			   "higher than maximum output voltage of card.\n" );
		THROW( EXCEPTION );
	}

	if ( rb8510_stored.Vmin <= rb8510.Vmin - 0.5 * rb8510.dV )
	{
		print( FATAL, "Voltage requested in preparation or test run was lower "
			   "than minimum output voltage of card.\n" );
		THROW( EXCEPTION );
	}

	if ( rb8510.volts_is_set &&
		 rulbus_rb8510_dac12_set_voltage( rb8510.handle, rb8510.volts )
																 != RULBUS_OK )
	{
		print( FATAL, "Initialization of card failed: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	return 1;
}


/*----------------------------------------------*
 * Function called at the end of the experiment
 *----------------------------------------------*/

int rb8510_1_end_of_exp_hook( void )
{
	if ( rb8510.handle >= 0 )
	{
		rulbus_card_close( rb8510.handle );
		rb8510.handle = -1;
	}

	return 1;
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *daq_name( UNUSED_ARG Var_T *v )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------*
 * Function sets or returns the output voltage
 *--------------------------------------------*/

Var_T *daq_set_voltage( Var_T *v )
{
	double volts;


	if ( v == NULL )
	{
		if ( ! rb8510.volts_is_set )
		{
			print( FATAL, "Can't determine output voltage because it never "
				   "has been set.\n" );
			THROW( EXCEPTION );
		}

		return vars_push( FLOAT_VAR, rb8510.volts );
	}

	volts = get_double( v, "DAC output voltage" );
	too_many_arguments( v );

	if ( FSC2_MODE == PREPARATION )
	{
		if ( rb8510.volts_is_set )
		{
			print( SEVERE, "Output voltage has already been set, keeping "
				   "old value of %.2f V.\n", rb8510.volts );
			return vars_push( FLOAT_VAR, rb8510.volts );
		}

		rb8510.volts_is_set = SET;
		rb8510.volts = volts;
		return vars_push( FLOAT_VAR, rb8510.volts );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( volts > rb8510.Vmax )
			rb8510.Vmax = volts;
		else if ( volts < rb8510.Vmin )
			rb8510.Vmin = volts;
		rb8510.volts = volts;

		return vars_push( FLOAT_VAR, rb8510.volts );
	}

	if ( volts >= rb8510.Vmax + 0.5 * rb8510.dV )
	{
		print( FATAL, "Requested output voltage is higher than maximum output "
			   "voltage of %.2f V\n", rb8510.Vmax );
		THROW( EXCEPTION );
	}

	if ( volts <= rb8510.Vmin - 0.5 * rb8510.dV )
	{
		print( FATAL, "Requested output voltage is lower than minimum output "
			   "voltage of %.2f V\n", rb8510.Vmin );
		THROW( EXCEPTION );
	}

	if ( rulbus_rb8510_dac12_set_voltage( rb8510.handle, volts ) != RULBUS_OK )
	{
		print( FATAL, "Setting output voltage failed: %s.\n",
			   rulbus_strerror( ) );
		THROW( EXCEPTION );
	}

	rb8510.volts = volts;

	return vars_push( FLOAT_VAR, volts );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */

