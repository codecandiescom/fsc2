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
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2_module.h"


/* Include configuration information for the device */

#include "hjs_sfc.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Exported functions */

int hjs_sfc_init_hook( void );
int hjs_sfc_test_hook( void );
int hjs_sfc_exp_hook( void );
int hjs_sfc_end_of_exp_hook( void );
void hjs_sfc_exit_hook( void );

Var *magnet_name( Var *v );
Var *magnet_setup( Var *v );
Var *set_field( Var *v );
Var *get_field( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_field( Var *v );
Var *magnet_set_B0( Var *v );
Var *magnet_set_slope( Var *v );


/* Local functions */

static double hjs_sfc_field_check( double field );
static double hjs_sfc_set_field( double field );


struct HJS_SFC {
	double B0V;             /* Field for DAC voltage of 0 V */
	double slope;           /* field step for 1 V DAC voltage increment */

	bool B0V_has_been_used;
	bool slope_has_been_used;

	double field;			/* the start field given by the user */
	double field_step;		/* the field steps to be used */

	bool is_field;			/* flag, set if start field is defined */
	bool is_field_step;		/* flag, set if field step size is defined */

	char *dac_func;

	double act_field;		/* used internally */
	bool is_act_field;
};


struct HJS_SFC hjs_sfc, hjs_sfc_stored;


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*---------------------------------------------------------------------*/
/* Initialize the structure for the module, then lock the required DAC */
/* and check that the function for setting the DAC can be found.       */
/*---------------------------------------------------------------------*/

int hjs_sfc_init_hook( void )
{
	Var *Func_ptr;
	int acc;
	Var *v;
	int dev_num;
	char *func;


	/* Set the default values for the structure for the device */

	hjs_sfc.B0V = DEFAULT_B0;
	hjs_sfc.slope = DEFAULT_SLOPE;

	hjs_sfc.B0V_has_been_used = UNSET;
	hjs_sfc.slope_has_been_used = UNSET;

	hjs_sfc.is_field = UNSET;
	hjs_sfc.is_field_step = UNSET;

	hjs_sfc.dac_func = NULL;
	hjs_sfc.act_field = UNSET;

	/* Check that the module for the DA/AD converter is loaded.*/

	if ( ( dev_num = exists_device( "hjs_daadc" ) ) == 0 )
	{
		print( FATAL, "Can't find the module for the DA/AD converter "
			   "(hjs_daadc) - it must be listed before this module.\n" );
		THROW( EXCEPTION );
	}

	/* Get a lock on the DAC */

	if ( dev_num )
		func = T_strdup( "daq_reserve_dac" );
	else
		func = get_string( "daq_reserve_dac#%d", dev_num );

	if ( ! func_exists( func ) ||
		 ( Func_ptr = func_get( func, &acc ) ) == NULL )
	{
		T_free( func );
		print( FATAL, "Function for reserving the DAC is missing.\n" );
		THROW( EXCEPTION );
	}

	T_free( func );
	vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

	if ( ( v = func_call( Func_ptr ) ) == NULL )
	{
		print( FATAL, "Internal error detected at %s:%d.\n",
			   __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	if ( v->val.lval != 1 )
	{
		print( FATAL, "Can't reserve the DAC.\n" );
		THROW( EXCEPTION );
	}

	vars_pop( v );

	/* Get the name for the function for setting a value at the DAC and
	   check if it exists */

	if ( dev_num == 1 )
		hjs_sfc.dac_func = T_strdup( "daq_set_voltage" );
	else
		hjs_sfc.dac_func = get_string( "daq_set_voltage#%ld", dev_num );

	if ( ! func_exists( hjs_sfc.dac_func ) )
	{
		hjs_sfc.dac_func = CHAR_P T_free( hjs_sfc.dac_func );
		print( FATAL, "Function for setting the DAC is missing.\n" );
		THROW( EXCEPTION );
	}

	return 1;
}


/*----------------------------------------------------------*/
/* Just store the current settings in case they get changed */
/* during the test run.                                     */
/*----------------------------------------------------------*/

int hjs_sfc_test_hook( void )
{
	hjs_sfc_stored = hjs_sfc;

	if ( hjs_sfc.is_field )
	{
		hjs_sfc.act_field = hjs_sfc_set_field( hjs_sfc.field );
		hjs_sfc.is_act_field = SET;
	}

	return 1;
}


/*-------------------------------------------------------------*/
/* Restore the settings back to what they were after executing */
/* the preparations section and then set the start field (if   */
/* one had been set.                                           */
/*-------------------------------------------------------------*/

int hjs_sfc_exp_hook( void )
{
	hjs_sfc = hjs_sfc_stored;

	if ( hjs_sfc.is_field )
	{
		hjs_sfc.act_field = hjs_sfc_set_field( hjs_sfc.field );
		hjs_sfc.is_act_field = SET;
	}

	return 1;
}


/*--------------------------------------------------------------*/
/* Function that is called just before the module gets unloaded */
/*--------------------------------------------------------------*/

void hjs_sfc_exit_hook( void )
{
	if ( hjs_sfc.dac_func )
		T_free( hjs_sfc.dac_func );
}


/*-------------------------------------------*/
/*                                           */
/*             EDL functions                 */
/*                                           */
/*-------------------------------------------*/

/*----------------------------------------------------------------*/
/* Function returns a string variable with the name of the device */
/*----------------------------------------------------------------*/

Var *magnet_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------*/
/* Function for registering the start field and the field step size. */
/*-------------------------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	double start_field;
	double field_step;


	hjs_sfc.B0V_has_been_used = hjs_sfc.slope_has_been_used = SET;

	/* Check that both variables are reasonable */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	start_field = get_double( v, "magnetic field" );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing field step size.\n" );
		THROW( EXCEPTION );
	}

	field_step = get_double( v, "field step width" );

	too_many_arguments( v );

	/* Check that field value is still within the limits */

	hjs_sfc_field_check( start_field );

	hjs_sfc.field = start_field;
	hjs_sfc.field_step = field_step;
	hjs_sfc.is_field = hjs_sfc.is_field_step = SET;

	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *set_field( Var *v )
{
	double field;


	if ( v != NULL )
	{
		print( FATAL, "Missing argument.\n" );
		THROW( EXCEPTION );
	}

	field = get_double( v, "field" );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		print( SEVERE, "This module does not allow setting a field precision, "
			   "discarding second argument.\n" );
		too_many_arguments( v );
	}

	field = hjs_sfc_field_check( field );

	hjs_sfc.act_field = hjs_sfc_set_field( field );
	hjs_sfc.is_act_field = SET;

	return vars_push( FLOAT_VAR, hjs_sfc.act_field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *get_field( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! hjs_sfc.is_act_field )
	{
		print( FATAL, "Field hasn't been set yet, can't determine current "
			   "field.\n" );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR, hjs_sfc.act_field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *sweep_up( Var *v )
{
	double field;


	UNUSED_ARGUMENT( v );

	if ( ! hjs_sfc.is_field_step )
	{
		print( FATAL, "Sweep step size has not been defined.\n" );
		THROW( EXCEPTION );
	}

	
	field = hjs_sfc_field_check( hjs_sfc.act_field + hjs_sfc.field_step );

	hjs_sfc.act_field = hjs_sfc_set_field( field );
	hjs_sfc.is_act_field = SET;

	return vars_push( FLOAT_VAR, field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *sweep_down( Var *v )
{
	double field;


	UNUSED_ARGUMENT( v );

	if ( ! hjs_sfc.is_field_step )
	{
		print( FATAL, "Sweep step size has not been defined.\n" );
		THROW( EXCEPTION );
	}

	
	field = hjs_sfc_field_check( hjs_sfc.act_field - hjs_sfc.field_step );

	hjs_sfc.act_field = hjs_sfc_set_field( field );
	hjs_sfc.is_act_field = SET;

	return vars_push( FLOAT_VAR, field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *reset_field( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! hjs_sfc.is_field )
	{
		print( FATAL, "Start field has not been defined.\n" );
		THROW( EXCEPTION );
	}

	hjs_sfc.act_field = hjs_sfc_set_field( hjs_sfc.field );
	hjs_sfc.is_act_field = SET;
	
	return vars_push( FLOAT_VAR, hjs_sfc.act_field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *magnet_set_B0( Var *v )
{
	double B0V;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, hjs_sfc.B0V );

	if ( hjs_sfc.B0V_has_been_used )
	{
		print( FATAL, "Field for DAC output of 0 V has already been used, "
			   "can't be changed anymore.\n" );
		THROW( EXCEPTION );
	}

	B0V = get_double( v, "field for DAC output of 0 V" );

	if ( B0V < 0.0 )
	{
		print( FATAL, "Invalid negative field for DAC output of 0 V.\n" );
		THROW( EXCEPTION );
	}

	hjs_sfc.B0V = B0V;
			
	return vars_push( FLOAT_VAR, hjs_sfc.B0V );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *magnet_set_slope( Var *v )
{
	double slope;


	if ( v == NULL )
		return vars_push( FLOAT_VAR, hjs_sfc.slope );

	if ( hjs_sfc.slope_has_been_used )
	{
		print( FATAL, "Field change for 1 V DAC voltage increment has "
			   "already been used, can't be changed anymore.\n" );
		THROW( EXCEPTION );
	}

	slope = get_double( v, "field change for 1 V DAC voltage incement" );

	if ( slope == 0.0 )
	{
		print( FATAL, "Invalid zero field change for 1 V DAC voltage "
			   "increment.\n" );
		THROW( EXCEPTION );
	}

	hjs_sfc.slope = slope;
			
	return vars_push( FLOAT_VAR, hjs_sfc.slope );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

static double hjs_sfc_field_check( double field )
{
	/* When checking the field we must take into consideration that for
	   some magnets the field at 0 V is the highest possible field, while
	   for others it's lowest field. */

	if ( hjs_sfc.slope < 0.0 )
	{
		if ( field > hjs_sfc.B0V )
		{
			if ( FSC2_MODE == EXPERIMENT )
			{
				print( FATAL, "Field of %.2lf G is too high, reducing to "
				   "maximum field of %.2lf G.\n", field, hjs_sfc.B0V );
				return hjs_sfc.B0V;
			}

			print( FATAL, "Field of %.2lf G is too high, maximum is "
				   "%.2lf G.\n", field, hjs_sfc.B0V );
			THROW( EXCEPTION );
		}

		if ( field < hjs_sfc.B0V + 10.0 * hjs_sfc.slope )
		{
			if ( FSC2_MODE == EXPERIMENT )
			{
				print( FATAL, "Field of %.2lf G is too low, increasing to "
					   "minimum of %.2lf G.\n",
					   field, hjs_sfc.B0V + 10.0 * hjs_sfc.slope );
				return hjs_sfc.B0V + 10.0 * hjs_sfc.slope;
			}

			print( FATAL, "Field of %.2lf G is too low, minimum is %.2lf G.\n",
				   field, hjs_sfc.B0V + 10.0 * hjs_sfc.slope );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( field < hjs_sfc.B0V )
		{
			if ( FSC2_MODE == EXPERIMENT )
			{
				print( FATAL, "Field of %.2lf G is too low, increasing to "
					   "minimum of %.2lf G.\n", field, hjs_sfc.B0V );
				return hjs_sfc.B0V;
			}

			print( FATAL, "Field of %.2lf G is too low, minimum is %.2lf G.\n",
				   field, hjs_sfc.B0V );
			THROW( EXCEPTION );
		}

		if ( field > hjs_sfc.B0V + 10.0 * hjs_sfc.slope )
		{
			if ( FSC2_MODE == EXPERIMENT )
			{
				print( FATAL, "Field of %.2lf G is too high, reducing to "
					   "maximum of %.2lf G.\n",
					   field, hjs_sfc.B0V + 10.0 * hjs_sfc.slope );
				return hjs_sfc.B0V + 10.0 * hjs_sfc.slope;
			}

			print( FATAL, "Field of %.2lf G is too high, maximum is "
				   "%.2lf G.\n", field, hjs_sfc.B0V + 10.0 * hjs_sfc.slope );
			THROW( EXCEPTION );
		}
	}

	return field;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

static double hjs_sfc_set_field( double field )
{
	double v_step;
	Var *Func_ptr;
	int acc;
	Var *v;


	v_step = ( field - hjs_sfc.B0V ) / hjs_sfc.slope;

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ( Func_ptr = func_get( hjs_sfc.dac_func, &acc ) ) == NULL )
		{
			print( FATAL, "Internal error detected at %s:%d.\n",
				   __FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
		vars_push( FLOAT_VAR, v_step );

		if ( ( v = func_call( Func_ptr ) ) == NULL )
		{
			print( FATAL, "Internal error detected at %s:%d.\n",
				   __FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		vars_pop( v );

		/* We probably will have to wait here a bit for the new field to
		   be reached */
	}

	return hjs_sfc.B0V + v_step * hjs_sfc.slope;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
