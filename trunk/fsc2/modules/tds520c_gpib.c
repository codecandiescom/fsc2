/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/



#include "tds520c.h"


static double tds520c_get_area_wo_cursor( int channel, WINDOW *w );
static double tds520c_get_amplitude_wo_cursor( int channel, WINDOW *w );


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_init( const char *name )
{
	int ch;
	double cp1, cp2;
	char buffer[ 100 ];
	long len = 100;


	tds520c.meas_source = -1;

	if ( gpib_init_device( name, &tds520c.device ) == FAILURE )
        return FAIL;

    /* Set digitizer to short form of replies */

    if ( gpib_write( tds520c.device, "VERB OFF;:HEAD OFF\n", 19 ) == FAILURE ||
		 gpib_write( tds520c.device, "*STB?\n", 6 ) == FAILURE ||
		 gpib_read( tds520c.device, buffer, &len ) == FAILURE )
	{
		gpib_local( tds520c.device );
        return FAIL;
	}

	tds520c.is_reacting = SET;

	sprintf( buffer, "LOC %s\n", tds520c.lock_state ? "ALL" : "NON" );
	if ( gpib_write( tds520c.device, buffer, strlen( buffer ) ) == FAILURE )
	{
		gpib_local( tds520c.device );
        return FAIL;
	}

    /* Get record length and trigger position */

	if ( tds520c.is_rec_len )
	{
		if ( ! tds520c_set_record_length( tds520c.rec_len ) )
		{
			gpib_local( tds520c.device );
			return FAIL;
		}		
	}
	else
	{
		if ( ! tds520c_get_record_length( &tds520c.rec_len ) )
		{
			gpib_local( tds520c.device );
			return FAIL;
		}

		tds520c.is_rec_len = SET;
	}

	if ( tds520c.is_trig_pos )
	{
		if ( ! tds520c_set_trigger_pos( tds520c.trig_pos ) )
		{
			gpib_local( tds520c.device );
			return FAIL;
		}
	}
	else
	{
		if ( ! tds520c_get_trigger_pos( &tds520c.trig_pos ) )
		{
			gpib_local( tds520c.device );
			return FAIL;
		}

		tds520c.is_trig_pos = SET;
	}

    /* Set format of data transfer (binary, INTEL format) */

    if ( gpib_write( tds520c.device, "DAT:ENC SRI;WID 2\n", 18 )
		 == FAILURE )
	{
		gpib_local( tds520c.device );
        return FAIL;
	}
		
    /* Set unit for cursor setting commands to seconds, cursor types to VBAR */

    if ( gpib_write( tds520c.device, "CURS:FUNC VBA;VBA:UNITS SECO\n", 29 )
		 == FAILURE )
    {
        gpib_local( tds520c.device );
        return FAIL;
    }

	/* Make sure cursor 1 is the left one */

	tds520c_get_cursor_position( 1, &cp1 );
	tds520c_get_cursor_position( 2, &cp2 );
	if ( cp1 > cp2 )
	{
		tds520c_set_cursor( 1, cp2 );
		tds520c_set_cursor( 2, cp1 );
		tds520c.cursor_pos = cp2;
	}
	else
		tds520c.cursor_pos = cp1;

    /* Switch off repetitive acquisition mode */

	if ( gpib_write( tds520c.device, "ACQ:REPE OFF\n", 13 ) == FAILURE )
    {
        gpib_local( tds520c.device );
        return FAIL;
    }

	/* Check if the the time base has been set in the test run, if so send it
	   to the device, otherwise get it */

	if ( tds520c.is_timebase )
		tds520c_set_timebase( tds520c.timebase );
	else
		tds520c.timebase = tds520c_get_timebase( );

	/* If sensitivities have been set in the preparation set them now */

	for ( ch = TDS520C_CH1; ch <= TDS520C_CH2; ch++ )
		if ( tds520c.is_sens[ ch ] )
			tds520c_set_sens( ch, tds520c.sens[ ch ] );

	/* If the number of averages has been set in the PREPARATIONS section send
       to the digitizer now */

	if ( tds520c.is_num_avg == SET )
		tds520c_set_num_avg( tds520c.num_avg );
	else
		tds520c.num_avg = tds520c_get_num_avg( );

	/* Switch on all channels that have been used in the experiment section
	   and that aren't already switched on */

	for ( ch = 0; ch < MAX_CHANNELS; ch++ )
		if ( tds520c.channels_in_use[ ch ] )
			tds520c_display_channel( ch );

    /* Switch to running until run/stop button is pressed and start running */

    if ( gpib_write( tds520c.device, "ACQ:STOPA RUNST;STATE RUN\n", 26 )
		 == FAILURE )
        gpib_local( tds520c.device );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520c_get_timebase( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds520c.device, "HOR:MAI:SCA?\n", 13 ) == FAILURE ||
		 gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_set_timebase( double timebase )
{
	char cmd[ 40 ] = "HOR:MAI:SCA ";


	gcvt( timebase, 6, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
	if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------*/
/* tds520c_set_record_length() sets the record length. */
/*-----------------------------------------------------*/

bool tds520c_set_record_length( long num_points )
{
    char cmd[ 100 ];


	sprintf( cmd, "HOR:RECO %ld\n", num_points );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
        return FAIL;

    return OK;
}


/*----------------------------------------------------------------*/
/* tds520c_get_record_length() returns the current record length. */
/*----------------------------------------------------------------*/

bool tds520c_get_record_length( long *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds520c.device, "HOR:RECO?\n", 10 ) == FAILURE ||
         gpib_read( tds520c.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = T_atol( reply );
    return OK;
}


/*------------------------------------------------------*/
/* tds520c_set_trigger_pos() sets the trigger position. */
/*------------------------------------------------------*/

bool tds520c_set_trigger_pos( double pos )
{
    char cmd[ 100 ];


	sprintf( cmd, "HOR:TRIG:POS %f\n", 100.0 * pos );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		return FAIL;

    return OK;
}


/*-----------------------------------------------------------------*/
/* tds520c_get_trigger_pos() returns the current trigger position. */
/*-----------------------------------------------------------------*/

bool tds520c_get_trigger_pos( double *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds520c.device, "HOR:TRIG:POS?\n", 14 ) == FAILURE ||
         gpib_read( tds520c.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = 0.01 * T_atof( reply );
    return OK;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

long tds520c_get_num_avg( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( tds520c_get_acq_mode( ) == AVERAGE )
	{
		if ( gpib_write( tds520c.device,"ACQ:NUMAV?\n", 11 ) == FAILURE ||
			 gpib_read( tds520c.device, reply, &length) == FAILURE )
			tds520c_gpib_failure( );

		reply[ length - 1 ] = '\0';
		return T_atol( reply );
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

bool tds520c_set_num_avg( long num_avg )
{
	char cmd[ 30 ];


	/* With number of acquisitions set to zero simply stop the digitizer */

	if ( num_avg == 0 )
	{
		if ( gpib_write( tds520c.device, "ACQ:STATE STOP\n", 15 ) == FAILURE )
			tds520c_gpib_failure( );
		return OK;
	}

	/* With number of acquisitions switch to sample mode, for all other numbers
	   set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
	{
		if ( gpib_write( tds520c.device, "ACQ:MOD SAM\n", 12 ) == FAILURE )
			tds520c_gpib_failure( );
	}
	else
	{
		sprintf( cmd, "ACQ:NUMAV %ld\n", num_avg );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_write( tds520c.device, "ACQ:MOD AVE\n", 12 ) == FAILURE )
			tds520c_gpib_failure( );
	}

	/* Finally restart the digitizer */

	if ( gpib_write( tds520c.device, "ACQ:STATE RUN\n", 14 ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}

/*-------------------------------------------------------------------------*/
/* Function returns the data acquisition mode. If the digitizer is neither */
/* in average nor in sample mode, it is switched to sample mode.           */ 
/*-------------------------------------------------------------------------*/

int tds520c_get_acq_mode( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( tds520c.device, "ACQ:MOD?\n", 9 ) == FAILURE ||
		 gpib_read ( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

	if ( *reply == 'A' )		/* digitizer is in average mode */
		return AVERAGE;

	if ( *reply != 'S' )		/* if not in sample mode set it */
	{
		if ( gpib_write( tds520c.device,"ACQ:MOD SAM\n", 12 ) == FAILURE )
			tds520c_gpib_failure( );
	}

	return SAMPLE;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_get_cursor_position( int cur_no, double *cp )
{
	char cmd[ 30 ] = "CURS:VBA:POSITION";
    char reply[ 30 ];
    long length = 30;


	fsc2_assert( cur_no == 1 || cur_no == 2 );

	strcat( cmd, cur_no == 1 ? "1?\n" : "2?\n" );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE ||
         gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cp = T_atof( reply );

	return OK;
}


/*----------------------------------------------------*/
/* tds520c_get_cursor_distance() returns the distance */
/* between the two cursors.                           */
/*----------------------------------------------------*/

bool tds520c_get_cursor_distance( double *cd )
{
	double cp2;


	tds520c_get_cursor_position( 1, cd );
	tds520c_get_cursor_position( 2, &cp2 );
    *cd -= cp2;

    return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_set_trigger_channel( const char *name )
{
	char cmd[ 40 ];


	if ( strlen( name ) > 20 )
		return FAIL;

	if ( name[ 0 ] != 'A' )
		sprintf( cmd, "TRIG:MAI:EDGE:SOU %s\n", name );
	else
	{
		if ( name[ 3 ] == '1' )
			strcpy( cmd, "TRIG:MAI:EDGE:SOU CH3\n" );
		else
			strcpy( cmd, "TRIG:MAI:EDGE:SOU CH4\n" );
	}

	if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int tds520c_get_trigger_channel( void )
{
    char reply[ 30 ];
    long length = 30;
	int val;


    if ( gpib_write( tds520c.device, "TRIG:MAI:EDGE:SOU?\n", 19 ) == FAILURE ||
		 gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

	/* Possible replies are "CH1", "CH2", "CH3", "CH4", or "LIN", where
	   "CH3" or "CH4" really mean "AUX1" or "AUX2", respectively */

    if ( ! strncmp( reply, "LIN", 3 ) )
        return TDS520C_LIN;

    val = ( int ) ( reply[ 2 ] - '1' );

	if ( val >= 2 )
		val = TDS520C_AUX1 + val - 2;

	return val;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520c_gpib_failure( void )
{
	eprint( FATAL, UNSET, "%s: Communication with device failed.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}


/*-------------------------------------------------------------------*/
/* tds520c_clear_SESR() reads the the standard event status register */
/* and thereby clears it - if this isn't done no SRQs are flagged !  */
/*-------------------------------------------------------------------*/

bool tds520c_clear_SESR( void )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds520c.device, "*ESR?\n", 6 ) == FAILURE ||
		 gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*/
/* tds520c_finished() does all the work after an experiment is finished. */
/*-----------------------------------------------------------------------*/

void tds520c_finished( void )
{
	const char *cmd = "*SRE 0;:ACQ:STOPA RUNST;STATE RUN;:LOC NON\n";


	if ( ! tds520c.is_reacting )
		return;

    gpib_write( tds520c.device, "ACQ:STATE STOP\n", 15 );
    tds520c_clear_SESR( );
    gpib_write( tds520c.device, cmd, strlen( cmd ) );
	gpib_local( tds520c.device );
	tds520c.is_reacting = UNSET;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_set_cursor( int cur_num, double pos )
{
    char cmd[ 60 ];


	fsc2_assert( cur_num == 1 || cur_num == 2 );

    /* set cursors to specified positions */

	sprintf( cmd, "CURS:VBA:POSITION%d ", cur_num );
    gcvt( pos, 9, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_set_track_cursors( bool flag )
{
	char cmd[ 20 ];


	sprintf( cmd, "CURS:MODE %s\n", flag ? "TRAC" : "IND" );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds520c_set_gated_meas( bool flag )
{
	char cmd[ 20 ];


	sprintf( cmd, "MEASU:GAT %s\n", flag ? "ON" : "OFF" );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool tds520c_set_snap( bool flag )
{
	char cmd[ 50 ];


	if ( flag )
	{
		if ( gpib_write( tds520c.device, "DAT SNA\n", 8 ) == FAILURE )
			tds520c_gpib_failure( );
	}
	else
	{
		sprintf( cmd, "DAT:STAR 1;STOP %ld\n", tds520c.rec_len );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520c_gpib_failure( );
	}

	return OK;
}


/*-------------------------------------------------*/
/* Function switches on a channel of the digitizer */
/*-------------------------------------------------*/

bool tds520c_display_channel( int channel )
{
	char cmd[ 30 ];
    char reply[ 10 ];
    long length = 10;


	fsc2_assert( channel >= TDS520C_CH1 && channel < TDS520C_AUX1 );

	/* Get the channels sensitivity */

	if ( channel >= TDS520C_CH1 && channel <= TDS520C_CH2 )
	{
		tds520c_get_sens( channel );
		tds520c.is_sens[ channel ] = SET;
	}

	/* Check if channel is already displayed */

	sprintf( cmd, "SEL:%s?\n", Channel_Names[ channel ] );
    if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE ||
         gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

    /* If it's not already switch it on */

    if ( reply[ 0 ] == '0' )
    {
		sprintf( cmd, "SEL:%s ON\n", Channel_Names[ channel ] );
        if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520c_gpib_failure( );
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520c_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


	fsc2_assert( channel >= TDS520C_CH1 && channel <= TDS520C_CH2 );

	sprintf( cmd, "%s:SCA?\n", Channel_Names[ channel ] );
	if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

    reply[ length - 1 ] = '\0';
	tds520c.sens[ channel ] = T_atof( reply );

	return tds520c.sens[ channel ];
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

bool tds520c_set_sens( int channel, double sens )
{
    char cmd[ 40 ];
	char reply[ 40 ];
	long length = 40;


	fsc2_assert( channel >= TDS520C_CH1 && channel <= TDS520C_CH2 );

	/* On this digitizer the sensitivity can only be set to higher values than
	   1 V when using 50 Ohm input impedance */ 

	if ( sens > 1.0 )
	{
		sprintf( cmd, "CH%1d:IMP?\n", channel );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_read( tds520c.device, reply, &length ) == FAILURE )
			tds520c_gpib_failure( );

		if ( strncmp( reply, "MEG", 3 ) )
		{
			if ( I_am == PARENT )
				eprint( FATAL, UNSET, "%s: Can't set sensitivity of channel "
						"%s to %f V while input impedance is 50 Ohm.\n",
						DEVICE_NAME, Channel_Names[ channel ], sens );
			else
				eprint( FATAL, SET, "%s: Can't set sensitivity of channel "
						"%s to %f V while input impedance is 50 Ohm.\n",
						DEVICE_NAME, Channel_Names[ channel ], sens );
			THROW( EXCEPTION )
		}
	}

	sprintf( cmd, "%s:SCA ", Channel_Names[ channel ] );
	gcvt( sens, 8, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
	if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_start_acquisition( void )
{
    /* Start an acquisition:
       1. clear the SESR register to allow SRQs
       2. set state to run
	   3. set stop after sequence */

    if ( ! tds520c_clear_SESR( ) ||
		 gpib_write( tds520c.device, "ACQ:STOPA SEQ;STATE RUN\n", 24 )
		 == FAILURE )
		tds520c_gpib_failure( );


	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520c_get_area( int channel, WINDOW *w, bool use_cursor )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	if ( ! use_cursor )
		return tds520c_get_area_wo_cursor( channel, w );

	/* Set measurement type to area */

    if ( gpib_write( tds520c.device, "MEASU:IMM:TYP ARE\n", 18 ) == FAILURE )
		tds520c_gpib_failure( );

	fsc2_assert( channel >= TDS520C_CH1 && channel < TDS520C_AUX1 );

	/* Set channel (if the channel is not already set) */

	if ( channel != tds520c.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		strcat( cmd, "\n" );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520c_gpib_failure( );
		tds520c.meas_source = channel;
	}

	/* Set the cursors */

	tds520c_set_meas_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION )

		length = 40;
		usleep( 100000 );
		if ( gpib_write( tds520c.device, "BUSY?\n", 6 ) == FAILURE ||
			 gpib_read( tds520c.device, reply, &length ) == FAILURE )
			tds520c_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 


	/* Get the the area */

	length = 40;
	if ( gpib_write( tds520c.device, "*WAI\n", 5 ) == FAILURE ||
		 gpib_write( tds520c.device, "MEASU:IMM:VAL?\n", 15 ) == FAILURE ||
		 gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*---------------------------------------------------------------------*/
/* Measures the area without using the built-in measurement method but */
/* by fetching the curve in the window and integrating it 'by hand'.   */
/*---------------------------------------------------------------------*/

static double tds520c_get_area_wo_cursor( int channel, WINDOW *w )
{
	double *data, area;
	long length, i;
	double pos = 0.0;
	char buf[ 100 ];
	long len = 100;


	tds520c_get_curve( channel, w, &data, &length, UNSET );

	for ( area = 0.0, i = 0; i < length; i++ )
		area += data[ i ];

	T_free( data );

	/* To be able to get comparable results to the built-in measurement
	   method we have to subtract the position setting */

	if ( channel >= TDS520C_CH1 && channel <= TDS520C_CH2 )
	{
		sprintf( buf, "CH%1d:POS?\n", channel + 1 );
		if ( gpib_write( tds520c.device, buf, strlen( buf ) ) == FAILURE ||
			 gpib_read( tds520c.device, buf, &len ) == FAILURE )
			tds520c_gpib_failure( );

		pos = T_atof( buf );
	}

	/* Return the integrated area, multiplied by the the time per point */

	return ( area - length * pos ) * tds520c.timebase / TDS_POINTS_PER_DIV;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds520c_get_curve( int channel, WINDOW *w, double **data, long *length,
						bool use_cursor)
{
	char cmd[ 50 ];
	char reply[ 10 ];
	long len = 10;
	char *buffer;
	char *b;
	long i;
	double scale;
	long len1, len2;


	fsc2_assert( channel >= TDS520C_CH1 && channel < TDS520C_AUX1 );

	/* Calculate the scale factor for converting the data returned by the
	   digitizer (2-byte integers) into real voltage levels */

	if ( channel >= TDS520C_CH1 && channel <= TDS520C_CH2 &&
		 ( ! tds520c.is_sens[ channel ] || ! tds520c.lock_state ) )
	{
		tds520c_get_sens( channel );
		tds520c.is_sens[ channel ] = SET;
	}
	scale = 10.24 * tds520c.sens[ channel ] / ( double ) 0xFFFF;

	/* Set the data source channel (if it's not already set correctly) */ 

	if ( channel != tds520c.data_source )
	{
		sprintf( cmd, "DATA:SOURCE %s\n", Channel_Names[ channel ] );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520c_gpib_failure( );
		tds520c.data_source = channel;
	}

	/* Set the cursors or start and end point of interval */

	if ( use_cursor )
		tds520c_set_curve_window( w );
	else
	{
		sprintf( cmd, "DAT:STAR %ld;STOP %ld\n", 
				 w != NULL ? w->start_num : 1,
				 w != NULL ? w->end_num - 1 : tds520c.rec_len );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520c_gpib_failure( );
	}

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION )

		len = 10;
		usleep( 100000 );
		if ( gpib_write( tds520c.device, "BUSY?\n", 6 ) == FAILURE ||
			 gpib_read( tds520c.device, reply, &len ) == FAILURE )
			tds520c_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 

	/* Calculate how long the curve (with header) is going to be and allocate
       enough memory (data are 2-byte integers) */

	*length = w != NULL ? w->end_num - w->start_num : tds520c.rec_len;
	len = 2 * *length;
	len2 = 1 + lrnd( floor( log10( len ) ) );
	len1 = 1 + lrnd( floor( log10( len2 ) ) );
	len += len1 + len2 + 2;

	*data = T_malloc( *length * sizeof( double ) );
	buffer = T_malloc( len );

	/* Now get all the data bytes... */

	if ( gpib_write( tds520c.device, "CURV?\n", 6 ) == FAILURE ||
		 gpib_read( tds520c.device, buffer, &len ) == FAILURE )
	{
		T_free( buffer );
		T_free( *data );
		tds520c_gpib_failure( );
	}

	/* ....and copy them to the final destination (the data are INTEL format
	   2-byte integers, so the following requires sizeof( short ) == 2 and
	   only works on a machine with INTEL format - there got to be better ways
	   to do it...) Also scale data so that we get the real measured
	   voltage. */

	fsc2_assert( sizeof( short ) == 2 );

	b = buffer + len1 + len2 + 1;

	for ( i = 0; i < *length; i++ )
		*( *data + i ) = scale * ( double ) *( ( short * ) b + i );

	T_free( buffer );
	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds520c_get_amplitude( int channel, WINDOW *w, bool use_cursor )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


	if ( ! use_cursor )
		return tds520c_get_amplitude_wo_cursor( channel, w );

	/* Set measurement type to area */

    if ( gpib_write( tds520c.device, "MEASU:IMM:TYP AMP\n", 18 ) == FAILURE )
		tds520c_gpib_failure( );

	fsc2_assert( channel >= TDS520C_CH1 && channel < TDS520C_AUX1 );

	/* Set channel (if the channel is not already set) */

	if ( channel != tds520c.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		strcat( cmd, "\n" );
		if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
			tds520c_gpib_failure( );
		tds520c.meas_source = channel;
	}

	/* Set the cursors */

	tds520c_set_meas_window( w );

	/* Wait for measurement to finish (use polling) */

	do
	{
		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION )

		length = 40;
		usleep( 100000 );
		if ( gpib_write( tds520c.device, "BUSY?\n", 6 ) == FAILURE ||
			 gpib_read( tds520c.device, reply, &length ) == FAILURE )
			tds520c_gpib_failure( );
	} while ( reply[ 0 ] == '1' ); 


	/* Get the the amplitude */

	length = 40;
	if ( gpib_write( tds520c.device, "*WAI\n", 5 ) == FAILURE ||
		 gpib_write( tds520c.device, "MEASU:IMM:VAL?\n", 15 ) == FAILURE ||
		 gpib_read( tds520c.device, reply, &length ) == FAILURE )
		tds520c_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-------------------------------------------------------------------*/
/* Gets the amplitude without the built-in measurement method but by */
/* fetching the curve in the window and integrating it 'by hand'.    */
/*-------------------------------------------------------------------*/

static double tds520c_get_amplitude_wo_cursor( int channel, WINDOW *w )
{
	double *data, min, max;
	long length, i;


	tds520c_get_curve( channel, w, &data, &length, UNSET );

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


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

void tds520c_free_running( void )
{
	if ( gpib_write( tds520c.device, "ACQ:STOPA RUNST;STATE RUN\n", 26 )
		 == FAILURE )
		tds520c_gpib_failure( );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool tds520c_lock_state( bool lock )
{
	char cmd[ 100 ];
	int channel;


	/* If we switch from unlocked to locked state we need to find out the
	   current sensitiviy settings */

	if ( ! tds520c.lock_state && lock )
		for ( channel = TDS520C_CH1; channel <= TDS520C_CH2; channel++ )
		{
			tds520c_get_sens( channel );
			tds520c.is_sens[ channel ] = SET;
		}

	sprintf( cmd, "LOC %s\n", lock ? "ALL" : "NON" );
	if ( gpib_write( tds520c.device, cmd, strlen( cmd ) ) == FAILURE )
		tds520c_gpib_failure( );
	tds520c.lock_state = lock;

	return OK;
}
