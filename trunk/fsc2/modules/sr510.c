/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"


/* name of device as given in GPIB configuration file /etc/gpib.conf */

#define DEVICE_NAME "STANFORD_SR510"


/* declaration of exported functions */

int sr510_init_hook( void );
int sr510_exp_hook( void );
int sr510_end_of_exp_hook( void );
void sr510_exit_hook( void );

Var *lockin_get_data( Var *v );
Var *lockin_get_adc_data( Var *v );
Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );


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

static bool lockin_init( const char *name );
static double sr510_get_data( void );
static double sr510_get_adc_data( long channel );
static double sr510_get_sens( void );
static void sr510_set_sens( int Sens );
static double sr510_get_tc( void );
static void sr510_set_tc( int TC );
static double sr510_get_phase( void );
static double sr510_set_phase( double phase );



/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int sr510_init_hook( void )
{
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

	if ( ! lockin_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
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


/*---------------------------------------------------------------------*/
/* Function returns the lock-in voltage. Returned value is the voltage */
/* in V, with the range depending on the current sensitivity setting.  */
/*---------------------------------------------------------------------*/

Var *lockin_get_data( Var *v )
{
	v = v;

	if ( TEST_RUN )                  /* return dummy value in test run */
		return vars_push( FLOAT_VAR, 0.0 );
	else
		return vars_push( FLOAT_VAR, sr510_get_data( ) );
}


/*-----------------------------------------------------------------*/
/* Function returns the voltage at one or more of the 4 ADC ports  */
/* on the backside of the lock-in amplifier. The argument(s) must  */
/* be integers between 1 and 4.                                    */
/* There are two different ways to call this function:             */
/* 1. If called with a single integer argument the voltage of the  */
/*    corresponding ADC port is returned.                          */
/* 2. If called with a list of integer arguments an array with the */
/*    voltages of all the corresponding ports is returned.         */
/* Returned values are in the interval [ -10.24V, +10.24V ].       */
/*-----------------------------------------------------------------*/

Var *lockin_get_adc_data( Var *v )
{
	long num_args, i;
	double *voltages;
	Var *cv;


	if ( v == NULL )
	{
		eprint( FATAL, "sr510: Missing argument for function "
				 "'lockin_get_adc_data'.\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR );

	/* If called with just one argument return the voltage of the
	   addressed port */

	if ( v->next == NULL )
	{
		if ( ( long ) VALUE( v ) < 1 || ( long ) VALUE( v ) > 4 )
		{
			eprint( FATAL, "sr510: Invalid ADC channel number (%ld) in call "
					"of 'lockin_get_adc_data', valid channel are in the "
					"range 1-4.\n", ( long ) VALUE( v ) );
			THROW( EXCEPTION );
		}

		if ( TEST_RUN )                  /* return dummy value in test run */
			return vars_push( FLOAT_VAR, 0.0 );

		return vars_push( FLOAT_VAR,
						  sr510_get_adc_data( ( long ) VALUE( v ) ) );
	}

	/* If function is called with a list of port numbers the voltage for each
	   of them is fetched and the results are returned as an float array */

	/* count number of arguments and check them */
	
	for ( cv = v->next, num_args = 1; cv != NULL;
		  cv = cv->next, num_args++ )
		vars_check( cv, INT_VAR );
		
	/* get memory for storing the voltages */

	voltages = T_malloc( num_args * sizeof( double ) );

	/* get voltage from each of the ports in the list */

	for ( i = 0; v != NULL; i++, v = v->next )
	{
		if ( ( long ) VALUE( v ) < 1 || ( long ) VALUE( v ) > 4 )
		{
			eprint( FATAL, "sr510: Invalid ADC channel number (%ld) in "
					"call of 'lockin_get_adc_data', valid channel are in "
					"the range 1-4.\n", ( long ) VALUE( v ) );
			T_free( voltages );
			THROW( EXCEPTION );
		}

		if ( TEST_RUN )
			voltages[ i ] = 0.0;      /* return dummy value in test run */
		else
			voltages[ i ] = sr510_get_adc_data( ( long ) VALUE( v ) );
	}

	/* push the array of results onto the variable stack */

	cv = vars_push( FLOAT_TRANS_ARR, voltages, num_args );

	/* finally free array of voltages and return the stack variable */

	T_free( voltages );
	return cv;
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
				eprint( FATAL, "sr510: Function `lockin_sensitivity' with no "
						"argument can only be used in the EXPERIMENT "
						"section.\n" );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr510_get_sens( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	sens = VALUE( v );

	if ( sens < 0.0 )
	{
		eprint( FATAL, "sr510: Invalid negative sensitivity.\n" );
		THROW( EXCEPTION );
	}

	/* We try to match the sensitivity passed to the function by checking if
	   it fits in between two of the valid values and setting it to the nearer
	   one and, if this doesn't work, we set it to the minimum or maximum
	   value, depending on the size of the argument. If the value does not fit
	   within 1 percent, we utter a warning message (but only once). */

	for ( i = 0; i < 23; i++ )
	{
		if ( sens <= slist[ i ] && sens >= slist[ i + 1 ] )
		{
			if ( slist[ i ] / sens < sens / slist[ i + 1 ] )
				Sens = i + 1;
			else
				Sens = i + 2;
			break;
		}
	}

	if ( Sens > 0 &&                                   /* value found ? */
		 fabs( sens - slist[ Sens - 1 ] ) > sens * 1.0e-2 && /* error > 1% ? */
		 ! sr510.Sens_warn  )                       /* no warn message yet ? */
	{
		if ( sens >= 1.0e-3 )
			eprint( WARN, "sr510: Can't set sensitivity to %.0lfmV, using "
					"%.0lfmV instead.\n", sens * 1.0e3,
					slist[ Sens - 1 ] * 1.0e3 );
		else if ( sens >= 1.0e-6 ) 
			eprint( WARN, "sr510: Can't set sensitivity to %.0lfuV, using "
					"%.0lfuV instead.\n", sens * 1.0e6,
					slist[ Sens - 1 ] * 1.0e6 );
		else
			eprint( WARN, "sr510: Can't set sensitivity to %.0lfnV, using "
					"%.0lfnV instead.\n", sens * 1.0e9, 
					slist[ Sens - 1 ] * 1.0e9 );
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
			eprint( WARN, "sr510: Invalid sensitivity to %.0lfmV, using "
					"%.0lfmV instead.\n", sens * 1.0e3,
					slist[ Sens - 1 ] * 1.0e3 );
		else
			eprint( WARN, "sr510: Invalid sensitivity to %.0lfnV, using "
					"%.0lfnV instead.\n", sens * 1.0e9, 
					slist[ Sens - 1 ] * 1.0e9 );
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
				eprint( FATAL, "sr510: Function `lockin_time_constant' with "
						"no argument can only be used in the EXPERIMENT "
						"section.\n" );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr510_get_tc( ) );
		}
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	tc = VALUE( v ) / 1.0e9;

	if ( tc < 0.0 )
	{
		eprint( FATAL, "sr510: Invalid negative time constant.\n" );
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
			eprint( WARN, "sr510: Can't set time constant to %.0lfs, using "
					"%.0lfs instead.\n", tc, tcs[ TC - 1 ] );
		else
			eprint( WARN, "sr510: Can't set time constant to %.0lfms, using "
					"%.0lfms instead.\n", tc * 1.0e3, tcs[ TC - 1 ] * 1.0e3 );
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
				eprint( WARN, "sr510: Invalid time constant (%.0lfs), using "
					"%.0lfs instead.\n", tc, tcs[ TC - 1 ] );
			else
				eprint( WARN, "sr510: Invalid time constant (%.0lfms), using "
					"%.0lfms instead.\n", tc * 1.0e3, tcs[ TC - 1 ] * 1.0e3 );
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

	if ( v == 0 )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, 0.0 );
		else
		{
			if ( I_am == PARENT )
			{
				eprint( FATAL, "sr510: Function `lockin_phase' with no "
						"argument can only be used in the EXPERIMENT "
						"section.\n" );
				THROW( EXCEPTION );
			}
			return vars_push( FLOAT_VAR, sr510_get_phase( ) );
		}
	}

	/* Otherwise set phase to value passed to the function */

	vars_check( v, INT_VAR | FLOAT_VAR );

	phase = VALUE( v );

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


/******************************************************/
/* The following functions are only used internally ! */
/******************************************************/


/*------------------------------------------------------------------*/
/* Function initialises the Stanford lock-in amplifier and tests if */
/* it can be accessed by asking it to send its status byte.         */
/*------------------------------------------------------------------*/

bool lockin_init( const char *name )
{
	char buffer[ 20 ];
	long length = 20;


	assert( sr510.device < 0 );

	if ( gpib_init_device( name, &sr510.device ) == FAILURE )
        return FAIL;

	/* Ask lock-in to send status byte and test if it does */

	if ( gpib_write( sr510.device, "Y\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
		return FAIL;

	/* Lock the keyboard */

	if ( gpib_write( sr510.device, "I1\n", 3 ) == FAILURE )
		return FAIL;

	/* If sensitivity, time constant or phase were set in one of the
	   preparation sections only the value was stored and we have to do the
	   actual setting now because the lock-in could not be accessed before */

	if ( sr510.Sens != -1 )
		sr510_set_sens( sr510.Sens );
	if ( sr510.P == SET )
		sr510_set_phase( sr510.phase );
	if ( sr510.TC != -1 )
		sr510_set_tc( sr510.TC );

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
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	buffer[ length - 2 ] = '\0';
	return atof( buffer );
}


/*----------------------------------------------------------*/
/* lockin_get_adc() returns the value of the voltage at one */
/* of the 4 ADC ports.                                      */
/* -> Number of the ADC channel (1-4)                       */
/*-------------------------- -------------------------------*/

double sr510_get_adc_data( long channel )
{
	char buffer[ 16 ] = "X*\n";
	long length = 16;


	buffer[ 1 ] = ( char ) channel + '0';

	if ( gpib_write( sr510.device, buffer, 3 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	buffer[ length - 2 ] = '\0';
	return atof( buffer );
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
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	buffer[ length - 2 ] = '\0';
	sens = slist[ 24 - atoi( buffer ) ];

    /* Check if EXPAND is switched on - this increases the sensitivity 
	   by a factor of 10 */

	length = 10;
	if ( gpib_write( sr510.device, "E\n", 2 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

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
	char buffer[10];


	/* Coding of sensitivity commands work just the other way round as
	   in the list of sensitivities 'slist', i.e. 1 stands for the highest
	   sensitivity (10nV) and 24 for the lowest (500mV) */

	Sens = 25 - Sens;

	/* For sensitivities lower than 100 nV EXPAND has to be switched on
	   otherwise it got to be switched off */

	if ( Sens <= 3 )
	{
		if ( gpib_write( sr510.device, "E1\n", 3 ) == FAILURE )
		{
			eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
			THROW( EXCEPTION );
		}
		Sens += 3;
	}
	else
	{
		if ( gpib_write( sr510.device, "E0\n", 3 ) == FAILURE )
		{
			eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
			THROW( EXCEPTION );
		}
	}

	/* Now set the sensitivity */

	sprintf( buffer, "G%d\n", Sens );

	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------------------------*/
/* Function returns the current time constant of the lock-in amplifier. */
/* See also the global array 'tcs' with the possible time constants at  */
/* the start of the file.                                               */
/*----------------------------------------------------------------------*/

double sr510_get_tc( void )
{
	char buffer[10];
	long length = 10;


	if ( gpib_write( sr510.device, "T1\n", 3 ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	buffer[ length - 2 ] = '\0';
	return tcs[ atoi( buffer ) - 1 ];
}


/*-------------------------------------------------------------------------*/
/* Fuunction sets the time constant (plus the post time constant) to one   */
/* of the valid values. The parameter can be in the range from 1 to 11,    */
/* where 1 is 1 ms and 11 is 100 s - these and the other values in between */
/* are listed in the global array 'tcs' (cf. start of file)                */
/*-------------------------------------------------------------------------*/

void sr510_set_tc( int TC )
{
	char buffer[10];


	sprintf( buffer, "T1,%d\n", TC );
	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	/* Also set the POST time constant where 'T2,0' switches it off, 'T2,1'
	   sets it to 100ms and 'T1,2' to 1s */

	/* Recheck this in the manual !!!!!!!!!! */

	if ( TC <= 4 && gpib_write( sr510.device, "T2,0\n", 5 ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	if ( TC > 4 && TC <= 6 &&
		 gpib_write( sr510.device, "T2,1\n", 5 ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	if ( TC > 6 && gpib_write( sr510.device, "T2,2\n", 5 ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}
}


/*-----------------------------------------------------------*/
/* Function returns the current phase setting of the lock-in */
/* amplifier (in degree between 0 and 359).                  */
/*-----------------------------------------------------------*/

double sr510_get_phase( void )
{
	char buffer[20];
	long length = 20;
	double phase;


	if ( gpib_write( sr510.device, "P\n", 2L ) == FAILURE ||
		 gpib_read( sr510.device, buffer, &length ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	buffer[ length - 2 ] = '\0';
fprintf( stderr, "Phase = %s\n", buffer );

	phase = atof( buffer );

	while ( phase >= 360.0 )    /* convert to 0-359 degree range */
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

double sr510_set_phase( double phase )
{
	char buffer[20];


	sprintf( buffer, "P%.2f\n", phase );
	if ( gpib_write( sr510.device, buffer, strlen( buffer ) ) == FAILURE )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	return phase;
}
