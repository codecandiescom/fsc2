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


/* Include configuration information for the device */

#include "bnm12.conf"


/* Include the header file for the library for the WITIO-48 card */

#include <witio_48.h>


/* Device specific defines */

#define RESOLUTION_VERY_HIGH       0
#define RESOLUTION_HIGH            1
#define RESOLUTION_MEDIUM          2
#define RESOLUTION_LOW             3
#define RESOLUTION_VERY_LOW        4
#define RESOLUTION_EXTREMELY_LOW   5

#define NO_LOCK_BIT        0x800000UL

#define DEFAULT_TEST_FIELD 3400.0


/* Global variables */

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* The structure for the device */

struct BNM12 {
	int resolution;
	double res_list[ 6 ];
	char *dio_value_func;
};


static struct BNM12 bnm12 = { RESOLUTION_MEDIUM,
							  { 0.0001, 0.001, 0.01, 0.1, 1.0, 10.0 },
							  NULL };
static struct BNM12 bnm12_stored;


/* Exported functions */

int bnm12_init_hook( void );
int bnm12_test_hook( void );
int bnm12_exp_hook( void );
void bnm12_exit_hook( void );

Var_T *gaussmeter_name( Var_T *v );
Var_T *gaussmeter_field( Var_T *v );
Var_T *gaussmeter_resolution( Var_T *v );


/* Local functions */

static double bnm12_get_field( void );
static void bnm12_check_field( void );


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*--------------------------------------------------------------*
 * The gaussmeter outputs its measured results at a 24-bit wide
 * digital output in BCD format. To be able to obtain the value
 * a DIO device is required, currently only the WITIO-48 card
 * is implemented to do the job. So the first thing the module
 * must do is to check if the witio_48 is already loaded and
 * complain if it isn't.
 *--------------------------------------------------------------*/

int bnm12_init_hook( void )
{
	Var_T *Func_ptr;
	int acc;
	Var_T *v;
	int dev_num;
	char *func;


	/* Check that the module for the home-built DA/AD converter has already
	   been loaded */

	if ( ! exists_device( "hjs_daadc" ) )
	{
		print( FATAL, "Can't find the module for the DA/AD converter "
			   "(hjs_daadc) - it must be listed before this module.\n" );
		THROW( EXCEPTION );
	}

	/* Check that the WITIO-48 module has already been loaded */

	if ( ( dev_num = exists_device( "witio_48" ) ) == 0 )
	{
		print( FATAL, "Can't find the module for the WITIO-48 DIO card "
			   "(witio_48) - it must be listed before the gaussmeter.\n" );
		THROW( EXCEPTION );
	}

	/* Get a lock on the required DIO of the WITIO-48 card */

	func = get_string( "dio_reserve_dio#%d", dev_num );

	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		T_free( func );
		print( FATAL, "Function for reserving a DIO is missing.\n" );
		THROW( EXCEPTION );
	}

	T_free( func );
	vars_push( INT_VAR, DIO_NUMBER );     /* push the DIO number */
	vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */
	v = func_call( Func_ptr );            /* call the function */

	if ( v->val.lval != 1 )
	{
		print( FATAL, "Can't reserve DIO_%ld of WITIO-48 card.\n",
			   DIO_NUMBER );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	/* Make sure "our" DIO of the WITIO-48 card is in "1x24" mode */

	func = get_string( "dio_mode#%d", dev_num );

	if ( ! func_exists( "dio_mode" ) ||
		 ( Func_ptr = func_get( "dio_mode", &acc ) ) == NULL )
	{
		T_free( func );
		print( FATAL, "Function for setting the DIO mode is missing.\n" );
		THROW( EXCEPTION );
	}

	T_free( func );

	vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
	vars_push( INT_VAR, DIO_NUMBER );          /* push the DIO number */
	vars_push( STR_VAR, "1x24" );              /* push the mode type string */
	vars_pop( func_call( Func_ptr ) );         /* call the function */

	/* Get the name for the function for reading from the DIO and check if
	   it exists */

	if ( dev_num == 1 )
		bnm12.dio_value_func = T_strdup( "dio_value" );
	else
		bnm12.dio_value_func = get_string( "dio_value#%ld", dev_num );

	if ( ! func_exists( bnm12.dio_value_func ) )
	{
		bnm12.dio_value_func = CHAR_P T_free( bnm12.dio_value_func );
		print( FATAL, "Function for reading from the DIO mode is missing.\n" );
		THROW( EXCEPTION );
	}

	/* Per default we assume the resolution is set to 0.01 G (medium) */

	bnm12.resolution = RESOLUTION_MEDIUM;

	return 1;
}


/*----------------------------------------------------------*
 * Just store the current settings in case they get changed
 * during the test run.
 *----------------------------------------------------------*/

int bnm12_test_hook( void )
{
	bnm12_stored = bnm12;
	return 1;
}


/*-------------------------------------------------------------*
 * Restore the settings back to what they were after executing
 * the preparations section.
 *-------------------------------------------------------------*/

int bnm12_exp_hook( void )
{
	bnm12 = bnm12_stored;

	bnm12_check_field( );

	return 1;
}


/*----------------------------------------------------------------*
 * Function that gets called just before the module gets unloaded
 *----------------------------------------------------------------*/

void bnm12_exit_hook( void )
{
	if ( bnm12.dio_value_func )
		T_free( bnm12.dio_value_func );
}


