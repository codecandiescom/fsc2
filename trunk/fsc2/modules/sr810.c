/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


/* name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "SR810"

#define NUM_ADC_PORTS         4
#define NUM_DAC_PORTS         4
#define DAC_MAX_VOLTAGE       10.5
#define DAC_MIN_VOLTAGE      -10.5

#define MAX_REF_FREQ          102000.0
#define MIN_REF_FREQ          0.001

#define REF_MODE_INTERNAL     1
#define REF_MODE_EXTERNAL     0

#define MAX_REF_LEVEL         5.0
#define MIN_REF_LEVEL         4.0e-3

#define MAX_HARMONIC          19999
#define MIN_HARMONIC          1

#define NUM_CHANNELS          4        /* 11 would be possible but this doesn't
										  make too much sense... */


/* declaration of exported functions */

int sr810_init_hook( void );
int sr810_exp_hook( void );
int sr810_end_of_exp_hook( void );
void sr810_exit_hook( void );

Var *lockin_get_data( Var *v );
Var *lockin_get_adc_data( Var *v );
Var *lockin_dac_voltage( Var *v );
Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );
Var *lockin_ref_freq( Var *v );
Var *lockin_ref_mode( Var *v );
Var *lockin_ref_level( Var *v );


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
	double ref_freq;
	bool RF;
	double ref_level;
	bool RL;
	long harmonic;
	bool H;
	double dac_voltage[ NUM_DAC_PORTS ];
} SR810;


static SR810 sr810;

/* lists of valid sensitivity settings */

static double slist[ ] = { 2.0e-9, 5.0e-9, 1.0e-8, 2.0e-8, 5.0e-8, 1.0e-7,
						   2.0e-7, 5.0e-7, 1.0e-6, 2.0e-6, 5.0e-6, 1.0e-5,
						   2.0e-5, 5.0e-5, 1.0e-4, 2.0e-4, 5.0e-4, 1.0e-3,
						   2.0e-3, 5.0e-3, 1.0e-2, 2.0e-2, 5.0e-2, 1.0e-1,
						   2.0e-1, 5.0e-1, 1.0 };

/* list of all available time constants */

static double tcs[ ] = { 1.0e-5, 3.0e-5, 1.0e-4, 3.0e-4, 1.0e-3, 3.0e-3,
						 1.0e-2, 3.0e-2, 1.0e-1, 3.0e-1, 1.0, 3.0, 1.0e1,
						 3.0e1, 1.0e2, 3.0e2, 1.0e3, 3.0e3, 1.0e4, 3.0e4 };


/* declaration of all functions used only within this file */

static bool sr810_init( const char *name );
static double sr810_get_data( void );
static void sr810_get_xy_data( double *data, long *channels,
							   int num_channels );
static double sr810_get_adc_data( long channel );
static double sr810_set_dac_data( long channel, double voltage );
static double sr810_get_sens( void );
static void sr810_set_sens( int Sens );
static double sr810_get_tc( void );
static void sr810_set_tc( int TC );
static double sr810_get_phase( void );
static double sr810_set_phase( double phase );
static double sr810_get_ref_freq( void );
static double sr810_set_ref_freq( double freq );
static long sr810_get_ref_mode( void );
static long sr810_get_harmonic( void );
static long sr810_set_harmonic( long harmonic );
static double sr810_get_ref_level( void );
static double sr810_set_ref_level( double level );




