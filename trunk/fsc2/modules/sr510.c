/*
  $Id$

  Copyright (c) 2001 Jens Thoms Toerring

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


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* This device needs '\n' (0x0A) as EOS - set this correctly in `gpib.conf' */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#include "fsc2.h"
#include "gpib_if.h"


/* name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "SR510"


/* declaration of exported functions */

int sr510_init_hook( void );
int sr510_exp_hook( void );
int sr510_end_of_exp_hook( void );
void sr510_exit_hook( void );

Var *lockin_name( Var *v );
Var *lockin_get_data( Var *v );
Var *lockin_get_adc_data( Var *v );
Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );
Var *lockin_ref_freq( Var *v );
Var *lockin_dac_voltage( Var *v );
Var *lockin_lock_keyboard( Var *v );


/* exported symbols (use by W-band power supply driver) */

int first_DAC_port = 5;
int last_DAC_port = 6;

/* typedefs and global variables used only in this file */

typedef struct
{
	int device;
	int Sens;
	bool Sens_warn;
	double phase;
	bool P;
	int TC;
	bool TC_warn;
	double dac_voltage[ 2 ];
} SR510;

static SR510 sr510;

/* lists of valid sensitivity and time constant settings (the last three
   entries in the sensitivity list are only usable when the EXPAND button
   is switched on!) */

static double slist[ ] = { 5.0e-1, 2.0e-1, 1.0e-1, 5.0e-2, 2.0e-2,
						   1.0e-2, 5.0e-3, 2.0e-3, 1.0e-3, 5.0e-4,
						   2.0e-4, 1.0e-4, 5.0e-5, 2.0e-5, 1.0e-5,
						   5.0e-6, 2.0e-6, 1.0e-6, 5.0e-7, 2.0e-7,
						   1.0e-7, 5.0e-8, 2.0e-8, 1.0e-8 };

/* list of all available time constants */

static double tcs[ ] = { 1.0e-3, 3.0e-3, 1.0e-2, 3.0e-2, 1.0e-1, 3.0e-1,
						 1.0, 3.0, 10.0, 30.0, 100.0 };


/* declaration of all functions used only in this file */

static bool sr510_init( const char *name );
static double sr510_get_data( void );
static double sr510_get_adc_data( long channel );
static double sr510_get_sens( void );
static void sr510_set_sens( int Sens );
static double sr510_get_tc( void );
static void sr510_set_tc( int TC );
static double sr510_get_phase( void );
static double sr510_set_phase( double phase );
static double sr510_get_ref_freq( void );
static double sr510_set_dac_voltage( long channel, double voltage );
static void sr510_lock_state( bool lock );
static void sr510_failure( void );