/*-------------------------------------------*/
/*                                           */
/*             EDL functions                 */
/*                                           */
/*-------------------------------------------*/

/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *gaussmeter_name( Var_T *v UNUSED_ARG )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------*
 * Measure a new field value and return it to the caller
 *-------------------------------------------------------*/

Var_T *gaussmeter_field( Var_T *v UNUSED_ARG )
{
	return vars_push( FLOAT_VAR, bnm12_get_field( ) );
}


/*----------------------------------------------------------------*
 * Function for querying or setting the current resolution of the
 * device. Since we can't ask the device about it we've got to
 * trust what the user tells us...
 *----------------------------------------------------------------*/

Var_T *gaussmeter_resolution( Var_T *v )
{
	double res;
	int i;
	int res_index = -1;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, bnm12.res_list[ bnm12.resolution ] );

	res = get_double( v, "resolution" );

	too_many_arguments( v );

	if ( res <= 0 )
	{
		print( FATAL, "Invalid resolution of %f G.\n", res );
		THROW( EXCEPTION );
	}

	/* Try to match the requested resolution, if necessary use the nearest
	   possible value */

	for ( i = 0; i < RESOLUTION_LOW; i++ )
		if ( res >= bnm12.res_list[ i ] && res < bnm12.res_list[ i + 1 ] )
		{
			res_index = i +
				 ( ( res / bnm12.res_list[ i ] <
					 bnm12.res_list[ i + 1 ] / res ) ? 0 : 1 );
			break;
		}

	if ( res_index == -1 )
	{
		if ( res >= bnm12.res_list[ RESOLUTION_EXTREMELY_LOW ] )
			res_index = RESOLUTION_EXTREMELY_LOW;
		if ( res <= bnm12.res_list[ RESOLUTION_VERY_HIGH ] )
			res_index = RESOLUTION_VERY_HIGH;
	}

	fsc2_assert( res_index >= RESOLUTION_VERY_HIGH &&
				 res_index <= RESOLUTION_EXTREMELY_LOW );

	if ( fabs( bnm12.res_list[ res_index ] - res ) >
										 1.0e-2 * bnm12.res_list[ res_index ] )
		print( WARN, "Can't set resolution to %.2f G, using %.2f G instead.\n",
			   res, bnm12.res_list[ res_index ] );

	bnm12.resolution = res_index;

	return vars_push( FLOAT_VAR, bnm12.res_list[ bnm12.resolution ] );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*---------------------------------------------------------------*
 * Function for obtaining a new field value by reading the input
 * at the WITIO-48 DIO card. The value still has to be converted
 * from BCD format.
 *---------------------------------------------------------------*/

static double bnm12_get_field( void )
{
	Var_T *Func_ptr;
	int acc;
	Var_T *v;
	unsigned long res;
	unsigned long raw_field;
	int i;
	unsigned long fac; 


	if ( FSC2_MODE == TEST )
		return DEFAULT_TEST_FIELD;

	/* Get a value from the gaussmeter via the WITIO-48 DIO card */

	if ( ( Func_ptr = func_get( bnm12.dio_value_func, &acc ) ) == NULL )
	{
		print( FATAL, "Internal error detected at %s:%d.\n",
			   __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
	vars_push( INT_VAR, DIO_NUMBER );          /* push the DIO number */
	vars_push( INT_VAR, WITIO_48_CHANNEL_0 );  /* push the channel number */
	v = func_call( Func_ptr );                 /* call the function */

	res = ( unsigned long ) v->val.lval;
	vars_pop( v );

	/* Convert the BCD data (there are 6 digits ) we got from the device
	   to an integer */

	for ( raw_field = 0, fac = 1, i = 0; i < 6; res >>= 4, fac *= 10, i++ )
	{
		if ( ( res & 0x0FUL ) > 9 )
		{
			print( FATAL, "Data received from the gaussmeter are garbled.\n" );
			THROW( EXCEPTION );
		}
				
		raw_field += fac * ( res & 0x0FUL );
	}

	/* Return the field after multiplying it with the correct field
	   resolution factor */

	return raw_field * bnm12.res_list[ bnm12.resolution ];
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static void bnm12_check_field( void )
{
	Var_T *Func_ptr;
	int acc;
	Var_T *v;
	unsigned long res = ~ 0,
				  old_res;


	/* Get the field again and again until the gaussmeter seems to have a
	   lock. That assumed when the difference between readings isn't larger
	   than about 1 G when measured at time intervals of 1 second - while
	   trying to get a lock the gaussmeter always searches downwards with
	   a speed of about 1 G per second. */

	do
	{
		old_res = res;

		/* Get a value from the gaussmeter via the WITIO-48 DIO card */

		if ( ( Func_ptr = func_get( bnm12.dio_value_func, &acc ) ) == NULL )
		{
			print( FATAL, "Internal error detected at %s:%d.\n",
				   __FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		vars_push( STR_VAR, DEVICE_NAME );        /* push the pass-phrase */
		vars_push( INT_VAR, DIO_NUMBER );         /* push the DIO number */
		vars_push( INT_VAR, WITIO_48_CHANNEL_0 ); /* push the channel number */
		v = func_call( Func_ptr );                /* call the function */

		res = ( unsigned long ) v->val.lval;
		vars_pop( v );

		stop_on_user_request( );

		fsc2_usleep( 1000000, UNSET );

	} while ( old_res - res > 100 );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
