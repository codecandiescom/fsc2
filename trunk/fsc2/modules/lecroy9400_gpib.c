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
	if ( gpib_init_device( name, &lecroy9400.device ) == FAILURE )
        return FAIL;

    /* Set digitizer to short form of replies and make it only send a line
       feed at the end of replies */

    if ( gpib_write( lecroy9400.device, "CHDR,OFF", 8 ) == FAILURE ||
		 gpib_write( lecroy9400.device, "CTRL,LF", 7 ) == FAILURE )
	{
		gpib_local( lecroy9400.device );
        return FAIL;
	}


	/* After we're know reasonable sure that the time constant is 50 ns/div
	   or lower we can switch off interleaved mode */

	if ( gpib_write( lecroy9400.device, "IL,OFF", 6 ) == FAILURE )
		return FAIL;

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
	else if ( channel == LECROY9400_LIN )
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


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_CH2 );

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


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_CH2 );

	sprintf( cmd, "C%1dVD,", channel +1 );
	gcvt( sens, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double lecroy9400_get_offset( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = 30;


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_CH2 );

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


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_CH2 );

	sprintf( cmd, "C%1dOF,", channel +1 );
	gcvt( offset, 8, cmd + strlen( cmd ) );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
		lecroy9400_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_get_coupling( int channel, int *type )
{
    char cmd[ 20 ];


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_CH2 );

	*type = INVALID_COUPL;

	sprintf( cmd, "C%1dCP,?", channel + 1 );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE ||
		 gpib_read( lecroy9400.device, reply, &length ) == FAILURE )
		lecroy9400_gpib_failure( );

    reply[ length - 1 ] = '\0';

	if ( reply[ 0 ] == 'A' )
		*type = AC_1_MOHM;
	else if ( reply[ 1 ] == '1' )
		*type = DC_1_MOHM;
	else
		*type = DC_50_OHM;

	fsc2_assert( *type != INVALID_COUPL );    /* call me paranoid... */

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool lecroy9400_set_coupling( int channel, int type )
{
    char cmd[ 30 ];
	char *cpl[ ] = { "A1M", "D1M", "D50" };


	fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_CH2 );
	fsc2_assert( type >= AC_1_MOHM && type <= DC_450_OHM );


	sprintf( cmd, "C%1dCP,%s", channel + 1, cpl[ type ] );
	if ( gpib_write( lecroy9400.device, cmd, strlen( cmd ) ) == FAILURE )
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
