/*
  $Id$
*/


#include "hfs9000.h"
#include "gpib_if.h"


static void hfs9000_gpib_failure( void );
static void hfs9000_setup_trig_in( void );


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

	strcpy( cmd, "FPAN:MESS \"  ***   REMOTE  -  Keyboard disabled !   ***\"");
	if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
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
	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		cmd[ 8 ] = ( char ) i + '0';
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set duty cycle to 100% for all used channels */

	strcpy( cmd, "PGENA:CH*:DCYC 100" );
	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		cmd[ 8 ] = ( char ) i + '0';
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set polarity for all used channels */

	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		sprintf( cmd, "PGENA:CH%1d:POL %s", i,
				 hfs9000.channel[ i ].function->is_inverted ?
				 "COMP" : "NORM" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set raise/fall times for pulses to maximum speed for all channels */
	/* !!!!!!!!!! This needs some checking, the manual doesn't make clear
	   if the transition time or the transition rate is ment when we set
	   this to MIN Or MAX !!!!!!!!! */

	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		sprintf( cmd, "PGENA:CH%1d:TRANS MIN", i );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set pulse type to RZ (Return to Zero) for all used channels */

	strcpy( cmd, "PGENA:CH*:TYPE RZ");
	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;
		cmd[ 8 ] = ( char ) i + '0';
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Set channel voltage levels */

	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;

		if ( hfs9000.channel[ i ].function->is_high_level )
		{
			sprintf( cmd, "PGENA:CH%1d:HLIM MAX", i );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );

			sprintf( cmd, "PGENA:CH%1d:HIGH ", i );
			gcvt( hfs9000.channel[ i ].function->high_level,
				  5, cmd + strlen( cmd ) );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}

		if ( hfs9000.channel[ i ].function->is_low_level )
		{
			sprintf( cmd, "PGENA:CH%1d:LLIM MIN", i );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );

			sprintf( cmd, "PGENA:CH%1d:LOW ", i );
			gcvt( hfs9000.channel[ i ].function->low_level,
				  5, cmd + strlen( cmd ) );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}
	}

	/* Set channel names */

	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;

		sprintf( cmd, "PGENA:CH%1d:SIGN %s\n", i,
				 hfs9000_names[ hfs9000.channel[ i ].function->self ] );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	/* Switch all used channels on, all other off */

	for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
	{
		if ( ! hfs9000.channel[ i ].function->is_used )
			continue;

		sprintf( cmd, "PGENA:CH%1d:OUTP %s",
				 hfs9000.channel[ i ].function->is_used ? "ON" : "OFF" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}

	hfs9000_setup_trig_in( );

	return OK;
}


/*-------------------------------------------------------------------*/
/* Sets up the way the pulser runs. If the pulser is to run without  */
/* on external trigger in event it is switched to ABURST mode (i.e.  */
/* the pulse sequence is repeated automatically with a delay for the */
/* re-arm time of ca. 15 us. If the pulser has to react to trigger   */
/* in events it is run in BURST mode, i.e. the pulse sequence is     */
/* output after a trigger in event. In this case also the parameter  */
/* for the trigger in are set. Because there's a delay of ca. 130 ns */
/* between the trigger in and the pulser outputting the data this is */
/* somewhat reduced by setting the maximum possible negative channel */
/* delay for all used channels.                                      */
/*-------------------------------------------------------------------*/

static void hfs9000_setup_trig_in( void )
{
	char cmd[ 100 ];


	if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL )
	{
		strcpy( cmd, "TBAS:TIN:INP OFF" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );

		strcpy( cmd, "TBAS:MODE ABUR" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );
	}
	else
	{
		strcpy( cmd, "TBAS:TIN:INP ON" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );

		strcpy( cmd, "TBAS:MODE BURS" );
		if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
			hfs9000_gpib_failure( );

		if ( hfs9000.is_trig_in_slope )
		{
			sprintf( cmd, "TBAS:TIN:SLOP %s",
					 hfs9000.trig_in_slope == POSITIVE ? "POS" : "NEG" );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}

		if ( hfs9000.is_trig_in_level )
		{
			strcpy( cmd, "TBAS:TIN:LEV " );
			gcvt( hfs9000.trig_in_level, 8, command + strlen( cmd ));
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}

		/* Set all channel delays to maximum negative value (-60 ns) */

		for ( i = MIN_CHANNEL; i <= MAX_CHANNEL; i++ )
		{
			if ( ! hfs9000.channel[ i ].function->is_used )
				continue;

			sprintf( cmd, "PGENA:CH%1d:CDEL MIN", i );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}

		if ( hfs9000.channel[ HFS9000_TRIG_OUT ].function->is_used )
		{
			strcpy( cmd, "TBAS:TOUT:PRET 60E-9" );
			if ( gpib_write( hfs9000.device, cmd, strlen( cmd ) ) == FAILURE )
				hfs9000_gpib_failure( );
		}
	}
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void hfs9000_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n",
			pulser_struct.name );
	THROW( EXCEPTION );
}
