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
static bool rs690_init_channels( void );
static void rs690_calc_tables( void );
static void rs690_gpib_failure( void );
static void rs690_check( void );


#define UNUSED_BIT -1


#define gpib_write( a, b, c ) fprintf( stderr, "%s\n", ( b ) )
#define gpib_init_device( a, b ) 1


/* Multiplicator used to convert lengths of FS to output times */

static long mult_array[ 3 ] = { 1, 4, 16 };
static long mult;


/*------------------------------*/
/* Initialization of the device */
/*------------------------------*/

bool rs690_init( const char *name )
{
	char cmd[ 100 ];
	char reply[ 100 ];
	long length = 100;


	if ( gpib_init_device( name, &rs690.device ) == FAILURE )
		return FAIL;

	/* Try to read the device indentification string to check if the pulser
	   responds. */
/*
	if ( gpib_write( rs690.device, "RUI!", 4 ) == FAILURE ||
		 gpib_read( rs690.device, reply, &length ) == FAILURE )
		rs690_gpib_failure( );
*/
	/* Disable the front panel and stop the pulser */

	rs690_lock_state( SET );
	rs690_run( UNSET );

	/* Load the operating paramters, i.e clean previous settings, switch
	   to timing simulator mode and set number of channels per connector
	   (the number depends on the timebase) */

	sprintf( cmd, "LOM0,TS,%s!", rs690.timebase_type == TIMEBASE_4_NS ?
			 "4" : ( rs690.timebase_type == TIMEBASE_8_NS ? "8" : "16" ) );
	if ( gpib_write( rs690.device, cmd, strlen( cmd ) ) == FAILURE )
		rs690_gpib_failure( );

	/* Set the clock source mode - currently we only support either internal
	   or external ECL and TTL oscillator (defaulting to TTL) */

	sprintf( cmd, "LCK,%c!", rs690.timebase_mode == INTERNAL ? '0' :
			 ( rs690.timebase_level == ECL_LEVEL ? '1' : '2' ) );
	if ( gpib_write( rs690.device, cmd, strlen( cmd ) ) == FAILURE )
		rs690_gpib_failure( );

	/* Set the trigger mode and slope */

	if ( rs690.is_trig_in_mode && rs690.trig_in_mode == EXTERNAL )
	{
		sprintf( cmd, "LET,%c!",
				 rs690.trig_in_level_type == ECL_LEVEL ? '1' : '2' );
		if ( gpib_write( rs690.device, cmd, 6 ) == FAILURE )
			rs690_gpib_failure( );

		if( rs690.is_trig_in_slope )
		{
			sprintf( cmd,
					 "LTE,%c!", rs690.trig_in_slope == POSITIVE ? '0' : '1' );
			if ( gpib_write( rs690.device, cmd, strlen( cmd ) ) == FAILURE )
				rs690_gpib_failure( );
		}
	}
	else if ( gpib_write( rs690.device, "LET,0!", 6 ) == FAILURE )
		rs690_gpib_failure( );

	/* Switch off use of an external gate */

	if ( gpib_write( rs690.device, "LEG,0!", 6 ) == FAILURE )
		rs690_gpib_failure( );

	/* Make the association between the fields bits and the output connector
	   channels and initialize the sequence and tables. */

	mult = mult_array[ rs690.timebase_type ];
	rs690_field_channel_setup( );
	rs690_do_update( );

	return OK;
}


/*-----------------------------------------------------------------*/
/* Function either starts or stops the pulser outputting the pulse */
/* sequence (when there's no external trigger we also have to      */
/* trigger the pulser by software).                                */
/*-----------------------------------------------------------------*/

