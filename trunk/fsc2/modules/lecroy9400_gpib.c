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



#include "lecroy9400.h"


static bool is_acquiring = UNSET;


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_init( const char *name )
{
	char buffer[ 100 ];
	long len = 100;
	int i;
	unsigned int j;


	is_acquiring = UNSET;

	if ( gpib_init_device( name, &lecroy9400.device ) == FAILURE )
        return FAIL;

    /* Set digitizer to short form of replies, make it only send a line feed
	   at the end of replies, switch off debugging, transmit data in one block
	   without an END block marker (#I) and set the data format to binary with
	   2 byte length. Then ask it for the status byte 1 to test if the device
	   reacts. */

    if ( gpib_write( lecroy9400.device,
					 "CHDR,OFF;CTRL,LF;CHLP,OFF;CBLS,-1;CFMT,A,WORD",
					 45 ) == FAILURE ||
		 gpib_write( lecroy9400.device, "STB,1", 5 ) == FAILURE ||
		 gpib_read( lecroy9400.device, buffer, &len ) == FAILURE )
	{
		gpib_local( lecroy9400.device );
        return FAIL;
	}

	lecroy9400.is_reacting = SET;
	lecroy9400.data_size = 1;

	/* Set or get the time base, if necessary change it to a valid value */

	if ( lecroy9400.is_timebase )
		lecroy9400_set_timebase( lecroy9400.timebase );
	else
	{
		lecroy9400.timebase = lecroy9400_get_timebase( );
		if ( lecroy9400.timebase < tb[ 0 ] )
		{
			lecroy9400_set_timebase( lecroy9400.timebase = tb[ 0 ] );
			eprint( SEVERE, UNSET, "%s: Timebase too fast, changing to %s.\n",
					DEVICE_NAME, lecroy9400_ptime( lecroy9400.timebase ) );
		}
		else if ( lecroy9400.timebase > tb[ TB_ENTRIES - 1 ] )
		{
			lecroy9400_set_timebase( lecroy9400.timebase
									 = tb[ TB_ENTRIES - 1 ] );
			eprint( SEVERE, UNSET, "%s: Timebase too slow, changing to %s.\n",
					DEVICE_NAME, lecroy9400_ptime( lecroy9400.timebase ) );
		}

		lecroy9400.tb_index = lecroy9400_get_tb_index( lecroy9400.timebase );
	}

	/* After we're know reasonable sure that the time constant is 50 ns/div
	   or longer we can switch off interleaved mode */

	if ( gpib_write( lecroy9400.device, "IL,OFF", 6 ) == FAILURE )
		return FAIL;

	/* Now get the waveform descriptor for all used channels */

	for ( i = LECROY9400_CH1; i <= LECROY9400_FUNC_F; i++ )
		if ( lecroy9400.channels_in_use[ i ] )
			lecroy9400_get_desc( i );

	/* Set up averaging for function channels */

	for ( i = LECROY9400_FUNC_E; i <= LECROY9400_FUNC_F; i++ )
	{
		/* If the record length hasn't been set use one that is as least as
		   long as the number of points we can fetch for an averaged curve
		   (i.e. the number of points displayed on the screen which in turn
		   is 10 times the number of points per division) */

		if ( lecroy9400.rec_len[ i ] == UNDEFINED_REC_LEN )
		{
			for ( j = 0; j < CL_ENTRIES; j++ )
				if ( cl[ j ] >= 10 * ppd[ lecroy9400.tb_index ] )
					break;
			lecroy9400.rec_len[ i ] = cl[ j ];
		}

		if ( lecroy9400.is_num_avg[ i ] )
			lecroy9400_set_up_averaging( i, lecroy9400.source_ch[ i ],
										 lecroy9400.num_avg[ i ],
										 lecroy9400.is_reject[ i ],
										 lecroy9400.rec_len[ i ] );
	}

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double lecroy9400_get_timebase( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( lecroy9400.device, "TD,?", 4 ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_timebase( double timebase )
{
	char cmd[ 40 ] = "TD,";


	gcvt( timebase, 6, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int lecroy9400_get_trigger_source( void )
{
	char reply[ 30 ];
	long length = 30;
	int src = LECROY9400_UNDEF;


	if ( gpib_write( lecroy9400.device, "TRD,?", 5 ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

	if ( reply[ 0 ] == 'C' )
		src = reply[ 1 ] == '1' ? LECROY9400_CH1 : LECROY9400_CH2;
	else if ( reply[ 0 ] == 'L' )
		src = LECROY9400_LIN;
	else if ( reply[ 0 ] == 'E' )
		src = reply[ 1 ] == 'X' ? LECROY9400_EXT : LECROY9400_EXT10;

	fsc2_assert( src != LECROY9400_UNDEF );

	return src;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_trigger_source( int channel )
{
	char cmd[ 40 ] = "TRS,";


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 ||
				 ( channel >= LECROY9400_LIN && channel <= LECROY9400_EXT10 )
		       );

	if ( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 )
		sprintf( cmd + 4, "C%1d", channel + 1 );
	else if ( channel == LECROY9400_LIN )
		strcat( cmd, "LI" );
	else if ( channel == LECROY9400_EXT )
		strcat( cmd, "EX" );
	else
		strcat( cmd, "E/10" );

	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double lecroy9400_get_trigger_level( void )
{
	char reply[ 30 ];
	long length = 30;


	if ( gpib_write( lecroy9400.device, "TRL,?", 5 ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_trigger_level( double level )
{
	char cmd[ 40 ] = "TRL,";


	gcvt( level, 6, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double lecroy9400_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

	sprintf( cmd, "C%1dVD,?", channel + 1 );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

    reply[ length - 1 ] = '\0';
	lecroy9400.sens[ channel ] = T_atof( reply );

	return lecroy9400.sens[ channel ];
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_sens( int channel, double sens )
{
    char cmd[ 40 ];


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

	sprintf( cmd, "C%1dVD,", channel + 1 );
	gcvt( sens, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	lecroy9400_get_desc( channel );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double lecroy9400_get_offset( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

	sprintf( cmd, "C%1dOF,?", channel + 1 );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

    reply[ length - 1 ] = '\0';
	lecroy9400.sens[ channel ] = T_atof( reply );

	return lecroy9400.sens[ channel ];
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_offset( int channel, double offset )
{
    char cmd[ 40 ];


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

	sprintf( cmd, "C%1dOF,", channel + 1 );
	gcvt( offset, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	lecroy9400_get_desc( channel );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int lecroy9400_get_coupling( int channel )
{
    char cmd[ 20 ];
	int type = INVALID_COUPL;
	char reply[ 100 ];
	long length = 100;


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

	sprintf( cmd, "C%1dCP,?", channel + 1 );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

    reply[ length - 1 ] = '\0';

	if ( reply[ 0 ] == 'A' )
		type = AC_1_MOHM;
	else if ( reply[ 1 ] == '1' )
		type = DC_1_MOHM;
	else
		type = DC_50_OHM;

	fsc2_assert( type != INVALID_COUPL );    /* call me paranoid... */

	return type;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_coupling( int channel, int type )
{
    char cmd[ 30 ];
	char const *cpl[ ] = { "A1M", "D1M", "D50" };


	fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );
	fsc2_assert( type >= AC_1_MOHM && type <= DC_50_OHM );


	sprintf( cmd, "C%1dCP,%s", channel + 1, cpl[ type ] );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	lecroy9400_get_desc( channel );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_is_displayed( int channel )
{
	char reply[ 30 ];
	long length = 30;
	char const *cmds[ ] = { "TRC1,?", "TRC2,?", "TRMC,?", "TRMD,?",
							"TRFE,?", "TRFF,?" };


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

	if ( gpib_write( lecroy9400.device, cmds[ channel - LECROY9400_CH1 ], 6 )
		 == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

    reply[ length - 1 ] = '\0';

	return strcmp( reply, "ON" ) ? UNSET : SET;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_display( int channel, int on_off )
{
	char cmds[ ][ 9 ] = { "TRC1,", "TRC2,", "TRMC,", "TRMD,",
						  "TRFE,", "TRFF," };


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

	if ( on_off && lecroy9400.num_used_channels >= MAX_USED_CHANNELS )
	{
		eprint( FATAL, UNSET, "%s: More than %d channels are needed to run "
				"the experiment.\n", DEVICE_NAME, MAX_USED_CHANNELS );
		THROW( EXCEPTION )
	}

	strcat( cmds[ channel - LECROY9400_CH1 ], on_off ? "ON" : "OFF" );

	if ( gpib_write( lecroy9400.device, cmds[ channel - LECROY9400_CH1 ],
					 strlen( cmds[ channel - LECROY9400_CH1 ] ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	if ( on_off )
	{
		lecroy9400.num_used_channels++;
		lecroy9400.channels_in_use[ channel ] = SET;
	}
	else
	{
		lecroy9400.num_used_channels--;
		lecroy9400.channels_in_use[ channel ] = UNSET;
	}

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

long lecroy9400_get_num_avg( int channel )
{
	int i;
	long num_avg;


	fsc2_assert( channel == LECROY9400_FUNC_E ||
				 channel == LECROY9400_FUNC_F );

	/* Check that the channel is in averaging mode (in which case the byte
	   at offset 34 of the waveform description is 0), if not return -1 */

	if ( ( int ) lecroy9400.wv_desc[ channel ][ 34 ] != 99 )
		return -1;

	/* 32 bit word (MSB first) at offset 70 of the waveform description is
	   the number of averages */

	for ( num_avg = 0, i = 70; i < 74; i++ )
		num_avg = num_avg * 256 + ( long ) lecroy9400.wv_desc[ channel ][ i ];

	return num_avg;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_get_desc( int channel )
{
	long len = MAX_DESC_LEN;
	char const *cmds[ ] = { "RD,C1.DE", "RD,C2.DE",
							"RD,MC.DE", "RD,MD.DE",
							"RD,FE.DE", "RD,FF.DE" };


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

	if ( gpib_write( lecroy9400.device, cmds[ channel ], 8 ) == FAILURE ||
		 gpib_read( lecroy9400.device, lecroy9400.wv_desc[ channel ], &len )
		 == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double lecroy9400_get_trigger_pos( void )
{
	return 0;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_trigger_pos( double position )
{
	position = position;
	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_set_up_averaging( long channel, long source, long num_avg,
								  bool reject, long rec_len )
{
	char cmd[ 100 ];


	fsc2_assert( channel >= LECROY9400_FUNC_E &&
				 channel <= LECROY9400_FUNC_F );
	fsc2_assert( source >= LECROY9400_CH1 && source <= LECROY9400_CH2 );

	lecroy9400.channels_in_use[ channel ] = SET;
	lecroy9400.channels_in_use[ source ] = SET;
	lecroy9400.source_ch[ channel ] = source;
	lecroy9400.num_avg[ channel ] = num_avg;
	lecroy9400.is_num_avg[ channel ] = SET;
	lecroy9400.rec_len[ channel ] = rec_len;
	lecroy9400.is_reject[ channel ] = reject;

	if ( FSC2_MODE != EXPERIMENT )
		return;

	snprintf( cmd, 100, "SEL,%s", channel == LECROY9400_FUNC_E ? "FE" : "FF" );
	if ( gpib_write( lecroy9400.device, cmd, 6 ) == FAILURE )
		lecroy9400_gpib_failure( );

	snprintf( cmd, 100, "RDF,AVG,SUMMED,%ld,%s,%ld,%s,0", rec_len,
			  source == LECROY9400_CH1 ? "C1" : "C2", num_avg,
			  reject ? "ON" : "OFF" );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );
}



/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_finished( void )
{
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_start_acquisition( void )
{
	if ( lecroy9400.channels_in_use[ LECROY9400_FUNC_E ] )
		if ( gpib_write( lecroy9400.device, "SEL,FE;ARST", 11 )
			 == FAILURE )
			lecroy9400_gpib_failure( );

	if ( lecroy9400.channels_in_use[ LECROY9400_FUNC_F ] )
		if ( gpib_write( lecroy9400.device, "SEL,FF;ARST", 11 )
						 == FAILURE )
			lecroy9400_gpib_failure( );

	is_acquiring = SET;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_get_curve( int ch, WINDOW *w, double **array, long *length,
						   bool use_cursor )
{
	unsigned char *data;
	unsigned char *dp;
	char cmd[ 100 ];
	long len;
	long i;
	double gain_fac, vgain_fac, offset_shift;


	w = w;
	use_cursor = use_cursor;


	if ( ch >= LECROY9400_CH1 && ch <= LECROY9400_CH2 )
	{
		eprint( FATAL, SET, "%s: Getting normal channels is not implemented "
				"yet.\n", DEVICE_NAME );
		THROW( EXCEPTION )
	}

	fsc2_assert( ch == LECROY9400_FUNC_E || ch == LECROY9400_FUNC_F );

	if ( ! lecroy9400.is_num_avg[ ch ] )
	{
		eprint( FATAL, SET, "%s: Averaging has not been initialized for "
				"channel %s.\n", DEVICE_NAME, Channel_Names[ ch ] );
		THROW( EXCEPTION )
	}

	if ( ! is_acquiring )
	{
		eprint( FATAL, SET, "%s: No acquisition has been started.\n",
				DEVICE_NAME );
		THROW( EXCEPTION )
	}

	while ( 1 )
	{
		long cur_avg;
		long j;

		if ( DO_STOP )
			THROW( USER_BREAK_EXCEPTION )

		usleep( 20000 );

		lecroy9400_get_desc( ch );
		for ( cur_avg = 0, j = 0; j < 4; j++ )
			cur_avg = cur_avg * 256 +
				( long ) lecroy9400.wv_desc[ ch ][ 102 + j ];

		if ( cur_avg >= lecroy9400.num_avg[ ch ] )
			break;
	}

	*length = lecroy9400.rec_len[ ch ];

	len = 2 * *length + 10;
	data = T_malloc( 2 * len );

	snprintf( cmd, 100, "RD,%s.DA,1,%ld,0,0",
			  ch == LECROY9400_FUNC_E ? "FE" : "FF", *length );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( lecroy9400.device, data, &len ) == FAILURE )
		lecroy9400_gpib_failure( );

	*length = ( ( long ) data[ 2 ] * 256 + ( long ) data[ 3 ] ) / 2;

	TRY
	{
		*array = T_malloc( *length * sizeof( double ) );
		TRY_SUCCESS;
	}
	CATCH( OUT_OF_MEMORY_EXCEPTION )
	{
		is_acquiring = UNSET;
		T_free( data );
		PASSTHROU( )
	}

	gain_fac = sset[ ( int ) lecroy9400.wv_desc[ ch ][ 4 ] - 22 ];
	vgain_fac = 200.0 / ( ( int ) lecroy9400.wv_desc[ ch ][ 5 ] + 80 );
	offset_shift = 0.04
		       * ( ( double ) lecroy9400.wv_desc[ ch ][ 8 ] * 256.0 
				   + ( double ) lecroy9400.wv_desc[ ch ][ 9 ] - 200 );

	for ( dp = data + 4, i = 0; i < *length; dp += 2, i++ )
		*( *array + i ) = ( double ) dp[ 0 ] * 256.0 + ( double ) dp[ 1 ];

	for ( i = 0; i < *length; i++ )
		*( *array + i ) = gain_fac * ( ( *( *array + i ) - 32768.0 ) / 8192.0
									   - offset_shift ) * vgain_fac;

	T_free( data );
	is_acquiring = UNSET;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_free_running( void )
{
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_gpib_failure( void )
{
	is_acquiring = UNSET;

	eprint( FATAL, UNSET, "%s: Communication with device failed.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
