/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"



#define DEVICE_NAME "STANFORD_SR510"    /* name, compare /etc/gpib.conf */


int sr510_init_hook( void );
int sr510_exp_hook( void );
void sr510_exit_hook( void );

Var *lockin_get_data( Var *v );
Var *lockin_sensitivity( Var *v );
Var *lockin_time_constant( Var *v );
Var *lockin_phase( Var *v );


typedef struct
{
	const char *name;
	int device;
	int Sens;
	bool Sens_warn;
	double phase;
	bool P;
	int TC;
	bool TC_warn;
} SR510;

static SR510 sr510;

/* lists of valid sensitivity and time constant settings
   (the last three entries in the sensitivity list are only usable when the
   EXPAND button is switched on!) */

static double slist[ ] = { 5.0e-1, 2.0e-1, 1.0e-1, 5.0e-2, 2.0e-2,
						   1.0e-2, 5.0e-3, 2.0e-3, 1.0e-3, 5.0e-4,
						   2.0e-4, 1.0e-4, 5.0e-5, 2.0e-5, 1.0e-5,
						   5.0e-6, 2.0e-6, 1.0e-6, 5.0e-7, 2.0e-7,
						   1.0e-7, 5.0e-8, 2.0e-8, 1.0e-8 };
static double tcs[ ] = { 1.0e-3, 3.0e-3, 1.0e-2, 3.0e-2, 1.0e-1, 3.0e-1,
						 1.0, 3.0, 10.0, 30.0, 100.0 };


static bool lockin_init( const char *name, int *device );
static double sr510_get_data( void );
static double sr510_get_adc_data( long channel );
static double sr510_get_sens( void );
static void sr510_set_sens( int Sens );
static double sr510_get_tc( void );
static void sr510_set_tc( int TC );
static double sr510_get_phase( void );
static double sr510_set_phase( double phase );



/*--------------------------------------------------*/
/*--------------------------------------------------*/


int sr510_init_hook( void )
{
	need_GPIB = SET;
	sr510.name = DEVICE_NAME;
	sr510.Sens = -1;
	sr510.Sens_warn = UNSET;
	sr510.P = UNSET;
	sr510.TC = -1;
	sr510.TC_warn = UNSET;
	return 1;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

int sr510_exp_hook( void )
{


	/* Nothing to be done yet in a test run */

	if ( TEST_RUN )
		return 1;

	/* Initialize the lock-in */

	if ( ! lockin_init( sr510.name, &sr510.device ) )
	{
		eprint( FATAL, "sr510: Can't access the lock-in amplifier.\n" );
		THROW( EXCEPTION );
	}

	return 1;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

void sr510_exit_hook( void )
{
	/* Switch lock-in back to local mode */

	gpib_write( sr510.device, "I0\n", 3 );
	gpib_local( sr510.device );
}


/*---------------------------------------------------------------------*/
/* This function returns either the lock-in voltage (if called with no */
/* argument) or the voltage at one of the 4 ADCs on the backside of    */
/* the lock-in (if called with a numeric argument between 1 and 4).    */
/* The returned value is always the voltage in V.                      */
/*---------------------------------------------------------------------*/

Var *lockin_get_data( Var *v )
{
	/* If no parameter is passed to the function this means we need the
	   lock-in voltage */

	if ( v == NULL )
	{
		if ( TEST_RUN )
			return vars_push( FLOAT_VAR, 0.0 );
		else
			return vars_push( FLOAT_VAR, sr510_get_data( ) );
	}

	/* Otherwise the voltage at one of the four ADC's at the backside is
	   needed */

	vars_check( v, INT_VAR );

	if ( ( long ) VALUE( v ) < 1 || ( long ) VALUE( v ) > 4 )
	{
		eprint( FATAL, "sr510: Invalid ADC channel number (%ld), valid "
				"channel are in the range 1-4.\n", ( long ) VALUE( v ) );
		THROW( EXCEPTION );
	}

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 0.0 );

	return vars_push( FLOAT_VAR, sr510_get_adc_data( ( long ) VALUE( v ) ) );
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


	if ( v == 0 )
	{
		if ( TEST_RUN )
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
			return vars_push( FLOAT_VAR, 0 );
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

bool lockin_init( const char *name, int *device )
{
	char buffer[ 20 ];
	long length = 20;


	if ( gpib_init_device( name, device ) == FAILURE )
        return FAIL;

	/* Ask lock-in to send status byte and test if it does */

	if ( gpib_write( *device, "Y\n", 2 ) == FAILURE ||
		 gpib_read( *device, buffer, &length ) == FAILURE )
		return FAIL;

	/* Lock the keyboard */

	if ( gpib_write( *device, "I1\n", 3 ) == FAILURE )
		return FAIL;

	/* If sensitivity, time constant or the phase values were set in one of
	   the preparation sections only the value to be used was stored and we
	   have to do the actual setting now because the lock-in could not be
	   accessed before */

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
/* of the six ADC ports.                                    */
/* -> Number of the ADC channel (1-6)                       */
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

	/* Also set the POST time constant */

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