/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int sr510_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	sr510.device = -1;

	sr510.Sens = -1;             /* no sensitivity has to be set at start of */
	sr510.Sens_warn = UNSET;     /* experiment and no warning concerning the */
                                 /* sensitivity setting has been printed yet */
	sr510.P = UNSET;             /* no phase has to be set at start of the */
	                             /* experiment */
	sr510.TC = -1;               /* no time constant has to be set at the */
	sr510.TC_warn = UNSET;       /* start of the experiment and no warning */
	                             /* concerning it has been printed yet */

	for ( i = 0; i < 2; i++ )
		sr510.dac_voltage[ i ] = 0.0;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int sr510_exp_hook( void )
{
	/* Nothing to be done yet in a test run */

	if ( TEST_RUN )
		return 1;

	/* Initialize the lock-in */

	if ( ! sr510_init( DEVICE_NAME ) )
	{
		eprint( FATAL, UNSET, "%s: Initialization of device failed: %s\n",
				DEVICE_NAME, gpib_error_msg );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int sr510_end_of_exp_hook( void )
{
	/* Switch lock-in back to local mode */

	if ( sr510.device >= 0 )
	{
		gpib_write( sr510.device, "I0\n", 3 );
		gpib_local( sr510.device );
	}

	sr510.device = -1;

	return 1;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void sr510_exit_hook( void )
{
	sr510_end_of_exp_hook( );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *lockin_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------------------*/
/* Function returns the lock-in voltage. Returned value is the voltage */
/* in V, with the range depending on the current sensitivity setting.  */
/*---------------------------------------------------------------------*/

Var *lockin_get_data( Var *v )
{
	if ( v != NULL )
		eprint( WARN, SET, "%s: Useless parameter in call of "
				"lockin_get_data().\n", DEVICE_NAME );

	if ( TEST_RUN )                  /* return dummy value in test run */
		return vars_push( FLOAT_VAR, 0.0 );
	else
		return vars_push( FLOAT_VAR, sr510_get_data( ) );
}


/*-----------------------------------------------------------------*/
/* Function returns the voltage at one or more of the 4 ADC ports  */
/* on the backside of the lock-in amplifier. The argument(s) must  */
/* be integers between 1 and 4.                                    */
/* Returned values are in the interval [ -10.24V, +10.24V ].       */
/*-----------------------------------------------------------------*/

Var *lockin_get_adc_data( Var *v )
{
	long port;


	vars_check( v, INT_VAR | FLOAT_VAR );

	if ( v->type == FLOAT_VAR )
		eprint( WARN, SET, "%s: Floating point number used as ADC port "
				"number.\n", DEVICE_NAME );

	port = v->type == INT_VAR ? v->val.lval : ( long ) v->val.dval;

	if ( port < 1 || port > 4 )
	{
		eprint( FATAL, SET, "%s: Invalid ADC channel number (%ld) "
				"in call of 'lockin_get_adc_data', valid channel are in "
				"the range 1-4.\n", DEVICE_NAME, port );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )                  /* return dummy value in test run */
		return vars_push( FLOAT_VAR, 0.0 );

	return vars_push( FLOAT_VAR, sr510_get_adc_data( port ) );
}


/*-------------------------------------------------------------------------*/
/* Returns or sets sensitivity of the lock-in amplifier. If called with no */
/* argument the current sensitivity is returned, otherwise the sensitivity */
/* is set to the argument. By using the EXPAND button the sensitivity can  */
/* be increased by a factor of 10.                                         */
/*-------------------------------------------------------------------------*/

Var *lockin_sensitivity( Var *v )
{
	double sens;
	int Sens = -1;
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )                  /* return dummy value in test run */
			return vars_push( FLOAT_VAR, 0.5 );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function `lockin_sensitivity' "
						"with no argument can only be used in the EXPERIMENT "
						"section.\n", DEVICE_NAME );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr510_get_sens( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "Integer value used as sensitivity.\n",
				DEVICE_NAME );
	sens = VALUE( v );
	vars_pop( v );

	if ( sens < 0.0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative sensitivity.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We try to match the sensitivity passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 1 percent, we utter a warning message (but only once). */

	for ( i = 0; i < 23; i++ )
		if ( sens <= slist[ i ] && sens >= slist[ i + 1 ] )
		{
			if ( slist[ i ] / sens < sens / slist[ i + 1 ] )
				Sens = i + 1;
			else
				Sens = i + 2;
			break;
		}

	if ( Sens < 0 && sens < slist[ 24 ] * 1.01 )
		Sens = 24;


	if ( Sens > 0 &&                                   /* value found ? */
		 fabs( sens - slist[ Sens - 1 ] ) > sens * 1.0e-2 && /* error > 1% ? */
		 ! sr510.Sens_warn  )                       /* no warn message yet ? */
	{
		if ( sens >= 1.0e-3 )
			eprint( WARN, SET, "%s: Can't set sensitivity to %.0lf mV, "
					"using %.0lf V instead.\n", DEVICE_NAME,
					sens * 1.0e3, slist[ Sens - 1 ] * 1.0e3 );
		else if ( sens >= 1.0e-6 ) 
			eprint( WARN, SET, "%s: Can't set sensitivity to %.0lf uV, "
					"using %.0lf uV instead.\n", DEVICE_NAME,
					sens * 1.0e6, slist[ Sens - 1 ] * 1.0e6 );
		else
			eprint( WARN, SET, "%s: Can't set sensitivity to %.0lf nV, "
					"using %.0lf nV instead.\n", DEVICE_NAME,
					sens * 1.0e9, slist[ Sens - 1 ] * 1.0e9 );
		sr510.Sens_warn = SET;
	}

	if ( Sens < 0 )                                   /* not found yet ? */
	{
		if ( sens > slist[ 0 ] )
			Sens = 1;
		else
		    Sens = 24;

		if ( ! sr510.Sens_warn )                      /* no warn message yet */
		{
		if ( sens >= 1.0e-3 )
			eprint( WARN, SET, "%s: Sensitivity of %.0lf mV is too low, "
					"using %.0lf mV instead.\n", DEVICE_NAME,
					sens * 1.0e3, slist[ Sens - 1 ] * 1.0e3 );
		else
			eprint( WARN, SET, "%s: Sensitivity of %.0lf nV is too high,"
					" using %.0lf nV instead.\n", DEVICE_NAME,
					sens * 1.0e9, slist[ Sens - 1 ] * 1.0e9 );
			sr510.Sens_warn = SET;
		}
	}

	if ( ! TEST_RUN )
	{
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			sr510_set_sens( Sens );
		else                         /* if called in a preparation sections */ 
			sr510.Sens = Sens;
	}
	
	return vars_push( FLOAT_VAR, slist[ Sens - 1 ] );
}


/*------------------------------------------------------------------------*/
/* Returns or sets the time constant of the lock-in amplifier. If called  */
/* without an argument the time constant is returned (in secs). If called */
/* with an argumet the time constant is set to this value.                */
/*------------------------------------------------------------------------*/

Var *lockin_time_constant( Var *v )
{
	double tc;
	int TC = -1;
	int i;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, 0.1 );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function `lockin_time_constant'"
						" with no argument can only be used in the EXPERIMENT "
						"section.\n", DEVICE_NAME );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr510_get_tc( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as time constant.\n",
				DEVICE_NAME );
	tc = VALUE( v );
	vars_pop( v );

	if ( tc < 0.0 )
	{
		eprint( FATAL, SET, "%s: Invalid negative time constant.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We try to match the time constant passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 1 percent, we utter a warning message (but only once). */
	
	for ( i = 0; i < 10; i++ )
		if ( tc >= tcs[ i ] && tc <= tcs[ i + 1 ] )
		{
			if ( tc / tcs[ i ] < tcs[ i + 1 ] / tc )
				TC = i + 1;
			else
				TC = i + 2;
			break;
		}

	if ( TC > 0 &&                                    /* value found ? */
		 fabs( tc - tcs[ TC - 1 ] ) > tc * 1.0e-2 &&  /* error > 1% ? */
		 ! sr510.TC_warn )                          /* no warn message yet ? */
	{
		if ( tc >= 1.0 )
			eprint( WARN, SET, "%s: Can't set time constant to %.0lf s, "
					"using %.0lf s instead.\n", DEVICE_NAME, tc,
					tcs[ TC - 1 ] );
		else
			eprint( WARN, SET, "%s: Can't set time constant to %.0lf ms,"
					" using %.0lf ms instead.\n", DEVICE_NAME,
					tc * 1.0e3, tcs[ TC - 1 ] * 1.0e3 );
		sr510.TC_warn = SET;
	}
	
	if ( TC < 0 )                                  /* not found yet ? */
	{
		if ( tc < tcs[ 0 ] )
			TC = 1;
		else
			TC = 11;

		if ( ! sr510.TC_warn )                      /* no warn message yet ? */
		{
			if ( tc >= 1.0 )
				eprint( WARN, SET, "%s: Time constant of %.0lf s is too "
						"large, using %.0lf s instead.\n",
						DEVICE_NAME, tc, tcs[ TC - 1 ] );
			else
				eprint( WARN, SET, "%s: Time constant of %.0lf ms is too"
						" short, using %.0lf ms instead.\n",
						DEVICE_NAME, tc * 1.0e3, tcs[ TC - 1 ] * 1.0e3 );
			sr510.TC_warn = SET;
		}
	}

	if ( ! TEST_RUN )
	{
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			sr510_set_tc( TC );
		else                         /* if called in a preparation sections */ 
			sr510.TC = TC;
	}
	
	return vars_push( FLOAT_VAR, tcs[ TC - 1 ] );
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
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, 0.0 );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, SET, "%s: Function `lockin_phase' with "
						"no argument can only be used in the EXPERIMENT "
						"section.\n", DEVICE_NAME );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr510_get_phase( ) );
		}
	}

	/* Otherwise set phase to value passed to the function */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as phase.\n",
				DEVICE_NAME );
	phase = VALUE( v );
	vars_pop( v );

	while ( phase >= 360.0 )    /* convert to 0-359 degree range */
		phase -= 360.0;

	if ( phase < 0.0 )
	{
		phase *= -1.0;
		while ( phase >= 360.0 )    /* convert to 0-359 degree range */
			phase -= 360.0;
		phase = 360.0 - phase;
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, phase );
	else
	{
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			return vars_push( FLOAT_VAR, sr510_set_phase( phase ) );
		else                         /* if called in a preparation sections */ 
		{
			sr510.phase = phase;
			sr510.P = SET;
			return vars_push( FLOAT_VAR, phase );
		}
	}
}


