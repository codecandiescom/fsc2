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


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_init( const char *name )
{
	char buffer[ 100 ];
	long len = 100;


	if ( gpib_init_device( name, &lecroy9400.device ) == FAILURE )
        return FAIL;

    /* Set digitizer to short form of replies, make it only send a line feed
	   at the end of replies, switch off debugging, transmit data in one block
	   without an END block marker (#I) and set the data format to binary with
	   1 byte length. Then ask it for the status byte 1 to test if the device
	   reacts. */

    if ( gpib_write( lecroy9400.device,
					 "CHDR,OFF;CTRL,LF;CHLP,OFF;CBLS,-1;CFMT,A,BYTE",
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

	if ( is_timebase )
		lecroy9400_set_timebase( lecroy9400.timebase );
	else
	{
		lecroy9400.timebase = lecroy9400_set_timebase( );
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

	/* Now get the waveform descriptor for all channels */

	for ( i = LECROY9400_CH1; i <= LECROY9400_FUNC_F; i++ )
		lecroy9400_get_desc( i );


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
	char cmd[ 40 ] = "TD,";


	gcvt( timebase, 6, cmd + strlen( cmd ) );
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

	fsc2_assert( *type != INVALID_COUPL );    /* call me paranoid... */

	return type;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_coupling( int channel, int type )
{
    char cmd[ 30 ];
	char *cpl[ ] = { "A1M", "D1M", "D50" };


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
	char *cmds[ ] = { "TRC1,?", "TRC2,?", "TRMC,?", "TRMD,?",
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
    char cmd[ 20 ];
	char cmds[ ][ 9 ] = { "TRC1,", "TRC2,", "TRMC,", "TRMD,",
						  "TRFE,", "TRFF," };


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

	strcat( cmds[ channel - LECROY9400_CH1 ], on_off ? "ON" : "OFF" );

	if ( gpib_write( lecroy9400.device, cmd[ channel - LECROY9400_CH1 ],
					 strlen( cmds[ channel - LECROY9400_CH1 ] ) ) == FAILURE )
		lecroy9400_gpib_failure( );

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

	if ( lecroy9400.wv_desc[ 34 ] != 99 )
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
	char *cmds[ ] = { "RD,C1.DE", "RD,C2.DE", "RD,MC.DE", "RD,MD.DE",
					  "RD,FE.DE", "RD,FF.DE" };


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

	if ( gpib_write( lecroy9400.device, cmds[ channel ], 8 ) == FAILURE ||
		 gpib_read( lecroy9400.device, lecroy9400.wv_desc, MAX_DESC_LEN )
		 == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void lecroy9400_gpib_failure( void )
{
	eprint( FATAL, UNSET, "%s: Communication with device failed.\n",
			DEVICE_NAME );
	THROW( EXCEPTION )
}
