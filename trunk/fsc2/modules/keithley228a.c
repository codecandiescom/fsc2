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

#include "keithley228a.conf"

const char generic_type[ ] = DEVICE_TYPE;


/* Here is a list of the supported lock-in amplifiers plus the default DAC
   ports used with each off them. The default DAC port list will only be
   used when LOCKIN_DAC isn't defined in 'keithley228a.conf'. */

static const char *lockins[ ] = { "sr510", "sr530", "sr810", "sr830", NULL };

#if ! defined( LOCKIN_DAC )
static int dac_ports[ ] = { 6,       6,       4,       4      };
#endif

#define KEITHLEY228A_MAX_CURRENT     10.0  /* admissible current range */
#define KEITHLEY228A_MAX_VOLTAGE      5.0  /* admissible voltage range */
#define KEITHLEY228A_MAX_SWEEP_SPEED  1.0  /* max. current change per second */

#define KEITHLEY228A_MAXMAX_CURRENT  10.01 /* really the maximum current ;-) */

#define KEITHLEY228A_MAX_JUMP         0.1  /* maximum uncontrolled jump */

#define	STANDBY  ( ( bool ) 0 )      
#define OPERATE  ( ( bool ) 1 )
	

/* Exported functions */

int keithley228a_init_hook( void );
int keithley228a_exp_hook( void );
int keithley228a_end_of_exp_hook( void );
void keithley228a__exit_hook( void );

