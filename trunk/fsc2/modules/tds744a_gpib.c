/*
  $Id$
*/



#include "tds744a.h"
#include "gpib_if.h"


int gpib_read_w( int device, char *buffer, long *length );



/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_init( const char *name )
{
	int ch;
	double cp1, cp2;


	tds744a.meas_source = -1;

	if ( gpib_init_device( name, &tds744a.device ) == FAILURE )
        return FAIL;

    /* Set digitizer to short form of replies */

    if ( gpib_write( tds744a.device, "*CLS;:VERB OFF;:HEAD OFF\n" )== FAILURE )
	{
		gpib_local( tds744a.device );
        return FAIL;
	}

    /* Get record length and trigger position */

    if ( ! tds744a_get_record_length( &tds744a.rec_len ) ||
		 ! tds744a_get_trigger_pos( &tds744a.trig_pos ) )
	{
		gpib_local( tds744a.device );
        return FAIL;
	}

    /* Set format of data transfer (binary, INTEL format) */

    if ( gpib_write( tds744a.device, "DAT:ENC SRI;WID 2\n" ) == FAILURE )
	{
		gpib_local( tds744a.device );
        return FAIL;
	}
		
    /* Set unit for cursor setting commands to seconds, cursor types to VBAR */

    if ( gpib_write( tds744a.device, "CURS:FUNC VBA;VBA:UNITS SECO\n" )
		 == FAILURE )
    {
        gpib_local( tds744a.device );
        return FAIL;
    }

	/* Make sure cursor 1 is the left one */

	tds744a_get_cursor_position( 1, &cp1 );
	tds744a_get_cursor_position( 2, &cp2 );
	if ( cp1 > cp2 )
	{
		tds744a_set_cursor( 1, cp2 );
		tds744a_set_cursor( 2, cp1 );
	}

    /* Switch off repetitive acquisition mode */

	if ( gpib_write( tds744a.device, "ACQ:REPE OFF\n" ) == FAILURE )
    {
        gpib_local( tds744a.device );
        return FAIL;
    }

	/* Check if the the time base has been set in the test run, if so send it
	   to the device, otherwise get it */

	if ( tds744a.is_timebase )
		tds744a_set_timebase( tds744a.timebase );
	else
		tds744a.timebase = tds744a_get_timebase( );

	/* If the number of averages has been set in the PREPARATIONS section send
       it to the digitizer now */

	if ( tds744a.is_num_avg == SET )
		tds744a_set_num_avg( tds744a.num_avg );
	else
		tds744a.num_avg = tds744a_get_num_avg( );

	/* Switch on all channels that have been used in the experiment section
	   if they aren't already switched on */

	for ( ch = 0; ch < MAX_CHANNELS; ch++ )
		if ( tds744a.channels_in_use[ ch ] )
			tds744a_display_channel( ch );

    /* Switch to running until run/stop button is pressed and start running */

    if ( gpib_write( tds744a.device, "ACQ:STOPA RUNST;STATE RUN\n" )
		 == FAILURE )
    {
        gpib_local( tds744a.device );
        return FAIL;
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds744a_get_timebase( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds744a.device, "HOR:MAI:SCA?\n" ) == FAILURE ||
		 gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_set_timebase( double timebase )
{
	char cmd[ 40 ] = "HOR:MAI:SCA ";


	gcvt( timebase, 6, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
	if ( gpib_write( tds744a.device, cmd ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/* tds744a_get_record_length() returns the current record length. */
/*----------------------------------------------------------------*/

bool tds744a_get_record_length( long *ret )
{
    char reply[ 30 ];
    long length = 30;
	char *r = reply;


    if ( gpib_write( tds744a.device, "HOR:RECO?\n" ) == FAILURE ||
         gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
	while ( ! isdigit( *r ) && *r++ )
		;
    *ret = T_atol( r );
    return OK;
}

/*-----------------------------------------------------------------*/
/* tds744a_get_trigger_pos() returns the current trigger position. */
/*-----------------------------------------------------------------*/

bool tds744a_get_trigger_pos( double *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds744a.device, "HOR:TRIG:POS?\n" ) == FAILURE ||
         gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = 0.01 * T_atof( reply );
    return OK;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

long tds744a_get_num_avg( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( tds744a_get_acq_mode( ) == AVERAGE )
	{
		if ( gpib_write( tds744a.device,"ACQ:NUMAV?\n" ) == FAILURE ||
			 gpib_read_w( tds744a.device, reply, &length) == FAILURE )
			tds744a_gpib_failure( );

		reply[ length - 1 ] = '\0';
		return T_atol( reply );
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

bool tds744a_set_num_avg( long num_avg )
{
	char cmd[ 30 ];


	/* With number of acquisitions set to zero simply stop the digitizer */

	if ( num_avg == 0 )
	{
		if ( gpib_write( tds744a.device, "ACQ:STATE STOP\n" ) == FAILURE )
			tds744a_gpib_failure( );
		return OK;
	}

	/* With number of acquisitions switch to sample mode, for all other numbers
	   set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
	{
		if ( gpib_write( tds744a.device, "ACQ:MOD SAM\n" ) == FAILURE )
			tds744a_gpib_failure( );
	}
	else
	{
		sprintf( cmd, "ACQ:NUMAV %ld\n", num_avg );
		if ( gpib_write( tds744a.device, cmd ) == FAILURE ||
			 gpib_write( tds744a.device, "ACQ:MOD AVE\n" ) == FAILURE )
			tds744a_gpib_failure( );
	}

	/* Finally restart the digitizer */

	if ( gpib_write( tds744a.device, "ACQ:STATE RUN\n" ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}

/*-------------------------------------------------------------------------*/
/* Function returns the data acquisition mode. If the digitizer is neither */
/* in average nor in sample mode, it is switched to sample mode.           */ 
/*-------------------------------------------------------------------------*/

int tds744a_get_acq_mode( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds744a.device, "ACQ:MOD?\n" ) == FAILURE ||
		 gpib_read_w ( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

	if ( *reply == 'A' )		/* digitizer is in average mode */
		return AVERAGE;

	if ( *reply != 'S' )		/* if not in sample mode set it */
	{
		if ( gpib_write( tds744a.device,"ACQ:MOD SAM\n" ) == FAILURE )
			tds744a_gpib_failure( );
	}

	return SAMPLE;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_get_cursor_position( int cur_no, double *cp )
{
	char cmd[ 40 ] = "CURS:VBA:POSITION";
    char reply[ 30 ];
    long length = 30;

	assert( cur_no == 1 || cur_no == 2 );

	strcat( cmd, cur_no == 1 ? "1?\n" : "2?\n" );
    if ( gpib_write( tds744a.device, cmd ) == FAILURE ||
         gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cp = T_atof( reply );

	return OK;
}


/*----------------------------------------------------*/
/* tds744a_get_cursor_distance() returns the distance */
/* between the two cursors.                           */
/*----------------------------------------------------*/

bool tds744a_get_cursor_distance( double *cd )
{
	double cp2;


	tds744a_get_cursor_position( 1, cd );
	tds744a_get_cursor_position( 2, &cp2 );
    *cd -= cp2;

    return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_set_trigger_channel( const char *name )
{
	char cmd[ 40 ];


	if ( strlen( name ) > 20 )
		return FAIL;

	sprintf( cmd, "TRIG:MAI:EDGE:SOU %s\n", name );
	if ( gpib_write( tds744a.device, cmd ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int tds744a_get_trigger_channel( void )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds744a.device, "TRIG:MAI:EDGE:SOU?\n" ) == FAILURE
		 || gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

    reply[ 3 ] = '\0';

	/* Possible replies are "CH1", "CH2", "CH3", "CH4", "AUX" or "LIN" */

    if ( ! strcmp( reply, "AUX" ) )
        return TDS744A_AUX;

    if ( ! strcmp( reply, "LIN" ) )
        return TDS744A_LIN;

    return ( int ) ( reply[ 2 ] - '1' );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds744a_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------------*/
/* tds744a_clear_SESR() reads the the standard event status register */
/* and thereby clears it - if this isn't done no SRQs are flagged !  */
/*-------------------------------------------------------------------*/

bool tds744a_clear_SESR( void )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds744a.device, "*ESR?\n" ) == FAILURE ||
		 gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*/
/* tds744a_finished() does all the work after an experiment is finished. */
/*-----------------------------------------------------------------------*/

void tds744a_finished( void )
{
    tds744a_clear_SESR( );
    gpib_write( tds744a.device,
				"ACQ:STATE STOP;*SRE 0;:ACQ:STOPA RUNST;STATE RUN\n" );
	gpib_local( tds744a.device );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_set_cursor( int cur_num, double pos )
{
    char cmd[ 60 ] = "CURS:VBA:POSITION";


	assert( cur_num == 1 || cur_num == 2 );

    /* set cursors to specified positions */

	sprintf( cmd + strlen( cmd ), "%d ", cur_num );
    gcvt( pos, 9, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
    if ( gpib_write( tds744a.device, cmd ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_set_track_cursors( bool flag )
{
	char cmd[ 20 ];


	sprintf( cmd, "CURS:MODE %s\n", flag ? "TRAC" : "IND" );
    if ( gpib_write( tds744a.device, cmd ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds744a_set_gated_meas( bool flag )
{
	char cmd[ 20 ];


	sprintf( cmd, "MEASU:GAT %s\n", flag ? "ON" : "OFF" );
    if ( gpib_write( tds744a.device, cmd ) == FAILURE )
		tds744a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds744a_set_snap( bool flag )
{
	char cmd[ 50 ];


	if ( flag )
	{
		if ( gpib_write( tds744a.device, "DAT SNA\n" ) == FAILURE )
			tds744a_gpib_failure( );
	}
	else
	{
		sprintf( cmd, "DAT STAR 1;:DAT STOP %ld\n", tds744a.rec_len );
		if ( gpib_write( tds744a.device, cmd ) == FAILURE )
			tds744a_gpib_failure( );
	}		

	return OK;
}


/*-------------------------------------------------*/
/* Function switches on a channel of the digitizer */
/*-------------------------------------------------*/

bool tds744a_display_channel( int channel )
{
	char cmd[ 30 ];
    char reply[ 10 ];
    long length = 10;


	assert( channel >= 0 && channel < TDS744A_AUX );

	/* Get the channels sensitivity */

	tds744a_get_sens( channel );

	/* check if channel is already displayed */

	sprintf( cmd, "SEL:%s?\n", Channel_Names[ channel ] );
    if ( gpib_write( tds744a.device, cmd ) == FAILURE ||
         gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

    /* if not switch it on */

    if ( reply[ 0 ] == '0' )
    {
        sprintf( cmd, "SEL:%s ON\n", Channel_Names[ channel ] );
        if ( gpib_write( tds744a.device, cmd ) == FAILURE )
			tds744a_gpib_failure( );
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds744a_get_sens( int channel )
{
    char cmd[ 30 ];
    char reply[ 30 ];
    long length = 30;


	assert( channel >= 0 && channel < TDS744A_AUX );

	sprintf( cmd, "%s:SCA?\n", Channel_Names[ channel ] );
	if ( gpib_write( tds744a.device, cmd ) == FAILURE ||
		 gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

    reply[ length - 1 ] = '\0';
	tds744a.channel_sens[ channel ] = T_atof( reply );

	return tds744a.channel_sens[ channel ];
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_start_aquisition( void )
{
    /* Start an acquisition:
       1. clear the SESR register to allow SRQs
       2. set state to run
	   3. set stop after sequence */

    if ( ! tds744a_clear_SESR( ) ||
         gpib_write( tds744a.device, "ACQ:STATE RUN;STOPA SEQ\n" ) == FAILURE )
		tds744a_gpib_failure( );


	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds744a_get_area( int channel, WINDOW *w )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	/* Set measurement type to area */

    if ( gpib_write( tds744a.device, "MEASU:IMM:TYP ARE\n" ) == FAILURE )
		tds744a_gpib_failure( );

	assert( channel >= 0 && channel < TDS744A_AUX );

	/* Set channel (if the channel is not already set) */

	if ( channel != tds744a.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		strcat( cmd, "\n" );
		if ( gpib_write( tds744a.device, cmd ) == FAILURE )
			tds744a_gpib_failure( );
		tds744a.meas_source = channel;
	}

	/* Set the cursors */

	tds744a_set_meas_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( do_quit )
			THROW( EXCEPTION );

		length = 40;
		usleep( 100000 );
		if ( gpib_write( tds744a.device, "BUSY?\n" ) == FAILURE ||
			 gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
			tds744a_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 

	/* Get the the area */

	length = 40;
	if ( gpib_write( tds744a.device, "*WAI;:MEASU:IMM:VAL?\n" ) == FAILURE ||
		 gpib_read_w( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds744a_get_curve( int channel, WINDOW *w, double **data, long *length )
{
	char cmd[ 40 ];
	char reply[ 10 ];
	long len = 10;
	char *buffer;
	long i;
	double scale;


	assert( channel >= 0 && channel < TDS744A_AUX );

	/* Calculate the scale factor for converting the data returned by the
	   digitizer (2-byte integers) into real voltage levels */

	scale = 10.24 * tds744a.channel_sens[ channel ] / ( double ) 0xFFFF;

	/* Set the data source channel (if it's not already set correctly) */ 

	if ( channel != tds744a.data_source )
	{
		sprintf( cmd, "DATA:SOURCE %s\n", Channel_Names[ channel ] );
		if ( gpib_write( tds744a.device, cmd ) == FAILURE )
			tds744a_gpib_failure( );
		tds744a.data_source = channel;
	}

	/* Set the cursors */

	tds744a_set_curve_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( do_quit )
			THROW( EXCEPTION );

		len = 10;
		usleep( 100000 );
		if ( gpib_write( tds744a.device, "BUSY?\n" ) == FAILURE ||
			 gpib_read_w( tds744a.device, reply, &len ) == FAILURE )
			tds744a_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 

	/* Ask digitizer to send the curve */

	if ( gpib_write( tds744a.device, "*WAI;:CURV?\n" ) == FAILURE )
		tds744a_gpib_failure( );

	/* Read just the first two bytes, these are a '#' character plus the
	   number of digits of the following number */

	len = 2;
	if ( gpib_read_w( tds744a.device, reply, &len ) == FAILURE )
		tds744a_gpib_failure( );

	/* Get the number of data bytes to follow */

	len = ( long ) ( reply[ 1 ] - '0' );
	if ( gpib_read_w( tds744a.device, reply, &len ) == FAILURE )
		tds744a_gpib_failure( );

	reply[ len ] = '\0';

	/* The number of data bytes is twice the number of data points plus
	   one byte for the line feed character */

    len = T_atol( reply );
	*length = len / 2;

	*data = T_malloc( *length * sizeof( double ) );
	buffer = T_malloc( len + 1 );

	/* Now get all the data bytes... */

	len = len + 1;
	if ( gpib_read_w( tds744a.device, buffer, &len ) == FAILURE )
	{
		T_free( buffer );
		T_free( *data );
		tds744a_gpib_failure( );
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


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds744a_get_amplitude( int channel, WINDOW *w )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	/* Set measurement type to area */

    if ( gpib_write( tds744a.device, "MEASU:IMM:TYP AMP\n" ) == FAILURE )
		tds744a_gpib_failure( );

	assert( channel >= 0 && channel < TDS744A_AUX );

	/* Set channel (if the channel is not already set) */

	if ( channel != tds744a.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		strcat( cmd, "\n" );
		if ( gpib_write( tds744a.device, cmd ) == FAILURE )
			tds744a_gpib_failure( );
		tds744a.meas_source = channel;
	}

	/* Set the cursors */

	tds744a_set_meas_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( do_quit )
			THROW( EXCEPTION );

		length = 40;
		usleep( 100000 );
		if ( gpib_write( tds744a.device, "BUSY?\n" ) == FAILURE ||
			 gpib_read( tds744a.device, reply, &length ) == FAILURE )
			tds744a_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 


	/* Get the the amplitude */

	length = 40;
	if ( gpib_write( tds744a.device, "*WAI\n" ) == FAILURE ||
		 gpib_write( tds744a.device, "MEASU:IMM:VAL?\n" ) == FAILURE ||
		 gpib_read( tds744a.device, reply, &length ) == FAILURE )
		tds744a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-------------------------------------------------------------------------*/
/* Unfortunately, the digitizer sometimes seems to have problems returning */
/* data when there's not a short delay between the write and read command. */
/* I found empirically that waiting 2 ms seems to get rid of the problem.  */
/*-------------------------------------------------------------------------*/

int gpib_read_w( int device, char *buffer, long *length )
{
	usleep( 2000 );
	return gpib_read( device, buffer, length );
}
