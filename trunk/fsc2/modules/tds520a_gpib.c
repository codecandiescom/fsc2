/*
  $Id$
*/



#include "tds520a.h"
#include "gpib.h"


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_init( const char *name )
{
	int ch;
	double cp1, cp2;


	tds520a.meas_source = -1;

	if ( gpib_init_device( name, &tds520a.device ) == FAILURE )
        return FAIL;

	/* Set digitzer into local lockout state */

    if ( gpib_write( tds520a.device, "LOC ALL", 7 ) == FAILURE )
	{
		gpib_local( tds520a.device );
        return FAIL;
	}

    /* Set digitizer to short form of replies */

    if ( gpib_write( tds520a.device, "VERB OFF", 8 ) == FAILURE ||
         gpib_write( tds520a.device, "HEAD OFF", 8 ) == FAILURE )
	{
		gpib_local( tds520a.device );
        return FAIL;
	}

    /* Get record length and trigger position */

    if ( ! tds520a_get_record_length( &tds520a.rec_len ) ||
         ! tds520a_get_trigger_pos( &tds520a.trig_pos ) )
	{
		gpib_local( tds520a.device );
        return FAIL;
	}

    /* Set format of data transfer (binary, INTEL format) */

    if ( gpib_write( tds520a.device, "DAT:ENC SRI", 11 ) == FAILURE ||
         gpib_write( tds520a.device, "DAT:WID 2", 9 )    == FAILURE )
	{
		gpib_local( tds520a.device );
        return FAIL;
	}
		
    /* Set unit for cursor setting commands to seconds, cursor types to VBAR */

    if ( gpib_write( tds520a.device, "CURS:FUNC VBA", 13 )       == FAILURE ||
         gpib_write( tds520a.device, "CURS:VBA:UNITS SECO", 19 ) == FAILURE )
    {
        gpib_local( tds520a.device );
        return FAIL;
    }

	/* Make sure cursor 1 is the left one */

	tds520a_get_cursor_position( 1, &cp1 );
	tds520a_get_cursor_position( 2, &cp2 );
	if ( cp1 > cp2 )
	{
		tds520a_set_cursor( 1, cp2 );
		tds520a_set_cursor( 2, cp1 );
	}

    /* Switch off repetitive acquisition mode */

	if ( gpib_write( tds520a.device, "ACQ:REPE OFF", 12 ) == FAILURE )
    {
        gpib_local( tds520a.device );
        return FAIL;
    }

	/* Check if the the time base has been set in the test run, if so send it
	   to the device, otherwise get it */

	if ( tds520a.is_timebase )
		tds520a_set_timebase( tds520a.timebase );
	else
		tds520a.timebase = tds520a_get_timebase( );

	/* If the number of averages has been set in the PREPARATIONS section send
       to the digitizer now */

	if ( tds520a.is_num_avg == SET )
		tds520a_set_num_avg( tds520a.num_avg );
	else
		tds520a.num_avg = tds520a_get_num_avg( );

	/* Switch on all channels that have been used in the experiment section
	   and that aren't already switched on */

	for ( ch = 0; ch < MAX_CHANNELS; ch++ )
		if ( tds520a.channels_in_use[ ch ] )
			tds520a_display_channel( ch );

    /* Switch to running until run/stop button is pressed and start running */

    if ( gpib_write( tds520a.device, "ACQ:STOPA RUNST", 15 ) == FAILURE ||
         gpib_write( tds520a.device, "ACQ:STATE RUN", 13 ) == FAILURE )
    {
        gpib_local( tds520a.device );
        return FAIL;
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520a_get_timebase( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds520a.device, "HOR:MAI:SCA?", 12 ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_set_timebase( double timebase )
{
	char command[40] = "HOR:MAI:SCA ";


	gcvt( timebase, 6, command + strlen( command ) );
	if ( gpib_write( tds520a.device, command, strlen( command ) ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/* tds520a_get_record_length() returns the current record length. */
/*----------------------------------------------------------------*/

bool tds520a_get_record_length( long *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds520a.device, "HOR:RECO?", 9 ) == FAILURE ||
         gpib_read( tds520a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = T_atol( reply );
    return OK;
}

/*-----------------------------------------------------------------*/
/* tds520a_get_trigger_pos() returns the current trigger position. */
/*-----------------------------------------------------------------*/

bool tds520a_get_trigger_pos( double *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds520a.device, "HOR:TRIG:POS?", 13 ) == FAILURE ||
         gpib_read( tds520a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = 0.01 * T_atof( reply );
    return OK;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

long tds520a_get_num_avg( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( tds520a_get_acq_mode() == AVERAGE )
	{
		if ( gpib_write( tds520a.device,"ACQ:NUMAV?", 10 ) == FAILURE ||
			 gpib_read( tds520a.device, reply, &length) == FAILURE )
			tds520a_gpib_failure( );

		reply[ length - 1 ] = '\0';
		return T_atol( reply );
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

bool tds520a_set_num_avg( long num_avg )
{
	char cmd[ 30 ];


	/* With number of acquisitions set to zero simply stop the digitizer */

	if ( num_avg == 0 )
	{
		if ( gpib_write( tds520a.device, "ACQ:STATE STOP", 14 ) == FAILURE )
			tds520a_gpib_failure( );
		return OK;
	}

	/* With number of acquisitions switch to sample mode, for all other numbers
	   set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
	{
		if ( gpib_write( tds520a.device, "ACQ:MOD SAM", 11 ) == FAILURE )
			tds520a_gpib_failure( );
	}
	else
	{
		strcpy(cmd, "ACQ:NUMAV ");
		sprintf( cmd + strlen( cmd ), "%ld", num_avg );
		if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_write( tds520a.device, "ACQ:MOD AVE", 11 ) == FAILURE )
			tds520a_gpib_failure( );
	}

	/* Finally restart the digitizer */

	if ( gpib_write( tds520a.device, "ACQ:STATE RUN", 13 ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}

/*-------------------------------------------------------------------------*/
/* Function returns the data acquisition mode. If the digitizer is neither */
/* in average nor in sample mode, it is switched to sample mode.           */ 
/*-------------------------------------------------------------------------*/

int tds520a_get_acq_mode(void)
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds520a.device, "ACQ:MOD?", 8 ) == FAILURE ||
		 gpib_read ( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	if ( *reply == 'A' )		/* digitizer is in average mode */
		return AVERAGE;

	if ( *reply != 'S' )		/* if not in sample mode set it */
	{
		if ( gpib_write( tds520a.device,"ACQ:MOD SAM", 11 ) == FAILURE )
			tds520a_gpib_failure( );
	}

	return SAMPLE;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_get_cursor_position( int cur_no, double *cp )
{
	char cmd[ 30 ] = "CURS:VBA:POSITION";
    char reply[ 30 ];
    long length = 30;

	assert( cur_no == 1 || cur_no == 2 );

	strcat( cmd, cur_no == 1 ? "1?" : "2?" );
    if ( gpib_write( tds520a.device, cmd, 19 ) == FAILURE ||
         gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cp = T_atof( reply );

	return OK;
}


/*----------------------------------------------------*/
/* tds520a_get_cursor_distance() returns the distance */
/* between the two cursors.                           */
/*----------------------------------------------------*/

bool tds520a_get_cursor_distance( double *cd )
{
	double cp2;


	tds520a_get_cursor_position( 1, cd );
	tds520a_get_cursor_position( 2, &cp2 );
    *cd -= cp2;

    return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_set_trigger_channel( const char *name )
{
	char cmd[ 40 ] = "TRIG:MAI:EDGE:SOU ";

	if ( strlen( name ) > 20 )
		return FAIL;

	strcat( cmd, name );

	if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int tds520a_get_trigger_channel( void )
{
    char reply[ 30 ];
    long length = 30;
	int val;


    if ( gpib_write( tds520a.device, "TRIG:MAI:EDGE:SOU?", 18 ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

    reply[3] = '\0';

	/* Possible replies are "CH1", "CH2", "CH3", "CH4", or "LIN", where
	   "CH3" or "CH4" really mean "AUX1" or "AUX2", respectively */

    if ( ! strcmp( reply, "LIN" ) )
        return TDS520A_LIN;

    val = ( int ) ( reply[ 2 ] - '1' );

	if ( val >= 2 )
		val = TDS520A_AUX1 + val - 2;

	return val;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520a_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.", DEVICE_NAME );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------------*/
/* tds520a_clear_SESR() reads the the standard event status register */
/* and thereby clears it - if this isn't done no SRQs are flagged !  */
/*-------------------------------------------------------------------*/

bool tds520a_clear_SESR( void )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds520a.device, "*ESR?", 5 ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*/
/* tds520a_finished() does all the work after an experiment is finished. */
/*-----------------------------------------------------------------------*/

void tds520a_finished( void )
{
    tds520a_clear_SESR( );
    gpib_write( tds520a.device, "ACQ:STATE STOP", 14 );

    gpib_write( tds520a.device, "*SRE 0", 5 );
    gpib_write( tds520a.device, "ACQ:STOPA RUNST", 15 );
    gpib_write( tds520a.device, "ACQ:STATE RUN", 13 );
    gpib_write( tds520a.device, "LOC NON", 7 ); 

	gpib_local( tds520a.device );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_set_cursor( int cur_num, double pos )
{
    char cmd[ 60 ];


	assert( cur_num == 1 || cur_num == 2 );

    /* set cursors to specified positions */

	sprintf( cmd, "CURS:VBA:POSITION%d ", cur_num );
    gcvt( pos, 9, cmd + strlen( cmd ) );
    if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_set_track_cursors( bool flag )
{
	char cmd[ 20 ] = "CURS:MODE ";

	strcat( cmd, flag ? "TRAC" : "IND" );
    if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds520a_set_gated_meas( bool flag )
{
	char cmd[ 20 ] = "MEASU:GAT ";

	strcat( cmd, flag ? "ON" : "OFF" );
    if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds520a_set_snap( bool flag )
{
	char cmd[ 50 ];

	if ( flag )
	{
		strcpy( cmd, "DAT SNA" );
		if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520a_gpib_failure( );
	}
	else
	{
		strcpy( cmd, "DAT STAR 1" );
		if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520a_gpib_failure( );
		sprintf( cmd, "DAT STOP %ld", tds520a.rec_len );
		if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520a_gpib_failure( );
	}		

	return OK;
}


/*-------------------------------------------------*/
/* Function switches on a channel of the digitizer */
/*-------------------------------------------------*/

bool tds520a_display_channel( int channel )
{
	char cmd[ 30 ] = "SEL:";
    char reply[ 10 ];
    long length = 10;


	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* Get the channels sensitivity */

	tds520a_get_sens( channel );

	/* check if channel is already displayed */

    strcat( cmd, Channel_Names[ channel ] );
    strcat( cmd, "?" );
    if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE ||
         gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

    /* if not switch it on */

    if ( reply[ 0 ] == '0' )
    {
        strcpy( cmd, "SEL:" );
        strcat( cmd, Channel_Names[ channel ] );
        strcat( cmd, " ON" );
        if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520a_gpib_failure( );
//		sleep( 2 );                  // do we really need this ?
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520a_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[30];
    long length = 30;


	assert( channel >= 0 && channel < TDS520A_AUX1 );

	strcpy( cmd, Channel_Names[ channel ] );
	strcat( cmd, ":SCA?" );

	if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

    reply[ length - 1 ] = '\0';
	tds520a.channel_sens[ channel ] = T_atof( reply );

	return tds520a.channel_sens[ channel ];
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_start_aquisition( void )
{
//	int status;


    /* Start an acquisition:
       1. clear the SESR register to allow SRQs
       2. set state to run
	   3. set stop after sequence */

    if ( ! tds520a_clear_SESR( ) ||
         gpib_write( tds520a.device, "ACQ:STATE RUN", 13 ) == FAILURE ||
		 gpib_write( tds520a.device, "ACQ:STOPA SEQ", 13 ) == FAILURE )
		tds520a_gpib_failure( );


	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520a_get_area( int channel, WINDOW *w )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	/* set measurement type to area */

    if ( gpib_write( tds520a.device, "MEASU:IMM:TYP ARE", 17 ) == FAILURE )
		tds520a_gpib_failure( );

	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* set channel (if the channel is not already set) */

	if ( channel != tds520a.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520a_gpib_failure( );
		tds520a.meas_source = channel;
	}

	/* set the cursors */

	tds520a_set_meas_window( w );

	/* get the the area */

	if ( gpib_write( tds520a.device, "*WAI", 4 ) == FAILURE ||
		 gpib_write( tds520a.device, "MEASU:IMM:VAL?", 14 ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_get_curve( int channel, WINDOW *w, double **data, long *length )
{
	char cmd[ 40 ];
	char reply[ 10 ];
	long len = 10;
	char *buffer;
	long i;
	double scale;


	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* Calculate the scale factor for converting the data returned by the
	   digitizer (2-byte integers) into real voltage levels */

	scale = 10.24 * tds520a.channel_sens[ channel ] / ( double ) 0xFFFF;

	/* Set the data source channel (if it's not already set correctly) */ 

	if ( channel != tds520a.data_source )
	{
		strcpy( cmd, "DATA:SOURCE " );
		strcat( cmd, Channel_Names[ channel ] );
		if ( gpib_write( tds520a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520a_gpib_failure( );
		tds520a.data_source = channel;
	}

	/* Set the cursors */

	tds520a_set_curve_window( w );

	/* Ask digitizer to send the curve */

	if ( gpib_write( tds520a.device, "*WAI", 4 ) == FAILURE ||
		 gpib_write( tds520a.device, "CURV?", 5 ) == FAILURE )
		tds520a_gpib_failure( );

	/* Read just the first two bytes, these are a '#' character plus the
	   number of digits of the following number */

	len = 2;
	if ( gpib_read( tds520a.device, reply, &len ) == FAILURE )
		tds520a_gpib_failure( );

	/* Get the number of data bytes to follow */

	len = ( long ) ( reply[ 1 ] - '0' );
	if ( gpib_read( tds520a.device, reply, &len ) == FAILURE )
		tds520a_gpib_failure( );

	reply[ len ] = '\0';

	/* The number of data bytes is twice the number of data points plus
	   one byte for the line feed character */

    len = T_atol( reply );
	*length = len / 2;

	*data = T_malloc( *length * sizeof( double ) );
	buffer = T_malloc( len );

	/* Now get all the data bytes... */

	if ( gpib_read( tds520a.device, buffer, &len ) == FAILURE )
	{
		T_free( buffer );
		T_free( *data );
		tds520a_gpib_failure( );
	}

	/* ....and copy them to the final destination (the data are INTEL format
	   2-byte integers, so the following requires sizeof( short ) == 2 and
	   only works on a machine with INTEL format - there got to be better ways
	   to do it...) Also scale data so that we get the real measured
	   voltage. */

	assert( sizeof( short ) == 2 );

	for ( i = 0; i < *length; i++ )
		*( *data + i ) = scale * ( double ) *( ( short * ) buffer + i );

	T_free( buffer );

	return OK;
}
