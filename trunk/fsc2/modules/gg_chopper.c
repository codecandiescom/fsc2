/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


/* Include configuration information for the device */

#include "gg_chopper.conf"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define MAX_DIO_VALUE 255      /* lowest possible frequency of 39.2 Hz */


static struct {
	char *dio_func;
	char *freq_out_func;
	unsigned char dio_value;
} gg_chopper, gg_chopper_reserved;


int gg_chopper_init_hook( void );
int gg_chopper_test_hook( void );
int gg_chopper_exp_hook( void );
int gg_chopper_end_of_exp_hook( void );
void gg_chopper_exit_hook( void );
Var *chopper_name( Var *v );
Var *chopper_sector_frequency( Var *v );
static void gg_chopper_set_dio( long val );
static void gg_chopper_set_freq_out( double freq );



/*-----------------------------------------------------------------*
 * Function that gets called when the module is loaded. It checks
 * that the required functions of the DAQ device are available and
 * initializes a few data.
 *-----------------------------------------------------------------*/

int gg_chopper_init_hook( void )
{
	int dev_num;
	Var *Func_ptr;
	Var *v;
	char *func;
	int acc;


	if ( ( dev_num = exists_device( DAQ_MODULE ) ) == 0 )
	{
		print( FATAL, "Can't find the module '%s' - it must be listed before "
			   "this module.\n", DAQ_MODULE );
		THROW( EXCEPTION );
	}

	/* Get a lock on the DIO of the DAQ device used to control the chopper */

	if ( dev_num )
		func = T_strdup( "daq_reserve_dio" );
	else
		func = get_string( "daq_reserve_dio#%d", dev_num );

	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		T_free( func );
		print( FATAL, "Function for reserving the DIO is missing from "
			   "module '%s'.\n", DAQ_MODULE );
		THROW( EXCEPTION );
	}

	T_free( func );
	vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

	v = func_call( Func_ptr );

	if ( v->val.lval != 1 )
	{
		print( FATAL, "Can't reserve the DIO using module '%s'.\n",
			   DAQ_MODULE );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	/* Get a lock on the FREQ_OUT pin of the DAQ device to be used to control
	   the chopper */

	if ( dev_num )
		func = T_strdup( "daq_reserve_freq_out" );
	else
		func = get_string( "daq_reserve_freq_out#%d", dev_num );

	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		T_free( func );
		print( FATAL, "Function for reserving the FREQ_OUT pin is missing "
			   "from module '%s'.\n", DAQ_MODULE );
		THROW( EXCEPTION );
	}

	T_free( func );
	vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

	v = func_call( Func_ptr );

	if ( v->val.lval != 1 )
	{
		print( FATAL, "Can't reserve the FREQ_OUT pin using module '%s'.\n",
			   DAQ_MODULE );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	/* Get the name for the function for setting a value at the DIO and
	   check if it exists */

	if ( dev_num == 1 )
		gg_chopper.dio_func = T_strdup( "daq_dio_write" );
	else
		gg_chopper.dio_func = get_string( "daq_dio_write#%d", dev_num );

	if ( ! func_exists( gg_chopper.dio_func ) )
	{
		gg_chopper.dio_func = CHAR_P T_free( gg_chopper.dio_func );
		print( FATAL, "Function for setting the DIO is missing from module "
			   "'%s'.\n", DAQ_MODULE ); 
		THROW( EXCEPTION );
	}

	/* Get the name for the function for setting a value at the FREQ_OUT pin
	   and check if it exists */

	if ( dev_num == 1 )
		gg_chopper.freq_out_func = T_strdup( "daq_freq_out" );
	else
		gg_chopper.freq_out_func = get_string( "daq_freq_out#%d", dev_num );

	if ( ! func_exists( gg_chopper.freq_out_func ) )
	{
		gg_chopper.dio_func = CHAR_P T_free( gg_chopper.dio_func );
		gg_chopper.freq_out_func = CHAR_P T_free( gg_chopper.freq_out_func );
		print( FATAL, "Function for setting the frequency of the FREQ_OUT "
			   "pin is missing from module '%s'.\n", DAQ_MODULE );
		THROW( EXCEPTION );
	}

	gg_chopper.dio_value = 0;

	return 1;
}


/*---------------------------------------------------*
 * Function gets called at the start of the test run
 *---------------------------------------------------*/

int gg_chopper_test_hook( void )
{
	gg_chopper_reserved = gg_chopper;
	return 1;
}


/*------------------------------------------------------------------*
 * Function gets called at the start of the experiment. It sets the
 * frequency of the FREQ_OUT pin of the DAQ device card to 1 MHz
 * and then sets the chopper sector frequency (if this was requested
 * during the PREPARATIONS section). Take note: the chopper sector
 * frequency is twice the chopper rotation speed because it has two
 * openings (of 45 degree each).
 *------------------------------------------------------------------*/

