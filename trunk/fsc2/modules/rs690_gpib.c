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
static void rs690_calc_tables_16ns( void );
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
	rs690_do_update( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool rs690_field_channel_setup( void )
{
	int i, j;
	int field_list[ 4 * NUM_HSM_CARDS ][ 16 ];
	char buf[ 72 * 4 * NUM_HSM_CARDS + 6 ];
	int free_channel = 0;
	CHANNEL *ch;


	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
		for ( j = 0; j < 16; j++ )
			field_list[ i ][ j ] = -1;

	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		ch = &rs690.channel[ i ];

		if ( ch->function == NULL )
			continue;

		field_list[ ch->field ][ ch->bit ] = i;

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
			if ( field_list[ i ][ j ] != -1 )
				sprintf( buf + strlen( buf ), ",%c%d",
						 ( char ) ( 'A' + field_list[ i ][ j ] / 16 ),
						 field_list[ i ][ j ] );
			else
				sprintf( buf + strlen( buf ), ",%c%d",
						 ( char ) ( 'A' + free_channel / 16 ), free_channel );
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

static void rs690_calc_tables_16ns( void )
{
	Ticks count = 0;


	rs690.new_table.count = 1;
	rs690.new_table.repeat_1 = 0;
	rs690.new_table.repeat_2 = 0;

	if ( rs690.last_new_fs )
	{
		count = rs690.last_new_fs->len;
		if ( count / MAX_TICKS_PER_ENTRY <= MAX_LOOP_REPETITIONS )
			rs690.new_table.count = 2;
		else
			rs690.new_table.count = 3;
	}

	if ( rs690.new_table.count == 1 )
		rs690.new_table.len = rs690.new_fs_count;
	else if ( rs690.new_table.count == 2 )
	{
		rs690.new_table.len = rs690.new_fs_count;
		if ( count % MAX_TICKS_PER_ENTRY == 0 )
			rs690.new_table.len--;
		rs690.new_table.repeat_1 = count / MAX_TICKS_PER_ENTRY;
	}
	else
	{
		rs690.new_table.len = rs690.new_fs_count;
		if ( count % MAX_TICKS_PER_ENTRY == 0 )
			rs690.new_table.len--;
		rs690.new_table.repeat_1 = ( count / MAX_TICKS_PER_ENTRY )
								   % MAX_LOOP_REPETITIONS;
		rs690.new_table.repeat_2 = MAX_LOOP_REPETITIONS;
	}
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static bool rs690_set_channels_16ns( void )
{
	char buf[ 1000 ];
	int i, k;
	FS *n, *o;


	if ( rs690.old_fs == NULL )
		return rs690_init_channels_16ns( );


	rs690.old_table = rs690.new_table;
	rs690_calc_tables_16ns( );

	if ( rs690.new_table.len != rs690.old_table.len )
	{
		sprintf( buf, "LTD0,T0,%d,T1,1,T2,1", rs690.new_table.len );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	if ( rs690.new_table.repeat_1 != rs690.old_table.repeat_1 ||
		 rs690.new_table.repeat_2 != rs690.old_table.repeat_2 )
	{
		sprintf( buf, "LOS0,%s,M1,1,T0,1,T1,%d,M2,1,T2,%d!",
				 rs690.trig_in_mode == EXTERNAL ? "1" : "CONT",
				 rs690.new_table.repeat_1, rs690.new_table.repeat_2 );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	for ( i = 0; i <= rs690.last_used_field; i++ )
	{
		for ( k = 0, n = rs690.new_fs, o = rs690.old_fs;
			  n != NULL && n->len % MAX_TICKS_PER_ENTRY != 0 &&
			  o != NULL && o->len % MAX_TICKS_PER_ENTRY != 0 ;
			  n = n->next, o = o->next, k++ )
		{
			if ( n->len != o->len || n->fields[ i ] != o->fields[ i ] )
			{
				sprintf( buf, "LDT,T0,FL%d,%d,1,%X,%ldns",
						 i, k, n->fields[ i ],
						 ( n->len % MAX_TICKS_PER_ENTRY ) * 16L );	

				if ( gpib_write( rs690.device, buf, strlen( buf ) )
					 == FAILURE )
					rs690_gpib_failure( );
			}
		}
	}

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool rs690_init_channels_16ns( void )
{
	int i, j;
	FS *n;
	char buf[ 1000 ];


	rs690_calc_tables_16ns( );

	sprintf( buf, "LTD0,T0,%d,T1,1,T2,1", rs690.new_table.len );
	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	sprintf( buf, "LOS0,%s,M1,1,T0,1,T1,%d,M2,1,T2,%d!",
			 rs690.trig_in_mode == EXTERNAL ? "1" : "CONT",
			 rs690.new_table.repeat_1, rs690.new_table.repeat_2 );
	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
	{
		sprintf( buf, "LDT,T0,FL%d,0,%d", i, rs690.new_table.len );
		for ( n = rs690.new_fs; n != NULL && n->len % MAX_TICKS_PER_ENTRY != 0;
			  n = n->next )
			sprintf( buf + strlen( buf ), ",%X,%ldns", n->fields[ i ],
					 ( n->len % MAX_TICKS_PER_ENTRY ) * 16L );
		strcat( buf, "!" );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );

		for ( j = 1; j <= 2; j++ )
		{
			sprintf( buf, "LDT,T%d,FL%d,0,1,%X,%ldns!", j, i,
					 rs690_default_fields[ i ],
					 MAX_TICKS_PER_ENTRY * 16L );
			if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
				rs690_gpib_failure( );
		}
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
