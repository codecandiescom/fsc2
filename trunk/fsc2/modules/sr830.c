/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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

#include "sr830.conf"

const char generic_type[ ] = DEVICE_TYPE;


/* Here values are defined that get returned by the driver in the test run
   when the lock-in can't be accessed - these values must really be
   reasonable ! */

#define SR830_TEST_ADC_VOLTAGE   0.0
#define SR830_TEST_SENSITIVITY   0.5
#define SR830_TEST_TIME_CONSTANT 0.1
#define SR830_TEST_PHASE         0.0
#define SR830_TEST_MOD_FREQUENCY 5.0e3
#define SR830_TEST_MOD_LEVEL     1.0
#define SR830_TEST_HARMONIC      1
#define SR830_TEST_MOD_MODE      1         // this must be INTERNAL, i.e. 1

#define NUM_ADC_PORTS         4
#define NUM_DAC_PORTS         4
#define DAC_MAX_VOLTAGE       10.5
#define DAC_MIN_VOLTAGE      -10.5

#define MAX_MOD_FREQ          102000.0
#define MIN_MOD_FREQ          0.001

#define MOD_MODE_INTERNAL     1
#define MOD_MODE_EXTERNAL     0

#define MAX_MOD_LEVEL         5.0
#define MIN_MOD_LEVEL         4.0e-3

#define MAX_HARMONIC          19999
#define MIN_HARMONIC          1

#define NUM_CHANNELS          4        /* 11 would be possible but this doesn't
										  make too much sense... */


/* Declaration of exported functions */

int sr830_init_hook( void );
int sr830_test_hook( void );
int sr830_exp_hook( void );
int sr830_end_of_exp_hook( void );
void sr830_exit_hook( void );

Var *lockin_name( Var *v );
Var *lockin_get_data( Var *v );
Var *lockin_get_adc_data( Var *v );
Var *lockin_dac_voltage( Var *v );
Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );
Var *lockin_ref_freq( Var *v );
Var *lockin_harmonic( Var *v );
Var *lockin_ref_mode( Var *v );
Var *lockin_ref_level( Var *v );
Var *lockin_lock_keyboard( Var *v );


/* Exported symbols (used by W-band power supply driver) */

int first_DAC_port = 1;
int last_DAC_port = 4;


/* Typedefs and global variables used only in this file */

typedef struct
{
	int device;
	int sens_index;
	bool sens_warn;
	double phase;
	bool is_phase;
	int tc_index;
	bool tc_warn;
	double mod_freq;
	bool is_mod_freq;
	double mod_level;
	bool is_mod_level;
	long harmonic;
	bool is_harmonic;
	double dac_voltage[ NUM_DAC_PORTS ];
} SR830;


static SR830 sr830;
static SR830 sr830_stored;


#define UNDEF_SENS_INDEX -1
#define UNDEF_TC_INDEX   -1

/* Lists of valid sensitivity settings */

static double sens_list[ ] = { 2.0e-9, 5.0e-9, 1.0e-8, 2.0e-8, 5.0e-8, 1.0e-7,
							   2.0e-7, 5.0e-7, 1.0e-6, 2.0e-6, 5.0e-6, 1.0e-5,
							   2.0e-5, 5.0e-5, 1.0e-4, 2.0e-4, 5.0e-4, 1.0e-3,
							   2.0e-3, 5.0e-3, 1.0e-2, 2.0e-2, 5.0e-2, 1.0e-1,
							   2.0e-1, 5.0e-1, 1.0 };

/* List of all available time constants */

static double tc_list[ ] = { 1.0e-5, 3.0e-5, 1.0e-4, 3.0e-4, 1.0e-3, 3.0e-3,
							 1.0e-2, 3.0e-2, 1.0e-1, 3.0e-1, 1.0, 3.0, 1.0e1,
							 3.0e1, 1.0e2, 3.0e2, 1.0e3, 3.0e3, 1.0e4, 3.0e4 };


#define SENS_ENTRIES ( sizeof sens_list / sizeof sens_list[ 0 ] )
#define TC_ENTRIES ( sizeof tc_list / sizeof tc_list[ 0 ] )


/* Declaration of all functions used only within this file */

static bool sr830_init( const char *name );
static double sr830_get_data( void );
static void sr830_get_xy_data( double *data, long *channels,
							   int num_channels );
static double sr830_get_adc_data( long channel );
static double sr830_set_dac_data( long channel, double voltage );
static double sr830_get_sens( void );
static void sr830_set_sens( int sens_index );
static double sr830_get_tc( void );
static void sr830_set_tc( int tc_index );
static double sr830_get_phase( void );
static double sr830_set_phase( double phase );
static double sr830_get_mod_freq( void );
static double sr830_set_mod_freq( double freq );
static long sr830_get_mod_mode( void );
static long sr830_get_harmonic( void );
static long sr830_set_harmonic( long harmonic );
static double sr830_get_mod_level( void );
static double sr830_set_mod_level( double level );
static void sr830_lock_state( bool lock );
static void sr830_failure( void );



