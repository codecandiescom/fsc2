/*
  $Id$
*/

/*
  I had some rather strange probems with this device. For example, when simply
  just sweeping up the output frequency I nearyl never ended up with the
  expected end frequency but a bit below eventhough all commands got send
  correctly. Obviously, some kind of backlog of commands had built up. To get
  rid of this (and also some spurious break-downs in the communication) I now
  check the operation complete after each setting of the synthesizer.
*/



#include "hp8647a.h"
#include "gpib_if.h"


static void hp8647a_comm_failure( void );
static void hp8647a_check_complete( void );


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8647a_init( const char *name )
{
	double att;
	int i;


	if ( gpib_init_device( name, &hp8647a.device ) == FAILURE )
        return FAIL;

	if ( gpib_write( hp8647a.device, "*OPC\n", 5 ) == FAILURE )
		return FAIL;

	HP8647A_INIT = SET;

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
				  + hp8647a_get_att_from_table( hp8647a.freq )
				  - hp8647a.att_at_ref_freq;
			if ( att < MAX_ATTEN )
			{
				eprint( SEVERE, "%s: Attenuation range is insufficient, using "
						"%f dB instead of %f dB.\n",
						DEVICE_NAME, MAX_ATTEN, att );
				att = MAX_ATTEN;
			}
			if ( att > MIN_ATTEN )
			{
				eprint( SEVERE, "%s: Attenuation range is insufficient, using "
						"%f dB instead of %f dB.\n",
						DEVICE_NAME, MIN_ATTEN, att );
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

	/* Now we set the modulation type if it has been set, otherwise ask the
	   synthesizer for its currents setting */

	if ( hp8647a.mod_type_is_set )
		hp8647a_set_mod_type( hp8647a.mod_type );
	else 
	{
		if ( ( hp8647a.mod_type = hp8647a_get_mod_type( ) ) != UNDEFINED )
			hp8647a.mod_type_is_set = SET;
	}

	/* Set source and amplitude for each modulation type as far as it's set */

	for ( i = 0; i < NUM_MOD_TYPES; i++ )
	{
		if ( hp8647a.mod_source_is_set[ i ] )
			hp8647a_set_mod_source( i, hp8647a.mod_source[ i ] );

		if ( hp8647a.mod_ampl_is_set[ i ] )
			hp8647a_set_mod_ampl( i, hp8647a.mod_ampl[ i ] );
	}

	HP8647A_INIT = UNSET;

	return OK;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

void hp8647a_finished( void )
{
	gpib_local( hp8647a.device );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8647a_set_output_state( bool state )
{
	char cmd[ 100 ];


	sprintf( cmd, "OUTP:STAT %s\n", state ? "ON" : "OFF" );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );
	hp8647a_check_complete( );

	return state;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

bool hp8647a_get_output_state( void )
{
	char buffer[ 10 ];
	long length = 10;


	if ( gpib_write( hp8647a.device, "OUTP:STAT?\n", 11 ) == FAILURE ||
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

	sprintf( cmd, "FREQ:CW %.0f\n", freq );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );
	hp8647a_check_complete( );

	return freq;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_get_frequency( void )
{
	char buffer[ 100 ];
	long length = 100;


	if ( gpib_write( hp8647a.device, "FREQ:CW?\n", 9 ) == FAILURE ||
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

	sprintf( cmd, "POW:AMPL %6.1f\n", att );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );
	hp8647a_check_complete( );

	return att;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_get_attenuation( void )
{
	char buffer[ 100 ];
	long length = 100;

	if ( gpib_write( hp8647a.device, "POW:AMPL?\n", 10 ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	return T_atof( buffer );
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8647a_set_mod_type( int type )
{
	char cmd[ 100 ];
	const char *types[ ] = { "FM", "AM", "PM" };
	int i;


	assert( type >= 0 && type < NUM_MOD_TYPES );

	/* The manual is not really clear about this but it looks as if we
	   have to make sure that only one modulation type is switched on... */

	if ( I_am == PARENT && ! HP8647A_INIT )
		return type;

	for ( i = 1; i < NUM_MOD_TYPES; i++ )
	{
		sprintf( cmd, "%s:STAT OFF\n", 
				 types[ ( type + i ) % NUM_MOD_TYPES ] );
		if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
			hp8647a_comm_failure( );
	}

	sprintf( cmd, "%s:STAT ON\n", types[ type ] );
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );
	hp8647a_check_complete( );

	return type;
}


/*--------------------------------------------------------------*/
/* Function returns a number between 0 and (NUM_MOD_TYPES - 1)  */
/* indicating the modulation type that is currently switched on */
/* or -1 if none is switched on.                                */
/*--------------------------------------------------------------*/

int hp8647a_get_mod_type( void )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];
	char buffer[ 100 ];
	long length;
	int i;


	if ( TEST_RUN )
	{
		if ( hp8647a.mod_type_is_set )
			return hp8647a.mod_type;
		else
			return MOD_TYPE_FM;
	}

	for ( i = 0; i < NUM_MOD_TYPES; i++ )
	{
		length = 100;
		sprintf( cmd, "%s:STAT?\n", types[ i ] );
		if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
			hp8647a_comm_failure( );

		if ( buffer[ 0 ] == '1' )
			return i;
	}

	return UNDEFINED;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8647a_set_mod_source( int type, int source )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd1[ 100 ], cmd2[ 100 ];


	assert( type >= 0 && type < NUM_MOD_TYPES );
	assert( source >= 0 && source < NUM_MOD_SOURCES );

	/* Neither AM nor phase modulation allows external DC as modulation
	   source */

	if ( type != MOD_TYPE_FM && source == MOD_SOURCE_DC )
	{
		if ( I_am == PARENT && HP8647A_INIT )
			eprint( SEVERE, "%s: Modulation source \"%s\" can't be used for "
					"%s modulation, using \"AC\" instead.\n", DEVICE_NAME,
					mod_sources[ source ], mod_types[ type ] );
		else
			eprint( SEVERE, "%s:%ld: %s: Modulation source \"%s\" can't be "
					"used for %s modulation, using \"AC\" instead.\n",
					Fname, Lc, DEVICE_NAME,
					mod_sources[ source ], mod_types[ type ] );
		source = MOD_SOURCE_AC;
	}

	sprintf( cmd1, "%s:SOUR ", types[ type ] );
	switch( source )
	{
		case MOD_SOURCE_AC :
			strcat( cmd1, "EXT\n" );
			sprintf( cmd2, "%s:EXT:COUP AC\n", types[ type ] );
			break;

		case MOD_SOURCE_DC :
			strcat( cmd1, "EXT\n" );
			sprintf( cmd2, "%s:EXT:COUP DC\n", types[ type ] );
			break;

		case MOD_SOURCE_1k :
			strcat( cmd1, "INT\n" );
			sprintf( cmd2, "%s:INT:FREQ 1 KHZ\n", types[ type ] );
			break;

		case MOD_SOURCE_400 :
			strcat( cmd1, "INT\n" );
			sprintf( cmd2, "%s:INT:FREQ 400 HZ\n", types[ type ] );
			break;

		default :                         /* this can never happen... */
			assert( 1 == 0 );
	}

	if ( I_am == PARENT && ! HP8647A_INIT )
		return source;

	if ( gpib_write( hp8647a.device, cmd1, strlen( cmd1 ) ) == FAILURE ||
		 gpib_write( hp8647a.device, cmd2, strlen( cmd2 ) ) == FAILURE )
		hp8647a_comm_failure( );
	hp8647a_check_complete( );

	return source;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

int hp8647a_get_mod_source( int type )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];
	char buffer[ 100 ];
	long length;
	int source;
	long freq;


	assert( type >= 0 && type < NUM_MOD_TYPES );

	if ( TEST_RUN )
	{
		if ( hp8647a.mod_source_is_set[ type ] )
			return hp8647a.mod_source[ type ];
		else
			return MOD_SOURCE_AC;
	}

	sprintf( cmd, "%s:SOUR?\n", types[ type ] );
	length = 100;
	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
		hp8647a_comm_failure( );

	source = buffer[ 0 ] == 'I' ? 0 : 1;

	length = 100;
	if ( source == 0 )
	{
		sprintf( cmd, "%s:INT:FREQ?\n", types[ type ] );
		if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
			hp8647a_comm_failure( );
		freq = lround( T_atof ( buffer ) );
		source = freq == 400 ? MOD_SOURCE_1k : MOD_SOURCE_400;
	}
	else
	{
		sprintf( cmd, "%s:EXT:COUP?\n", types[ type ] );
		if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
			hp8647a_comm_failure( );
		source = buffer[ 0 ] == 'A' ? MOD_SOURCE_AC : MOD_SOURCE_DC;
	}

	return source;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_set_mod_ampl( int type, double ampl )
{
	const char *types[ ] = { "FM", "AM", "PM" };
	char cmd[ 100 ];


	assert( type >= 0 && type < NUM_MOD_TYPES );

	if ( ampl < 0.0 )
	{
		if ( I_am == PARENT && HP8647A_INIT )
			eprint( FATAL, "%s: Invalid negative %s modulation amplitude of "
					"%g %s.\n", DEVICE_NAME,
					type != MOD_TYPE_PHASE ? types[ type ] : "phase",
					type == MOD_TYPE_FM ? "kHz" :
					( type == MOD_TYPE_AM ? "%%" : "rad" ) );
		else
			eprint( FATAL, "%s:%ld: %s: Invalid negative %s modulation "
					"amplitude of %g %s.\n", Fname, Lc, DEVICE_NAME,
					type != MOD_TYPE_PHASE ? types[ type ] : "phase",
					type == MOD_TYPE_FM ? "kHz" :
					( type == MOD_TYPE_AM ? "%%" : "rad" ) );
		THROW( EXCEPTION );
	}

	switch ( type )
	{
		case MOD_TYPE_FM :
			if ( ampl > MAX_FM_AMPL )
			{
				if ( I_am == PARENT && HP8647A_INIT )
					eprint( FATAL, "%s: FM modulation amplitude of %.1f kHz "
							"is too large, valid range is 0 - %.1f kHz.\n",
							DEVICE_NAME, ampl * 1.0e-3, MAX_FM_AMPL * 1.0e-3 );
				else
					eprint( FATAL, "%s:%ld: %s: FM modulation amplitude of "
							"%.1f kHz is too large, valid range is "
							"0 - %.1f kHz.\n", Fname, Lc, DEVICE_NAME,
							ampl * 1.0e-3, MAX_FM_AMPL * 1.0e-3 );
				THROW( EXCEPTION );
			}
			sprintf( cmd, "FM:DEV %ld HZ\n", 10 * lround( 0.1 * ampl ) );
			break;

		case MOD_TYPE_AM :
			if ( ampl > MAX_AM_AMPL )
			{
				if ( I_am == PARENT && HP8647A_INIT )
					eprint( FATAL, "%s: AM modulation amplitude of %.1f %% is "
							"too large, valid range is 0 - %.1f %%.\n",
							DEVICE_NAME, ampl, ( double ) MAX_AM_AMPL );
				else
					eprint( FATAL, "%s:%ld: %s: AM modulation amplitude of "
							"%.1f %% is too large, valid range is "
							"0 - %.1f %%.\n", Fname, Lc, DEVICE_NAME,
							ampl, ( double ) MAX_AM_AMPL );
				THROW( EXCEPTION );
			}
			sprintf( cmd, "AM:DEPT %.1f PCT\n", ampl );
			break;

		case MOD_TYPE_PHASE :
			if ( ampl > MAX_PHASE_AMPL )
			{
				if ( I_am == PARENT && HP8647A_INIT )
					eprint( FATAL, "%s: Phase modulation amplitude of "
							"%.1f rad is too large, valid range is "
							"0 - %.1f rad.\n", DEVICE_NAME, ampl,
							( double ) MAX_PHASE_AMPL );
				else
					eprint( FATAL, "%s:%ld: %s: Phase modulation amplitude of "
							"%.1f rad is too large, valid range is "
							"0 - %.1f rad.\n", Fname, Lc, DEVICE_NAME, ampl,
							( double ) MAX_PHASE_AMPL );
				THROW( EXCEPTION );
			}
			sprintf( cmd, "PM:DEV %.*f RAD\n", ampl < 9.95 ? 2 : 1, ampl );
			break;

		default :                         /* this can never happen... */
			assert( 1 == 0 );
	}

	if ( I_am == PARENT && ! HP8647A_INIT )
		return ampl;

	if ( gpib_write( hp8647a.device, cmd, strlen( cmd ) ) == FAILURE )
		hp8647a_comm_failure( );
	hp8647a_check_complete( );

	return ampl;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

double hp8647a_get_mod_ampl( int type )
{
	const char *cmds[ ] = { "FM:DEV?\n", "AM:DEPT?\n", "PM:DEV?\n" };
	char buffer[ 100 ];
	long length = 100;
	double defaults[ ] = { 1.0e5, 100.0, 10.0 };


	assert( type >= 0 && type < NUM_MOD_TYPES );

	if ( TEST_RUN )
	{
		if ( hp8647a.mod_ampl_is_set[ type ] )
			return hp8647a.mod_ampl[ type ];
		else
			return defaults[ type ];
	}

	if ( gpib_write( hp8647a.device, cmds[ type ], strlen( cmds[ type] ) )
		 == FAILURE ||
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


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8647a_check_complete( void )
{
	char buffer[ 10 ];
	long length = 10;


	do {
		if ( gpib_write( hp8647a.device, "*OPC?\n", 6 ) == FAILURE ||
			 gpib_read( hp8647a.device, buffer, &length ) == FAILURE )
			hp8647a_comm_failure( );
	} while ( buffer[ 1 ] != '1' );
}
	
