/*
  $Id$
*/



#include "tds520a.h"
#include "gpib_if.h"


static double tds520a_get_area_wo_cursor( int channel, WINDOW *w );
static double tds520a_get_amplitude_wo_cursor( int channel, WINDOW *w );


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_init( const char *name )
{
	int ch;
	double cp1, cp2;


	tds520a.meas_source = -1;

	if ( gpib_init_device( name, &tds520a.device ) == FAILURE )
        return FAIL;

	gpib_clr( tds520a.device );

    /* Set digitizer to short form of replies */

    if ( gpib_write( tds520a.device, "VERB OFF;:HEAD OFF\n" ) == FAILURE )
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

    if ( gpib_write( tds520a.device, "DAT:ENC SRI;DAT:WID 2\n" ) == FAILURE )
	{
		gpib_local( tds520a.device );
        return FAIL;
	}
		
    /* Set unit for cursor setting commands to seconds, cursor types to VBAR */

    if ( gpib_write( tds520a.device, "CURS:FUNC VBA;VBA:UNITS SECO\n" )
		 == FAILURE )
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

	if ( gpib_write( tds520a.device, "ACQ:REPE OFF\n" ) == FAILURE )
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

    if ( gpib_write( tds520a.device, "ACQ:STOPA RUNST;STATE RUN\n" )
		 == FAILURE )
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


	if ( gpib_write( tds520a.device, "HOR:MAI:SCA?\n" ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_set_timebase( double timebase )
{
	char cmd[ 40 ] = "HOR:MAI:SCA ";


	gcvt( timebase, 6, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
	if ( gpib_write( tds520a.device, cmd ) == FAILURE )
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


    if ( gpib_write( tds520a.device, "HOR:RECO?\n" ) == FAILURE ||
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


    if ( gpib_write( tds520a.device, "HOR:TRIG:POS?\n" ) == FAILURE ||
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


	if ( tds520a_get_acq_mode( ) == AVERAGE )
	{
		if ( gpib_write( tds520a.device,"ACQ:NUMAV?\n" ) == FAILURE ||
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
		if ( gpib_write( tds520a.device, "ACQ:STATE STOP\n" ) == FAILURE )
			tds520a_gpib_failure( );
		return OK;
	}

	/* With number of acquisitions switch to sample mode, for all other numbers
	   set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
	{
		if ( gpib_write( tds520a.device, "ACQ:MOD SAM\n" ) == FAILURE )
			tds520a_gpib_failure( );
	}
	else
	{
		sprintf( cmd, "ACQ:NUMAV %ld\n", num_avg );
		if ( gpib_write( tds520a.device, cmd ) == FAILURE ||
			 gpib_write( tds520a.device, "ACQ:MOD AVE\n" ) == FAILURE )
			tds520a_gpib_failure( );
	}

	/* Finally restart the digitizer */

	if ( gpib_write( tds520a.device, "ACQ:STATE RUN\n" ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}

/*-------------------------------------------------------------------------*/
/* Function returns the data acquisition mode. If the digitizer is neither */
/* in average nor in sample mode, it is switched to sample mode.           */ 
/*-------------------------------------------------------------------------*/

int tds520a_get_acq_mode( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds520a.device, "ACQ:MOD?\n" ) == FAILURE ||
		 gpib_read ( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	if ( *reply == 'A' )		/* digitizer is in average mode */
		return AVERAGE;

	if ( *reply != 'S' )		/* if not in sample mode set it */
	{
		if ( gpib_write( tds520a.device,"ACQ:MOD SAM\n" ) == FAILURE )
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

	strcat( cmd, cur_no == 1 ? "1?\n" : "2?\n" );
    if ( gpib_write( tds520a.device, cmd ) == FAILURE ||
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
	char cmd[ 40 ];


	if ( strlen( name ) > 20 )
		return FAIL;

	sprintf( cmd, "TRIG:MAI:EDGE:SOU %s\n", name );
	if ( gpib_write( tds520a.device, cmd ) == FAILURE )
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


    if ( gpib_write( tds520a.device, "TRIG:MAI:EDGE:SOU?\n" ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	/* Possible replies are "CH1", "CH2", "CH3", "CH4", or "LIN", where
	   "CH3" or "CH4" really mean "AUX1" or "AUX2", respectively */

    if ( ! strncmp( reply, "LIN", 3 ) )
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
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
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


    if ( gpib_write( tds520a.device, "*ESR?\n" ) == FAILURE ||
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
    gpib_write( tds520a.device,
				"ACQ:STATE STOP;*SRE 0;:ACQ:STOPA RUNST;STATE RUN\n" );
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
	strcat( cmd, "\n" );
    if ( gpib_write( tds520a.device, cmd ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_set_track_cursors( bool flag )
{
	char cmd[ 20 ];


	sprintf( cmd, "CURS:MODE %s\n", flag ? "TRAC" : "IND" );
    if ( gpib_write( tds520a.device, cmd ) == FAILURE )
		tds520a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds520a_set_gated_meas( bool flag )
{
	char cmd[ 20 ];


	sprintf( cmd, "MEASU:GAT %s\n", flag ? "ON" : "OFF" );
    if ( gpib_write( tds520a.device, cmd ) == FAILURE )
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
		if ( gpib_write( tds520a.device, "DAT SNA\n" ) == FAILURE )
			tds520a_gpib_failure( );
	}
	else
	{
		sprintf( cmd, "DAT STAR 1;:DAT STOP %ld\n", tds520a.rec_len );
		if ( gpib_write( tds520a.device, cmd ) == FAILURE )
			tds520a_gpib_failure( );
	}

	return OK;
}


/*-------------------------------------------------*/
/* Function switches on a channel of the digitizer */
/*-------------------------------------------------*/

bool tds520a_display_channel( int channel )
{
	char cmd[ 30 ];
    char reply[ 10 ];
    long length = 10;


	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* Get the channels sensitivity */

	tds520a_get_sens( channel );

	/* Check if channel is already displayed */

	sprintf( cmd, "SEL:%s?\n", Channel_Names[ channel ] );
    if ( gpib_write( tds520a.device, cmd ) == FAILURE ||
         gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

    /* If it's not already switch it on */

    if ( reply[ 0 ] == '0' )
    {
		sprintf( cmd, "SEL:%s ON\n", Channel_Names[ channel ] );
        if ( gpib_write( tds520a.device, cmd ) == FAILURE )
			tds520a_gpib_failure( );
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520a_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


	assert( channel >= 0 && channel < TDS520A_AUX1 );

	sprintf( cmd, "%s:SCA?\n", Channel_Names[ channel ] );
	if ( gpib_write( tds520a.device, cmd ) == FAILURE ||
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
    /* Start an acquisition:
       1. clear the SESR register to allow SRQs
       2. set state to run
	   3. set stop after sequence */

    if ( ! tds520a_clear_SESR( ) ||
		 gpib_write( tds520a.device, "ACQ:STOPA SEQ;STATE RUN\n" ) == FAILURE )
		tds520a_gpib_failure( );


	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520a_get_area( int channel, WINDOW *w, bool use_cursor )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	if ( ! use_cursor )
		return tds520a_get_area_wo_cursor( channel, w );

	/* Set measurement type to area */

    if ( gpib_write( tds520a.device, "MEASU:IMM:TYP ARE\n" ) == FAILURE )
		tds520a_gpib_failure( );

	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* Set channel (if the channel is not already set) */

	if ( channel != tds520a.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		strcat( cmd, "\n" );
		if ( gpib_write( tds520a.device, cmd ) == FAILURE )
			tds520a_gpib_failure( );
		tds520a.meas_source = channel;
	}

	/* Set the cursors */

	tds520a_set_meas_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( do_quit )
			THROW( EXCEPTION );

		length = 40;
		usleep( 100000 );
		if ( gpib_write( tds520a.device, "BUSY?\n" ) == FAILURE ||
			 gpib_read( tds520a.device, reply, &length ) == FAILURE )
			tds520a_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 


	/* Get the the area */

	length = 40;
	if ( gpib_write( tds520a.device, "*WAI\n" ) == FAILURE ||
		 gpib_write( tds520a.device, "MEASU:IMM:VAL?\n" ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-------------------------------------------------------------------*/
/* Measures the area without using te buit-in measurement method but */
/* by fetching the curve in the window and integrating it 'by hand'. */
/*-------------------------------------------------------------------*/

static double tds520a_get_area_wo_cursor( int channel, WINDOW *w )
{
	double *data, area;
	long length, i;


	tds520a_get_curve( channel, w, &data, &length, UNSET );

	for ( area = 0.0, i = 0; i < length; i++ )
		area += data[ i ];

	T_free( data );

	/* Return the integrated area, multiplied by the the time per point */

	return area * tds520a.timebase / TDS_POINTS_PER_DIV;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520a_get_curve( int channel, WINDOW *w, double **data, long *length,
						bool use_cursor)
{
	char cmd[ 50 ];
	char reply[ 10 ];
	long len = 10;
	char *buffer;
	char *b;
	long i;
	double scale;
	long num_points, len1, len2;


	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* Calculate the scale factor for converting the data returned by the
	   digitizer (2-byte integers) into real voltage levels */

	scale = 10.24 * tds520a.channel_sens[ channel ] / ( double ) 0xFFFF;

	/* Set the data source channel (if it's not already set correctly) */ 

	if ( channel != tds520a.data_source )
	{
		sprintf( cmd, "DATA:SOURCE %s\n", Channel_Names[ channel ] );
		if ( gpib_write( tds520a.device, cmd ) == FAILURE )
			tds520a_gpib_failure( );
		tds520a.data_source = channel;
	}

	/* Set the cursors or start and end point of interval */

	if ( use_cursor )
		tds520a_set_curve_window( w );
	else
	{
		sprintf( cmd, "DAT:START %ld;:DAT:STOP %ld\n", 
				 w != NULL ? w->start_num : 1,
				 w != NULL ? w->end_num : tds520a.rec_len );
		if ( gpib_write( tds520a.device, cmd ) == FAILURE )
			tds520a_gpib_failure( );
	}

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( do_quit )
			THROW( EXCEPTION );

		len = 10;
		usleep( 100000 );
		if ( gpib_write( tds520a.device, "BUSY?\n" ) == FAILURE ||
			 gpib_read( tds520a.device, reply, &len ) == FAILURE )
			tds520a_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 

	/* Calculate how long the curve (with header) is going to be and allocate
       enough memory (data are 2-byte integers) */

	*length =   ( w != NULL ? w->end_num : tds520a.rec_len )
		      - ( w != NULL ? w->start_num : 1 ) + 1;
	len = 2 * *length;
	len2 = 1 + ( long ) floor( log10( len ) );
	len1 = 1 + ( long ) floor( log10( len2 ) );
	len += len1 + len2 + 2;

	*data = T_malloc( *length * sizeof( double ) );
	buffer = T_malloc( len );

	/* Now get all the data bytes... */

	if ( gpib_write( tds520a.device, "CURV?\n" ) == FAILURE ||
		 gpib_read( tds520a.device, buffer, &len ) == FAILURE )
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

	b = buffer + len1 + len2 + 1;

	for ( i = 0; i < *length; i++ )
		*( *data + i ) = scale * ( double ) *( ( short * ) b + i );

	T_free( buffer );
	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520a_get_amplitude( int channel, WINDOW *w, bool use_cursor )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	if ( ! use_cursor )
		return tds520a_get_amplitude_wo_cursor( channel, w );

	/* Set measurement type to area */

    if ( gpib_write( tds520a.device, "MEASU:IMM:TYP AMP\n" ) == FAILURE )
		tds520a_gpib_failure( );

	assert( channel >= 0 && channel < TDS520A_AUX1 );

	/* Set channel (if the channel is not already set) */

	if ( channel != tds520a.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		strcat( cmd, "\n" );
		if ( gpib_write( tds520a.device, cmd ) == FAILURE )
			tds520a_gpib_failure( );
		tds520a.meas_source = channel;
	}

	/* Set the cursors */

	tds520a_set_meas_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( do_quit )
			THROW( EXCEPTION );

		length = 40;
		usleep( 100000 );
		if ( gpib_write( tds520a.device, "BUSY?\n" ) == FAILURE ||
			 gpib_read( tds520a.device, reply, &length ) == FAILURE )
			tds520a_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 


	/* Get the the amplitude */

	length = 40;
	if ( gpib_write( tds520a.device, "*WAI\n" ) == FAILURE ||
		 gpib_write( tds520a.device, "MEASU:IMM:VAL?\n" ) == FAILURE ||
		 gpib_read( tds520a.device, reply, &length ) == FAILURE )
		tds520a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-------------------------------------------------------------------*/
/* Gets the amplitude without the built-in measurement method but by */
/* fetching the curve in the window and integrating it 'by hand'.    */
/*-------------------------------------------------------------------*/

static double tds520a_get_amplitude_wo_cursor( int channel, WINDOW *w )
{
	double *data, min, max;
	long length, i;


	tds520a_get_curve( channel, w, &data, &length, UNSET );

	min = HUGE_VAL;
	max = - HUGE_VAL;
	for ( i = 0; i < length; i++ )
	{
		max = d_max( data[ i ], max );
		min = d_min( data[ i ], min );
	}

	T_free( data );

	/* Return the difference between highest and lowest value */

	return max - min;
}
