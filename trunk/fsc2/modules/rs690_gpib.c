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
static void rs690_table_set( int i, int k, FS *n );
static void rs690_gpib_failure( void );
static void rs690_check( void );


#define UNUSED_BIT -1


#define gpib_write( a, b, c ) fprintf( stderr, "%s\n", ( b ) )
#define gpib_init_device( a, b ) 1



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

	sprintf( cmd, "LOM0,TS,%d!", ( 4 << rs690.timebase_type ) );
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
				 rs690.trig_in_level_type == ECL_LEVEL ? '0' : '1' );
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


	rs690_calc_tables( );

	if ( rs690.old_fs == NULL )
		return rs690_init_channels( );

	/* If necessary change the main tables (table #0) length */

	if ( rs690.new_fs_count != rs690.old_fs_count )
	{
		sprintf( buf, "LTD,T0,%d,T1,1,T2,1!", rs690.new_fs_count );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	/* If necessary change the loop counts for the tables making up the
	   repetition time */

	if ( rs690.new_table.table_loops_1 != rs690.old_table.table_loops_1 ||
		 rs690.new_table.table_loops_2 != rs690.old_table.table_loops_2 ||
		 rs690.new_table.middle_loops  != rs690.old_table.middle_loops )
	{
		sprintf( buf, "LOS,%s,M1,1,T0,1,T1,%d,M2,%d,T2,%d!",
				 rs690.trig_in_mode == EXTERNAL ? "1" : "CONT",
				 rs690.new_table.table_loops_1, rs690.new_table.middle_loops,
				 rs690.new_table.table_loops_2 );
		if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
			rs690_gpib_failure( );
	}

	/* Change all fields in the main table (table #0) where the data or
	   lengths did change - of course, this only needs to be done for the
	   fields we really use (and keep in mind that the number of words in
	   the table might have changed since the last time) */

	for ( i = 0; i <= rs690.last_used_field; i++ )
	{
		for ( k = 1, n = rs690.new_fs, o = rs690.old_fs;
			  n != NULL && o != NULL; n = n->next, o = o->next, k++ )
			if ( n->len != o->len || n->fields[ i ] != o->fields[ i ] )
				rs690_table_set( i, k, n );

		for ( ; n != NULL; n = n->next, k++ )
			rs690_table_set( i, k, n );
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
	int i, j, k;
	FS *n;
	char buf[ 256 ];


	/* Set up the main table for the pulse data and two additional tables
	   (each with just one word) to make up for the repetition time */

	sprintf( buf, "LTD,T0,%d,T1,1,T2,1!", rs690.new_fs_count );
	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	/* Make the sequence and set the loop counts for the tables */

	sprintf( buf, "LOS0,%s,M1,1,T0,1,T1,%d,M2,%d,T2,%d!",
			 rs690.trig_in_mode == EXTERNAL ? "1" : "CONT",
			 rs690.new_table.table_loops_1, rs690.new_table.middle_loops,
			 rs690.new_table.table_loops_2 );
	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );

	/* Now set all the tables */

	for ( i = 0; i <= rs690.last_used_field; i++ )
	{
		/* Table #0 contains the "real" data, i.e. everything until the end
		   of the very last pulse */

		for ( k = 1, n = rs690.new_fs; n != NULL; n = n->next, k++ )
			rs690_table_set( i, k, n );

		/* Table #1 and #2 need to be set only once since their contents and
		   lengths never change, just their number of repetitions. The data
		   are always the data for a state with no pulse and their lengths
		   are always the maximum length, i.e. MAX_TICKS_PER_ENTRY. */

		for ( j = 1; j <= 2; j++ )
		{
			sprintf( buf, "LDT,T%d,FL%d,1,1,%X,%ldns!", j, i,
					 rs690_default_fields[ i ],
					 MAX_TICKS_PER_ENTRY * ( 4 << rs690.timebase_type ) );

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


	rs690.old_table = rs690.new_table;

	rs690.new_table.table_loops_1 = 0;
	rs690.new_table.middle_loops = 0;
	rs690.new_table.table_loops_2 = 0;

	/* If no repetition time is to be used or the length of the last FS
	   structure is not larger than MAX_TICKS_PER_ENTRY we're already done */

	if ( ! rs690.is_repeat_time ||
		rs690.last_new_fs->len <= MAX_TICKS_PER_ENTRY )
		return;

	/* Otherwise we reduce the last FS's length to everything that that isn't
	   a multiple of MAX_TICKS_PER_ENTRY (only in case it then would be 0 we
	   set it to MAX_TICKS_PER_ENTRY). */

	count = rs690.last_new_fs->len;

	if ( ( rs690.last_new_fs->len %= MAX_TICKS_PER_ENTRY ) == 0 )
		rs690.last_new_fs->len = MAX_TICKS_PER_ENTRY;

	/* The remaining time is now dealt with by the tables #1 and #2, both of
	   the maximum length time slice. The first one is the only one needed
	   when the remaining time can be made up by up to MAX_LOOP_REPETITIONS
	   of the maximum time length, otherwise we also need table #2, where we
	   also may use middle loop repetitions to achieve even longer times */

	count = ( count - rs690.last_new_fs->len ) / MAX_TICKS_PER_ENTRY;

	if ( count <= MAX_LOOP_REPETITIONS )
		rs690.new_table.table_loops_1 = count;
	else
	{
		rs690.new_table.table_loops_1 = count % MAX_LOOP_REPETITIONS;
		rs690.new_table.table_loops_2 = MAX_LOOP_REPETITIONS;
		rs690.new_table.middle_loops = count / MAX_LOOP_REPETITIONS;
	}
}


/*-----------------------------------------------------------------*/
/* Utility function for constructing and sending the data for the  */
/* individual table words.                                         */
/* 'i' is the field number, 'k' is the word of table #1 to write   */
/* to and 'n' points to the FS structure with the data and length. */
/*-----------------------------------------------------------------*/

static void rs690_table_set( int i, int k, FS *n )
{
	char buf[ 256 ];


	switch ( rs690.timebase_type )
	{
		case TIMEBASE_16_NS :
			sprintf( buf, "LDT,T0,FL%d,%d,1,%X,%ldns!", i, k,
					 n->fields[ i ] & 0xFFFF, n->len * 16 );
			break;

		case TIMEBASE_8_NS :
			if ( ! n->is_composite )
				sprintf( buf, "LDT,T0,FL%d,%d,1,%X,%ldns!", i, k,
						 n->fields[ i ] & 0xFF, n->len * 8 );
			else
				sprintf( buf, "LDT,T0,FL%d,%d,1,%X,8ns,%X!", i, k,
						 ( n->fields[ i ] >> 8 ) & 0xFF,
						 n->fields[ i ] & 0xFF );
			break;

		case TIMEBASE_4_NS :
			if ( ! n->is_composite )
				sprintf( buf, "LDT,T0,FL%d,%d,1,%X,%ldns!", i, k,
						 n->fields[ i ] & 0xF, n->len * 4 );
			else
				sprintf( buf, "LDT,T0,FL%d,%d,1,%X,4ns,%X,%X,%X!", i, k,
						 ( n->fields[ i ] >> 12 ) & 0xF,
						 ( n->fields[ i ] >> 8 ) & 0xF,
						 ( n->fields[ i ] >> 4 ) & 0xF,
						 n->fields[ i ] & 0xF );
			break;
	}

	if ( gpib_write( rs690.device, buf, strlen( buf ) ) == FAILURE )
		rs690_gpib_failure( );
}


/*---------------------------------------------------------------*/
/* Funcion to be called when the pulser does not react properly. */
/*---------------------------------------------------------------*/

static void rs690_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	rs690_check( );
	THROW( EXCEPTION );
}


/*--------------------------------------*/
/* Function for debugging purposes only.*/
/*--------------------------------------*/

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