Var *magnet_name( Var *v );
Var *magnet_setup( Var *v );
Var *magnet_use_correction( Var *v );
Var *magnet_use_dac_port( Var *v );
Var *set_field( Var *v );
Var *get_field( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_field( Var *v );


/* internally used functions */

static bool keithley228a_init( const char *name );
static void keithley228a_to_local(void);
static bool keithley228a_set_state( bool new_state );
static double keithley228a_goto_current( double current );
static double keithley228a_set_current( double current );
static void keithley228a_gpib_failure( void );
static double keithley228a_current_check( double current );
static void keithley228a_get_corrected_current( double c, double *psc,
												double *dacv );


typedef struct {
	int device;               /* GPIB number of the device */
	bool state;               /* STANDBY or OPERATE */
	const char *lockin_name;  /* name of the lock-in to use */
	int lockin_dac_port;      /* number of the DAC port of the lock-in the
								 modulation input is connected to */
	double current;           /* actual current output by the power supply */
	double req_current;       /* the start current given by the user */
	double current_step;      /* the current steps to be used */
	bool is_req_current;      /* flag, set if start current is defined */
	bool is_current_step;     /* flag, set if current step size is defined */
	bool use_correction;      /* flag, set if corrections are to be applied */
	char *lockin_append;      /* string to append to lockin function names */
} Keithley228a;


static Keithley228a keithley228a;



/*----------------------------------------------------------------*/
/* Here we check if also a driver for a field meter is loaded and */
/* test if this driver will be loaded before the magnet driver.   */
/*----------------------------------------------------------------*/

int keithley228a_init_hook( void )
{
	Var *func_ptr;
	int acc;
	int i;
	int *first_DAC_port;
	int *last_DAC_port;
	int seq;
	char *fn;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* We need the lock-in driver called *before* the magnet driver since
	   the lock-ins DAC is needed in the initialization of the magnet.
	   Probably we should implement a solution that brings the devices
	   automatically into the correct sequence instead of this hack, but
	   that's not as simple as it might look...
	   If LOCKIN_NAME has not been defined the first available lockin (i.e.
	   the first defined in the devices section) is going to be used,
	   otherwise the one defined by the name (if the name is valid).
	   Then the DAC port number is set, if LOCKIN_DAC isn't set the default
	   value from the array dac_ports is used, otherwise the number defined
	   by LOCKIN_DAC (if the number is valid) */


#if ! defined( LOCKIN_NAME )
	keithley228a.lockin_name = NULL;
	for ( i = 0; lockins[ i ] != NULL; i++ )
		if ( exists_device( lockins[ i ] ) )
		{
			keithley228a.lockin_name = lockins[ i ];
			break;
		}

	if ( keithley228a.lockin_name == NULL )
	{
		eprint( FATAL, UNSET, "%s: Can't find a driver for a lock-in "
				"amplifier with a DAC.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}
#else
	keithley228a.lockin_name = string_to_lower( T_strdup( LOCKIN_NAME ) );

	for ( i = 0; lockins[ i ] != 0; i++ )
		if ( ! strcmp( keithley228a.lockin_name, lockins[ i ] ) )
			 break;

	if ( lockins[ i ] == NULL )
	{
		eprint( FATAL, SET, "%s: Unknown lock-in amplifier '%s' specified for "
				"use with power supply.\n", DEVICE_NAME, LOCKIN_NAME );
		THROW( EXCEPTION )
	}

	if ( ! exists_device( keithley228a.lockin_name ) )
	{
		eprint( FATAL, SET, "%s: Module for lock-in amplifier '%s' needed by "
				"the magnet power supply isn't loaded.\n", DEVICE_NAME,
				keithley228a.lockin_name );
		
		THROW( EXCEPTION )
	}
#endif

	/* Now we've got to figure out what kind of string we need to append to
	   lock-in functions, i.e. if the lock-in to be used is the first, second
	   etc. lock-in module loaded. Only if it is the first lock-in nothing
	   got to be appended, otherwise a string of the form "#n", were n is the
	   lock-in number. */

	if ( ( seq = get_lib_number( keithley228a.lockin_name ) ) > 1 )
		keithley228a.lockin_append = get_string( "#%d", seq );
		keithley228a.lockin_append = T_strdup( "" );

	/* Set the DAC port to be used and check that it is a valid number */

	keithley228a.lockin_dac_port = -1;

#if ! defined( LOCKIN_DAC )
	keithley228a.lockindac_port = dac_ports[ i ];
#else
	keithley228a.lockin_dac_port = LOCKIN_DAC;
#endif

	if ( get_lib_symbol( keithley228a.lockin_name, "first_DAC_port",
						 ( void ** ) &first_DAC_port ) != LIB_OK ||
		 get_lib_symbol( keithley228a.lockin_name, "last_DAC_port",
						 ( void ** ) &last_DAC_port ) != LIB_OK )
	{
		eprint( FATAL, SET, "%s: Can't find necessary symbols in library "
				"for lock-in amplifier '%s'.\n",
				DEVICE_NAME, keithley228a.lockin_name );
		THROW( EXCEPTION )
	}

	if ( keithley228a.lockin_dac_port < *first_DAC_port ||
		 keithley228a.lockin_dac_port > *last_DAC_port )
	{
		eprint( FATAL, SET, "%s: Invalid DAC port number %d, valid range "
				"for lock-in '%s' is %d to %d\n", DEVICE_NAME,
				keithley228a.lockin_dac_port,
				keithley228a.lockin_name, *first_DAC_port, *last_DAC_port );
		THROW( EXCEPTION )
	}

	/* Check if a function for setting the DAC port exists */

	fn = get_string( "lockin_dac_voltage%s", keithley228a.lockin_append );
	if ( ( func_ptr = func_get( fn, &acc ) ) == NULL )
	{
		eprint( FATAL, SET, "%s: No lock-in amplifier module loaded "
				"supplying a function for setting a DAC.\n", DEVICE_NAME );
		T_free( fn );
		THROW( EXCEPTION )
	}
	T_free( fn );
	vars_pop( func_ptr );

	/* Unset some flags in the power supplies structure */

	keithley228a.is_req_current  = UNSET;
	keithley228a.is_current_step = UNSET;
	keithley228a.use_correction  = UNSET;

	return 1;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

int keithley228a_exp_hook( void )
{
	Cur_Func = "keithley228a_exp_hook";
	keithley228a_init( DEVICE_NAME );
	Cur_Func = NULL;
	return 1;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

int keithley228a_end_of_exp_hook( void )
{
	Cur_Func = "keithley228a_end_of_exp_hook";
	keithley228a_to_local( );
	Cur_Func = NULL;
	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void keithley228a__exit_hook( void )
{
	keithley228a.lockin_append = T_free( keithley228a.lockin_append );
}


/***********************************************************************/
/*                                                                     */
/*           exported functions, i.e. EDL functions                    */
/*                                                                     */
/***********************************************************************/


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *magnet_name( Var *v )
{
	v = v;
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------------*/
/* Function for registering the start current and the current step size. */
/*-----------------------------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	double cur;
	double cur_step;


	/* Check that both variables are reasonable */

	if ( v->next == NULL )
	{
		eprint( FATAL, SET, "%s: Missing parameter in call of function "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	cur = get_double( v, "magnet current", DEVICE_NAME );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		eprint( FATAL, SET, "%s: Missing magnet current step size in call "
				"of %s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	cur_step = get_double( v, "magnet current step width", DEVICE_NAME );

	/* Check that new field value is still within bounds */

	keithley228a_current_check( cur );

	keithley228a.req_current = cur;
	keithley228a.current_step = cur_step;
	keithley228a.is_req_current = keithley228a.is_current_step = SET;

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

Var *magnet_use_correction( Var *v )
{
	v = v;

	keithley228a.use_correction = SET;
	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

Var *magnet_use_dac_port( Var *v )
{
	int port;
	int *first_DAC_port;
	int *last_DAC_port;


	port = ( int ) get_long( v, "DAC port", DEVICE_NAME );

	if ( get_lib_symbol( keithley228a.lockin_name, "first_DAC_port",
						 ( void ** ) &first_DAC_port ) != LIB_OK ||
		 get_lib_symbol( keithley228a.lockin_name, "last_DAC_port",
						 ( void ** ) &last_DAC_port ) != LIB_OK )
	{
		eprint( FATAL, SET, "%s: Can't find necessary symbols in library "
				"for lock-in amplifier '%s' in function %s().\n",
				DEVICE_NAME, keithley228a.lockin_name, Cur_Func );
		THROW( EXCEPTION )
	}

	if ( port < *first_DAC_port || port > *last_DAC_port )
	{
		eprint( FATAL, SET, "%s: Invalid DAC port number %d, valid range "
				"for lock-in '%s' is %d to %d\n", DEVICE_NAME, port,
				keithley228a.lockin_name, *first_DAC_port, *last_DAC_port );
		THROW( EXCEPTION )
	}

	keithley228a.lockin_dac_port = port;
	return vars_push( INT_VAR, ( long ) port );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

Var *set_field( Var *v )
{
	double new_current;
	double cur;


	if ( v == NULL )
	{
		eprint( FATAL, SET, "%s: Missing parameter in function %s().\n",
				DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	cur = get_double( v, "magnet field current", DEVICE_NAME );

	/* Check the new current value and reduce it if necessary */

	new_current = keithley228a_current_check( cur );

	too_many_arguments( v, DEVICE_NAME );

	return vars_push( FLOAT_VAR,
					  keithley228a_goto_current( new_current ) );
}


/*------------------------------------------------*/
/* Convenience function: just returns the current */
/*------------------------------------------------*/

Var *get_field( Var *v )
{
	v = v;
	return vars_push( FLOAT_VAR, keithley228a.current );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *sweep_up( Var *v )
{
	double new_current;


	v = v;

	if ( ! keithley228a.is_current_step )
	{
		eprint( FATAL, SET, "%s: Current step size has not been defined in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	/* Check the new current value and reduce it if necessary */

	new_current = keithley228a_current_check( keithley228a.current
											  + keithley228a.current_step );

	return vars_push( FLOAT_VAR,
					  keithley228a_goto_current( new_current ) );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *sweep_down( Var *v )
{
	double new_current;


	v = v;

	if ( ! keithley228a.is_current_step )
	{
		eprint( FATAL, SET, "%s: Current step size has not been defined in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	/* Check the new current value and reduce it if necessary */

	new_current = keithley228a_current_check( keithley228a.current
											  - keithley228a.current_step );

	return vars_push( FLOAT_VAR,
					  keithley228a_goto_current( new_current ) );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

Var *reset_field( Var *v )
{
	v = v;

	if ( ! keithley228a.is_req_current )
	{
		eprint( FATAL, SET, "Start current has not been defined in "
				"%s().\n", DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION )
	}

	return vars_push( FLOAT_VAR,
					  keithley228a_goto_current( keithley228a.req_current ) );
}


/*****************************************************/
/*                                                   */
/*            Internally used functions              */
/*                                                   */
/*****************************************************/

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool keithley228a_init( const char *name )
{
	char cmd[ 100 ];
	char reply[ 100 ];
	long length = 100;


	/* Initialize GPIB communication with the power supply */

	if ( gpib_init_device( name, &keithley228a.device ) == FAILURE )
		return FAIL;

	/* The power supply can be a bit slow to react, so set timeout to 3 s */

	gpib_timeout( keithley228a.device, T3s );

	/* Default settings:
	   1. No voltage modulation (A0)
	   2. Current modulation on (C1)
	   3. Sink mode off (S0)
	   4. Trigger setting: Start on X (T4)
	   5. Send EOI, do not hold off bus while executing command (K2)
	   6. Service requests disabled (M0)
	   7. Display shows volts and amps (D0)
	   8. Range 10 V, 10 A (R6)
	   9. Volt and amp readings without prefix (G5)
	   The final 'X' is the execute command.
	*/

	strcpy( cmd, "A0C1S0T4K2M0D0R6G5X\r\n");
	if ( gpib_write( keithley228a.device, cmd, strlen( cmd ) ) == FAILURE )
		keithley228a_gpib_failure( );

	/* Set maximum voltage to 5 V */

	sprintf( cmd, "V%.2fX\r\n", KEITHLEY228A_MAX_VOLTAGE );
	if ( gpib_write( keithley228a.device, cmd, strlen( cmd ) ) == FAILURE )
		keithley228a_gpib_failure( );

	/* Get state of power supply and switch state to OPERATE */

	length = 100;
	if ( gpib_write( keithley228a.device, "U0X\r\n", 5 ) == FAILURE ||
		 gpib_read( keithley228a.device, reply, &length ) == FAILURE )
		keithley228a_gpib_failure( );

	keithley228a.state = ( reply[ 1 ] == '1')  ? OPERATE : STANDBY;

	keithley228a.state = keithley228a_set_state( OPERATE );

	/* If a start current has been set tell the power supply about it */

	if ( keithley228a.is_req_current )
		keithley228a.current =
			keithley228a_goto_current( keithley228a.req_current );

	return OK;
}


/*-----------------------------------------------------------------------*/
/* Brings the power supply back into local state after sweeping down the */
/* current down to 0 A.                                                  */
/*-----------------------------------------------------------------------*/

static void keithley228a_to_local( void )
{
	/* Go to STANDBY state and switch off current modulation */

	keithley228a_set_state( STANDBY );
	if ( gpib_write( keithley228a.device, "C0X\r\n", 5 ) == FAILURE )
		keithley228a_gpib_failure( );
	gpib_local( keithley228a.device );
}


/*-----------------------------------------------------------------------*/
/* Sets the power supply in either STANDBY or OPERATE mode. If switching */
/* into STANDBY mode the current is first reset to 0 A.                  */
/*-----------------------------------------------------------------------*/

static bool keithley228a_set_state( bool new_state )
{
	char reply[ 100 ];
	long length = 100;
	double dummy;


	if ( FSC2_MODE == TEST )
		return new_state;

	/* If the state is already the required state do nothing except getting
	   the current setting if in OPERATE state so we know the real current */

	if ( keithley228a.state == new_state )
	{
		if ( keithley228a.state == OPERATE )
		{
			length = 100;
			if ( gpib_read( keithley228a.device, reply, &length ) == FAILURE )
				keithley228a_gpib_failure( );
			sscanf( reply, "%lf,%lf", &dummy, &keithley228a.current);
		}
		return keithley228a.state;
	}

	if ( new_state == STANDBY )
	{
		/* To be on the safe side first get the current, and if it's not zero
		   yet sweep it down to 0 A - otherwise there could appear large
		   voltages across the terminals induced by the decaying field in
		   the sweep coil ! */

		length = 100;
		if ( gpib_read( keithley228a.device, reply, &length ) == FAILURE )
			keithley228a_gpib_failure( );
		sscanf( reply, "%lf,%lf", &dummy, &keithley228a.current);

		if ( keithley228a.current != 0.0 )
			keithley228a.current = keithley228a_goto_current( 0.0 );
	}
	else
	{
		/* Set current to 0 A before switching power supply to OPERATE
		   state (output terminals are still disconnected) */

		if ( gpib_write( keithley228a.device, "I0.0X\r\n", 7 ) == FAILURE )
			keithley228a_gpib_failure( );
		keithley228a.current = 0.0;
	}

	/* Set the new state */

	if ( gpib_write( keithley228a.device, new_state == STANDBY ?
					 "F0X\r\n" : "F1X\r\n", 5 ) == FAILURE )
		keithley228a_gpib_failure( );

	/* When switching to STANDBY the power supply needs about half a second
	   of timeout */

	if ( new_state == STANDBY )
		usleep( 500000 );

	return keithley228a.state = new_state;
}


/*-------------------------------------------------------------------*/
/* In principle, the Keithley power supply is not supposed to drive  */
/* large inductive loads. On the other hand, the sweep coil of the   */
/* magnet has an inductance of about 4 H and a very low resistance   */
/* (mainly just due to the leads). The inductance reaction voltage   */
/* (the voltage induced by the coil due to the current change as     */
/* imposed by the power supply) and given by the product of the      */
/* inductance of the coil, multiplied by the currents rate of        */
/* change, should not exceed 10 V for the normally used current      */
/* range of the power supply of 10 A. This means that the maximum    */
/* rate of current change must be below 2.5 A/s, or, to be on the    */
/* safe side, below 1 A/s (defined as KEITHLEY228A_MAX_SWEEP_SPEED). */
/* While I think, that because of the output voltage being limited   */
/* to 5 V also the inductance reaction voltage can't be larger, I    */
/* prefer to be real careful and thus setting the new current is     */
/* done in several steps of changes of 0.1 A, after which the power  */
/* supply is given a settling time of 100 ms.                        */
/* The only situation where this could become critical is when       */
/* switching from OPERATE to STANDBY state. In this case the voltage */
/* applied to the terminals and the current are suddenly set to zero */
/* which would lead to huge induced voltages. Therefore one has to   */
/* be very careful to sweep the current down to 0 A before switching */
/* to STANDBY state !                                                */
/*-------------------------------------------------------------------*/

static double keithley228a_goto_current( double new_current )
{
	double del_amps;
	double act_amps;
	char reply[ 100 ];
	long length = 100;
	double dummy;
	int max_tries = 100;
	bool do_test;

	
	fsc2_assert( fabs( new_current ) <= KEITHLEY228A_MAXMAX_CURRENT );

	/* Nothing really to be done in a test run */

	if ( FSC2_MODE == TEST )
		return keithley228a.current = new_current;

	do_test =
		fabs( keithley228a.current - new_current ) > KEITHLEY228A_MAX_JUMP;

	/* Calculate the size of the current steps */

	del_amps = 0.1 * KEITHLEY228A_MAX_SWEEP_SPEED
		       * ( keithley228a.current > new_current ) ? -1.0 : 1.0;

	/* Use as many current steps as necessary to get near to the final current
	   and wait 100 ms after each step */

	while ( fabs( new_current - keithley228a.current ) > fabs( del_amps ) )
	{
		keithley228a.current += del_amps;
		keithley228a_set_current( keithley228a.current );
		usleep( 100000 );
	}

	/* Do the final step (smaller than the previously used current steps)
	   after we're done we still need to wait soem small amount of time -
	   otherwise the Keithley starts acting up... */

	keithley228a.current = keithley228a_set_current( new_current );
	usleep( 100000 );

	/* Wait for the current to stabilize at the requested value (checking
	   also the voltage to go down to zero won't do because there is some
	   resistance due to the leads which forces the power supply to maintain
	   a small voltage, depending on the current) */

	if ( do_test )
	{
		do
		{
			usleep( 100000 );
			length = 100;
			if ( gpib_read( keithley228a.device, reply, &length ) == FAILURE )
				keithley228a_gpib_failure( );
			sscanf( reply, "%lf,%lf", &dummy, &act_amps );
		} while ( fabs( act_amps - keithley228a.current ) > 0.05 &&
				  max_tries-- > 0 );

		if ( max_tries < 0 )
		{
			eprint( FATAL, UNSET, "%s: Can't set requested current in %s().\n",
					DEVICE_NAME, Cur_Func );
			THROW( EXCEPTION )
		}
	}

	return keithley228a.current;
}


/*--------------------------------------------------------------------*/
/* In the normally used range of 10 V and 10 A the current resolution */
/* of the power supply is just 10 mA. This would limit the field      */
/* resolution to about 1 G which isn't good enough. Therefore, an     */
/* additional voltage is applied to the current modulation input of   */
/* the power supply. This additional voltage is created by one of the */
/* DACs of the lock-in amplifier. Because the voltages needed are in  */
/* the 100 mV range but the output of the DAC is in the 10 V range    */
/* (with a 12 bit resolution) the DAC is connected to the modulation  */
/* input with a parallel resistor of 100 Ohm (the exact value plus a  */
/* sign for taking care of the correct polarity) is defined by        */
/* V_TO_A_FACTOR.                                                     */
/* Another problem is that very near to 0 A (below about 40 mA) the   */
/* power supply doesn't work correctly anymore except when the        */
/* current is set via the current modulation input.                   */
/*--------------------------------------------------------------------*/

static double keithley228a_set_current( double new_current )
{
	double power_supply_current;
	double dac_volts;
	char cmd[ 100 ];
	Var *func_ptr;
	int acc;
	char *fn;


	fsc2_assert( fabs( new_current ) <= KEITHLEY228A_MAXMAX_CURRENT );

	if ( ! keithley228a.use_correction )
	{
		if ( fabs( new_current ) >= 0.04 )
		{
			if ( new_current >= 0.0)
			{
				power_supply_current = 1.0e-2 * floor( 1.0e2 * new_current );
				if ( fabs( power_supply_current - new_current ) > 0.01 )
					power_supply_current += 1.0e-2;
			}
			else
			{
				power_supply_current = 1.0e-2 * ceil( 1.0e2 * new_current );
				if ( fabs( power_supply_current - new_current ) > 0.01 )
					power_supply_current -= 1.0e-2;
			}

			dac_volts = V_TO_A_FACTOR
				        * fabs( power_supply_current - new_current );
		}
		else
		{
			if ( new_current > 0.0 )
			{
				power_supply_current = 0.04;
				dac_volts = V_TO_A_FACTOR * ( new_current - 0.04 );
			}
			else
			{
				power_supply_current = -0.04;
				dac_volts = - V_TO_A_FACTOR * ( new_current + 0.04 );
			}
		}
	}
	else
		keithley228a_get_corrected_current( new_current, &power_supply_current,
											&dac_volts );

	/* Set current on power supply */

	sprintf( cmd, "I%.2fX\r\n", power_supply_current );
	if ( gpib_write( keithley228a.device, cmd, strlen( cmd ) ) == FAILURE )
		keithley228a_gpib_failure( );

	/* Set the voltage on the lock-ins DAC - the function needs two arguments,
	   the port number and the voltage */

	fn = get_string( "lockin_dac_voltage%s", keithley228a.lockin_append );
	func_ptr = func_get( fn, &acc );
	T_free( fn );
	vars_push( INT_VAR, keithley228a.lockin_dac_port );
	vars_push( FLOAT_VAR, dac_volts );
	vars_pop( func_call( func_ptr ) );

	return new_current;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void keithley228a_gpib_failure( void )
{
	eprint( FATAL, UNSET, "%s: Communication with device failed.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double keithley228a_current_check( double current )
{

	if ( fabs( current ) > KEITHLEY228A_MAX_CURRENT )
	{
		if ( FSC2_MODE == TEST )
		{
			eprint( FATAL, UNSET, "%s: Magnet current of %.2f A out of range "
					"in %s().\n", DEVICE_NAME, current, Cur_Func );
			THROW( EXCEPTION )
		}

		eprint( FATAL, SET, "%s: Magnet current of %.2f A out of range "
				"in %s().\n", DEVICE_NAME, current, Cur_Func );
		return current > 0.0 ? 10.0 : -10.0;
	}

	return current;
}


/*--------------------------------------------------------------------------*/
/* The following function for calculating corrections to reduce non-        */
/* linearities and small jumps has been directly taken from the previous    */
/* program. Here's the comment from this program about the rationale and    */
/* the way it is done (sorry, to lazy to translate it especially since I'm  */
/* not sure if it still is a reasonable approach and if the data used are   */
/* still valid):                                                            */
/* Nun zu den weiteren Feinheiten : Leider stellte sich heraus, dass das    */
/* Power Supply zwar sein Spezifikationen erfuellt, aber eben nur gerade.   */
/* Die erreichte Genauigkeit von 10 mA beim Sweepbereich von 10 A ist	    */
/* aber bei weitem nicht ausreichend, da dies bereits einem Fehler von	    */
/* ca. 1 G entspricht. Allerdings sind die Abweichungen von der Lineari-    */
/* taet einigermassen reproduzierbar, was dazu genutzt wird, diese Nicht-   */
/* linearitaeten per Software durch Anlegen der passenden Spannungen am	    */
/* Modulationseingang des Power Supplys wieder auszugleichen.			    */
/* Die Nichtlinearitaeten bestehen aus zwei Teilen, einmal einer langsamen  */
/* Drift in Abhaengigkeit vom gesetzten Strom und vielen ca. 3 mA grossen   */
/* Stromspruengen, die in (nicht ganz regelmaessigen) Abstaenden von 0.12 A	*/
/* bis 0.14 A auftreten.												    */
/* Die langsame Drift wurde mehrfach gemessen und dann die sich ergebende   */
/* Kurve stueckweise durch Gerade approximiert - die Endpunkte dieser In-   */
/* tervalle sind im Array 'ranges' gespeichert, der zu den jeweiligen In-   */
/* tervallen gehoerende Anstieg und Offset der angepassten Geraden in den   */
/* beiden Arrays 'slopes' und 'offsets'. Bei der Berechnung der vom DAC	    */
/* auszugebenden Spannung wird zuerst bestimmt, in welches Intervall der zu */
/* setzende Strom gehoert und anschliessend die zugehoerige Korrektur auf   */
/* die Spannung fuer den DAC 'dac_volts' aufgeschlagen.					    */
/* Fuer die oben genannten kleinen Spruenge stellte sich heraus, dass diese */
/* bei negativen Stroemen immer in Abstaenden von 0.12 A oder 0.13 A auf-	*/
/* traten, und zwar jeweils mehrere Spruenge in Abstaenden von 0.13 A, ge-  */
/* folgt von einem Sprung nach 0.12 A, die Amplitude der Spruenge betraegt  */
/* im negativen Strombereich 3.2 mA. Im positiven Strombereich traten	    */
/* ebenfalls jeweils mehrere Spruenge in 0.13 A Abstaenden auf, gefolgt von */
/* einem im Abstand von 0.14 A. Die Amplitude der Spruenge betraegt 3.0 mA. */
/* Genauer gesagt heisst das, dass der Sweep nicht linear ist, sondern zwi- */
/* schen den Sprungpunkten der Sweep nicht steil genug ist, was dann	    */
/* durch den Stromsprung ausgeglichen ist -	der Sweep stellt also mehr	    */
/* oder minder eine Art Treppenfunktion dar, mit allerdings nicht waage-    */
/* rechten 'Treppenstufen'.												    */
/* Hier die vollstaendige Liste der am Power Supply gesetzten Stroeme, bei  */
/* denen Spruenge im ausgegebenen Strom auftraten :						    */
/* -9.98, -9.85, -9.72, -9.60, -9.47, -9.35, -9.22, -9.09, -8.97, -8.84,    */
/* -8.71, -8.59, -8.46, -8.34, -8.21, -8.08, -7.96, -7.83, -7.70, -7.58,    */
/* -7.45, -7.33, -7.20, -7.07, -6.95, -6.82, -6.69, -6.57, -6.44, -6.32,    */
/* -6.19, -6.06, -5.94, -5.81, -5.69, -5.56, -5.43, -5.31, -5.18, -5.05,    */
/* -4.93, -4.80, -4.68, -4.55, -4.42, -4.30, -4.17, -4.04, -3.92, -3.79,    */
/* -3.67, -3.54, -3.41, -3.29, -3.16, -3.03, -2.91, -2.78, -2.66, -2.53,    */
/* -2.40, -2.28, -2.15, -2.02, -1.90, -1.77, -1.65, -1.52, -1.39, -1.27,    */
/* -1.14, -1.01, -0.89, -0.76, -0.64, -0.51, -0.38, -0.26, -0.13,  0.00,    */
/* 0.14, 0.27, 0.40, 0.53, 0.66, 0.79, 0.92, 1.06, 1.19, 1.32, 1.45,	    */
/* 1.58, 1.71, 1.84, 1.97, 2.11, 2.24, 2.37, 2.50, 2.63, 2.76, 2.89,	    */
/* 3.02, 3.16, 3.29, 3.42, 3.55, 3.68, 3.81, 3.94, 4.07, 4.21, 4.34,	    */
/* 4.47, 4.60, 4.73, 4.86, 4.99, 5.12, 5.26, 5.39, 5.52, 5.65, 5.78,	    */
/* 5.91, 6.04, 6.18, 6.31, 6.44, 6.57, 6.70, 6.83, 6.96, 7.09, 7.23,	    */
/* 7.36, 7.49, 7.62, 7.75, 7.88, 8.01, 8.14, 8.28, 8.41, 8.54, 8.67,	    */
/* 8.80, 8.93, 9.06, 9.19, 9.33, 9.46, 9.59, 9.72, 9.85, 9.98			    */
/* In den beiden Listen 'neg_jumps' und 'pos_jumps' sind jeweils die	    */
/* Punkte gespeichert, bei denen sich die Abstaende zwischen den Spruengen  */
/* aendern. Hieraus wird dann die notwendige Korrektur berechnet. Im Be-	*/
/* reich direkt um Null wird keine Korrektur vorgenommen, da dort die	    */
/* Nichtlinearitaeten des Power Supply selbst mit allem Tricks nicht aus-   */
/* zugleichen sind.														    */
/*--------------------------------------------------------------------------*/

static void keithley228a_get_corrected_current( double c, double *psc,
												double *dacv )
{
	int i;
	double del = 0.0;

	static double
		ranges[ ] =    { -7.5, -5.5, -4.5, -1.7, -0.007, 0.004, 5.3, 7.2,
						 9.0 },
		slopes[ ] =    { 0.0010, 0.000916, 0.000441, -0.000444, -0.001576,
						 0.0, -0.001027, 0.001396, 0.00429, 0.005472 },
		offsets[ ] =   { 0.00793, 0.007257, 0.004831, 0.000866, -0.000962,
						 0.0, 0.000252, -0.012237, -0.033361, -0.043436 },
	    pos_jumps[ ] = { 0.00, 0.14, 0.92, 1.06, 1.97, 2.11, 3.02, 3.16,
					    4.07, 4.21, 5.12, 5.26, 6.04, 6.18, 7.09, 7.23,
					    8.14, 8.28, 9.19, 9.33, 10.0, 100.0 },
	    neg_jumps[ ] = { 0.00, -0.13, -0.26, -0.38, -0.64, -0.76, -0.89,
						 -1.01, -1.27, -1.39, -1.65, -1.77, -1.90, -2.02,
						 -2.28, -2.40, -2.66, -2.78, -2.91, -3.03, -3.29,
						 -3.41, -3.67, -3.79, -3.92, -4.04, -4.30, -4.42,
						 -4.68, -4.80, -4.93, -5.05, -5.31, -5.43, -5.69,
						 -5.81, -5.94, -6.06, -6.32, -6.44, -6.57, -6.69,
						 -6.95, -7.07, -7.33, -7.45, -7.58, -7.70, -7.96,
						 -8.08, -8.34, -8.46, -8.59, -8.71, -8.97, -9.09,
						 -9.35, -9.47, -9.60, -9.72, -10.0, -100.0 };


	 if ( fabs( c ) >= 0.04 )
	 {
		 if ( c >= 0.0)
		 {
			 *psc = 1.0e-2 * floor( 1.0e2 * c );
			 if ( fabs( *psc - c ) > 9.9999999999e-3)
				 *psc += 1.0e-2;
		 }
		 else
		 {
			 *psc = 1.0e-2 * ceil( 1.0e2 * c );
			 if ( fabs( *psc - c ) > 9.9999999999e-3 )
				 *psc -= 1.0e-2;
		 }

		 *dacv = V_TO_A_FACTOR * fabs( *psc - c );
	 }
	 else
	 {
		 if ( c >= 0.0 )
		 {
			 if ( c < 0.001)
				 c = 0.001;
			 *psc = 0.04;
			 *dacv = V_TO_A_FACTOR * ( c - 0.04 );
		 }
		 else
		 {
			 if ( c > -0.007)
				 c = -0.007;
			 *psc = - 0.04;
			 *dacv = - V_TO_A_FACTOR * ( c + 0.04 );
		 }
	 }
	 
	 /* Try to correct for non-linearities */

	 for ( i = 0; i < 9; i++ )
		 if ( c < ranges[ i ] )
			 break;
			
	 if ( c >= 0.0 )
		 *dacv -= V_TO_A_FACTOR * ( slopes[ i ] * c + offsets[ i ] );
	 else
		 *dacv += V_TO_A_FACTOR * ( slopes[ i ] * c + offsets[ i ] );


	 /* Try to correct for the 3 mA jumps */

	 if ( c >= 0.0 )
	 {
		 i = 1;
		 while ( lrnd( 1.0e6 * c ) >= lrnd( 1.0e6 * pos_jumps[ i ] ) )
			 i++;

		 c -= pos_jumps[ i - 1 ];
		 if ( i & 1 )
			 del = 0.14;
		 else
			 c = fmod( lrnd( 1.0e6 * c ) * 1.0e-6, del = 0.13 );
		 *dacv += V_TO_A_FACTOR * 0.0030 * ( c / del - 0.5 );
		 *dacv -= V_TO_A_FACTOR * 0.004;
	 }
	 else
	 {
		 i = 0;
		 do
		 {
			 if ( lrnd( 1.0e6 * c ) <= lrnd( 1.0e6 * neg_jumps[ i ] ) &&
				  lrnd( 1.0e6 * c ) > lrnd( 1.0e6 * neg_jumps[ i + 1 ] ) )
			 {
				 c -= neg_jumps[ i ];
				 if ( i & 1 )
					 c = fmod( 1.0e-6 * lrnd( 1.0e6 * c ), del = 0.13 );
				 else
					 del = 0.12;
				 break;
			 }
		 } while ( ++i );

		 *dacv -= V_TO_A_FACTOR * 0.0032 * ( c / del + 0.5 );
		 *dacv -= V_TO_A_FACTOR * 0.004;
	 }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