bool rs690_run( bool state )
{
	if ( gpib_write( rs690.device, state ? "RUN!" : "RST!" , 4 ) == FAILURE )
		rs690_gpib_failure( );

	if ( state && rs690.trig_in_mode == INTERNAL &&
		 gpib_write( rs690.device, "TRG!", 4 ) == FAILURE )
		rs690_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------*/
/* Function locks or unlocks the front panel keyboard. */
/*-----------------------------------------------------*/

bool rs690_lock_state( bool lock )
{
	if ( gpib_write( rs690.device, lock ? "LOC!" : "LCL!", 4 ) == FAILURE )
		rs690_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------------*/
/* The RS690 has 4 (or, when there are 2 HSM cards 8) 16-bit fields.  */
/* Each bit of a field can be associated with one of the channels of  */
/* the (4 or 8) output connectors (different bits of the fields can   */
/* be assigned to the same channel!). The tables of the RS690 consist */
/* of series of field settings, representing the voltage levels to be */
/* output through the channels. In this function the association      */
/* between field bits and channels of the output connectors is done.  */
/* In the function rs690_setup_channels( ) in rs690_init.c the number */
/* of required channels (i.e. bits in the fields) has been counted    */
/* also determined which bit in the fields will represent a certain   */
/* channel (taking into account that for a time base of 4 ns only 4   */
/* the 16 bits of a field can be used and for a time base of 8 ns     */
/* only 8). Here we now tell the pulser about all this.               */
/* To make things simpler we only set the bits of fields really       */
/* needed and mark fields not needed as 'OFF'. To be able to set the  */
/* bits in a field not needed for a channel we figure out a channel   */
/* we're not going to use anyway and assign it to all the superfluous */
/* bits of needed fields.                                             */
/*--------------------------------------------------------------------*/

static bool rs690_field_channel_setup( void )
{
	int i, j;
	int field_list[ 4 * NUM_HSM_CARDS ][ 16 ];
	char buf[ 72 * 4 * NUM_HSM_CARDS + 6 ];
	int free_channel = 0;
	CHANNEL *ch;


	/* For a start mark all bits of all fields as unused */

	for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
		for ( j = 0; j < ( 4 << rs690.timebase_type ); j++ )
			field_list[ i ][ j ] = UNUSED_BIT;

	/* Loop over all channels and for each set the entry in the field_list
	   to the channel number.. While we're at also it find the first unused
	   channel. */

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

	/* Now tell the pulser about it */

	for ( i = 0; i <= rs690.last_used_field; i++ )
	{
		sprintf( buf, "LFD,FL%d,HEX", i );

		for ( j = ( 4 << rs690.timebase_type ) - 1; j >= 0; j-- )
		{
			if ( field_list[ i ][ j ] != UNUSED_BIT )
				sprintf( buf + strlen( buf ), ",%c%d",
						 ( char ) ( 'A' + field_list[ i ][ j ] / 16 ),
						 field_list[ i ][ j ] % 16 );
			else
				sprintf( buf + strlen( buf ), ",%c%d",
						 ( char ) ( 'A' + free_channel / 16 ),
						 free_channel % 16 );
		}

		strcat( buf, "!" );

		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	/* Mark all unused fields as 'OFF' (that you can do this is not
	 documented in the manual but it works...) */

	for ( ; i < 4 * NUM_HSM_CARDS; i++ )
	{
		sprintf( buf, "LFD,FL%d,OFF!", i );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	return OK;
}


/*---------------------------------------------------------------*/
/* Sends all commands to the device to produce the current pulse */
/* sequence - unless this is the very first call the amount of   */
/* data to be send is reduced by just sending data for table     */
/* entries and loop counters that need to be changed. This also  */
/* includes sending only data for fields that are really used.   */
/*---------------------------------------------------------------*/

bool rs690_set_channels( void )
{
	char buf[ 100 ];
	int i, k;
	FS *n, *o;


	if ( rs690.old_fs == NULL )
		return rs690_init_channels( );

	rs690.old_table = rs690.new_table;
	rs690_calc_tables( );

	if ( rs690.new_table.len != rs690.old_table.len )
	{
		sprintf( buf, "LTD,T0,%d,T1,1,T2,1!", rs690.new_table.len );
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
		for ( k = 1, n = rs690.new_fs, o = rs690.old_fs;
			  n != NULL; n = n->next, k++ )
		{
			if ( o == NULL ||
				 n->len != o->len || n->fields[ i ] != o->fields[ i ] )
			{
				sprintf( buf, "LDT,T0,FL%d,%d,1,%X,%ldns!",
						 i, k, n->fields[ i ],
						 ( n->len % MAX_TICKS_PER_ENTRY ) * mult );	

				if ( gpib_write( rs690.device, buf, strlen( buf ) )
					 == FAILURE )
					rs690_gpib_failure( );
			}

			if ( o != NULL )
				o = o->next;
		}
	}

	return OK;
}


/*----------------------------------------------------------------*/
/* When no tables habe been set up yet we need to set all entries */
/* of the table (latter we only have to send information about    */
/* entries that need to be changed).                              */
/*----------------------------------------------------------------*/

static bool rs690_init_channels( void )
{
	int i, j;
	FS *n;
	char buf[ 17000 ];


	rs690_calc_tables( );

	sprintf( buf, "LTD,T0,%d,T1,1,T2,1!", rs690.new_table.len );
	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	sprintf( buf, "LOS0,%s,M1,1,T0,1,T1,%d,M2,1,T2,%d!",
			 rs690.trig_in_mode == EXTERNAL ? "1" : "CONT",
			 rs690.new_table.repeat_1, rs690.new_table.repeat_2 );
	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	for ( i = 0; i <= rs690.last_used_field; i++ )
	{
		sprintf( buf, "LDT,T0,FL%d,1,%d", i, rs690.new_table.len );

		for ( n = rs690.new_fs; n != NULL && n->len % MAX_TICKS_PER_ENTRY != 0;
			  n = n->next )
			sprintf( buf + strlen( buf ), ",%X,%ldns", n->fields[ i ],
					 ( n->len % MAX_TICKS_PER_ENTRY ) * mult );

		strcat( buf, "!" );

		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );

		for ( j = 1; j <= 2; j++ )
		{
			sprintf( buf, "LDT,T%d,FL%d,1,1,%X,%ldns!", j, i,
					 rs690_default_fields[ i ], MAX_TICKS_PER_ENTRY * mult );
			if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
				rs690_gpib_failure( );
		}
	}

	return OK;
}


/*-------------------------------------------------------------*/
/* Function calculates how many tables and loop repetition are */
/* needed to produce the current pulse sequence.               */
/*-------------------------------------------------------------*/

static void rs690_calc_tables( void )
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


/*---------------------------------------------------------------*/
/* Funcion to be called when the pulser does not react properly. */
/*---------------------------------------------------------------*/

static void rs690_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void rs690_check( void )
{
	char b[ 100 ];
	long l = 100;


	if ( gpib_write( rs690.device, "RCS!", 4 ) == FAILURE ||
		 gpib_read( rs690.device, b, &l ) == FAILURE )
		rs690_gpib_failure( );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
