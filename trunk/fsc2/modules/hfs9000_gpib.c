/*
  $Id$
*/


#include "hfs9000.h"
#include "gpib_if.h"


static void hfs9000_gpib_failure( void );


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool hfs9000_init( const char *name )
{
	int i;
	char cmd[ 100 ];


	if ( gpib_init_device( name, &hfs9000.device ) == FAILURE )
		return FAIL;

	if ( gpib_write( hfs9000.device, "HEADER OFF", 10 ) == FAILURE )
		hfs9000_gpib_failure( );

	/* Stop the pulser */

	if ( gpib_write( hfs9000.device, "TBAS:RUN OFF", 12 ) == FAILURE )
		hfs9000_gpib_failure( );

	/* Set timebase for pulser */

	if ( hfs9000.is_timebase )
	{
		strcpy( cmd, "TBAS:PER " );
		gcvt( hfs9000.timebase, 9, cmd + strlen( cmd) );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}
	else
	{
		eprint( FATAL, "%s: Timebase of pulser has not been set.\n",
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* Set lead delay to zero for all used channels */

	strcpy( cmd, "PGENA:CH*:LDEL MIN" );
	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		cmd[ 8 ] = ( char ) i + '1';
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set duty cycle to 100% for all used channels */

	strcpy( cmd, "PGENA:CH*:DCYC 100" );
	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		cmd[ 8 ] = ( char ) i + '1';
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set polarity for all used channels */

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		sprintf( cmd, "PGENA:CH%1d:POL %s", i + 1,
				 hfs9000.channel[ i ].function->is_inverted ?
				 "COMP" : "NORM" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set pulse type to RZ (Return to Zero) for all used channels */

	strcpy( cmd, "PGENA:CH*:TYPE RZ");
	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		cmd[ 8 ] = ( char ) i + '1';
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set channel voltage levels */

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;

		if ( hfs9000.channel[ i ].function->is_high_level )
		{
			sprintf( cmd, "PGENA:CH%1d:HIGH ", i + 1 );
			gcvt( hfs9000.channel[ i ].function->high_level,
				  5, cmd + strlen( cmd ) );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}

		if ( hfs9000.channel[ i ].function->is_low_level )
		{
			sprintf( cmd, "PGENA:CH%1d:LOW ", i + 1 );
			gcvt( hfs9000.channel[ i ].function->low_level,
				  5, cmd + strlen( cmd ) );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}
	}

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void hfs9000_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n",
			pulser_struct.name );
	THROW( EXCEPTION );
}