/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int sr830_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	sr830.device = -1;

	sr830.sens_index   = UNDEF_SENS_INDEX;
	sr830.sens_warn    = UNSET;
	sr830.is_phase     = UNSET;
	sr830.tc_index     = UNDEF_TC_INDEX;
	sr830.tc_warn      = UNSET;
	sr830.is_mod_freq  = UNSET;
	sr830.is_mod_level = UNSET;
	sr830.is_harmonic  = UNSET;

	for ( i = 0; i < NUM_DAC_PORTS; i++ )
		sr830.dac_voltage[ i ] = 0.0;

	return 1;
}


/*-----------------------------------*/
/* Test hook function for the module */
/*-----------------------------------*/

int sr830_test_hook( void )
{
	memcpy( &sr830_stored, &sr830, sizeof( SR830 ) );
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int sr830_exp_hook( void )
{
	/* Reset the device structure to the state it had before the test run */

	memcpy( &sr830, &sr830_stored, sizeof( SR830 ) );

	if ( ! sr830_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION )
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int sr830_end_of_exp_hook( void )
{
	/* Switch lock-in back to local mode */

	if ( sr830.device >= 0 )
		gpib_local( sr830.device );

	sr830.device = -1;

	return 1;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void sr830_exit_hook( void )
{
	if ( sr830.device >= 0 )
		sr830_end_of_exp_hook( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *lockin_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------*/
/* Function returns the lock-in voltage(s). If called without an   */
/* argument the value is the X channel voltage is returned, other- */
/* wise a transient array is returned with the voltages of the X   */
/* and Y channel. All voltages are in in V, the range depending on */
/* the current sensitivity setting.                                */
/*-----------------------------------------------------------------*/

Var *lockin_get_data( Var *v )
{
	double data[ 6 ] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	long channels[ 6 ];
	int num_channels;
	bool using_dummy_channels = UNSET;
	int i;


	if ( v == NULL )
	{
		if ( FSC2_MODE == TEST )           /* return dummy value in test run */
			return vars_push( FLOAT_VAR, 0.0 );
		else
			return vars_push( FLOAT_VAR, sr830_get_data( ) );
	}

	for ( num_channels = i = 0; i < 6 && v != NULL; i++, v = vars_pop( v ) )
	{
		channels[ i ] = get_long( v, "channel number", DEVICE_NAME );

		if ( channels[ i ] < 1 || channels[ i ] > NUM_CHANNELS )
		{
			eprint( FATAL, SET, "%s: Invalid channel number %ld in call of "
					"`lockin_get_data'.\n", DEVICE_NAME, channels[ i ] );
			THROW( EXCEPTION )
		}

		num_channels++;
	}

	if ( v != NULL )
	{
		eprint( SEVERE, SET, "%s: More than 6 parameters in call of "
				"%s(), discarding superfluous ones.\n",
				DEVICE_NAME, Cur_Func );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( FSC2_MODE == TEST )
	{
		if ( num_channels == 1 )
			return vars_push( FLOAT_VAR, data[ 0 ] );
		else
			return vars_push( FLOAT_ARR, data, num_channels );
	}

	/* If we need less than two channels we've got to pass the function an
	   extra value, it expects at least 2 - just take the next channel */

	if ( num_channels == 1 )
	{
		using_dummy_channels = SET;
		channels[ num_channels++ ] = channels[ 0 ] % NUM_CHANNELS + 1;
	}

	sr830_get_xy_data( data, channels, num_channels );

	if ( using_dummy_channels )
		return vars_push( FLOAT_VAR, data[ 0 ] );

	return vars_push( FLOAT_ARR, data, num_channels );
}


/*---------------------------------------------------------------*/
/* Function returns the voltage at one of the 4 ADC ports on the */
/* backside of the lock-in amplifier. The argument must be an    */
/* integers between 1 and 4.                                     */
/* Returned values are in the interval [ -10.5 V, +10.5 V ].     */
/*---------------------------------------------------------------*/

Var *lockin_get_adc_data( Var *v )
{
	long port;


	port = get_double( v, "ADC port number", DEVICE_NAME );

	if ( port < 1 || port > NUM_ADC_PORTS )
	{
		eprint( FATAL, SET, "%s: Invalid ADC channel number (%ld) in "
				"call of 'lockin_get_adc_data', valid channel are in the "
				"range 1-%d.\n", DEVICE_NAME, port, NUM_ADC_PORTS );
		THROW( EXCEPTION )
	}

	if ( FSC2_MODE == TEST )               /* return dummy value in test run */
		return vars_push( FLOAT_VAR, SR830_TEST_ADC_VOLTAGE );

	return vars_push( FLOAT_VAR, sr830_get_adc_data( port ) );
}


/*-----------------------------------------------------------*/
/* Function sets or returns the voltage at one of the 4 DAC  */
/* ports on the backside of the lock-in amplifier. The first */
/* argument must be an integer between 1 and 4, the second   */
/* the voltage in the range between -10.5 V and +10.5 V. If  */
/* there isn't a second argument the set DAC voltage is      */
/* returned (which is initially set to 0 V).                 */
/*-----------------------------------------------------------*/

Var *lockin_dac_voltage( Var *v )
{
	long port;
	double voltage;


	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing arguments in call of function "
				"`lockin_dac_voltage`.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	/* Get and check the port number */

	port = get_long( v, "DAC port number", DEVICE_NAME );

	if ( port < 1 || port > NUM_DAC_PORTS )
	{
		eprint( FATAL, SET, "%s: Invalid DAC channel number (%ld) in "
				"call of 'lockin_dac_voltage', valid channel are in the "
				"range 1-%d.\n", DEVICE_NAME, port, NUM_DAC_PORTS );
		THROW( EXCEPTION )
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == PREPARATION )
		{
			eprint( FATAL, SET, "%s: Function %s() with only one argument can "
					"only be used in the EXPERIMENT section.\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}

		return vars_push( FLOAT_VAR, sr830.dac_voltage[ port - 1 ] );
	}

	/* Get and check the voltage */

	voltage = get_double( v, "DAC voltage", DEVICE_NAME );

	if ( voltage < DAC_MIN_VOLTAGE || voltage > DAC_MIN_VOLTAGE )
	{
		eprint( FATAL, SET, "%s: Invalid DAC voltage (%f V) in call of "
				"'lockin_dac_voltage', valid range is between %f V and "
				"%f V.\n", DEVICE_NAME, DAC_MIN_VOLTAGE,
				DAC_MAX_VOLTAGE );
		THROW( EXCEPTION )
	}

	too_many_arguments( v, DEVICE_NAME );
	
	sr830.dac_voltage[ port - 1 ] = voltage;

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, voltage );

	return vars_push( FLOAT_VAR, sr830_set_dac_data( port, voltage ) );
}


/*-------------------------------------------------------------------------*/
/* Returns or sets sensitivity of the lock-in amplifier. If called with no */
/* argument the current sensitivity is returned, otherwise the sensitivity */
/* is set to the argument.                                                 */
/*-------------------------------------------------------------------------*/

Var *lockin_sensitivity( Var *v )
{
	double sens;
	int sens_index = UNDEF_SENS_INDEX;
	unsigned int i;


	if ( v == NULL )
		switch( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( DEVICE_NAME );

			case TEST :
				return vars_push( FLOAT_VAR,
								  sr830.sens_index == UNDEF_SENS_INDEX ?
								  SR830_TEST_SENSITIVITY :
								  sens_list[ sr830.sens_index ] );

			case EXPERIMENT :
			return vars_push( FLOAT_VAR, sr830_get_sens( ) );
		}

	sens = get_double( v, "sensitivity", DEVICE_NAME );

	if ( sens < 0.0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative sensitivity.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	/* Try to match the sensitivity passed to the function by checking if it
	   fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, set it to the minimum or maximum value,
	   depending on the size of the argument. If the value does not fit within
	   1 percent, utter a warning message (but only once). */

	for ( i = 0; i < SENS_ENTRIES - 2; i++ )
		if ( sens >= sens_list[ i ] && sens <= sens_list[ i + 1 ] )
		{
			sens_index = i +
				   ( ( sens_list[ i ] / sens >
					   sens / sens_list[ i + 1 ] ) ? 0 : 1 );
			break;
		}

	if ( sens_index == UNDEF_SENS_INDEX &&
		 sens < sens_list[ SENS_ENTRIES - 1 ] * 1.01 )
		sens_index = SENS_ENTRIES - 1;

	if ( sens_index >= 0 &&                                 /* value found ? */
		 fabs( sens - sens_list[ sens_index ] ) > sens * 1.0e-2 &&
                                                            /* error > 1% ? */
		 ! sr830.sens_warn  )                            /* no warning yet ? */
	{
		if ( sens >= 1.0e-3 )
			eprint( WARN, SET, "%s: Can't set sensitivity to %.0lf mV, "
					"using %.0lf mV instead.\n", DEVICE_NAME,
					sens * 1.0e3, sens_list[ sens_index ] * 1.0e3 );
		else if ( sens >= 1.0e-6 ) 
			eprint( WARN, SET, "%s: Can't set sensitivity to %.0lf uV, "
					"using %.0lf uV instead.\n", DEVICE_NAME,
					sens * 1.0e6, sens_list[ sens_index ] * 1.0e6 );
		else
			eprint( WARN, SET, "%s: Can't set sensitivity to %.0lf nV, "
					"using %.0lf nV instead.\n", DEVICE_NAME,
					sens * 1.0e9, sens_list[ sens_index ] * 1.0e9 );
		sr830.sens_warn = SET;
	}

	if ( sens_index == UNDEF_SENS_INDEX )                 /* not found yet ? */
	{
		if ( sens < sens_list[ 0 ] )
			sens_index = 0;
		else
		    sens_index = SENS_ENTRIES - 1;

		if ( ! sr830.sens_warn )                      /* no warn message yet */
		{
			if ( sens >= 1.0 )
				eprint( WARN, SET, "%s: Sensitivity of %.0lf V is too "
						"low, using %.0lf V instead.\n", DEVICE_NAME,
						sens * 1.0e3, sens_list[ sens_index ] * 1.0e3 );
			else
				eprint( WARN, SET, "%s: Sensitivity of %.0lf nV is too "
						"high, using %.0lf nV instead.\n", DEVICE_NAME,
						sens * 1.0e9, sens_list[ sens_index ] * 1.0e9 );
			sr830.sens_warn = SET;
		}
	}

	too_many_arguments( v, DEVICE_NAME );

	sr830.sens_index = sens_index;

	if ( FSC2_MODE == EXPERIMENT )
		sr830_set_sens( sens_index );
	
	return vars_push( FLOAT_VAR, sens_list[ sens_index ] );
}


/*------------------------------------------------------------------------*/
/* Returns or sets the time constant of the lock-in amplifier. If called  */
/* without an argument the time constant is returned (in secs). If called */
/* with an argumet the time constant is set to this value.                */
/*------------------------------------------------------------------------*/

Var *lockin_time_constant( Var *v )
{
	double tc;
	int tc_index = UNDEF_TC_INDEX;
	unsigned int i;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( DEVICE_NAME );

			case TEST :
				return vars_push( FLOAT_VAR, sr830.tc_index == UNDEF_TC_INDEX ?
								  SR830_TEST_TIME_CONSTANT :
								  tc_list[ sr830.tc_index ] );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, sr830_get_tc( ) );
		}

	tc = get_double( v, "time constant", DEVICE_NAME );

	if ( tc < 0.0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative time constant.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	/* We try to match the time constant passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 1 percent, we utter a warning message (but only once). */
	
	for ( i = 0; i < TC_ENTRIES - 2; i++ )
		if ( tc >= tc_list[ i ] && tc <= tc_list[ i + 1 ] )
		{
			tc_index = i + ( ( tc / tc_list[ i ] <
							   tc_list[ i + 1 ] / tc ) ? 0 : 1 );
			break;
		}

	if ( tc_index >= 0 &&                                   /* value found ? */
		 fabs( tc - tc_list[ tc_index ] ) > tc * 1.0e-2 &&  /* error > 1% ? */
		 ! sr830.tc_warn )                          /* no warning yet ? */
	{
		if ( tc > 1.0e3 )
			eprint( WARN, SET, "%s: Can't set time constant to %.0lf ks,"
					" using %.0lf ks instead.\n", DEVICE_NAME,
					tc * 1.0e-3, tc_list[ tc_index ] );
		else if ( tc > 1.0 )
			eprint( WARN, SET, "%s: Can't set time constant to %.0lf s, "
					"using %.0lf s instead.\n", DEVICE_NAME, tc,
					tc_list[ tc_index ] );
		else if ( tc > 1.0e-3 )
			eprint( WARN, SET, "%s: Can't set time constant to %.0lf ms,"
					" using %.0lf ms instead.\n", DEVICE_NAME,
					tc * 1.0e3, tc_list[ tc_index ] * 1.0e3 );
		else
			eprint( WARN, SET, "%s: Can't set time constant to %.0lf us,"
					" using %.0lf us instead.\n", DEVICE_NAME,
					tc * 1.0e6, tc_list[ tc_index ] * 1.0e6 );
		sr830.tc_warn = SET;
	}

	if ( tc_index == UNDEF_TC_INDEX )                     /* not found yet ? */
	{
		if ( tc < tc_list[ 0 ] )
			tc_index = 0;
		else
			tc_index = TC_ENTRIES - 1;

		if ( ! sr830.tc_warn )                      /* no warn message yet ? */
		{
			if ( tc >= 3.0e4 )
				eprint( WARN, SET, "%s: Time constant of %.0lf ks is too"
						" large, using %.0lf ks instead.\n", DEVICE_NAME,
						tc * 1.0e-3, tc_list[ tc_index ] * 1.0e-3 );
			else
				eprint( WARN, SET, "%s: Time constant of %.0lf us is too"
						" small, using %.0lf us instead.\n", DEVICE_NAME,
						tc * 1.0e6, tc_list[ tc_index ] * 1.0e6 );
			sr830.tc_warn = SET;
		}
	}

	too_many_arguments( v, DEVICE_NAME );

	sr830.tc_index = tc_index;

	if ( FSC2_MODE == EXPERIMENT )
		sr830_set_tc( tc_index );
	
	return vars_push( FLOAT_VAR, tc_list[ tc_index ] );
}


/*-----------------------------------------------------------------*/
/* Returns or sets the phase of the lock-in amplifier. If called   */
/* without an argument the current phase setting is returned (in   */
/* degree between 0 and 359). Otherwise the phase is set to value  */
/* passed to the function (after conversion to 0-359 degree range) */
/* and the value the phase is set to is returned.                  */
/*-----------------------------------------------------------------*/

Var *lockin_phase( Var *v )
{
	double phase;


	/* Without an argument just return current phase settting */

	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				eprint( FATAL, SET, "%s: Function `lockin_phase' with "
						"no argument can only be used in the EXPERIMENT "
						"section.\n", DEVICE_NAME );
				THROW( EXCEPTION )

			case TEST :
				return vars_push( FLOAT_VAR, sr830.is_phase ?
								  sr830.phase : SR830_TEST_PHASE );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, sr830_get_phase( ) );
		}

	/* Otherwise set phase to value passed to the function */

	phase = get_double( v, "phase", DEVICE_NAME );

	while ( phase >= 360.0 )    /* convert to 0-359 degree range */
		phase -= 360.0;

	if ( phase < 0.0 )
	{
		phase *= -1.0;
		while ( phase >= 360.0 )    /* convert to 0-359 degree range */
			phase -= 360.0;
		phase = 360.0 - phase;
	}

	too_many_arguments( v, DEVICE_NAME );

	sr830.phase    = phase;
	sr830.is_phase = SET;

	if ( FSC2_MODE == EXPERIMENT )
		sr830_set_phase( phase );

	return vars_push( FLOAT_VAR, phase );
}


/*-----------------------------------------*/
/* Sets or returns the harmonic to be used */
/*-----------------------------------------*/

Var *lockin_harmonic( Var *v )
{
	long harm;
	double freq;


	/* Without an argument just return current harmonic settting */

	if ( v == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( INT_VAR, sr830.is_harmonic ? 
							  sr830.is_harmonic : SR830_TEST_HARMONIC );
		else
			return vars_push( INT_VAR, sr830_get_harmonic( ) );
	}

	harm = get_long( v, "harmonic", DEVICE_NAME );
	
	if ( FSC2_MODE == TEST )
		freq = MIN_MOD_FREQ;
	else
		freq = sr830_get_mod_freq( );

	if ( harm > MAX_HARMONIC || harm < MIN_HARMONIC )
	{
		eprint( FATAL, SET, "%s: Harmonic of %ld not within allowed range of "
				"%ld-%ld.\n", DEVICE_NAME, harm, MIN_HARMONIC, MAX_HARMONIC );
		THROW( EXCEPTION )
	}

	if ( freq > MAX_MOD_FREQ / ( double ) harm )
	{
		eprint( FATAL, SET, "%s: Modulation frequency of %f Hz with "
				"harmonic set to %ld is not within valid range "
				"(%f Hz - %f kHz).\n", DEVICE_NAME, freq, harm,
				MIN_MOD_FREQ, 1.0e-3 * MAX_MOD_FREQ / ( double ) harm );
		THROW( EXCEPTION )
	}

	too_many_arguments( v, DEVICE_NAME );

	sr830.harmonic = harm;
	sr830.is_harmonic = SET;

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( INT_VAR, harm );

	return vars_push( INT_VAR, sr830_set_harmonic( harm ) );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var *lockin_ref_mode( Var *v )
{
	v = v;


	if ( FSC2_MODE == TEST )
		return vars_push( INT_VAR, SR830_TEST_MOD_MODE );
	return vars_push( INT_VAR, sr830_get_mod_mode( ) );
}


/*--------------------------------------------------*/
/* Sets or returns the lock-in modulation frequency */
/*--------------------------------------------------*/

Var *lockin_ref_freq( Var *v )
{
	long harm;
	double freq;


	/* Without an argument just return current phase settting */

	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( DEVICE_NAME );

			case TEST :
				return vars_push( FLOAT_VAR, sr830.is_mod_freq ?
								  sr830.mod_freq : SR830_TEST_MOD_FREQUENCY );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, sr830_get_mod_freq( ) );
		}

	freq = get_double( v, "modulation frequency", DEVICE_NAME );
	
	if ( FSC2_MODE != TEST && sr830_get_mod_mode( ) != MOD_MODE_INTERNAL )
	{
		eprint( FATAL, SET, "%s: Can't set modulation frequency while "
				"modulation source isn't internal.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	if ( FSC2_MODE == TEST )
		harm = sr830.is_harmonic ? sr830.harmonic : SR830_TEST_HARMONIC;
	else
		harm = sr830_get_harmonic( );

	if ( freq < MIN_MOD_FREQ || freq > MAX_MOD_FREQ / ( double ) harm )
	{
		if ( harm == 1 )
			eprint( FATAL, SET, "%s: Modulation frequency of %f Hz is "
					"not within valid range (%f Hz - %f kHz).\n",
					DEVICE_NAME, freq, MIN_MOD_FREQ, MAX_MOD_FREQ * 1.0e-3 );
		else
			eprint( FATAL, SET, "%s: Modulation frequency of %f Hz with "
					"harmonic set to %ld is not within valid range "
					"(%f Hz - %f kHz).\n", DEVICE_NAME, freq, harm,
					MIN_MOD_FREQ, 1.0e-3 * MAX_MOD_FREQ / ( double ) harm );
		THROW( EXCEPTION )
	}

	too_many_arguments( v, DEVICE_NAME );

	sr830.mod_freq    = freq;
	sr830.is_mod_freq = SET;

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, freq );

	return vars_push( FLOAT_VAR, sr830_set_mod_freq( freq ) );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var *lockin_ref_level( Var *v )
{
	double level;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( DEVICE_NAME );

			case TEST :
				return vars_push( FLOAT_VAR, sr830.is_mod_level ?
								  sr830.mod_level : SR830_TEST_MOD_LEVEL );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, sr830_get_mod_level( ) );
		}

	level = get_double( v, "modulation level", DEVICE_NAME );

	if ( level < MIN_MOD_LEVEL || level > MAX_MOD_LEVEL )
	{
		eprint( FATAL, SET, "%s: Modulation level of %f V is not within "
				"valid range (%f V - %f V).\n", DEVICE_NAME,
				MIN_MOD_LEVEL, MAX_MOD_LEVEL );
		THROW( EXCEPTION )
	}

	too_many_arguments( v, DEVICE_NAME );

	sr830.mod_level = level;
	sr830.is_mod_level = SET;

	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, level );

	return vars_push( FLOAT_VAR, sr830_set_mod_level( level ) );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

Var *lockin_lock_keyboard( Var *v )
{
	bool lock;


	if ( v == NULL )
		lock = SET;
	else
	{
		lock = get_boolean( v, DEVICE_NAME );
		too_many_arguments( v, DEVICE_NAME );
	}

	if ( FSC2_MODE == EXPERIMENT )
		sr830_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0 );
}



/******************************************************/
/* The following functions are only used internally ! */
/******************************************************/


/*------------------------------------------------------------------*/
/* Function initialises the Stanford lock-in amplifier and tests if */
/* it can be accessed by asking it to send its status byte.         */
/*------------------------------------------------------------------*/

static bool sr830_init( const char *name )
{
	char buffer[ 20 ];
	long length = 20;
	int i;


	if ( gpib_init_device( name, &sr830.device ) == FAILURE )
	{
		sr830.device = -1;
        return FAIL;
	}

	/* Tell the lock-in to use the GPIB bus for communication, clear all
	   the relevant registers  and make sure the keyboard is locked */

	if ( gpib_write( sr830.device, "OUTX 1\n", 7 ) == FAILURE ||
		 gpib_write( sr830.device, "*CLS\n", 5 )   == FAILURE ||
		 gpib_write( sr830.device, "OVRM 0\n", 7 ) == FAILURE )
		return FAIL;
	   
	/* Ask lock-in to send the error status byte and test if it does */

	length = 20;
	if ( gpib_write( sr830.device, "ERRS?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		return FAIL;

	/* If the lock-in did send more than 2 byte this means its write buffer
	   isn't empty. We now read it until its empty for sure. */

	if ( length > 2 )
	{
		do
			length = 20;
		while ( gpib_read( sr830.device, buffer, &length ) != FAILURE &&
				length == 20 );
	}

	/* If sensitivity, time constant or phase were set in one of the
	   preparation sections only the value was stored and we have to do the
	   actual setting now because the lock-in could not be accessed before */

	if ( sr830.sens_index != UNDEF_SENS_INDEX )
		sr830_set_sens( sr830.sens_index );
	if ( sr830.is_phase )
		sr830_set_phase( sr830.phase );
	if ( sr830.tc_index != UNDEF_TC_INDEX )
		sr830_set_tc( sr830.tc_index );
	if ( sr830.is_harmonic )
		sr830_set_harmonic( sr830.harmonic );
	if ( sr830.is_mod_freq )
	{
		if ( sr830_get_mod_mode( ) != MOD_MODE_INTERNAL )
			sr830_set_mod_freq( sr830.mod_freq );
		else
			eprint( SEVERE, UNSET, "%s: Can't set modulation frequency, "
					"lock-in uses internal modulation.\n", DEVICE_NAME );
	}
	if ( sr830.is_mod_level )
		sr830_set_mod_level( sr830.mod_level );

	for ( i = 0; i < NUM_DAC_PORTS; i++ )
		sr830_set_dac_data( i + 1, sr830.dac_voltage[ i ] );

	return OK;
}


/*----------------------------------------------------------------*/
/* lockin_get_data() returns the measured voltage of the lock-in. */
/*----------------------------------------------------------------*/

static double sr830_get_data( void )
{
	char buffer[ 50 ];
	long length = 50;


	if ( gpib_write( sr830.device, "OUTP?1\n", 7 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*------------------------------------------------------------*/
/* lockin_data() returns the measured voltage of the lock-in. */
/*------------------------------------------------------------*/

static void sr830_get_xy_data( double *data, long *channels, int num_channels )
{
	char cmd[ 100 ] = "SNAP?";
	char buffer[ 200 ];
	long length = 200;
	char *bp_cur, *bp_next;
	int i;


	fsc2_assert( num_channels >= 2 && num_channels <= 6 );

	/* Assemble the command to be send to the lock-in */

	for ( i = 0; i < num_channels; i++ )
	{
		fsc2_assert( channels[ i ] > 0 && channels[ i ] <= NUM_CHANNELS );

		if ( i == 0 )
			sprintf( cmd + strlen( cmd ), "%ld\n", channels[ i ] );
		else
			sprintf( cmd + strlen( cmd ), ",%ld\n", channels[ i ] );
	}

	/* Get the data from the lock-in */

	if ( gpib_write( sr830.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	/* Disassemble the reply */

	buffer[ length - 1 ] = '\0';
	bp_cur = buffer;

	for ( i = 0; i < num_channels; bp_cur = bp_next, i++ )
	{
		bp_next = strchr( bp_cur, ',' ) + 1;

		if ( bp_next == NULL )
		{
			eprint( FATAL, UNSET, "%s: Lock-in amplifier does not react "
					"properly.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}
		else
			*( bp_next - 1 ) = '\0';

		data[ i ] = T_atof( bp_cur );
	}
}


/*----------------------------------------------------------*/
/* lockin_get_adc() returns the value of the voltage at one */
/* of the 4 ADC ports.                                      */
/* -> Number of the ADC channel (1-4)                       */
/*-------------------------- -------------------------------*/

static double sr830_get_adc_data( long channel )
{
	char buffer[ 16 ] = "OAUX?*\n";
	long length = 16;


	fsc2_assert( channel >= 1 && channel <= 4 );
	buffer[ 5 ] = ( char ) channel + '0';

	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*--------------------------------------------------------------*/
/* lockin_set_dac() sets the voltage at one of the 4 DAC ports. */
/* -> Number of the DAC channel (1-4)                           */
/* -> Voltage to be set (-10.5 V - +10.5 V)                     */
/*-------------------------- -----------------------------------*/

static double sr830_set_dac_data( long port, double voltage )
{
	char buffer [ 40 ];

	fsc2_assert( port >= 1 && port <= 4 );
	fsc2_assert( voltage >= DAC_MIN_VOLTAGE && voltage <= DAC_MAX_VOLTAGE );

	sprintf( buffer, "AUXV %ld,%f\n", port, voltage );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );

	return voltage;
}

/*-----------------------------------------------------------------------*/
/* Function determines the sensitivity setting of the lock-in amplifier. */
/*-----------------------------------------------------------------------*/

static double sr830_get_sens( void )
{
	char buffer[ 20 ];
	long length = 20;
	double sens;

	/* Ask lock-in for the sensitivity setting */

	if ( gpib_write( sr830.device, "SENS?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	sens = sens_list[ T_atol( buffer ) ];

	return sens;
}


/*----------------------------------------------------------------------*/
/* Function sets the sensitivity of the lock-in amplifier to one of the */
/* valid values. The parameter can be in the range from 0 to 26,  where */
/* 0 is 2 nV and 26 is 1 V - these and the other values in between are  */
/* listed in the global array 'sens_list' at the start of the file.     */
/*----------------------------------------------------------------------*/

static void sr830_set_sens( int sens_index )
{
	char buffer[ 20 ];


	sprintf( buffer, "SENS %d\n", sens_index );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );
}


/*----------------------------------------------------------------------*/
/* Function returns the current time constant of the lock-in amplifier. */
/* See also the global array 'tc_list' with the possible time constants */
/* at the start of the file.                                            */
/*----------------------------------------------------------------------*/

static double sr830_get_tc( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( sr830.device, "OFLT?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return tc_list[ T_atol( buffer ) ];
}


/*-------------------------------------------------------------------*/
/* Fuunction sets the time constant to one of the valid values. The  */
/* parameter can be in the range from 0 to 19, where 0 is 10 us and  */
/* 19 is 30 ks - these and the other values in between are listed in */
/* the global array 'tc_list' (cf. start of file).                   */
/*-------------------------------------------------------------------*/

static void sr830_set_tc( int tc_index )
{
	char buffer[ 20 ];


	sprintf( buffer, "OFLT %d\n", tc_index );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );
}


/*-----------------------------------------------------------*/
/* Function returns the current phase setting of the lock-in */
/* amplifier (in degree between 0 and 359).                  */
/*-----------------------------------------------------------*/

static double sr830_get_phase( void )
{
	char buffer[ 20 ];
	long length = 20;
	double phase;


	if ( gpib_write( sr830.device, "PHAS?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	phase = T_atof( buffer );

	while ( phase >= 360.0 )        /* convert to 0-359 degree range */
		phase -= 360.0;

	if ( phase < 0.0 )
	{
		phase *= -1.0;
		while ( phase >= 360.0 )    /* convert to 0-359 degree range */
			phase -= 360.0;
		phase = 360.0 - phase;
	}

	return phase;
}


/*---------------------------------------------------------------*/
/* Functions sets the phase to a value between 0 and 360 degree. */
/*---------------------------------------------------------------*/

static double sr830_set_phase( double phase )
{
	char buffer[ 20 ];


	sprintf( buffer, "PHAS %.2f\n", phase );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );

	return phase;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr830_get_mod_freq( void )
{
	char buffer[ 40 ];
	long length = 40;


	if ( gpib_write( sr830.device, "FREQ?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr830_set_mod_freq( double freq )
{
	char buffer[ 40 ];
	double real_freq;


	sprintf( buffer, "FREQ %.4f\n", freq );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );

	/* Take care: The product of the harmonic and the modulation frequency
	   can't be larger than 102 kHz, otherwise the modulation frequency is
	   reduced to a value that fits this restriction. Thus we better check
	   which value has been really set... */

	real_freq = sr830_get_mod_freq( );
	if ( ( real_freq - freq ) / freq > 1.0e-4 && real_freq - freq > 1.0e-4 )
	{
		eprint( FATAL, UNSET, "%s: Failed to set modulation frequency to %f "
				"Hz.\n", DEVICE_NAME, freq );
		THROW( EXCEPTION )
	}

	return real_freq;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static long sr830_get_mod_mode( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( sr830.device, "FMOD?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return T_atol( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static long sr830_get_harmonic( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( sr830.device, "HARM?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return  T_atol( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static long sr830_set_harmonic( long harmonic )
{
	char buffer[ 20 ];


	fsc2_assert( harmonic >= MIN_HARMONIC && harmonic <= MAX_HARMONIC );

	sprintf( buffer, "HARM %ld\n", harmonic );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );

	/* Take care: The product of the harmonic and the modulation frequency
	   can't be larger than 102 kHz, otherwise the harmonic is reduced to a
	   value that fits this restriction. So we better check on the value
	   that has been really set... */

	if ( harmonic != sr830_get_harmonic( ) )
	{
		eprint( FATAL, UNSET, "%s: Failed to set harmonic to %ld.\n",
				DEVICE_NAME, harmonic );
		THROW( EXCEPTION )
	}

	return harmonic;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr830_get_mod_level( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( sr830.device, "SLVL?\n", 6 ) == FAILURE ||
		 gpib_read( sr830.device, buffer, &length ) == FAILURE )
		sr830_failure( );

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr830_set_mod_level( double level )
{
	char buffer[ 50 ];


	fsc2_assert( level >= MIN_MOD_LEVEL && level <= MAX_MOD_LEVEL );

	sprintf( buffer, "SLVL %f\n", level );
	if ( gpib_write( sr830.device, buffer, strlen( buffer ) ) == FAILURE )
		sr830_failure( );

	return level;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void sr830_lock_state( bool lock )
{
	char cmd[ 100 ];


	sprintf( cmd, "OVRM %c\n", lock ? '0' : '1' );
	if ( gpib_write( sr830.device, cmd, strlen( cmd ) ) == FAILURE )
		sr830_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void sr830_failure( void )
{
	eprint( FATAL, UNSET, "%s: Can't access the lock-in amplifier.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
