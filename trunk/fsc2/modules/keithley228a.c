/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


/* Exported functions */

int keithley228a_init_hook( void );
int keithley228a_exp_hook( void );
int keithley228a_end_of_exp_hook( void );
void keithley228a_exit_hook( void );

Var *magnet_setup( Var *v );
Var *set_current( Var *v );
Var *sweep_up( Var *v );
Var *sweep_down( Var *v );
Var *reset_current( Var *v );



static double keithley228a_current_check( double current, bool *err_flag );
static bool keithley228a_init( const char *name );
static void keithley228a_gpib_failure( void );



#define DEVICE_NAME "KEITHLEY228A"         /* name of device */

#define KEITHLEY228A_MAX_CURRENT     10.0  /* admissible current range */
#define KEITHLEY228A_MAX_SWEEP_SPEED  1.0  /* max. current change per second */

#define V_TO_A_FACTOR	      -97.5   /* Conversion factor of voltage at DAC */


#define	STANDBY  ( ( bool ) 0 )      
#define OPERATE  ( ( bool ) 1 )
	


typedef struct {
	int device;               /* GPIB number of the device */

	bool state;               /* STANDBY (0) or OPERATE (1) */

	double current;           /* the start current given by the user */
	double current_step;      /* the current steps to be used */

	bool is_current;          /* flag, set if start current is defined */
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


	/* Check if there's a lock-in amplifier with a DAC */

	if ( ! exist_device( "sr510" ) &&
		 ! exist_device( "sr530" ) &&
		 ! exist_device( "sr810" ) &&
		 ! exist_device( "sr830" ) )
	{
		eprint( FATAL, "%s: Can't find a lock-in amplifier with a DAC.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We need the lock-in driver called *before* the magnet driver since
	   the lock-ins DAC is needed in the initialization of the magnet.
	   Probably we should implement a solution that brings the devices
	   automatically into the correct sequence instead of this hack, but
	   that's not as simple as it might look... */

	if ( ( func_ptr = func_get( "lockin_dac_voltage", &access ) ) == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: No lock-in amplifier module loaded "
				"supplying a function for setting a DAC.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}
	vars_pop( func_ptr );

	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Set some variables in the magnets structure */

	keithley228a.is_current = UNSET;
	keithley228a.is_current_step = UNSET;

	return 1;
}


int keithley228a_exp_hook( void )
{
	keithley228a_init( DEVICE_NAME );
	return 1;
}


int keithley228a_end_of_exp_hook( void )
{
	return 1;
}



/*-----------------------------------------------------------------------*/
/* Function for registering the start current and the current step size. */
/*-----------------------------------------------------------------------*/

Var *magnet_setup( Var *v )
{
	/* Check that both variables are reasonable */

	if ( v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Missing parameter in call of "
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

	keithley228a_current_check( VALUE( v ), NULL );

	keithley228a.current = VALUE( v );
	keithley228a.current_step = VALUE( v->next );
	keithley228a.is_current = keithley228a.is_current_step = SET;

	return vars_push( INT_VAR, 1 );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static double keithley228a_current_check( double current, bool *err_flag )
{

	if ( fabs( current ) > KEITHLEY228A_MAX_CURRENT )
	{
		if ( ! TEST_RUN )
		{
			eprint( FATAL, "%s:%ld: %s: Magnet current %f A out of range.\n",
					Fname, Lc, DEVICE_NAME, current );
			*err_flag = SET;
			return current > 0.0 ? 10.0 : -10.0;
		}

		eprint( FATAL, "%s: Magnet current %f A out of range.\n",
				DEVICE_NAME, current );
		THROW( EXCEPTION );
	}

	return current;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool keithley228a_init( const char *name )
{
	char cmd[ 100 ];
	char reply[ 100 ];
	long length = 100;


	/* Initialize the power supply */

	if ( gpib_init_device( name, &keithley228a.device ) == FAILURE )
		return FAIL;

	/* The power supply can be a bit slow to react... */

	gpib_timeout( keithley228a.device, T3s );

	/* Default settings:
	 * 1. No voltage modulation (A0)
	 * 2. Current modulation on (C1)
	 * 3. Sink mode off (S0)
	 * 4. Trigger setting: Start on X (T4)
	 * 5. Send EOI, do not hold off bus on X (K2)
	 * 6. Service requests disabled (M0)
	 * 7. Display shows volts and amps (D0)
	 * 8. Volt and amp readings without prefix (G5)
	 (The final 'X' is the execute command)
	*/

	strcpy( cmd, "A0C1S0T4K2M0D0G5X\r\n");
	if ( gpib_write( keithley228a.device, cmd, strlen( cmd ) ) == FAILURE )
		keithley228a_gpib_failure( );

	/* Get state of power supply */

	if ( gpib_write( keithley228a.device, "U0X\r\n", 5 ) == FAILURE ||
		 gpib_read( keithley228a.device, reply, &length ) == FAILURE )
		keithley228a_gpib_failure( );

	keithley228a.state = reply[ 0 ] == '1' ? OPERATE : STANDBY;

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void keithley228a_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
	THROW( EXCEPTION );
}