int gg_chopper_exp_hook( void )
{
	gg_chopper = gg_chopper_reserved;


	/* Make sure the chopper is stopped (by setting the DIO value to 0)
	   before adjusting the FREQ_OUT frequency */

	gg_chopper_set_dio( 0 );

	/* Set the frequency of the FREQ_OUT pin of the DAQ device to 10 MHz */

	gg_chopper_set_freq_out( 1.0e6 );

	/* If a choppper frequency had been set during the PREPARATIONS section
	   set it now by outputting a value at the  DIO of the DAQ device */

	if ( gg_chopper.dio_value != 0 )
		gg_chopper_set_dio( gg_chopper.dio_value );

	return 1;
}


/*------------------------------------------------------------------------*
 * Function called at the end of an experiment, just stopping the chopper.
 *------------------------------------------------------------------------*/

int gg_chopper_end_of_exp_hook( void )
{
	/* Stop the chopper */

	gg_chopper_set_dio( 0 );
	gg_chopper_set_freq_out( 0.0 );

	return 1;
}


/*--------------------------------------------------------------*
 * Function that gets called just before the module is removed.
 *--------------------------------------------------------------*/

void gg_chopper_exit_hook( void )
{
	if ( gg_chopper.dio_func )
		gg_chopper.dio_func = CHAR_P T_free( gg_chopper.dio_func );
	if ( gg_chopper.freq_out_func )
		gg_chopper.freq_out_func = CHAR_P T_free( gg_chopper.freq_out_func );
}


/*----------------------------------------------------------------*/
/* Function returns a string variable with the name of the device */
/*----------------------------------------------------------------*/

Var *chopper_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------*
 * The only EDL function of the module: It can be used to either
 * determine or set the sector frequency of the chopper.
 *---------------------------------------------------------------*/

Var *chopper_sector_frequency( Var *v )
{
	double freq;
	long dio_value;


	if ( v == NULL )
	{
		if ( gg_chopper.dio_value == 0 )
			return vars_push( FLOAT_VAR, 0.0 );

		return vars_push( FLOAT_VAR, 1.0e4 / gg_chopper.dio_value );
	}

	freq = get_double( v, "chopper sector frequency" );

	too_many_arguments( v );

	if ( freq <= 1.0e-9 )
		dio_value = 0L;
	else
	{
		dio_value = lrnd( 1.0e4 / freq );

		if ( dio_value > MAX_DIO_VALUE || dio_value < MIN_DIO_VALUE )
		{
			print( FATAL, "Invalid chopper sector frequency of %.2f Hz, it "
				   "must be between %.2f Hz and %.2f Hz (or 0 Hz to stop the "
				   "chopper).\n", freq, 1.0e4 / MAX_DIO_VALUE,
				   1.0e4 / MIN_DIO_VALUE );
			THROW( EXCEPTION );
		}

		/* Warn the user if the sector frequency we're going to set deviates
		   by more than 1% from the requested frequency. */

		if ( fabs( freq - 1.0e4 / dio_value ) > 0.01 * freq )
			print( WARN, "Chopper sector frequency had to be adjusted from "
				   "%.2f Hz to %.2f Hz.\n", freq, 1.0e4 / dio_value );
	}

	/* Set the sector frequency of the chopper by outputting a value at
	   the DIO of the DAQ device card */

	if ( FSC2_MODE == EXPERIMENT )
		gg_chopper_set_dio( dio_value );

	gg_chopper.dio_value = ( unsigned char ) dio_value;

	return vars_push( FLOAT_VAR, 1.0e4 / dio_value );
}


/*---------------------------------------------------------------*
 * Internally used function to output a value at the DIO of the
 * DAQ device.
 *--------------------------------------------------------------*/

static void gg_chopper_set_dio( long val )
{
	Var *Func_ptr;
	int acc;

	if ( ( Func_ptr = func_get( gg_chopper.dio_func, &acc ) ) == NULL )
	{
		print( FATAL, "Internal error detected at %s:%d.\n",
			   __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
	vars_push( INT_VAR, val );
	vars_pop( func_call( Func_ptr ) );
}


/*--------------------------------------------------------------*
 * Internally used function to set the output frequency of the
 * FREQ_OUT pin of the DAQ device.
 *--------------------------------------------------------------*/

static void gg_chopper_set_freq_out( double freq )
{
	Var *Func_ptr;
	int acc;


	if ( ( Func_ptr = func_get( gg_chopper.freq_out_func, &acc ) ) == NULL )
	{
		print( FATAL, "Internal error detected at %s:%d.\n",
			   __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}
	
	vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
	vars_push( FLOAT_VAR, freq );
	vars_pop( func_call( Func_ptr ) );
}
