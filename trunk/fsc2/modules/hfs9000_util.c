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


#include "hfs9000.h"



/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. an integer multiple of the time base                       */
/*-----------------------------------------------------------------*/

Ticks hfs9000_double2ticks( double time )
{
	double ticks;


	if ( ! hfs9000.is_timebase )
	{
		eprint( FATAL, SET, "%s: Can't set a time because no pulser time "
				"base has been set.\n", pulser_struct.name );
		THROW( EXCEPTION );
	}

	ticks = time / hfs9000.timebase;

	if ( fabs( ticks - lround( ticks ) ) > 1.0e-2 )
	{
		char *t = T_strdup( hfs9000_ptime( time ) );
		eprint( FATAL, SET, "%s: Specified time of %s is not an integer "
				"multiple of the pulser time base of %s.\n",
				pulser_struct.name, t, hfs9000_ptime( hfs9000.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return ( Ticks ) lround( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

double hfs9000_ticks2double( Ticks ticks )
{
	fsc2_assert( hfs9000.is_timebase );
	return ( double ) ( hfs9000.timebase * ticks );
}


/*----------------------------------------------------------------------*/
/* Checks if the difference of the levels specified for a pod connector */
/* are within the valid limits.                                         */
/*----------------------------------------------------------------------*/

void hfs9000_check_pod_level_diff( double high, double low )
{
	if ( low > high )
	{
		eprint( FATAL, SET, "%s: Low voltage level is above high level, "
				"use keyword INVERT to invert the polarity.\n",
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING + 0.1 * VOLTAGE_RESOLUTION )
	{
		eprint( FATAL, SET, "%s: Difference between high and low "
				"voltage of %g V is too big, maximum is %g V.\n",
				pulser_struct.name, high - low, MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING - 0.1 * VOLTAGE_RESOLUTION )
	{
		eprint( FATAL, SET, " %s: Difference between high and low "
				"voltage of %g V is too small, minimum is %g V.\n",
				pulser_struct.name, high - low, MIN_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}
}


/*-----------------------------------------------*/
/* Returns the structure for pulse numbered pnum */
/*-----------------------------------------------*/

PULSE *hfs9000_get_pulse( long pnum )
{
	PULSE *cp = hfs9000_Pulses;


	if ( pnum < 0 )
	{
		eprint( FATAL, SET, "%s: Invalid pulse number: %ld.\n",
				pulser_struct.name, pnum );
		THROW( EXCEPTION );
	}

	while ( cp != NULL )
	{
		if ( cp->num == pnum )
			break;
		cp = cp->next;
	}

	if ( cp == NULL )
	{
		eprint( FATAL, SET, "%s: Referenced pulse %ld does not exist.\n",
				pulser_struct.name, pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *hfs9000_ptime( double time )
{
	static char buffer[ 128 ];

	if ( fabs( time ) >= 1.0 )
		sprintf( buffer, "%g s", time );
	else if ( fabs( time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * time );
	else if ( fabs( time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * time );
	else
		sprintf( buffer, "%g ns", 1.e9 * time );

	return buffer;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *hfs9000_pticks( Ticks ticks )
{
	return hfs9000_ptime( hfs9000_ticks2double( ticks ) );
}


/*---------------------------------------------------------------------------
  Comparison function for two pulses: returns 0 if both pulses are inactive,
  -1 if only the second pulse is inactive or starts at a later time and 1 if
  only the first pulse is inactive pulse or the second pulse starts earlier.
---------------------------------------------------------------------------*/

int hfs9000_start_compare( const void *A, const void *B )
{
	PULSE *a = *( PULSE ** ) A,
		  *b = *( PULSE ** ) B;

	if ( ! a->is_active )
	{
		if ( ! b->is_active )
			return 0;
		else
			return 1;
	}

	if ( ! b->is_active || a->pos <= b->pos )
		return -1;

	return 1;
}


/*---------------------------------------------------------
  Determines the longest sequence of all pulse functions.
-----------------------------------------------------------*/

Ticks hfs9000_get_max_seq_len( void )
{
	int i;
	Ticks max = 0;
	FUNCTION *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &hfs9000.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		max = Ticks_max( max, f->max_seq_len + f->delay );
	}

	if ( hfs9000.is_max_seq_len )
		max = Ticks_max( max, hfs9000.max_seq_len );

	return max;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void hfs9000_set( char *arena, Ticks start, Ticks len, Ticks offset )
{
	fsc2_assert( start + len + offset <= hfs9000.max_seq_len );

	memset( ( void * ) ( arena + offset + start ),
			( int ) SET, len * sizeof( bool ) );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

int hfs9000_diff( char *old, char *new, Ticks *start, Ticks *length )
{
	static Ticks where = 0;
	int ret;
	char *a = old + where,
		 *b = new + where;
	char cur_state;


	/* If we reached the end of the arena in the last call return 0 */

	if ( where == -1 )
	{
		where = 0;
		return 0;
	}

	/* Search for next difference in the arena */

	while ( where < hfs9000.max_seq_len && *a == *b )
	{
		where++;
		a++;
		b++;
	}

	/* If none was found anymore */

	if ( where == hfs9000.max_seq_len )
	{
		where = 0;
		return 0;
	}

	/* store the start position (including the offset and the necessary one
	   due to the pulsers firmware bug) and store if we wave to reset (-1)
	   or to set (1) */

	*start = where;
	ret = *a == SET ? -1 : 1;
	cur_state = *a;

	/* Now figure out the length of the area we have to set or reset */

	*length = 0;
	while ( where < hfs9000.max_seq_len && *a != *b && *a == cur_state )
	{
		where++;
		a++;
		b++;
		( *length )++;
	}

	/* Set a marker that we reached the end of the arena */

	if ( where == hfs9000.max_seq_len )
		where = -1;

	return ret;
}
