/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "KEITHLEY228A"         /* name of device */


/*******************************************************************/
/* Here is defined to which DAC port of the different lock-ins the */
/*     modulation input of the power supply is connected           */
/*******************************************************************/

#define	SR510_DAC_PORT 6
#define	SR530_DAC_PORT 6
#define	SR810_DAC_PORT 1
#define	SR830_DAC_PORT 1

#define V_TO_A_FACTOR	      -97.5   /* Conversion factor of voltage at DAC */



#define KEITHLEY228A_MAX_CURRENT     10.0  /* admissible current range */
#define KEITHLEY228A_MAX_VOLTAGE      5.0  /* admissible voltage range */
#define KEITHLEY228A_MAX_SWEEP_SPEED  1.0  /* max. current change per second */


#define	STANDBY  ( ( bool ) 0 )      
#define OPERATE  ( ( bool ) 1 )
	

/* Exported functions */

int keithley228a_init_hook( void );
int keithley228a_exp_hook( void );
int keithley228a_end_of_exp_hook( void );

Var *magnet_setup( Var *v );
Var *set_current( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_current( Var *v );

/* internally used functions */

static bool keithley228a_init( const char *name );
static void keithley228a_to_local(void);
static bool keithley228a_set_state( bool new_state );
static double keithley228a_goto_current( double current );
static double keithley228a_set_current( double current );
static void keithley228a_gpib_failure( void );
static double keithley228a_current_check( double current );



typedef struct {
	int device;               /* GPIB number of the device */

	bool state;               /* STANDBY (0) or OPERATE (1) */

	int lockin_dac_port;      /* number of the DAC port of the lock-in the
								 modulation input is connected to */

	double current;           /* actual current output by the power supply */

	double req_current;       /* the start current given by the user */
	double current_step;      /* the current steps to be used */

	bool is_req_current;      /* flag, set if start current is defined */
	bool is_current_step;     /* flag, set if current step size is defined */

} Keithley228a;


static Keithley228a keithley228a;


/*----------------------------------------------------------------*/
/* Here we check if also a driver for a field meter is loaded and */
/* test if this driver will be loaded before the magnet driver.   */
/*----------------------------------------------------------------*/

int keithley228a_init_hook( void )
{
	Var *func_ptr;
	int access;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Check if there's a lock-in amplifier with a DAC */

	if ( ! exist_device( "sr510" ) &&
		 ! exist_device( "sr530" ) &&
		 ! exist_device( "sr810" ) &&
		 ! exist_device( "sr830" ) )
	{
		eprint( FATAL, "%s: Can't find a driver for a lock-in amplifier with "
				"a DAC.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We need the lock-in driver called *before* the magnet driver since
	   the lock-ins DAC is needed in the initialization of the magnet.
	   Probably we should implement a solution that brings the devices
	   automatically into the correct sequence instead of this hack, but
	   that's not as simple as it might look... */

	if ( ( func_ptr = func_get( "lockin_dac_voltage", &access ) ) == NULL )
	{
		eprint( FATAL, "%s: No lock-in amplifier module loaded supplying a "
				"function for setting a DAC.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	/* Set the port number of the lock-ins DAC the modulation input of
	   the power supply is connected to */

	if ( exist_device( "sr510" ) )
		keithley228a.lockin_dac_port = SR510_DAC_PORT;
	else if ( exist_device( "sr530" ) )
		keithley228a.lockin_dac_port = SR530_DAC_PORT;
	else if ( exist_device( "sr810" ) )
		keithley228a.lockin_dac_port = SR810_DAC_PORT;
	else if ( exist_device( "sr830" ) )
		keithley228a.lockin_dac_port = SR830_DAC_PORT;

	/* Unset some flags in the power supplies structure */

	keithley228a.is_req_current = UNSET;
	keithley228a.is_current_step = UNSET;

	return 1;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

int keithley228a_exp_hook( void )
{
	keithley228a_init( DEVICE_NAME );
	return 1;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

int keithley228a_end_of_exp_hook( void )
{
	keithley228a_to_local( );
	return 1;
}


/***********************************************************************/
/*                                                                     */
/*           exported functions, i.e. EDL functions                    */
/*                                                                     */
/***********************************************************************/


/*-----------------------------------------------------------------------*/
/* Function for registering the start current and the current step size. */
/*-----------------------------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	/* Check that both variables are reasonable */

	if ( v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in call of function "
				"`magnet_setup'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnet current.\n",
				Fname, Lc, DEVICE_NAME );

	if ( v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing magnet current step size in call "
				"of `magnet_setup'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v->next, INT_VAR | FLOAT_VAR );
	if ( v->next->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnet current "
				"step width.\n", Fname, Lc, DEVICE_NAME );

	/* Check that new field value is still within bounds */

	keithley228a_current_check( VALUE( v ) );

	keithley228a.req_current = VALUE( v );
	keithley228a.current_step = VALUE( v->next );
	keithley228a.is_req_current = keithley228a.is_current_step = SET;

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

Var *set_current( Var *v )
{
	double new_current;


	if ( v == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in function "
				"`set_current'.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used for magnetic field.\n",
				Fname, Lc, DEVICE_NAME );

	/* Check the new current value and reduce it if necessary */

	new_current = keithley228a_current_check( VALUE( v ) );

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"function `set_field'.\n", Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}


	return vars_push( FLOAT_VAR, keithley228a_goto_current( new_current ) );
}


/*-----------------------------------------------------*/
/*-----------------------------------------------------*/

Var *sweep_up( Var *v )
{
	double new_current;


	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"`sweep_up'.\n", Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) )
			;
	}

	if ( ! keithley228a.is_current_step )
	{
		eprint( FATAL, "%s:%ld: %s: Current step size has not been defined.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
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


	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"`sweep_down'.\n", Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) )
			;
	}

	if ( ! keithley228a.is_current_step )
	{
		eprint( FATAL, "%s:%ld: %s: Current step size has not been defined.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Check the new current value and reduce it if necessary */

	new_current = keithley228a_current_check( keithley228a.current
											  - keithley228a.current_step );

	return vars_push( FLOAT_VAR,
					  keithley228a_goto_current( new_current ) );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

Var *reset_current( Var *v )
{
	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of function "
				"`reset_current'.\n", Fname, Lc, DEVICE_NAME );
		while ( ( v = vars_pop( v ) ) )
			;
	}

	if ( ! keithley228a.is_req_current )
	{
		eprint( FATAL, "%s:%ld: %s: Start current has not been defined.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
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

	keithley228a.state = reply[ 0 ] == '1' ? OPERATE : STANDBY;

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


	if ( TEST_RUN )
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

	
	assert( fabs( new_current > 10.0 ) );       /* paranoia ? */

	/* Nothing really to be done in a test run */

	if ( TEST_RUN )
		return keithley228a.current = new_current;

	/* Calculate the size of the current steps */

	del_amps = 0.1 * KEITHLEY228A_MAX_SWEEP_SPEED
		       * keithley228a.current > new_current ? -1.0 : 1.0;

	/* Use as many current steps as necessary to get near to the final current
	   and wait 100 ms after each step */

	while ( fabs( new_current - keithley228a.current) > fabs( del_amps ) )
	{
		keithley228a.current += del_amps;
		keithley228a_set_current( keithley228a.current );
		usleep( 100000 );
	}

	/* Do the final step (smaller than the previously used current steps) */

	keithley228a.current = keithley228a_set_current( new_current );
	usleep( 100000 );

	/* Wait for the current to stabilize to the required value (checking
	   also the voltage to go down to zero won't do because there is some
	   resistance due to the leads which forces the power supply to maintain
	   a small voltage, depending on the current) */

	do
	{
		length = 100;
		if ( gpib_read( keithley228a.device, reply, &length ) == FAILURE )
			keithley228a_gpib_failure( );
		sscanf( reply, "%lf,%lf", &dummy, &act_amps );
	} while ( fabs( act_amps - keithley228a.current ) > 0.05 );
	
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
	int access;


	assert( fabs( new_current > 10.0 ) );       /* paranoia ? */

	if ( fabs( new_current ) >= 0.04 )
	{
		power_supply_current = 1.0e-2 * lround( 1.e-2 * new_current );
		dac_volts = V_TO_A_FACTOR * fabs( power_supply_current - new_current );
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

	/* Set current on power supply */

	sprintf( cmd, "I%.2fX\r\n", power_supply_current );
	if ( gpib_write( keithley228a.device, cmd, strlen( cmd ) ) == FAILURE )
		keithley228a_gpib_failure( );

	/* Set the voltage on the lock-ins DAC - the function needs two arguments,
	   the port number and the voltage */

	func_ptr = func_get( "lockin_dac_voltage", &access );
	vars_push( INT_VAR, keithley228a.lockin_dac_port );
	vars_push( FLOAT_VAR, dac_volts );
	vars_pop( func_call( func_ptr ) );

	return new_current;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void keithley228a_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
	THROW( EXCEPTION );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double keithley228a_current_check( double current )
{

	if ( fabs( current ) > KEITHLEY228A_MAX_CURRENT )
	{
		if ( ! TEST_RUN )
		{
			eprint( FATAL, "%s:%ld: %s: Magnet current of %.2f A out of "
					"range.\n", Fname, Lc, DEVICE_NAME, current );
			return current > 0.0 ? 10.0 : -10.0;
		}

		eprint( FATAL, "%s: Magnet current of %.2f A out of range.\n",
				DEVICE_NAME, current );
		THROW( EXCEPTION );
	}

	return current;
}