/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int sr810_init_hook( void )
{
	int i;


	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Reset several variables in the structure describing the device */

	sr810.device = -1;

	sr810.Sens = -1;             /* no sensitivity has to be set at start of */
	sr810.Sens_warn = UNSET;     /* experiment and no warning concerning the */
                                 /* sensitivity setting has been printed yet */
	sr810.P = UNSET;             /* no phase has to be set at start of the */
	                             /* experiment */
	sr810.TC = -1;               /* no time constant has to be set at the */
	sr810.TC_warn = UNSET;       /* start of the experiment and no warning */
	                             /* concerning it has been printed yet */
	sr810.RF = UNSET;            /* no reference frequency has to be set */
	sr810.H = UNSET;             /* no harmonics has to be set */
	sr810.RL = UNSET;            /* no rference level has to b set */

	for ( i = 0; i < NUM_DAC_PORTS; i++ )
		sr810.dac_voltage[ i ] = 0.0;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int sr810_exp_hook( void )
{
	/* Nothing to be done yet in a test run */

	if ( TEST_RUN )
		return 1;

	/* Initialize the lock-in */

	if ( ! sr810_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int sr810_end_of_exp_hook( void )
{
	/* Switch lock-in back to local mode */

	if ( sr810.device >= 0 )
		gpib_local( sr810.device );

	sr810.device = -1;

	return 1;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void sr810_exit_hook( void )
{
//	sr810_end_of_exp_hook( );
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
		if ( TEST_RUN )                  /* return dummy value in test run */
			return vars_push( FLOAT_VAR, 0.0 );
		else
			return vars_push( FLOAT_VAR, sr810_get_data( ) );
	}

	for ( num_channels = i = 0; i < 6; i++ )
	{
		vars_check( v, INT_VAR | FLOAT_VAR );
		if ( v->type == INT_VAR )
			channels[ i ] = v->val.lval;
		else
		{
			eprint( WARN, "%s:%ld: %s: Integer value (parameter #%d) used as "
					"channel number in call of `lockin_get_data'.\n",
					Fname, Lc, DEVICE_NAME, i + 1 );
			channels[ i ] = lround( v->val.dval );
		}

		if ( channels[ i ] < 1 || channels[ i ] > NUM_CHANNELS )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid channel number %ld in call of "
					"`lockin_get_data'.\n",
					Fname, Lc, DEVICE_NAME, channels[ i ] );
			THROW( EXCEPTION );
		}

		num_channels++;

		if ( ( v = vars_pop( v ) ) == NULL )
			break;
	}

	if ( v != NULL )
	{
		eprint( SEVERE, "%s:%ld: %s: More than 6 parameters in call of "
				"`lockin_get_data', discarding superfluous ones.\n",
				Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_TRANS_ARR, data, num_channels );

	/* If we need less than two channels we've got to pass the function an
	   extra value, it expects at least 2 - just take the next channel */

	if ( num_channels == 1 )
	{
		using_dummy_channels = SET;
		channels[ num_channels++ ] = channels[ 0 ] % NUM_CHANNELS + 1;
	}

	sr810_get_xy_data( data, channels, num_channels );

	if ( using_dummy_channels )
		return vars_push( FLOAT_VAR, data[ 0 ] );

	return vars_push( FLOAT_TRANS_ARR, data, num_channels );
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


	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
		eprint( WARN, "%s:%ld: %s: Floating point number used as ADC port "
				"number.\n", Fname, Lc, DEVICE_NAME );

	port = v->type == INT_VAR ? v->val.lval : ( long ) v->val.dval;

	if ( port < 1 || port > NUM_ADC_PORTS )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid ADC channel number (%ld) in "
				"call of 'lockin_get_adc_data', valid channel are in the "
				"range 1-%d.\n", Fname, Lc, DEVICE_NAME, port, NUM_ADC_PORTS );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )                  /* return dummy value in test run */
		return vars_push( FLOAT_VAR, 0.0 );

	return vars_push( FLOAT_VAR, sr810_get_adc_data( port ) );
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
		eprint( FATAL, "%s:%ld: %s: Missing arguments in call of function "
				"`lockin_dac_voltage`.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Get and check the port number */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == FLOAT_VAR )
		eprint( WARN, "%s:%ld: %s: Floating point number used as DAC port "
				"number.\n", Fname, Lc, DEVICE_NAME );

	port = v->type == INT_VAR ? v->val.lval : ( long ) v->val.dval;
	v = vars_pop( v );

	if ( port < 1 || port > NUM_DAC_PORTS )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid DAC channel number (%ld) in "
				"call of 'lockin_dac_voltage', valid channel are in the "
				"range 1-%d.\n", Fname, Lc, DEVICE_NAME, port, NUM_DAC_PORTS );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
		return vars_push( FLOAT_VAR, sr810.dac_voltage[ port - 1 ] );

	/* Get and check the voltage */

	vars_check( v, INT_VAR | FLOAT_VAR );
	voltage = VALUE( v );
	if ( voltage < DAC_MIN_VOLTAGE || voltage > DAC_MIN_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid DAC voltage (%f V) in call of "
				"'lockin_dac_voltage', valid range is between %f V and "
				"%f V.\n", Fname, Lc, DEVICE_NAME, DAC_MIN_VOLTAGE,
				DAC_MAX_VOLTAGE );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		eprint( WARN, "%s:%ld: %s: Superfluous arguments in call of function "
				"`lockin_dac_voltage'.\n", Fname, Lc, DEVICE_NAME );

		while ( ( v = vars_pop( v ) ) != NULL )
			;
	}
	
	sr810.dac_voltage[ port - 1 ] = voltage;

	if ( TEST_RUN || I_am == PARENT )
		return vars_push( FLOAT_VAR, voltage );

	return vars_push( FLOAT_VAR, sr810_set_dac_data( port, voltage ) );
}