/*------------------------------------------------------------*/
/* Function returns the reference frequency, can only be used */
/* for queries.                                               */
/*------------------------------------------------------------*/

Var *lockin_ref_freq( Var *v )
{
	if ( v != NULL )
	{
		eprint( FATAL, SET, "%s: Reference frequency cannot be set for "
				"this model.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 0.0 );
	else
	{
		if ( I_am == PARENT )
		{
			eprint( FATAL, SET, "%s: Function `lockin_ref_freq' "
					"can only be used in the EXPERIMENT section.\n",
					DEVICE_NAME );
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, sr510_get_ref_freq( ) );
	}
}


/*-----------------------------------------------------------*/
/* Function sets or returns the voltage at one of the 2 DAC  */
/* ports on the backside of the lock-in amplifier. The first */
/* argument must be the channel number, either 5 or 6, the   */
/* second the voltage in the range between -10.24 V and      */
/* +10.24 V. If there isn't a second argument the set DAC    */
/* voltage is returned (which is initially set to 0 V).      */
/*-----------------------------------------------------------*/

Var *lockin_dac_voltage( Var *v )
{
	long channel;
	double voltage;


	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing arguments in call of function "
				"`lockin_dac_voltage'.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* First argument must be the channel number (5 or 6) */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
		eprint( WARN, SET, "%s: Floating point number used as DAC channel "
				"number.\n", DEVICE_NAME );

	channel = v->type == INT_VAR ? v->val.lval : ( long ) v->val.dval;
	v = vars_pop( v );

	if ( channel < first_DAC_port || channel > last_DAC_port )
	{
		eprint( FATAL, SET, "%s: Invalid lock-in DAC channel number %ld, "
				"valid channels are in the range from %d to %d.\n",
				DEVICE_NAME, channel, first_DAC_port, last_DAC_port );
		THROW( EXCEPTION );
	}

	/* If no second argument is specified return the current DAC setting */

	if ( v == NULL )
		return vars_push( FLOAT_VAR,
						  sr510.dac_voltage[ channel - first_DAC_port ] );

	/* Second argument must be a voltage between -10.24 V and +10.24 V */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, SET, "%s: Integer value used as DAC voltage.\n",
				DEVICE_NAME );

	voltage = VALUE( v );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, SET, "%s: Superfluous arguments in call of function "
				"`lockin_dac_voltage'.\n", DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL ) 
				;
	}

	if ( fabs( voltage ) > 10.24 )
	{
		eprint( FATAL, SET, "%s: DAC voltage of %f V is out of valid "
				"range (+/-10.24 V).\n", DEVICE_NAME, voltage );
		THROW( EXCEPTION );
	}

	sr510.dac_voltage[ channel - first_DAC_port ] = voltage;

	if ( TEST_RUN || I_am == PARENT)
		return vars_push( FLOAT_VAR, voltage );

	return vars_push( FLOAT_VAR, sr510_set_dac_voltage( channel, voltage ) );
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
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

		if ( v->type == INT_VAR )
			lock = v->val.lval == 0 ? UNSET : UNSET;
		else if ( v->type == FLOAT_VAR )
			lock = v->val.dval == 0.0 ? UNSET : UNSET;
		else
		{
			if ( ! strcasecmp( v->val.sptr, "OFF" ) )
				lock = UNSET;
			else if ( ! strcasecmp( v->val.sptr, "ON" ) )
				lock = SET;
			else
			{
				eprint( FATAL, SET, "%s: Invalid argument in call of "
						"`lockin_lock_keyboard'.\n", DEVICE_NAME );
				THROW( EXCEPTION );
			}
		}
	}

	if ( ! TEST_RUN )
		sr510_lock_state( lock );

	return vars_push( INT_VAR, lock ? 1 : 0);
}



