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

#include "hjs_fc.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Exported functions */

int hjs_fc_init_hook( void );
int hjs_fc_test_hook( void );
int hjs_fc_exp_hook( void );
void hjs_fc_exit_hook( void );
void hjs_fc_child_exit_hook( void );

Var *magnet_name( Var *v );
Var *magnet_setup( Var *v );
Var *set_field( Var *v );
Var *get_field( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_field( Var *v );


/* Local functions */

static void hjs_fc_init( void );
static double hjs_fc_set_field( double field, double error_margin );
static double hjs_fc_sweep_to( double new_field );
static double hjs_fc_get_field( void );
static double hjs_fc_field_check( double field );
static void hjs_fc_set_dac( double volts );


static struct HJS_FC {
	double B0V;             /* Field for DAC voltage of 0 V */
	double slope;           /* field step for 1 V DAC voltage increment */

	double min_test_field;  /* minimum and maximum field during test run */
	double max_test_field;

	double B_max;           /* maximum and minimum field the magnet can */
	double B_min;           /* produce */

	double field;			/* the start field given by the user */
	double field_step;		/* the field steps to be used for sweeps */

	bool is_field;			/* flag, set if start field is defined */
	bool is_field_step;		/* flag, set if field step size is defined */

	char *dac_func;
	char *gm_gf_func;
	char *gm_res_func;

	double cur_volts;       /* current voltage at the DAC output */
	double act_field;		/* used internally */
	double target_field;
} hjs_fc;


/* Maximum mumber of times we retry to get a consistent reading (i.e. the
   same value for MIN_NUM_IDENTICAL_READINGS times) from the gaussmeter */

#define MAX_GET_FIELD_RETRIES          500


/* Number of identical readings from the gaussmeter that we take to indicate
   that the the field is stable */

#define MIN_NUM_IDENTICAL_READINGS      3


/* Time (in microseconds) that we'll wait between fetching new values
   from the gaussmeter when trying to get a consistent reading */

#define WAIT_LENGTH         100000UL    /* 100 ms */


#define DAC_MAX_VOLTAGE     10.0
#define DAC_MIN_VOLTAGE      0.0
#define DAC_RESOLUTION      ( ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE ) / 4095.0 )

/* Field value that will be returned during a test run */

#define HJS_FC_TEST_FIELD      3300.0

#define HJS_FC_TEST_B0V        2900.0
#define HJS_FC_TEST_SLOPE        50.0


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*-------------------------------------------------------------------*/
/* Function that gets called when the module is loaded. Since this   */
/* module does not control a real device but relies on the existence */
/* of both the BNM12 gaussmeter and the home-built DA/AD converter   */
/* the main purpose of this function is to check that the modules    */
/* for these devices are already loaded and then to get a lock on    */
/* the DA-channel of the DA/AD converter. Of course, also some       */
/* internally used data are initialized by the function.             */
/*-------------------------------------------------------------------*/

int hjs_fc_init_hook( void )
{
	Var *Func_ptr;
	int acc;
	Var *v;
	int dev_num;
	char *func = NULL;


	CLOBBER_PROTECT( func );

	/* Set the default values for the structure for the device */

	hjs_fc.B0V = HJS_FC_TEST_B0V;
	hjs_fc.slope = HJS_FC_TEST_SLOPE;

	hjs_fc.min_test_field = HUGE_VAL;
	hjs_fc.max_test_field = - HUGE_VAL;

	hjs_fc.is_field = UNSET;
	hjs_fc.is_field_step = UNSET;

	hjs_fc.dac_func = NULL;
	hjs_fc.gm_gf_func = NULL;
	hjs_fc.gm_res_func = NULL;

	TRY
	{
		/* Check that the module for the BNM12 gaussmeter is loaded */

		if ( ( dev_num = exists_device( "bnm12" ) ) == 0 )
		{
			print( FATAL, "Can't find the module for the BNM12 gaussmeter "
				   "(bnm12) - it must be listed before this module.\n" );
			THROW( EXCEPTION );
		}

		if ( dev_num == 1 )
			hjs_fc.gm_gf_func = T_strdup( "gaussmeter_field" );
		else
			hjs_fc.gm_gf_func = get_string( "gaussmeter_field#%ld", dev_num );

		if ( ! func_exists( hjs_fc.gm_gf_func ) )
		{
			print( FATAL, "Function for getting the field from the gaussmeter "
				   "is missing.\n" );
			THROW( EXCEPTION );
		}

		/* Check that we can ask it for the field and its resolution. */

		if ( dev_num == 1 )
			hjs_fc.gm_res_func = T_strdup( "gaussmeter_resolution" );
		else
			hjs_fc.gm_res_func = get_string( "gaussmeter_resolution#%ld",
											 dev_num );
		if ( ! func_exists( hjs_fc.gm_res_func ) )
		{
			print( FATAL, "Function for determining the field resolution of "
				   "the gaussmeter is missing.\n" );
			THROW( EXCEPTION );
		}

		/* Check that the module for the DA/AD converter is loaded.*/

		if ( ( dev_num = exists_device( "hjs_daadc" ) ) == 0 )
		{
			print( FATAL, "Can't find the module for the DA/AD converter "
				   "(hjs_daadc) - it must be listed before this module.\n" );
			THROW( EXCEPTION );
		}

		/* Get a lock on the DAC */

		func = get_string( "daq_reserve_dac#%d", dev_num );

		if ( ! func_exists( func ) ||
			 ( Func_ptr = func_get( func, &acc ) ) == NULL )
		{
			print( FATAL, "Function for reserving the DAC is missing.\n" );
			THROW( EXCEPTION );
		}

		func = CHAR_P T_free( func );
		vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

		v = func_call( Func_ptr );

		if ( v->val.lval != 1 )
		{
			print( FATAL, "Can't reserve the DAC.\n" );
			THROW( EXCEPTION );
		}

		vars_pop( v );

		/* Get the name for the function for setting a value at the DAC and
		   check if it exists */

		if ( dev_num == 1 )
			hjs_fc.dac_func = T_strdup( "daq_set_voltage" );
		else
			hjs_fc.dac_func = get_string( "daq_set_voltage#%ld", dev_num );

		if ( ! func_exists( hjs_fc.dac_func ) )
		{
			print( FATAL, "Function for setting the DAC is missing.\n" );
			THROW( EXCEPTION );
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( func )
			T_free( func );
		if ( hjs_fc.gm_gf_func )
			hjs_fc.gm_gf_func = CHAR_P T_free( hjs_fc.gm_gf_func );
		if ( hjs_fc.gm_res_func )
			hjs_fc.gm_res_func = CHAR_P T_free( hjs_fc.gm_res_func );
		if ( hjs_fc.dac_func )
			hjs_fc.dac_func = CHAR_P T_free( hjs_fc.dac_func );
		RETHROW( );
	}

	return 1;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

int hjs_fc_test_hook( void )
{
	if ( hjs_fc.is_field )
		hjs_fc.target_field = hjs_fc.act_field = hjs_fc.field;
	else
		hjs_fc.target_field = hjs_fc.act_field = HJS_FC_TEST_FIELD;

	return 1;
}


/*---------------------------------------------------------------*/
/* Function gets called when the experiment is started. First of */
/* all we need to do some initialization. This mainly includes   */
/* setting the field to some positions, covering the whole range */
/* of possible values, to figure out the maximum field range and */
/* the relationship between the DAC output voltage and the       */
/* resulting field. We also try to get an estimate on how long   */
/* it takes to reach a field value in the process. When we then  */
/* know the field range we have to check that during the test    */
/* run the field never was set to a value that can't be reached. */
/* If the function magnet_setup() had been called during the     */
/* PREPARATIONS section we also have to make sure that the sweep */
/* step size isn't smaller than the minimum field step we can    */
/* set with the DAC resolution.                                  */
/*---------------------------------------------------------------*/

int hjs_fc_exp_hook( void )
{
	/* Try to figure out what the field for the minimum and maximum DAC
	   output voltage is and how (and how fast) the field changes with
	   the DAC output voltage */

	TRY
	{
		hjs_fc_init( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		hjs_fc_child_exit_hook( );
		RETHROW( );
	}

	/* If we found in the test run that the requested field is going to
	   be not within the limits of the field we can set give up */

	if ( hjs_fc.max_test_field > hjs_fc.min_test_field )
	{
		if ( hjs_fc.max_test_field > hjs_fc.B_max )
		{
			print( FATAL, "During test run a field of %.2lf G was requested "
				   "but the maximum field is %.2lf G.\n",
				   hjs_fc.max_test_field, hjs_fc.B_max );
			THROW( EXCEPTION );
		}

		if ( hjs_fc.min_test_field < hjs_fc.B_min )
		{
			print( FATAL, "During test run a field of %.2lf G was requested "
				   "but the minimum field is %.2lf G.\n",
				   hjs_fc.min_test_field, hjs_fc.B_min );
			THROW( EXCEPTION );
		}
	}

	/* Check that the field step size (if it has been given) isn't that
	   small that it can't be produced with the voltage resolution of
	   the DAC. */

	if ( hjs_fc.is_field_step &&
		 fabs( hjs_fc.field_step / hjs_fc.slope ) < DAC_RESOLUTION )
	{
		print( FATAL, "Field step size for sweeps is too small, the minimum "
			   "possible step size is %.2lf G.\n",
			   DAC_RESOLUTION * hjs_fc.slope );
		THROW( EXCEPTION );
	}

	/* If a start field had been defined by a call of magnet_setup() set
	   it now. */

	if ( hjs_fc.is_field )
	{
		hjs_fc.act_field = hjs_fc_set_field( hjs_fc.field, 0.0 );
		hjs_fc.target_field = hjs_fc.field;
	}

	return 1;
}


/*--------------------------------------------------------------*/
/* Function that is called just before the module gets unloaded */
/*--------------------------------------------------------------*/

void hjs_fc_exit_hook( void )
{
	if ( hjs_fc.dac_func )
		T_free( hjs_fc.dac_func );
	if ( hjs_fc.gm_gf_func )
		T_free( hjs_fc.gm_gf_func );
	if ( hjs_fc.gm_res_func )
		T_free( hjs_fc.gm_res_func );
}


/*--------------------------------------------------------------*/
/* Function gets executed just before the child process commits */
/* suicide to bring the field back to the value it has for a    */
/* DAC output voltage of 0 V.                                   */
/*--------------------------------------------------------------*/

void hjs_fc_child_exit_hook( void )
{
	double cur_volts;
	double mini_step;


	if ( ( cur_volts = hjs_fc.cur_volts ) == 0.0 )
		return;
/*
	mini_step = 0.1 * MAX_SWEEP_SPEED * ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE )
				/ ( hjs_fc.B_max - hjs_fc.B_min );
*/
	mini_step = ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE ) / 4095.0;

	/* If the constants for the field at a DAC voltage of 0 V and for
	   the change of the field with the DAC voltage as measured during
	   intitalization are correct we should arrive at the target field
	   with just one try. But if this isn't the case we retry by
	   adjusting the output voltage according to the measured difference
	   from the target field (at least as the deviation is larger than
	   'error_margin'). */

	while ( cur_volts > 0.0 )
	{
		if ( ( cur_volts -= mini_step ) < 0.0 )
			cur_volts = 0.0;

		hjs_fc_set_dac( cur_volts );

		hjs_fc.cur_volts = 0.0;

		fsc2_usleep( 20000, UNSET );
	}
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
	hjs_fc.field = get_double( v, "magnetic field" );
	hjs_fc.field_step = get_double( v->next, "field step width" );

	hjs_fc.min_test_field = hjs_fc.max_test_field = hjs_fc.field;

	hjs_fc.is_field = hjs_fc.is_field_step = SET;
	return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *set_field( Var *v )
{
	double field;
	double error_margin = 0.0;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument(s).\n" );
		THROW( EXCEPTION );
	}

	field = get_double( v, "field" );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		error_margin = get_double( v, "field precision" );
		too_many_arguments( v );

		if ( error_margin < 0.0 )
		{
			print( FATAL, "Invalid negative field precision.\n ");
			THROW( EXCEPTION );
		}

		if ( error_margin > 0.1 * fabs( field ) )
			print( SEVERE, "Field precision is larger than 10% of field "
				   "value.\n" );
	}

	if ( FSC2_MODE == TEST )
		hjs_fc.act_field = hjs_fc.target_field = field;
	else
	{
		hjs_fc.target_field = hjs_fc_field_check( field );
		hjs_fc.act_field = hjs_fc_set_field( hjs_fc.target_field,
											 error_margin );
	}

	return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*---------------------------------------------------------*/
/* Function asks the used gaussmeter for the current field */
/*---------------------------------------------------------*/

Var *get_field( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( FSC2_MODE != TEST )
		hjs_fc.act_field = hjs_fc_get_field( );

	return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

Var *sweep_up( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! hjs_fc.is_field_step )
	{
		print( FATAL, "Sweep step size has not been defined.\n" );
		THROW( EXCEPTION );
	}

	hjs_fc.target_field += hjs_fc.field_step;
	hjs_fc.act_field = hjs_fc_sweep_to( hjs_fc.target_field );

	return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

Var *sweep_down( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! hjs_fc.is_field_step )
	{
		print( FATAL, "Sweep step size has not been defined.\n" );
		THROW( EXCEPTION );
	}

	hjs_fc.target_field -= hjs_fc.field_step;
	hjs_fc.act_field = hjs_fc_sweep_to( hjs_fc.target_field );

	return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

Var *reset_field( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! hjs_fc.is_field )
	{
		print( FATAL, "Start field has not been defined.\n" );
		THROW( EXCEPTION );
	}

	hjs_fc.act_field = hjs_fc_set_field( hjs_fc.field, 0.0 );
	hjs_fc.target_field = hjs_fc.field;	

	return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*--------------------------------------------------------------------*/
/* In the initialization process we need to figure out the field for  */
/* an output voltage of the DAC of 0 V and how the field changes with */
/* the DAC voltage.                                                   */
/*--------------------------------------------------------------------*/

static void hjs_fc_init( void )
{
	size_t i;
	double test_volts[ ] = { DAC_MIN_VOLTAGE, 1.0, 2.0, 7.5, DAC_MAX_VOLTAGE };
	size_t num_test_data = 	sizeof test_volts / sizeof test_volts[ 0 ];
	double test_gauss[ num_test_data + 1 ];
	double cur_volts;
	double mini_step;


	/* We can be sure that the output of the DAC is set to 0 V */

	hjs_fc.B0V = test_gauss[ 0 ] = hjs_fc_get_field( );

	cur_volts = DAC_MIN_VOLTAGE;
/*
	mini_step = 0.05 * MAX_SWEEP_SPEED * ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE )
				/ FIELD_RANGE_GUESS;
*/
	mini_step = ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE ) / 4095.0;

	/* For the test voltages set in 'test_volts' get the magnetic field */

	for ( i = 1; i < num_test_data; i++ )
	{
		if ( test_volts[ i ] < DAC_MIN_VOLTAGE ||
			 test_volts[ i ] > DAC_MAX_VOLTAGE ||
			 ( i > 0 && fabs( test_volts[ i - 1 ] - test_volts[ i ] )
			   			< DAC_RESOLUTION ) )
		{
			print( FATAL, "Internal error detected at %s:%d.\n",
				   __FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		while ( cur_volts < test_volts[ i ] )
		{
			cur_volts += mini_step;
			if ( cur_volts > test_volts[ i ] )
				cur_volts = test_volts[ i ];

			hjs_fc_set_dac( cur_volts );

			hjs_fc.cur_volts = cur_volts;

			fsc2_usleep( 20000, SET );
			stop_on_user_request( );
		}

		/* Get a consistent reading and finally calculate how long it took  */

		fsc2_usleep( 1000000L, SET );
		stop_on_user_request( );

		test_gauss[ i ] = hjs_fc_get_field( );
	}

#if 0
	hjs_fc.slope = 0.0;

	for ( i = 1; i < num_test_data; i++ )
	{
		if ( test_gauss[ i ] == hjs_fc.B0V )
		{
			print( FATAL, "Tests of field sweep indicate that the field can't "
			   "be swept at all.\n" );
			THROW( EXCEPTION );
		}

		hjs_fc.slope += ( test_gauss[ i ] - hjs_fc.B0V ) /
						( test_volts[ i ] - test_volts[ 0 ] );
	}

	hjs_fc.slope /= num_test_data - 1;
#endif

	hjs_fc.slope = ( test_gauss[ num_test_data - 1 ] - hjs_fc.B0V ) /
				   ( test_volts[ num_test_data - 1 ] - test_volts[ 0 ] );

	if ( hjs_fc.slope == 0.0 )         /* this should be impossible... */
	{
		print( FATAL, "Tests of field sweep indicate that the field can't be "
			   "swept at all.\n" );
		print( FATAL, "Tests of field sweep failed, field did not change.\n" );
		THROW( EXCEPTION );
	}

	/* Check that we at least have a field resolution of 0.1 G with the
	   value we got from our tests */

	if ( DAC_RESOLUTION / hjs_fc.slope > 0.1 )
	{
		print( FATAL, "Minimum field resolution is higher than 0.1 G.\n" );
		THROW( EXCEPTION );
	}

	if ( hjs_fc.slope >= 0.0 )
	{
		hjs_fc.B_min = hjs_fc.B0V;
		hjs_fc.B_max = test_gauss[ num_test_data - 1 ];
	}
	else
	{
		hjs_fc.B_max = hjs_fc.B0V;
		hjs_fc.B_min = test_gauss[ num_test_data - 1 ];
	}

	hjs_fc.act_field = test_gauss[ num_test_data - 1 ];
}


/*---------------------------------------------------------------*/
/* Function tries to set a new field and checks if the field was */
/* reached by measuring the new field via the BNM12 gaussmeter.  */
/*---------------------------------------------------------------*/

static double hjs_fc_set_field( double field, double error_margin )
{
	double v_step = 0.0;
	double cur_field = hjs_fc.B0V;
	double cur_volts;
	double mini_step;


	if ( error_margin < 0.2 )
		error_margin = 0.2;
/*
	mini_step = 0.1 * MAX_SWEEP_SPEED * ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE )
				/ ( hjs_fc.B_max - hjs_fc.B_min );
*/
	mini_step = ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE ) / 4095.0;

	/* If the constants for the field at a DAC voltage of 0 V and for
	   the change of the field with the DAC voltage as measured during
	   intitalization are correct we should arrive at the target field
	   with just one try. But if this isn't the case we retry by
	   adjusting the output voltage according to the measured difference
	   from the target field (at least as the deviation is larger than
	   'error_margin'). */

	do
	{
		v_step += ( field - cur_field ) / hjs_fc.slope;
		cur_volts = hjs_fc.cur_volts;

		if ( v_step > cur_volts )
			mini_step = fabs( mini_step );
		else
			mini_step = - fabs( mini_step );

		while ( ( mini_step > 0.0 && cur_volts < v_step ) ||
				( mini_step < 0.0  && cur_volts > v_step ) )
		{
			cur_volts += mini_step;

			if ( ( mini_step > 0.0 && cur_volts > v_step ) ||
				 ( mini_step < 0.0 && cur_volts < v_step ) )
				cur_volts = v_step;

			hjs_fc_set_dac( cur_volts );
			hjs_fc.cur_volts = cur_volts;

			if ( FSC2_MODE == TEST )
			{
				hjs_fc.cur_volts = v_step;
				break;
			}

			fsc2_usleep( 20000, UNSET );
			stop_on_user_request( );
		}

		cur_field = hjs_fc_get_field( );

		if ( FSC2_MODE == TEST )
			cur_field = field;

	} while ( lrnd( 10.0 * fabs( cur_field - field ) ) >
			  lrnd( 10.0 * error_margin ) );

	return cur_field;
}


/*-------------------------------------------------------------------*/
/* Function for sweeping the field, i.e. just setting a new value of */
/* the DAC output voltage but without checking the field using the   */
/* gaussmeter.                                                       */
/*-------------------------------------------------------------------*/

static double hjs_fc_sweep_to( double new_field )
{
	double v_step;
	double cur_volts;
	double mini_step;


	if ( FSC2_MODE == TEST )
		return new_field;

/*
	mini_step = 0.1 * MAX_SWEEP_SPEED * ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE )
				/ ( hjs_fc.B_max - hjs_fc.B_min );
*/

	mini_step = ( DAC_MAX_VOLTAGE - DAC_MIN_VOLTAGE ) / 4095.0;

	/* Set the DAC voltage for the new field */

	cur_volts = hjs_fc.cur_volts;
	v_step = cur_volts + ( new_field - hjs_fc.act_field ) / hjs_fc.slope;

	if ( v_step > cur_volts )
		mini_step = fabs( mini_step );
	else
		mini_step = - fabs( mini_step );

	while ( ( mini_step > 0.0 && cur_volts < v_step ) ||
			( mini_step < 0.0 && cur_volts > v_step ) )
	{
		cur_volts += mini_step;

		if ( ( mini_step > 0.0 && cur_volts > v_step ) ||
			 ( mini_step < 0.0 && cur_volts < v_step ) )
			cur_volts = v_step;

		hjs_fc_set_dac( cur_volts );

		hjs_fc.cur_volts = cur_volts;

		fsc2_usleep( 20000, UNSET );
		stop_on_user_request( );
	}

	return new_field;
}


/*-------------------------------------------------------------------*/
/* Function tries to get a consistent reading from the gaussmeter by */
/* repeatedly fetching new values until it stays unchanged for at    */
/* least MIN_NUM_IDENTICAL_READINGS times. If the readings stay      */
/* unstable for more than MAX_GET_FIELD_RETRIES times the function   */
/* throws an exception. Between fetching new values from the gauss-  */
/* meter the function pauses for WAIT_LENGTH microseconds. Each of   */
/* the three constants need to be defined at the top of this file.   */
/*-------------------------------------------------------------------*/

static double hjs_fc_get_field( void )
{
	Var *v;
	int acc;
	long cur_field;
	long old_field;
	int repeat_count;
	int ident_count;


	/* Repeatedly read the field until we get a consistent reading (i.e.
	   the same field value for MIN_NUM_IDENTICAL_READINGS times) or we
	   have to decide that we can't get one... */

	for ( repeat_count = 0, ident_count = 0, old_field = LONG_MIN;
		  ident_count < MIN_NUM_IDENTICAL_READINGS &&
		  repeat_count < MAX_GET_FIELD_RETRIES; repeat_count++ )
	{
		if ( repeat_count )
			fsc2_usleep( WAIT_LENGTH, UNSET );

		stop_on_user_request( );

		v = func_call( func_get( hjs_fc.gm_gf_func, &acc ) );
		cur_field = lrnd( 10.0 * v->val.dval );
		vars_pop( v );

		if ( cur_field != old_field )
		{
			old_field = cur_field;
			ident_count = 0;
		}
		else
			ident_count++;
	}

	if ( repeat_count >= MAX_GET_FIELD_RETRIES )
	{
		print( FATAL, "Impossible to get a consistent field reading.\n" );
		THROW( EXCEPTION );
	}

	return 0.1 * cur_field;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

static double hjs_fc_field_check( double field )
{
	if ( FSC2_MODE == EXPERIMENT )
	{
		hjs_fc.max_test_field = d_max( hjs_fc.max_test_field, field );
		hjs_fc.min_test_field = d_min( hjs_fc.min_test_field, field );
		return field;
	}

	if ( field > hjs_fc.B_max )
	{
		print( SEVERE, "Field of %.2lf G is too high, using maximum field of "
			   "%.2lf G.\n", field, hjs_fc.B_max );
		field = hjs_fc.B_max;
	}
	else if ( field < hjs_fc.B_min )
	{
		print( SEVERE, "Field of %.2lf G is too low, using minimum field of "
			   "%.2lf G.\n", field, hjs_fc.B_min );
		field = hjs_fc.B_min;
	}

	return field;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

static void hjs_fc_set_dac( double volts )
{
	Var *Func_ptr;
	int acc;


	if ( ( Func_ptr = func_get( hjs_fc.dac_func, &acc ) ) == NULL )
	{
		print( FATAL, "Internal error detected at %s:%d.\n",
			   __FILE__, __LINE__ );
		THROW( EXCEPTION );
	}

	vars_push( STR_VAR, DEVICE_NAME );       /* push the pass-phrase */
	vars_push( FLOAT_VAR, volts );
	vars_pop( func_call( Func_ptr ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