/*-------------------------------------------------------------------------*/
/* Returns or sets sensitivity of the lock-in amplifier. If called with no */
/* argument the current sensitivity is returned, otherwise the sensitivity */
/* is set to the argument.                                                 */
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
				eprint( FATAL, "%s:%ld: %s: Function `lockin_sensitivity' "
						"with no argument can only be used in the EXPERIMENT "
						"section.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr810_get_sens( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used as sensitivity.\n",
				Fname, Lc, DEVICE_NAME );
	sens = VALUE( v );
	vars_pop( v );

	if ( sens < 0.0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative sensitivity.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We try to match the sensitivity passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 1 percent, we utter a warning message (but only once). */

	for ( i = 0; i < 25; i++ )
		if ( sens >= slist[ i ] && sens <= slist[ i + 1 ] )
		{
			Sens = i +
				   ( ( slist[ i ] / sens > sens / slist[ i + 1 ] ) ? 0 : 1 );
			break;
		}

	if ( Sens >= 0 &&                                    /* value found ? */
		 fabs( sens - slist[ Sens ] ) > sens * 1.0e-2 && /* error > 1% ? */
		 ! sr810.Sens_warn  )                            /* no warning yet ? */
	{
		if ( sens >= 1.0e-3 )
			eprint( WARN, "%s:%ld: %s: Can't set sensitivity to %.0lf mV, "
					"using %.0lf mV instead.\n", Fname, Lc, DEVICE_NAME,
					sens * 1.0e3, slist[ Sens ] * 1.0e3 );
		else if ( sens >= 1.0e-6 ) 
			eprint( WARN, "%s:%ld: %s: Can't set sensitivity to %.0lf uV, "
					"using %.0lf uV instead.\n", Fname, Lc, DEVICE_NAME,
					sens * 1.0e6, slist[ Sens ] * 1.0e6 );
		else
			eprint( WARN, "%s:%ld: %s: Can't set sensitivity to %.0lf nV, "
					"using %.0lf nV instead.\n", Fname, Lc, DEVICE_NAME,
					sens * 1.0e9, slist[ Sens ] * 1.0e9 );
		sr810.Sens_warn = SET;
	}

	if ( Sens < 0 )                                   /* not found yet ? */
	{
		if ( sens < slist[ 0 ] )
			Sens = 0;
		else
		    Sens = 26;

		if ( ! sr810.Sens_warn )                      /* no warn message yet */
		{
			if ( sens >= 1.0 )
				eprint( WARN, "%s:%ld: %s: Sensitivity of %.0lf V is too "
						"low, using %.0lf V instead.\n", Fname, Lc,
						DEVICE_NAME, sens * 1.0e3, slist[ Sens ] * 1.0e3 );
			else
				eprint( WARN, "%s:%ld: %s: Sensitivity of %.0lf nV is too "
						"high, using %.0lf nV instead.\n", Fname, Lc,
						DEVICE_NAME, sens * 1.0e9, slist[ Sens ] * 1.0e9 );
			sr810.Sens_warn = SET;
		}
	}

	if ( ! TEST_RUN )
	{
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			sr810_set_sens( Sens );
		else                         /* if called in a preparation sections */ 
			sr810.Sens = Sens;
	}
	
	return vars_push( FLOAT_VAR, slist[ Sens ] );
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
				eprint( FATAL, "%s:%ld: %s: Function `lockin_time_constant'"
						" with no argument can only be used in the EXPERIMENT "
						"section.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr810_get_tc( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used as time constant.\n",
				Fname, Lc, DEVICE_NAME );
	tc = VALUE( v );
	vars_pop( v );

	if ( tc < 0.0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid negative time constant.\n",
				Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* We try to match the time constant passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 1 percent, we utter a warning message (but only once). */
	
	for ( i = 0; i < 18; i++ )
		if ( tc >= tcs[ i ] && tc <= tcs[ i + 1 ] )
		{
			TC = i + ( ( tc / tcs[ i ] < tcs[ i + 1 ] / tc ) ? 0 : 1 );
			break;
		}

	if ( TC >= 0 &&                                 /* value found ? */
		 fabs( tc - tcs[ TC ] ) > tc * 1.0e-2 &&    /* error > 1% ? */
		 ! sr810.TC_warn )                          /* no warning yet ? */
	{
		if ( tc > 1.0e3 )
			eprint( WARN, "%s:%ld: %s: Can't set time constant to %.0lf ks,"
					" using %.0lf ks instead.\n", Fname, Lc, DEVICE_NAME,
					tc * 1.0e-3, tcs[ TC ] );
		else if ( tc > 1.0 )
			eprint( WARN, "%s:%ld: %s: Can't set time constant to %.0lf s, "
					"using %.0lf s instead.\n", Fname, Lc, DEVICE_NAME, tc,
					tcs[ TC ] );
		else if ( tc > 1.0e-3 )
			eprint( WARN, "%s:%ld: %s: Can't set time constant to %.0lf ms,"
					" using %.0lf ms instead.\n", Fname, Lc, DEVICE_NAME,
					tc * 1.0e3, tcs[ TC ] * 1.0e3 );
		else
			eprint( WARN, "%s:%ld: %s: Can't set time constant to %.0lf us,"
					" using %.0lf us instead.\n", Fname, Lc, DEVICE_NAME,
					tc * 1.0e6, tcs[ TC ] * 1.0e6 );
		sr810.TC_warn = SET;
	}
	
	if ( TC < 0 )                                  /* not found yet ? */
	{
		if ( tc < tcs[ 0 ] )
			TC = 0;
		else
			TC = 19;

		if ( ! sr810.TC_warn )                      /* no warn message yet ? */
		{
			if ( tc >= 3.0e4 )
				eprint( WARN, "%s:%ld: %s: Time constant of %.0lf ks is too"
						" large, using %.0lf ks instead.\n", Fname, Lc,
						DEVICE_NAME, tc * 1.0e-3, tcs[ TC ] * 1.0e-3 );
			else
				eprint( WARN, "%s:%ld: %s: Time constant of %.0lf us is too"
						" small, using %.0lf us instead.\n", Fname, Lc,
						DEVICE_NAME, tc * 1.0e6, tcs[ TC ] * 1.0e6 );
			sr810.TC_warn = SET;
		}
	}

	if ( ! TEST_RUN )
	{
		if ( I_am == CHILD )         /* if called in EXPERIMENT section */
			sr810_set_tc( TC );
		else                         /* if called in a preparation sections */ 
			sr810.TC = TC;
	}
	
	return vars_push( FLOAT_VAR, tcs[ TC ] );
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
				eprint( FATAL, "%s:%ld: %s: Function `lockin_phase' with "
						"no argument can only be used in the EXPERIMENT "
						"section.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr810_get_phase( ) );
		}
	}

	/* Otherwise set phase to value passed to the function */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used as phase.\n",
				Fname, Lc, DEVICE_NAME );
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
			return vars_push( FLOAT_VAR, sr810_set_phase( phase ) );
		else                         /* if called in a preparation sections */ 
		{
			sr810.phase = phase;
			sr810.P = SET;
			return vars_push( FLOAT_VAR, phase );
		}
	}
}