/******************************************************/
/* The following functions are only used internally ! */
/******************************************************/


/*------------------------------------------------------------------*/
/* Function initialises the Stanford lock-in amplifier and tests if */
/* it can be accessed by asking it to send its status byte.         */
/*------------------------------------------------------------------*/

bool sr510_init( const char *name )
{
	char buffer[ 20 ];
	long length = 20;
	int i;


	if ( gpib_init_device( name, &sr510.device ) == FAILURE )
	{
		sr510.device = -1;
        return FAIL;
	}

	/* Ask lock-in to send status byte and test if it does */

	if ( gpib_write( sr510.device, "Y\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		return FAIL;

	/* Lock the keyboard */

	if ( gpib_write( sr510.device, "I1\n", 3 ) == FAILURE )
		return FAIL;

	/* If sensitivity, time constant or phase were set in one of the
	   preparation sections only the value was stored and we have to do the
	   actual setting now because the lock-in could not be accessed before.
	   Finally set the DAC output voltages to a defined value (default 0 V).*/

	if ( sr510.Sens != -1 )
		sr510_set_sens( sr510.Sens );
	if ( sr510.P == SET )
		sr510_set_phase( sr510.phase );
	if ( sr510.TC != -1 )
		sr510_set_tc( sr510.TC );
	for ( i = 0; i < 2; i++ )
		sr510_set_dac_voltage( i + first_DAC_port, sr510.dac_voltage[ i ] );
		

	return OK;
}


/*------------------------------------------------------------*/
/* lockin_data() returns the measured voltage of the lock-in. */
/*------------------------------------------------------------*/

double sr510_get_data( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( sr510.device, "Q\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	buffer[ length - 2 ] = '\0';
	return T_atof( buffer );
}


/*----------------------------------------------------------*/
/* lockin_get_adc() returns the value of the voltage at one */
/* of the 4 ADC ports.                                      */
/* -> Number of the ADC channel (1-4)                       */
/*-------------------------- -------------------------------*/

double sr510_get_adc_data( long channel )
{
	char buffer[ 5 ] = "X*\n";
	long length = 16;


	fsc2_assert( channel >= 1 && channel <= 4 );

	buffer[ 1 ] = ( char ) channel + '0';

	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	buffer[ length - 2 ] = '\0';
	return T_atof( buffer );
}


/*-----------------------------------------------------------------------*/
/* Function determines the sensitivity setting of the lock-in amplifier. */
/*-----------------------------------------------------------------------*/

double sr510_get_sens( void )
{
	char buffer[ 10 ];
	long length = 10;
	double sens;

	/* Ask lock-in for the sensitivity setting */

	if ( gpib_write( sr510.device, "G\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	buffer[ length - 2 ] = '\0';
	sens = slist[ 24 - T_atol( buffer ) ];

    /* Check if EXPAND is switched on - this increases the sensitivity 
	   by a factor of 10 */

	length = 10;
	if ( gpib_write( sr510.device, "E\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	if ( buffer[ 0 ] == '1' )
		sens *= 0.1;

	return sens;
}


/*----------------------------------------------------------------------*/
/* Function sets the sensitivity of the lock-in amplifier to one of the */
/* valid values. The parameter can be in the range from 1 to 27,  where */
/* 1 is 0.5 V and 27 is 10 nV - these and the other values in between   */
/* are listed in the global array 'slist' at the start of the file. To  */
/* achieve sensitivities below 100 nV EXPAND has to switched on.        */
/*----------------------------------------------------------------------*/

void sr510_set_sens( int Sens )
{
	char buffer[ 10 ];


	/* Coding of sensitivity commands work just the other way round as
	   in the list of sensitivities 'slist', i.e. 1 stands for the highest
	   sensitivity (10nV) and 24 for the lowest (500mV) */

	Sens = 25 - Sens;

	/* For sensitivities lower than 100 nV EXPAND has to be switched on
	   otherwise it got to be switched off */

	if ( Sens <= 3 )
	{
		if ( gpib_write( sr510.device, "E1\n", 3 ) == FAILURE )
			sr510_failure( );
		Sens += 3;
	}
	else
	{
		if ( gpib_write( sr510.device, "E0\n", 3 ) == FAILURE )
			sr510_failure( );
	}

	/* Now set the sensitivity */

	sprintf( buffer, "G%d\n", Sens );

	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
		sr510_failure( );
}


/*----------------------------------------------------------------------*/
/* Function returns the current time constant of the lock-in amplifier. */
/* See also the global array 'tcs' with the possible time constants at  */
/* the start of the file.                                               */
/*----------------------------------------------------------------------*/

double sr510_get_tc( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( sr510.device, "T1\n", 3 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	buffer[ length - 2 ] = '\0';
	return tcs[ T_atol( buffer ) - 1 ];
}


/*-------------------------------------------------------------------------*/
/* Fuunction sets the time constant (plus the post time constant) to one   */
/* of the valid values. The parameter can be in the range from 1 to 11,    */
/* where 1 is 1 ms and 11 is 100 s - these and the other values in between */
/* are listed in the global array 'tcs' (cf. start of file)                */
/*-------------------------------------------------------------------------*/

void sr510_set_tc( int TC )
{
	char buffer[ 10 ];


	sprintf( buffer, "T1,%d\n", TC );
	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
		sr510_failure( );

	/* Also set the POST time constant where 'T2,0' switches it off, 'T2,1'
	   sets it to 100ms and 'T1,2' to 1s */

	if ( TC <= 4 && gpib_write( sr510.device, "T2,0\n", 5 ) == FAILURE )
		sr510_failure( );

	if ( TC > 4 && TC <= 6 &&
		 gpib_write( sr510.device, "T2,1\n", 5 ) == FAILURE )
		sr510_failure( );

	if ( TC > 6 && gpib_write( sr510.device, "T2,2\n", 5 ) == FAILURE )
		sr510_failure( );
}


/*-----------------------------------------------------------*/
/* Function returns the current phase setting of the lock-in */
/* amplifier (in degree between 0 and 359).                  */
/*-----------------------------------------------------------*/

double sr510_get_phase( void )
{
	char buffer[ 20 ];
	long length = 20;
	double phase;


	if ( gpib_write( sr510.device, "P\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	buffer[ length - 2 ] = '\0';
	phase = T_atof( buffer );

	while ( phase >= 360.0 )    /* convert to 0-359 degree range */
		phase -= 360.0;

	while ( phase < 0.0 )
		phase += 360.0;

	return phase;
}


/*---------------------------------------------------------------*/
/* Functions sets the phase to a value between 0 and 360 degree. */
/*---------------------------------------------------------------*/

double sr510_set_phase( double phase )
{
	char buffer[ 20 ];


	sprintf( buffer, "P%.2f\n", phase );
	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
		sr510_failure( );

	return phase;
}


/*------------------------------------------*/
/* Function returns the reference frequency */
/*------------------------------------------*/

static double sr510_get_ref_freq( void )
{
	char buffer[ 50 ];
	long length = 50;

	if ( gpib_write( sr510.device, "F\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		sr510_failure( );

	buffer[ length - 2 ] = '\0';
	return T_atof( buffer );
}


/*----------------------------------------*/
/* Functions sets the DAC output voltage. */
/*----------------------------------------*/

static double sr510_set_dac_voltage( long channel, double voltage )
{
	char buffer[ 30 ];


	/* Just some more sanity checks, should already been done by calling
       function... */

	fsc2_assert( channel >= first_DAC_port || channel <= last_DAC_port );
	if ( fabs( voltage ) >= 10.24 )
	{
		if ( voltage > 0.0 )
			voltage = 10.24;
		else
			voltage = -10.24;
	}

	sprintf( buffer, "X%1ld,%f\n", channel, voltage );
	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
		sr510_failure( );
	
	return voltage;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void sr510_lock_state( bool lock )
{
	char cmd[ 100 ];


	sprintf( cmd, "I%c\n", lock ? '2' : '0' );
	if ( gpib_write( sr510.device, cmd, strlen( cmd ) ) == FAILURE )
		sr510_failure( );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static void sr510_failure( void )
{
	eprint( FATAL, UNSET, "%s: Can't access the lock-in amplifier.\n",
			DEVICE_NAME );
	THROW( EXCEPTION );
}
