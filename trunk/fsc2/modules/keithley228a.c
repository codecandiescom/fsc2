/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "KEITHLEY228A"         /* name of device */


/*******************************************************************/
/* Here is defined to which DAC port of the different lock-ins the */
/* modulation input of the power supply is connected to, if this   */
/* needs to be changed just for a few experiments use the function */
/* 'magnet_use_dac_port' instead of changing the defaults and      */
/* and recompiling!                                                */
/*******************************************************************/

static const char *lockins[ ] = { "sr510", "sr530", "sr810", "sr830", NULL };
static       int dac_ports[ ] = { 6,       6,       4,       4      };


#define V_TO_A_FACTOR	      -97.5   /* Conversion factor of voltage at DAC */



#define KEITHLEY228A_MAX_CURRENT     10.0  /* admissible current range */
#define KEITHLEY228A_MAX_VOLTAGE      5.0  /* admissible voltage range */
#define KEITHLEY228A_MAX_SWEEP_SPEED  1.0  /* max. current change per second */

#define KEITHLEY228A_MAXMAX_CURRENT 10.01  /* really maximum current ;-) */


#define	STANDBY  ( ( bool ) 0 )      
#define OPERATE  ( ( bool ) 1 )
	

/* Exported functions */

int keithley228a_init_hook( void );
int keithley228a_exp_hook( void );
int keithley228a_end_of_exp_hook( void );

Var *magnet_setup( Var *v );
Var *magnet_use_correction( Var *v );
Var *magnet_use_dac_port( Var *v );
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
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* We need the lock-in driver called *before* the magnet driver since
	   the lock-ins DAC is needed in the initialization of the magnet.
	   Probably we should implement a solution that brings the devices
	   automatically into the correct sequence instead of this hack, but
	   that's not as simple as it might look...
	   First check if there's a lock-in amplifier with a DAC and get the
	   default DAC port number, then check if a function for setting the
	   DAC port exists */

	keithley228a.lockin_dac_port = -1;
	keithley228a.lockin_name = NULL;
	for ( i = 0; lockins[ i ] != NULL; i++ )
		if ( exist_device( lockins[ i ] ) )
		{
			keithley228a.lockin_name = lockins[ i ];
			keithley228a.lockin_dac_port = dac_ports[ i ];
			break;
		}

	if ( keithley228a.lockin_name == NULL )
	{
		eprint( FATAL, "%s: Can't find a driver for a lock-in amplifier with "
				"a DAC.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( ( func_ptr = func_get( "lockin_dac_voltage", &access ) ) == NULL )
	{
		eprint( FATAL, "%s: No lock-in amplifier module loaded supplying a "
				"function for setting a DAC.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}
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

Var *magnet_use_correction( Var *v )
{
	if ( v != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous parameter in call of "
				"`sweep_up'.\n", Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) )
			;
	}

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


	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
	{
		eprint( WARN, "%s:%ld: %s: Integer value used for DAC port.\n",
				Fname, Lc, DEVICE_NAME );
		port = ( int ) lround( v->val.dval );
	}
	else
		port = ( int ) v->val.lval;

	if ( get_lib_symbol( keithley228a.lockin_name, "first_DAC_port",
						 ( void ** ) &first_DAC_port ) != LIB_OK ||
		 get_lib_symbol( keithley228a.lockin_name, "last_DAC_port",
						 ( void ** ) &last_DAC_port ) != LIB_OK )
	{
		eprint( FATAL, "%s:%ld: %s: Can't find necessary symbols in library "
				"for lock-in amplifier '%s'\n", Fname, Lc, DEVICE_NAME,
				keithley228a.lockin_name );
		THROW( EXCEPTION );
	}

	if ( port < *first_DAC_port || port > *last_DAC_port )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid DAC port number %d, valid range "
				"for lock-in '%s' is %d to %d\n", Fname, Lc, DEVICE_NAME, port,
				keithley228a.lockin_name, *first_DAC_port, *last_DAC_port );
		THROW( EXCEPTION );
	}

	keithley228a.lockin_dac_port = port;
	return vars_push( INT_VAR, ( long ) port );
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
	int max_tries = 100;

	
	assert( fabs( new_current ) <= KEITHLEY228A_MAXMAX_CURRENT );

	/* Nothing really to be done in a test run */

	if ( TEST_RUN )
		return keithley228a.current = new_current;

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

	/* Do the final step (smaller than the previously used current steps) */

	keithley228a.current = keithley228a_set_current( new_current );
	usleep( 100000 );

	/* Wait for the current to stabilize at the requested value (checking
	   also the voltage to go down to zero won't do because there is some
	   resistance due to the leads which forces the power supply to maintain
	   a small voltage, depending on the current) */

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
		eprint( FATAL, "%s: Can't set requested current.\n", DEVICE_NAME );
		THROW( EXCEPTION );
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
	int access;


	assert( fabs( new_current ) <= KEITHLEY228A_MAXMAX_CURRENT );

	if ( ! keithley228a.use_correction )
	{
		if ( fabs( new_current ) >= 0.04 )
		{
			power_supply_current = 1.0e-2 * lround( 1.e2 * new_current );
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
	double del;

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
		 *psc = 1.0e-2 * lround( 1.e-2 * c );
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
		 while ( lround( 1.0e6 * c ) >= lround( 1.0e6 * pos_jumps[ i ] ) )
			 i++;

		 c -= pos_jumps[ i - 1 ];
		 if ( i & 1 )
			 del = 0.14;
		 else
			 c = fmod( lround( 1.0e6 * c ) * 1.0e-6, del = 0.13 );
		 *dacv += V_TO_A_FACTOR * 0.0030 * ( c / del - 0.5 );
		 *dacv -= V_TO_A_FACTOR * 0.004;
	 }
	 else
	 {
		 i = 0;
		 do
		 {
			 if ( lround( 1.0e6 * c ) <= lround( 1.0e6 * neg_jumps[ i ] ) &&
				  lround( 1.0e6 * c ) > lround( 1.0e6 * neg_jumps[ i + 1 ] ) )
			 {
				 c -= neg_jumps[ i ];
				 if ( i & 1 )
					 c = fmod( 1.0e-6 * lround( 1.0e6 * c ), del = 0.13 );
				 else
					 del = 0.12;
				 break;
			 }
		 } while ( ++i );

		 *dacv -= V_TO_A_FACTOR * 0.0032 * ( c / del + 0.5 );
		 *dacv -= V_TO_A_FACTOR * 0.004;
	 }
}