/*-------------------------------------------------*/
/* Sets or returns the lock-in reference frequency */
/*-------------------------------------------------*/

Var *lockin_ref_freq( Var *v )
{
	long harm;
	double freq;


	/* Without an argument just return current phase settting */

	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, 1.0e5 );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, "%s:%ld: %s: Function `lockin_ref_freq' "
						"with no argument can only be used in the EXPERIMENT "
						"section.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}

			return vars_push( FLOAT_VAR, sr810_get_ref_freq( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used as reference "
				"frequency.\n", Fname, Lc, DEVICE_NAME );
	freq = VALUE( v );
	vars_pop( v );
	
	if ( sr810_get_ref_mode( ) != REF_MODE_INTERNAL )
	{
		eprint( FATAL, "%s:%ld: %s: Can't set reference frequency while "
				"reference source isn't internal.\n", Fname, Lc, DEVICE_NAME );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		harm = sr810.H ? sr810.harmonic : 1;
	else
		harm = sr810_get_harmonic( );

	if ( freq < MIN_REF_FREQ || freq > MAX_REF_FREQ / ( double ) harm )
	{
		if ( harm == 1 )
			eprint( FATAL, "%s:%ld: %s: Reference frequency of %f Hz is "
					"not within valid range (%f Hz - %f Hz).\n", Fname, Lc,
					DEVICE_NAME, MIN_REF_FREQ, MAX_REF_FREQ );
		else
			eprint( FATAL, "%s:%ld: %s: Reference frequency of %f Hz with "
					"harmonic set to %ld is not within valid range "
					"(%f Hz - %f Hz).\n", Fname, Lc, DEVICE_NAME, MIN_REF_FREQ,
					MAX_REF_FREQ / ( double ) harm );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, freq );

	if ( I_am == CHILD )                /* if called in EXPERIMENT section */
		return vars_push( FLOAT_VAR, sr810_set_ref_freq( freq ) );
	else                                /* if called in preparation sections */
	{
		sr810.ref_freq = freq;
		sr810.RF = SET;
		return vars_push( FLOAT_VAR, freq );
	}
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var *lockin_ref_mode( Var *v )
{
	v = v;
	return vars_push( INT_VAR, sr810_get_ref_mode( ) );
}


/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

Var *lockin_ref_level( Var *v )
{
	double level;


	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, 1.0 );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, "%s:%ld: %s: Function `lockin_ref_level' "
						"with no argument can only be used in the EXPERIMENT "
						"section.\n", Fname, Lc, DEVICE_NAME );
				THROW( EXCEPTION );
			}

			return vars_push( FLOAT_VAR, sr810_get_ref_level( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		eprint( WARN, "%s:%ld: %s: Integer value used as reference level.\n",
				Fname, Lc, DEVICE_NAME );
	level = VALUE( v );
	vars_pop( v );
	
	if ( level < MIN_REF_LEVEL || level > MAX_REF_LEVEL )
	{
		eprint( FATAL, "%s:%ld: %s: Reference level of %f V is not within "
				"valid range (%f V - %f V).\n", Fname, Lc, DEVICE_NAME,
				MIN_REF_LEVEL, MAX_REF_LEVEL );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, level );

	if ( I_am == CHILD )                /* if called in EXPERIMENT section */
		return vars_push( FLOAT_VAR, sr810_set_ref_level( level ) );
	else                                /* if called in preparation sections */
	{
		sr810.ref_level = level;
		sr810.RL = SET;
		return vars_push( FLOAT_VAR, level );
	}
}



/******************************************************/
/* The following functions are only used internally ! */
/******************************************************/


/*------------------------------------------------------------------*/
/* Function initialises the Stanford lock-in amplifier and tests if */
/* it can be accessed by asking it to send its status byte.         */
/*------------------------------------------------------------------*/

static bool sr810_init( const char *name )
{
	char buffer[ 20 ];
	long length = 20;
	int i;


	assert( sr810.device < 0 );

	if ( gpib_init_device( name, &sr810.device ) == FAILURE )
        return FAIL;

	/* Tell the lock-in to use the GPIB bus for communication, clear all
	   the relevant registers  and make sure the keyboard is locked */

	if ( gpib_write( sr810.device, "OUTX 1\n" ) == FAILURE ||
		 gpib_write( sr810.device, "*CLS\n" )   == FAILURE ||
		 gpib_write( sr810.device, "OVRM 0\n" ) == FAILURE )
		return FAIL;
	   
	/* Ask lock-in to send the error status byte and test if it does */

	length = 20;
	if ( gpib_write( sr810.device, "ERRS?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
		return FAIL;

	/* If the lock-in did send more than 2 byte this means its write buffer
	   isn't empty. We now read it until its empty for sure. */

	if ( length > 2 )
	{
		do
			length = 20;
		while ( gpib_read( sr810.device, buffer, &length ) != FAILURE &&
				length == 20 );
	}

	/* If sensitivity, time constant or phase were set in one of the
	   preparation sections only the value was stored and we have to do the
	   actual setting now because the lock-in could not be accessed before */

	if ( sr810.Sens != -1 )
		sr810_set_sens( sr810.Sens );
	if ( sr810.P )
		sr810_set_phase( sr810.phase );
	if ( sr810.TC != -1 )
		sr810_set_tc( sr810.TC );
	if ( sr810.H )
		sr810_set_harmonic( sr810.harmonic );
	if ( sr810.RF )
		sr810_set_ref_freq( sr810.ref_freq );
	if ( sr810.RL )
		sr810_set_ref_level( sr810.ref_level );

	for ( i = 0; i < NUM_DAC_PORTS; i++ )
		sr810_set_dac_data( i + 1, sr810.dac_voltage[ i ] );

	return OK;
}


/*----------------------------------------------------------------*/
/* lockin_get_data() returns the measured voltage of the lock-in. */
/*----------------------------------------------------------------*/

static double sr810_get_data( void )
{
	char buffer[ 50 ] = "OUTP?1\n";
	long length = 50;


	if ( gpib_write( sr810.device, buffer ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*------------------------------------------------------------*/
/* lockin_data() returns the measured voltage of the lock-in. */
/*------------------------------------------------------------*/

static void sr810_get_xy_data( double *data, long *channels, int num_channels )
{
	char cmd[ 100 ] = "SNAP?";
	char buffer[ 200 ];
	long length = 200;
	char *bp_cur, *bp_next;
	int i;


	assert( num_channels >= 2 && num_channels <= 6 );

	/* Assembe te command to be send to the lock-in */

	for ( i = 0; i < num_channels; i++ )
	{
		assert( channels[ i ] > 0 && channels[ i ] <= NUM_CHANNELS );

		if ( i == 0 )
			sprintf( cmd + strlen( cmd ), "%ld\n", channels[ i ] );
		else
			sprintf( cmd + strlen( cmd ), ",%ld\n", channels[ i ] );
	}

	/* Get the data from the lock-in */

	if ( gpib_write( sr810.device, cmd ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Disassemble the reply */

	buffer[ length - 1 ] = '\0';
	bp_cur = buffer;

	for ( i = 0; i < num_channels; bp_cur = bp_next, i++ )
	{
		bp_next = strchr( bp_cur, ',' ) + 1;

		if ( bp_next == NULL )
		{
			eprint( FATAL, "%s: Lock-in amplifier does not react properly.\n",
					DEVICE_NAME );
			THROW( EXCEPTION );
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

static double sr810_get_adc_data( long channel )
{
	char buffer[ 16 ] = "OAUX?*\n";
	long length = 16;


	assert( channel >= 1 && channel <= 4 );
	buffer[ 5 ] = ( char ) channel + '0';

	if ( gpib_write( sr810.device, buffer ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*--------------------------------------------------------------*/
/* lockin_set_dac() sets the voltage at one of the 4 DAC ports. */
/* -> Number of the DAC channel (1-4)                           */
/* -> Voltage to be set (-10.5 V - +10.5 V)                     */
/*-------------------------- -----------------------------------*/

static double sr810_set_dac_data( long port, double voltage )
{
	char buffer [ 40 ];

	assert( port >= 1 && port <= 4 );
	assert( voltage >= DAC_MIN_VOLTAGE && voltage <= DAC_MAX_VOLTAGE );

	sprintf( buffer, "AUXV %ld,%f\n", port, voltage );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return voltage;
}

/*-----------------------------------------------------------------------*/
/* Function determines the sensitivity setting of the lock-in amplifier. */
/*-----------------------------------------------------------------------*/

static double sr810_get_sens( void )
{
	char buffer[ 20 ];
	long length = 20;
	double sens;

	/* Ask lock-in for the sensitivity setting */

	if ( gpib_write( sr810.device, "SENS?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	sens = slist[ T_atol( buffer ) ];

	return sens;
}


/*----------------------------------------------------------------------*/
/* Function sets the sensitivity of the lock-in amplifier to one of the */
/* valid values. The parameter can be in the range from 0 to 26,  where */
/* 0 is 2 nV and 26 is 1 V - these and the other values in between are  */
/* listed in the global array 'slist' at the start of the file.         */
/*----------------------------------------------------------------------*/

static void sr810_set_sens( int Sens )
{
	char buffer[ 20 ];


	sprintf( buffer, "SENS %d\n", Sens );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------------------------*/
/* Function returns the current time constant of the lock-in amplifier. */
/* See also the global array 'tcs' with the possible time constants at  */
/* the start of the file.                                               */
/*----------------------------------------------------------------------*/

static double sr810_get_tc( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( sr810.device, "OFLT?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return tcs[ T_atol( buffer ) ];
}


/*-------------------------------------------------------------------*/
/* Fuunction sets the time constant to one of the valid values. The  */
/* parameter can be in the range from 0 to 19, where 0 is 10 us and  */
/* 19 is 30 ks - these and the other values in between are listed in */
/* the global array 'tcs' (cf. start of file)                        */
/*-------------------------------------------------------------------*/

static void sr810_set_tc( int TC )
{
	char buffer[ 10 ] = "OFLT ";


	sprintf( buffer + 5, "%d\n", TC );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}
}


/*-----------------------------------------------------------*/
/* Function returns the current phase setting of the lock-in */
/* amplifier (in degree between 0 and 359).                  */
/*-----------------------------------------------------------*/

static double sr810_get_phase( void )
{
	char buffer[ 20 ];
	long length = 20;
	double phase;


	if ( gpib_write( sr810.device, "PHAS?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

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

static double sr810_set_phase( double phase )
{
	char buffer[ 20 ];


	sprintf( buffer, "PHAS %.2f\n", phase );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return phase;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr810_get_ref_freq( void )
{
	char buffer[ 40 ];
	long length = 40;


	if ( gpib_write( sr810.device, "FREQ?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*---------------------------------------------------------------*/
/* Functions sets the phase to a value between 0 and 360 degree. */
/*---------------------------------------------------------------*/

static double sr810_set_ref_freq( double freq )
{
	char buffer[ 40 ];
	double real_freq;


	sprintf( buffer, "FREQ %.4f\n", freq );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Take care: The product of the harmonic and the reference frequency
	   can't be larger than 102 kHz, otherwise the reference frequency is
	   reduced to a value that fits this restriction. Thus we better check
	   which value has been really set... */

	real_freq = sr810_get_ref_freq( );
	if ( ( real_freq - freq ) / freq > 1.0e-4 && real_freq - freq > 1.0e-4 )
	{
		eprint( FATAL, "%s: Failed to set reference frequency to %f Hz.\n",
				DEVICE_NAME, freq );
		THROW( EXCEPTION );
	}

	return real_freq;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static long sr810_get_ref_mode( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( sr810.device, "FMOD?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return T_atol( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static long sr810_get_harmonic( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( sr810.device, "HARM?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return  T_atol( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static long sr810_set_harmonic( long harmonic )
{
	char buffer[ 20 ];


	assert( harmonic >= MIN_HARMONIC && harmonic <= MAX_HARMONIC );

	sprintf( buffer, "HARM %ld\n", harmonic );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Take care: The product of the harmonic and the reference frequency
	   can't be larger than 102 kHz, otherwise the harmonic is reduced to a
	   value that fits this restriction. So we better check on the value
	   that has been really set... */

	if ( harmonic != sr810_get_harmonic( ) )
	{
		eprint( FATAL, "%s: Failed to set harmonic to %ld.\n",
				DEVICE_NAME, harmonic );
		THROW( EXCEPTION );
	}

	return harmonic;
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr810_get_ref_level( void )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_write( sr810.device, "SLVL?\n" ) == FAILURE ||
		 gpib_read( sr810.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	buffer[ length - 1 ] = '\0';
	return T_atof( buffer );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static double sr810_set_ref_level( double level )
{
	char buffer[ 50 ];


	assert( level >= MIN_REF_LEVEL && level <= MAX_REF_LEVEL );

	sprintf( buffer, "SLVL %f\n", level );
	if ( gpib_write( sr810.device, buffer ) == FAILURE )
	{
		eprint( FATAL, "%s: Can't access the lock-in amplifier.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	return level;
}
