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


/* Include configuration information for the device */

#include "er032m.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD  3480.0   /* returned as current field in test run */

#define SWEEP_RANGE_RESOLUTION 0.1

#define MIN_SWA            0
#define CENTER_SWA         2048
#define MAX_SWA            4095
#define SWA_RANGE          4096

/* Defines the time before we test again if it is found that the overload
   LED is still on */

#define ER032M_WAIT_TIME   100000     /* in us */ 

/* Define the maximum number of retries before giving up if the
   overload LED is on */

#define ER032M_MAX_RETRIES   300


/* When setting a new center field or sweep width the value send to the
   device is read back to check if it really got set. If the values differ
   the ER032M_MAX_SET_RETRIES times the value is again set and also checked
   before we give up */

#define ER032M_MAX_SET_RETRIES 3


/* The maximum field resolution: this is, according to the manual the best
   result that can be achieved when repeatedly setting the center field and
   it doesn't seem to make sense too much sense to set a new field when the
   difference from the current field is lower */

#define ER032M_RESOLUTION   5.e-3


/* Exported functions */

int er032m_init_hook( void );
int er032m_test_hook( void );
int er032m_exp_hook( void );
int er032m_end_of_exp_hook( void );
void er032m_exit_hook( void );


Var *magnet_name( Var *v );
Var *magnet_setup( Var *v );
Var *set_field( Var *v );
Var *get_field( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_field( Var *v );



static void er032m_init( void );
static void er032m_field_check( double field );
static void er032m_start_field( void );
static double er032m_get_field( void );
static double er032m_set_field( double field );
static void er032m_change_field_and_set_sw( double field );
static void er032m_change_field_and_sw( double field );
static void er032m_change_field_and_keep_sw( double field );
static bool er032m_guess_sw( double field_diff );
static double er032m_set_cf( double center_field );
static double er032m_get_cf( void );
static double er032m_set_sw( double sweep_width );
static double er032m_get_sw( void );
static int er032m_set_swa( int sweep_address );
#if 0
static int er032m_get_swa( void );        // currently not needed
#endif
static void er032m_test_leds( void );
static void er032m_failure( void );


static struct
{
	int device;

	double act_field;       /* the real current field */

	double cf;              /* the current CF setting */
	double sw;              /* the current SW setting */
	int swa;                /* the current SWA setting */

	bool is_sw;             /* set if SW is known */
	double max_sw;          /* maximum sweep width */

	double swa_step;        /* field step between two SWAs */
	int step_incr;          /* how many SWAs to step for a sweep step */

	double start_field;		/* the start field given by the user */
	double field_step;		/* the field steps to be used */

	bool is_init;			/* flag, set if magnet_setup() has been called */
} magnet;


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

int er032m_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the magnets structure */

	magnet.device = -1;
	magnet.is_init = UNSET;
	magnet.is_sw = UNSET;

	/* Make sure the maximum sweep width isn't larger than the difference
	   between the maximum and minimum field */

	if ( ( magnet.max_sw = ER032M_MAX_SWEEP_WIDTH ) >
		 ER032M_MAX_FIELD - ER032M_MIN_FIELD )
		magnet.max_sw = ER032M_MAX_FIELD - ER032M_MIN_FIELD;

	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int er032m_test_hook( void )
{
	magnet.act_field = magnet.is_init ? magnet.start_field : ER035M_TEST_FIELD;
	return 1;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

int er032m_exp_hook( void )
{
	er032m_init( );
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

int er032m_end_of_exp_hook( void )
{
	if ( magnet.device >= 0 )
		gpib_local( magnet.device );
	return 1;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

void er032m_exit_hook( void )
{
	er032m_end_of_exp_hook( );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *magnet_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------*/
/* Function for registering the start field and the */
/* field step size during the PREPARATIONS section. */ 
/*--------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	double start_field;
	double field_step;


	/* Check that both the variables are reasonable */

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	start_field = get_double( v, "start field" );

	er032m_field_check( start_field );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing field step size.\n" );
		THROW( EXCEPTION );
	}

	field_step = get_double( v, "field step width" );

	if ( fabs( field_step ) < ER032M_MIN_FIELD_STEP )
	{
		print( FATAL, "Field sweep step size (%lf G) too small, minimum is "
			   "%f G.\n", VALUE( v ), ER032M_MIN_FIELD_STEP );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	magnet.start_field = start_field;
	magnet.field_step = field_step;
	magnet.is_init = SET;

	return vars_push( INT_VAR, 1 );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *sweep_up( Var *v )
{
	int steps;


	v = v;

	if ( ! magnet.is_init )
	{
		print( FATAL, "No sweep step size has been set - you must call "
			   "function magnet_setup() to be able to do sweeps.\n" );
		THROW( EXCEPTION );
	}

	er032m_field_check( magnet.act_field + magnet.field_step );

	if ( FSC2_MODE == EXPERIMENT )
	{
		/* If the new field is still within the sweep range set the new field
		   by changing the SWA, otherwise shift the center field up by a
		   quarter of the total sweep width. If this would move the upper
		   sweep limit above the maximum allowed field move the center field
		   only as far as possible. */

		if ( ( magnet.swa += magnet.step_incr ) <= MAX_SWA )
			er032m_set_swa( magnet.swa );
		else
		{
			if ( magnet.cf + 0.75 * SWA_RANGE * magnet.swa_step <
															 ER032M_MAX_FIELD )
				steps = SWA_RANGE / 4;
			else
				steps = irnd( floor( ( ER032M_MAX_FIELD - magnet.cf )
									 / magnet.swa_step ) ) - SWA_RANGE / 2;

			er032m_set_swa( magnet.swa -= steps );
			er032m_set_cf( magnet.cf += steps * magnet.swa_step );
		}

		er032m_test_leds( );
	}

	magnet.act_field += magnet.field_step;
	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *sweep_down( Var *v )
{
	int steps;


	v = v;

	if ( ! magnet.is_init )
	{
		print( FATAL, "No sweep step size has been set - you must call "
			   "function magnet_setup() to be able to do sweeps.\n" );
		THROW( EXCEPTION );
	}

	er032m_field_check( magnet.act_field + magnet.field_step );

	if ( FSC2_MODE == EXPERIMENT )
	{
		/* If the new field is still within the sweep range set the new field
		   by changing the SWA, otherwise shift the center field down by a
		   quarter of the total sweep width. If this would move the lower
		   sweep limit below the minimum allowed field move the center field
		   only as far as possible. */

		if ( ( magnet.swa -= magnet.step_incr ) >= MIN_SWA )
			er032m_set_swa( magnet.swa );
		else
		{
			if ( magnet.cf - 0.75 * SWA_RANGE * magnet.swa_step >
															 ER032M_MIN_FIELD )
				steps = SWA_RANGE / 4;
			else
				steps = irnd( floor( ( magnet.cf - ER032M_MIN_FIELD )
									 / magnet.swa_step ) ) - SWA_RANGE / 2;

			er032m_set_swa( magnet.swa += steps );
			er032m_set_cf( magnet.cf -= steps * magnet.swa_step );
		}

		er032m_test_leds( );
	}

	magnet.act_field -= magnet.field_step;
	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *reset_field( Var *v )
{
	v = v;

	if ( ! magnet.is_init )
	{
		print( FATAL, "Start field has not been defined  - you must call "
			   "function magnet_setup() before.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
		er032m_start_field( );
	else
		magnet.act_field = magnet.start_field;

	return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *get_field( Var *v )
{
	v = v;

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, magnet.act_field );
	return vars_push( FLOAT_VAR, er032m_get_field( ) );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *set_field( Var *v )
{
	double field;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	field = get_double( v, "magnetic field" );

	if ( ( v = vars_pop( v ) ) != NULL )
		print( SEVERE, "Can't use a maximum field error.\n" );

	too_many_arguments( v );

	er032m_field_check( field );

	if ( FSC2_MODE == TEST )
	{
		magnet.act_field = field;
		return vars_push( FLOAT_VAR, magnet.act_field );
	}

	return vars_push( FLOAT_VAR, er032m_set_field( field ) );
}


/*------------------------------*/
/* Initialization of the device */
/*------------------------------*/

static void er032m_init( void )
{
	if ( gpib_init_device( DEVICE_NAME, &magnet.device ) == FAILURE )
	{
		magnet.device = -1;
		print( FATAL, "Initialization of device failed: %s\n",
			   gpib_error_msg );
		THROW( EXCEPTION );
	}

	/* Switch to computer mode */

	if ( gpib_write( magnet.device, "CO\r", 3 ) == FAILURE )
		er032m_failure( );

	/* Enable Hall controller */

	if ( gpib_write( magnet.device, "EC\r", 3 ) == FAILURE )
		er032m_failure( );

	/* Switch to mode 0 */

	if ( gpib_write( magnet.device, "MO0\r", 4 ) == FAILURE )
		er032m_failure( );

	/* Switch off service requests */

	if ( gpib_write( magnet.device, "SR0\r", 4 ) == FAILURE )
		er032m_failure( );

	/* Set IM0 sweep mode (we don't use it, just to make sure we don't
	   trigger a sweep start inadvertently) */

	if ( gpib_write( magnet.device, "IM0\r", 4 ) == FAILURE )
		er032m_failure( );

	/* Check the status of the LEDs */

	er032m_test_leds( );

	/* Set the start field and the field step if magnet_setup() has been
	   called, otherwise measure current field and make CF identical to
	   the current field */

	if ( magnet.is_init )
		er032m_start_field( );
	else
	{
		magnet.act_field = er032m_get_field( );

		magnet.swa = er032m_set_swa( CENTER_SWA );
		magnet.sw = er032m_set_sw( 0.0 );
		magnet.cf = er032m_set_cf( magnet.act_field );

		er032m_test_leds( );
	}
}


/*--------------------------------------------------------------------*/
/* Function sets up the magnet in case magnet_setp() had been called, */
/* assuming that field will be changed via sweep_up() or sweep_down() */
/* calls, in which case the sweep usually can be done via changes of  */
/* the SWA and without changing the center field. The center field is */
/* set to the required start field, so that up and down sweeps can be */
/* done without changing the center field.                            */
/*--------------------------------------------------------------------*/

static void er032m_start_field( void )
{
	int factor;
	int shift;
	double cur_cf;
	double cur_sw;


	magnet.cf = magnet.start_field;
	magnet.sw = SWA_RANGE * magnet.field_step;
	magnet.swa = CENTER_SWA;

	magnet.swa_step = magnet.field_step;
	magnet.step_incr = 1;

	/* Make sure that the sweep width isn't larger than the maximum sweep
	   width otherwise divide it by two as often as needed and remember
	   that have to step not by one SWA but by a power of 2 SWA's */

	if ( magnet.sw > magnet.max_sw )
	{
		factor = irnd( ceil( magnet.sw / magnet.max_sw ) );

		magnet.sw /= factor;
		magnet.swa_step /= factor;
		magnet.step_incr *= factor;
	}

	/* Sweep range must be a multiple of 0.1 G and we adjust it silently
	   to fit this requirement - hopefully, this isn't going to lead to
	   any real field precision problems */

	magnet.sw = SWEEP_RANGE_RESOLUTION *
									lrnd( magnet.sw / SWEEP_RANGE_RESOLUTION );
	magnet.swa_step = magnet.sw / SWA_RANGE;
	magnet.field_step = magnet.step_incr * magnet.swa_step;

	/* Make sure the sweep range is still within the allowed field range,
	   otherwise shift the center field and calculate the SWA that's needed
	   in this case */

	if ( magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD )
	{
		shift = irnd( ceil( ( magnet.cf + 0.5 * magnet.sw
							  - ER032M_MAX_FIELD ) / magnet.swa_step ) );
		magnet.swa += shift;
		magnet.cf -= shift * magnet.swa_step;
	}

	if ( magnet.cf - 0.5 * magnet.sw < ER032M_MIN_FIELD )
	{
		shift = irnd( ceil( ( ER032M_MIN_FIELD
							  - ( magnet.cf - 0.5 * magnet.sw ) )
							/ magnet.swa_step ) );
		magnet.swa -= shift;
		magnet.cf += shift * magnet.swa_step;
	}

	/* Now set the new field but avoid that center field plus sweep range
	   ever exceed the field limits (probably I'm just too paranoid) */

	cur_cf  = er032m_get_cf( );
	cur_sw  = er032m_get_sw( );

	if ( magnet.cf + 0.5 * cur_sw <= ER032M_MAX_FIELD &&
		 magnet.cf - 0.5 * cur_sw >= ER032M_MIN_FIELD )
	{
		er032m_set_cf( magnet.cf );
		er032m_set_sw( magnet.sw );
		er032m_set_swa( magnet.swa );
	}
	else if ( cur_cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD &&
			  cur_cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD )
	{
		er032m_set_sw( magnet.sw );
		er032m_set_cf( magnet.cf );
		er032m_set_swa( magnet.swa );
	}
	else
	{
		er032m_set_sw( 0.5 * d_min( ER032M_MAX_FIELD - 
									d_min( magnet.cf, cur_cf ),
									d_min( magnet.cf, cur_cf )
									- ER032M_MIN_FIELD ) );
		er032m_set_cf( magnet.cf );
		er032m_set_swa( magnet.swa );
		er032m_set_sw( magnet.sw );
	}

	er032m_test_leds( );

	magnet.is_sw = SET;
	magnet.act_field = magnet.start_field;
}


/*---------------------------------------------------------------------*/
/* This function is called when the user asks for setting a new field. */
/* One important point here is to change the center field only if      */
/* absolutely necessary. We have to distinguish several cases:         */
/* 1. There is no sweep step size set and thus the sweep width is set  */
/*    to zero. In this case we try to guess a step size from the       */
/*    difference between the currrent field and the target field. This */
/*    is used to set a sweep width, and if we're lucky (i.e. the user  */
/*    does not require random field changes in the future) we can use  */
/*    the SWAs also for later field changes.                           */
/* 2. If there is a sweep width set but there was no magnet_setup()    */
/*    call we try to set the field using SWAs if one of them fits the  */
/*    the new field value. If not we have to shift the center field,   */
/*    but we first try to keep the change as small as possible.        */
/*    Because our guess about the typical field changes didn't work    */
/*    out we also have to adapt the sweep width.                       */
/* 3. If magnet_setup() has been called changing the sweep width can't */
/*    be done. If possible field change is done via changing the SWA,  */
/*    otherwise a combination of changing the SWA and shifting the     */
/*    center field is used.                                            */
/*---------------------------------------------------------------------*/

static double er032m_set_field( double field )
{
	fsc2_assert( field >= ER032M_MIN_FIELD && field <= ER032M_MAX_FIELD );

	/* We don't try to set a new field when the change is extremely small,
	   i.e. lower than the accuracy for setting a center field */

	if ( fabs( magnet.act_field - field ) < ER032M_RESOLUTION )
		return magnet.act_field;

	if ( ! magnet.is_sw )
		er032m_change_field_and_set_sw( field );
	else
	{
		if ( ! magnet.is_init )
			er032m_change_field_and_sw( field );
		else
			er032m_change_field_and_keep_sw( field );
	}

	er032m_test_leds( );
	return magnet.act_field = field;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void er032m_change_field_and_set_sw( double field )
{
	/* Try to determine a sweep width so that we can set the field without
	   changing the center field, i.e. alone by setting a SWA */

	magnet.is_sw = er032m_guess_sw( fabs( field - magnet.act_field ) );

	/* If this failed we have to change the center field, otherwise
	   we use the newly calculated sweep width */

	if ( ! magnet.is_sw )
		magnet.cf = er032m_set_cf( field );
	else
	{
		magnet.swa += irnd( ( field - magnet.cf ) / magnet.swa_step );

		er032m_set_sw( magnet.sw );
		er032m_set_swa( magnet.swa );

		er032m_test_leds( );
	}
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void er032m_change_field_and_sw( double field )
{
	int steps;


	/* First check if the new field value can be reached (within an accuracy
	   of 1% of the SWA step size) by setting a SWA do just this */

	steps = irnd( ( field - magnet.act_field ) / magnet.swa_step );

	if ( fabs( fabs( field - magnet.act_field )
			   - magnet.swa_step * abs( steps ) ) < 0.01 * magnet.swa_step &&
		 magnet.swa + steps <= MAX_SWA && magnet.swa + steps >= MIN_SWA )
	{
		er032m_set_swa( magnet.swa += steps );
		er032m_test_leds( );
		return;
	}

	/* Try to get a new sweep width so that we can handle the sweep by setting
	   a SWA */

	if ( er032m_guess_sw( fabs( field - magnet.act_field ) ) )
	{
		magnet.swa += irnd( ( field - magnet.cf ) / magnet.swa_step );

		er032m_set_sw( magnet.sw );
		er032m_set_swa( magnet.swa );

		er032m_test_leds( );
		return;
	}

	/* Couldn't figure out a new sweep width to use - if the new field is
	   also not within the current sweep width all we can do is set the
	   new field by the center field alone */

	if ( magnet.swa + steps > MAX_SWA || magnet.swa + steps < MIN_SWA )
	{
		magnet.swa = er032m_set_swa( CENTER_SWA );

		if ( field + 0.5 * magnet.sw > ER032M_MAX_FIELD )
			magnet.sw = er032m_set_sw( 0.5 * ( ER032M_MAX_FIELD - field ) );
		if ( field - 0.5 * magnet.sw < ER032M_MIN_FIELD )
			magnet.sw = er032m_set_sw( 0.5 * ( field - ER032M_MAX_FIELD ) );

		magnet.cf = er032m_set_cf( field );

		er032m_test_leds( );
		return;
	}

	/* New field still within the sweep range: Try to do as much of the field
	   change using SWA and only the rest by shifting the center field. But
	   take care that the sweep limits remain within the maximum and minimum
	   field boundaries. */

	magnet.swa += steps;
	magnet.cf  += field - magnet.act_field - steps * magnet.swa_step;

	if ( magnet.cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD &&
		 magnet.cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD )
	{
		er032m_set_cf( magnet.cf );
		er032m_set_swa( magnet.swa );

		er032m_test_leds( );
		return;
	}

	if ( magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD )
	{
		steps = irnd( ceil( ( magnet.cf + 0.5 * magnet.sw - ER032M_MAX_FIELD )
							/ magnet.swa_step ) );

		er032m_set_cf( magnet.cf -= steps * magnet.swa_step );
		er032m_set_swa( magnet.swa += steps );
	}
	else
	{
		steps = irnd( ceil( ( ER032M_MIN_FIELD - magnet.cf - 0.5 * magnet.sw )
							/ magnet.swa_step ) );

		er032m_set_cf( magnet.cf += steps * magnet.swa_step );
		er032m_set_swa( magnet.swa -= steps );
	}

	er032m_test_leds( );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void er032m_change_field_and_keep_sw( double field )
{
	int steps;


	/* First check if the new field value can be reached (within an accuracy
	   of 1% of the SWA step size) by setting a SWA do just this */

	steps = irnd( ( field - magnet.act_field ) / magnet.swa_step );

	if ( fabs( fabs( field - magnet.act_field )
			   - magnet.swa_step * abs( steps ) ) < 1e-2 * magnet.swa_step &&
		 magnet.swa + steps <= MAX_SWA && magnet.swa + steps >= MIN_SWA )
	{
		er032m_set_swa( magnet.swa += steps );
		er032m_test_leds( );
	}

	magnet.swa += steps;
	magnet.cf  += field - magnet.act_field - steps * magnet.swa_step;

	if ( magnet.cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD &&
		 magnet.cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD )
	{

		er032m_set_cf( magnet.cf );
		er032m_set_swa( magnet.swa );

		er032m_test_leds( );
		return;
	}

	if ( magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD )
	{
		steps = irnd( ( magnet.cf + 0.5 * magnet.sw - ER032M_MAX_FIELD )
					  / magnet.swa_step );

		er032m_set_cf( magnet.cf -= steps * magnet.swa_step );
		er032m_set_swa( magnet.swa += steps );
	}
	else
	{
		steps = irnd( ( ER032M_MIN_FIELD - magnet.cf - 0.5 * magnet.sw )
					  / magnet.swa_step );

		er032m_set_cf( magnet.cf += steps * magnet.swa_step );
		er032m_set_swa( magnet.swa -= steps );
	}

	er032m_test_leds( );
}


/*----------------------------------------------------------------------*/
/* This function tries to guess from the difference between the current */
/* center field and the target field a useful sweep range so that the   */
/* jump can be done by setting a SWA. If it isn't possible to find such */
/* a setting the function returns FAIL, otherwise both the entries sw,  */
/* swa and swa_step in the magnet structure get set and OK is returned. */
/*----------------------------------------------------------------------*/

static bool er032m_guess_sw( double field_diff )
{
	int i;
	double swa_step;
	double sw;
	double factor;



	/* For very small or huge changes we can't deduce a sweep range */

	if ( field_diff * SWA_RANGE < SWEEP_RANGE_RESOLUTION ||
		 field_diff > magnet.max_sw / 2 )
		return FAIL;

	/* This also doesn't work if the center field is nearer to one of the
	   limits than to the new field value */

	if ( ER032M_MAX_FIELD - magnet.cf < field_diff ||
		 magnet.cf - ER032M_MIN_FIELD < field_diff )
		return FAIL;

	swa_step = field_diff;
	sw = SWA_RANGE * field_diff;

	/* The following reduces the sweep width to the maximum possible sweep
	   width and will always happen for field changes above ca. 3.9 G */

	if ( sw > magnet.max_sw )
	{
		factor = irnd( ceil( sw / magnet.max_sw ) );

		sw /= factor;
		swa_step /= factor;
	}

	/* If with this corrected SWA step size the field change can't be achieved 
	   give up */

	if ( swa_step * ( SWA_RANGE / 2 - 1 ) < field_diff )
		return FAIL;

	/* We also can't use a sweep range that exceeds the maximum or minimum
	   field (keeping the center field) thus we must reduce it further as
	   far as necessary (we already tested at the start that this wont lead
	   to a situation were the step size becomes so small that the field
	   difference can't be adjusted anymore by setting a SWA) */

	for ( i = 1; magnet.cf + 0.5 * sw / i > ER032M_MAX_FIELD; i++ )
		;
	sw /= i;
	swa_step /= i;

	for ( i = 1; magnet.cf - 0.5 * sw / i < ER032M_MAX_FIELD; i++ )
		;
	sw /= i;
	swa_step /= i;

	fsc2_assert( swa_step * ( SWA_RANGE / 2 - 1 ) >= field_diff );

	/* Make sweep range an integer multiple of the SWEEP_RANGE_RESOLUTION */

	magnet.sw = SWEEP_RANGE_RESOLUTION * lrnd( sw / SWEEP_RANGE_RESOLUTION );
	magnet.swa_step = magnet.sw / SWA_RANGE;
	magnet.swa = CENTER_SWA;

	return OK;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

static void er032m_field_check( double field )
{
	if ( field > ER032M_MAX_FIELD )
	{
		print( FATAL, "Field of %f G is too high, maximum field is %f G.\n",
			   field, ER032M_MAX_FIELD );
		THROW( EXCEPTION );
	}

	if ( field < ER032M_MIN_FIELD )
	{
		print( FATAL, "Field of %f G is too low, minimum field is %f G.\n",
			   field, ER032M_MIN_FIELD );
		THROW( EXCEPTION );
	}
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

static void er032m_test_leds( void )
{
	char buf[ 20 ];
	long length;
	char *bp;
	int max_retries = ER032M_MAX_RETRIES;
	bool is_overload;
	bool is_remote;


	while ( 1 )
	{
		stop_on_user_request( );

		is_overload = is_remote = UNSET;

		length = 20;

		if ( gpib_write( magnet.device, "LE\r", 3 ) == FAILURE ||
			 gpib_read( magnet.device, buf, &length ) == FAILURE )
			er032m_failure( );

		bp = buf;
		while ( *bp && *bp != '\r' )
		{
			switch ( *bp )
			{
				case '1' :
					is_overload = SET;
					break;

				case '2' :
					print( FATAL, "Probehead thermostat not in "
						   "equilibrilum.\n" );
					THROW( EXCEPTION );
					break;

				case '4' :
					is_remote = SET;
					break;

				default :
					break;
			}

			bp++;
		}

		/* If remote LED isn't on we're out of luck... */

		if ( ! is_remote )
		{
			print( FATAL, "Device isn't in remote state.\n" );
			THROW( EXCEPTION );
		}

		/* If there's no overload we're done, otherwise we retry several
		   times before giving up */

		if ( ! is_overload )
			break;

		if ( max_retries-- > 0  )
			usleep( ER032M_WAIT_TIME );
		else
		{
			print( FATAL, "Field regulation loop not balanced.\n" );
			THROW( EXCEPTION );
		}
	}
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static double er032m_get_field( void )
{
	char buf[ 30 ];
	long length = 30;


	if ( gpib_write( magnet.device, "FC\r", 3 ) == FAILURE ||
		 gpib_read( magnet.device, buf, &length ) == FAILURE )
		er032m_failure( );

	buf[ length ] = '\0';
	return T_atod( buf + 2 );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static double er032m_set_cf( double center_field )
{
	char buf[ 30 ];
	int i;


	fsc2_assert( center_field >= ER032M_MIN_FIELD &&
				 center_field <= ER032M_MAX_FIELD );

	sprintf( buf, "CF%.3f\r", center_field );

	for ( i = ER032M_MAX_SET_RETRIES; i > 0; i-- )
	{
		if ( gpib_write( magnet.device, buf, strlen( buf ) ) == FAILURE )
			er032m_failure( );

		if ( fabs( center_field - er032m_get_cf( ) ) <= 1.0e-3 )
			break;
	}

	if ( i == 0 )
	{
		print( FATAL, "Failed to set center field.\n" );
		THROW( EXCEPTION );
	}

	return center_field;
}	


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static double er032m_get_cf( void )
{
	char buf[ 30 ];
	long len = 30;


	if ( gpib_write( magnet.device, "CF\r", 3 ) == FAILURE ||
		 gpib_read( magnet.device, buf, &len ) == FAILURE )
		er032m_failure( );

	buf[ len ] = '\0';
	return T_atod( buf + 2 );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static double er032m_set_sw( double sweep_width )
{
	char buf[ 30 ];
	int i;


	fsc2_assert( sweep_width >= 0.0 && sweep_width <= magnet.max_sw );

	sprintf( buf, "SW%.1f\r", sweep_width );

	for ( i = ER032M_MAX_SET_RETRIES; i > 0; i-- )
	{
		if ( gpib_write( magnet.device, buf, strlen( buf ) ) == FAILURE )
			er032m_failure( );

		if ( fabs( sweep_width - er032m_get_sw( ) ) <= 1.0e-3 )
			break;
	}

	if ( i == 0 )
	{
		print( FATAL, "Failed to set sweep width.\n" );
		THROW( EXCEPTION );
	}

	return sweep_width;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static double er032m_get_sw( void )
{
	char buf[ 30 ];
	long len = 30;


	if ( gpib_write( magnet.device, "SW\r", 3 ) == FAILURE ||
		 gpib_read( magnet.device, buf, &len ) == FAILURE )
		er032m_failure( );

	buf[ len ] = '\0';
	return T_atod( buf + 2 );
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static int er032m_set_swa( int sweep_address )
{
	char buf[ 30 ];


	fsc2_assert( sweep_address >= MIN_SWA && sweep_address <= MAX_SWA );

	sprintf( buf, "SS%d\r", sweep_address );
	if ( gpib_write( magnet.device, buf, strlen( buf ) ) == FAILURE )
		er032m_failure( );

	return sweep_address;
}


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

#if 0
static int er032m_get_swa( void )
{
	char buf[ 30 ];
	long len = 30;


	if ( gpib_write( magnet.device, "SA\r", 3 ) == FAILURE ||
		 gpib_read( magnet.device, buf, &len ) == FAILURE )
		er032m_failure( );

	buf[ len ] = '\0';
	return T_atoi( buf + 2 );
}
#endif


/*-------------------------------------------------------*/
/*-------------------------------------------------------*/

static void er032m_failure( void )
{
	print( FATAL, "Can't access the field controller.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
