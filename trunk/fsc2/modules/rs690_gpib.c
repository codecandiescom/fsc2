/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


#include "rs690.h"


static bool rs690_field_channel_setup( void );
static bool rs690_set_channels_4ns( void );
static bool rs690_set_channels_8ns( void );
static bool rs690_set_channels_16ns( void );
static bool rs690_init_channels_16ns( void );
static void rs690_gpib_failure( void );


#define gpib_write( a, b, c ) fprintf( stderr, "%s\n", ( b ) )
#define gpib_init_device( a, b ) 1



/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool rs690_init( const char *name )
{
	char cmd[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( gpib_init_device( name, &rs690.device ) == FAILURE )
		return FAIL;

	/* Try to read the device indentification string to check if the pulser
	   responds */
/*
	if ( gpib_write( rs690.device, "RUI!", 4 ) == FAILURE ||
		 gpib_read( rs690.device, reply, &length ) == FAILURE )
		rs690_gpib_failure( );
*/
	/* Disable the front panel */

	rs690_lock_state( SET );

	/* Stop and reset pulser */

	if ( gpib_write( rs690.device, "RST!", 4 ) == FAILURE )
		rs690_gpib_failure( );

	/* Load the operating paramters, i.e clean previous settings, switch
	   to timing simulator mode and set number of channels per connector
	   (the maximum number depends on the timebase) */

	sprintf( cmd, "LOM0,TS,%s!", rs690.timebase_type == TIMEBASE_4_NS ?
			 "4" : ( rs690.timebase_type == TIMEBASE_8_NS ? "8" : "16" ) );
	if ( gpib_write( rs690.device, cmd, strlen( cmd ) ) == FAILURE )
		rs690_gpib_failure( );

	/* Set the clock source mode - currently we only support either internal
	   or external TTL oscillator */

	sprintf( cmd, "LCK,%c!", rs690.timebase_mode == INTERNAL ? '0' : '2' );
	if ( gpib_write( rs690.device, cmd, strlen( cmd ) ) == FAILURE )
		rs690_gpib_failure( );

	/* Set the trigger mode and slope */

	if ( rs690.is_trig_in_mode && rs690.trig_in_mode == EXTERNAL )
	{
		if ( gpib_write( rs690.device, "LET,2!", 6 ) == FAILURE )
			rs690_gpib_failure( );

		if( rs690.is_trig_in_slope )
		{
			sprintf( cmd,
					 "LTE,%c!", rs690.trig_in_slope == POSITIVE ? '0' : '1' );
			if ( gpib_write( rs690.device, cmd, strlen( cmd ) ) == FAILURE )
				rs690_gpib_failure( );
		}
	}
	else
	{
		if ( gpib_write( rs690.device, "LET,0!", 6 ) == FAILURE )
			rs690_gpib_failure( );
	}

	/* Switch off use of an external gate */

	if ( gpib_write( rs690.device, "LEG,0!", 6 ) == FAILURE )
		rs690_gpib_failure( );

	/* Make the association between the fields bits and the output connector
	   channels */

	rs690_field_channel_setup( );

	rs690_set_channels( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool rs690_field_channel_setup( void )
{
	int i, j;
	int fields[ 4 * NUM_HSM_CARDS ][ 16 ];
	char buf[ 72 * 4 * NUM_HSM_CARDS + 6 ];
	int free_channel = 0;
	CHANNEL *ch;


	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
		for ( j = 0; j < 16; j++ )
			fields[ i ][ j ] = -1;

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		ch = &rs690.channel[ i ];

		if ( ch->function == NULL )
			continue;

		fields[ ch->field ][ ch->bit ] = i;

		if ( i == free_channel )
		{
			free_channel++;
			if ( free_channel % 16 >= ( 4 << rs690.timebase_type ) )
				free_channel = ( free_channel / 16 + 1 ) * 16;
		}
	}

	strcpy( buf, "LFD0" );

	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
	{
		sprintf( buf + strlen( buf ), ",FL%d,HEX", i );

		for ( j = ( 4 << rs690.timebase_type ) - 1; j >= 0; j-- )
			if ( fields[ i ][ j ] != -1 )
				sprintf( buf + strlen( buf ), ",%c%d",
						 ( char ) ( 'A' + fields[ i ][ j ] / 16 ),
						 fields[ i ][ j ] & 0x0f );
			else
				sprintf( buf + strlen( buf ), ",%c%d",
						 ( char ) ( 'A' + free_channel / 16 ),
						 free_channel & 0x0f );
	}


	strcat( buf, "!" );

	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool rs690_run( bool state )
{
	if ( gpib_write( rs690.device, state ? "RUN!" : "STP!" , 4 ) == FAILURE )
		rs690_gpib_failure( );

	if ( state && rs690.is_trig_in_mode && rs690.trig_in_mode == INTERNAL &&
		 gpib_write( rs690.device, "TRG!", 4 ) == FAILURE )
		rs690_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool rs690_set_channels( void )
{
	switch ( rs690.timebase_type )
	{
		case TIMEBASE_4_NS :
			return rs690_set_channels_4ns( );

		case TIMEBASE_8_NS :
			return rs690_set_channels_8ns( );

		case TIMEBASE_16_NS :
			return rs690_set_channels_16ns( );

		default :
			fsc2_assert( 1 == 0 );
	}

	return FAIL;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static bool rs690_set_channels_4ns( void )
{
	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static bool rs690_set_channels_8ns( void )
{
	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static bool rs690_set_channels_16ns( void )
{
	if ( rs690.old_fs == NULL )
		return rs690_init_channels_16ns( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool rs690_init_channels_16ns( void )
{
	int i;
	FS *n;
	char buf[ 10000 ];


	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
	{
		sprintf( buf, "LDT,T0,FL%d,0,%d", i, rs690.new_fs_count );
		for ( n = rs690.new_fs; n != NULL; n = n->next )
			sprintf( buf, ",%X,%ldns", n->fields[ i ], n->len * 16 );
		strcat( buf, "!" );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool rs690_lock_state( bool lock )
{
	if ( gpib_write( dg2020.device, lock ? "LOC!" : "LCL", 4 ) == FAILURE )
		rs690_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void rs690_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
