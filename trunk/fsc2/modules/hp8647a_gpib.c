/*
  $Id$
*/


#include "hp8647a.h"
#include "gpib.h"


static void hp8647a_comm_failure( void );


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8647a_init( const char *name )
{
	double att;


	if ( gpib_init_device( name, &hp8647a.device ) == FAILURE )
        return FAIL;

	/* If frequency and attenuation need to be set do it now, otherwise get
	   frequency and attenuation set at the synthesizer and store it */

	if ( hp8647a.freq_is_set )
		hp8647a_set_frequency( hp8647a.freq );
	else
		hp8647a.freq = hp8647a_get_frequency( );

	if ( hp8647a.attenuation_is_set )
	{
		if ( hp8647a.use_table )
		{
			att =   hp8647a.attenuation
				  - hp8647a_get_att_from_table( hp8647a.freq )
				  + hp8647a.att_at_ref_freq;
			if ( att < MAX_ATTEN )
			{
				eprint( SEVERE, "%s:%ld: %s: Attenuation dynamic range is  "
						"insufficient, using %f dB instead of %f dB.\n",
						Fname, Lc, DEVICE_NAME, MAX_ATTEN, att );
				att = MAX_ATTEN;
			}
			if ( att > MIN_ATTEN )
			{
				eprint( SEVERE, "%s:%ld: %s: Attenuation dynamic range is  "
						"insufficient, using %f dB instead of %f dB.\n",
						Fname, Lc, DEVICE_NAME, MIN_ATTEN, att );
				att = MIN_ATTEN;
			}
		}
		else
			att = hp8647a.attenuation;

		hp8647a_set_attenuation( att );
	}
	else
		hp8647a.attenuation = hp8647a_get_attenuation( );

	hp8647a_set_output_state( hp8647a.state );

	return OK;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

void hp8647a_finished( void )
{
	gpib_local( hp8647a.device );
	if ( hp8647a.att_table != NULL )
	{
		T_free( hp8647a.att_table );
		hp8647a.att_table = NULL;
	}
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8647a_set_output_state( bool state )
{
	char cmd[ 100 ];


	sprintf( cmd, "OUTP:STAT %s", state ? "ON" : "OFF" );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );

	return state;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8647a_get_output_state( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( hp8647a.device, "OUTP:STAT?", 10 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return buffer[ 0 ] == '1';
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_set_frequency( double freq )
{
	char cmd[ 100 ];


	assert( freq >= MIN_FREQ && freq <= MAX_FREQ );

	sprintf( cmd, "FREQ:CW %f", freq );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );

	return freq;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;


	if ( gpib_write( hp8647a.device, "FREQ:CW?", 8 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return T_atof( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_set_attenuation( double att )
{
	char cmd[ 100 ];


	assert( att >= MAX_ATTEN && att <= MIN_ATTEN );

	sprintf( cmd, "POW:AMPL %6.1f", att );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );

	return att;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_get_attenuation( void )
{
	char buffer[ 100 ];
	long length = 100;

	if ( gpib_write( hp8647a.device, "POW:AMPL?", 9 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return T_atof( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8647a_comm_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
	THROW( EXCEPTION );
}
