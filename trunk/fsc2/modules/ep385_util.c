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


#include "ep385.h"



/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. an integer multiple of the time base                       */
/*-----------------------------------------------------------------*/

Ticks ep385_double2ticks( double p_time )
{
	double ticks;


	/* If the time base hasn't been set yet this indicates that we should
	   use the built-in time base (by having *no* TIMEBASE command in the
	   PREPARATIONS section) */

	if ( ! ep385.is_timebase )
	{
		ep385.is_timebase = SET;
		ep385.timebase_mode = INTERNAL;
		ep385.timebase = FIXED_TIMEBASE;
	}

	ticks = p_time / ep385.timebase;

	if ( fabs( ticks - lrnd( ticks ) ) > 1.0e-2 )
	{
		char *t = T_strdup( ep385_ptime( p_time ) );
		print( FATAL, "Specified time of %s is not an integer multiple of the "
			   "fixed pulser time base of %s.\n",
			   t, ep385_ptime( ep385.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return ( Ticks ) lrnd( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

double ep385_ticks2double( Ticks ticks )
{
	return ( double ) ( ep385.timebase * ticks );
}


/*-----------------------------------------------*/
/* Returns the structure for pulse numbered pnum */
/*-----------------------------------------------*/

PULSE *ep385_get_pulse( long pnum )
{
	PULSE *cp = ep385_Pulses;


	if ( pnum < 0 )
	{
		print( FATAL, "Invalid pulse number: %ld.\n", pnum );
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
		print( FATAL, "Referenced pulse %ld does not exist.\n", pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *ep385_ptime( double p_time )
{
	static char buffer[ 128 ];

	if ( fabs( p_time ) >= 1.0 )
		sprintf( buffer, "%g s", p_time );
	else if ( fabs( p_time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * p_time );
	else if ( fabs( p_time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * p_time );
	else
		sprintf( buffer, "%g ns", 1.e9 * p_time );

	return buffer;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *ep385_pticks( Ticks ticks )
{
	return ep385_ptime( ep385_ticks2double( ticks ) );
}


/*---------------------------------------------------------------------------
  Comparison function for two pulses: returns 0 if both pulses are inactive,
  -1 if only the second pulse is inactive or starts at a later time and 1 if
  only the first pulse is inactive pulse or the second pulse starts earlier.
---------------------------------------------------------------------------*/

int ep385_pulse_compare( const void *A, const void *B )
{
	PULSE_PARAMS *a = *( PULSE_PARAMS ** ) A,
		         *b = *( PULSE_PARAMS ** ) B;

	return a->pos <= b->pos ? -1 : 1;
}


/*---------------------------------------------------------
  Determines the longest sequence of all pulse functions.
-----------------------------------------------------------*/

Ticks ep385_get_max_seq_len( void )
{
	int i;
	Ticks max = 0;
	FUNCTION *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &ep385.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		max = Ticks_max( max, f->max_seq_len + f->delay );
	}

	if ( ep385.is_max_seq_len )
		max = Ticks_max( max, ep385.max_seq_len );

	return max;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void ep385_set( char *arena, Ticks start, Ticks len, Ticks offset )
{
	fsc2_assert( start + len + offset <= ep385.max_seq_len );

	memset( arena + offset + start, SET, len );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

int ep385_diff( char *old_p, char *new_p, Ticks *start, Ticks *length )
{
	static Ticks where = 0;
	int ret;
	char *a = old_p + where,
		 *b = new_p + where;
	char cur_state;


	/* If we reached the end of the arena in the last call return 0 */

	if ( where == -1 )
	{
		where = 0;
		return 0;
	}

	/* Search for next difference in the arena */

	while ( where < ep385.max_seq_len && *a == *b )
	{
		where++;
		a++;
		b++;
	}

	/* If none was found anymore */

	if ( where == ep385.max_seq_len )
	{
		where = 0;
		return 0;
	}

	/* Store the start position (including the offset and the necessary one
	   due to the pulsers firmware bug) and store if we wave to reset (-1)
	   or to set (1) */

	*start = where;
	ret = *a == SET ? -1 : 1;
	cur_state = *a;

	/* Now figure out the length of the area we have to set or reset */

	*length = 0;
	while ( where < ep385.max_seq_len && *a != *b && *a == cur_state )
	{
		where++;
		a++;
		b++;
		( *length )++;
	}

	/* Set a marker that we reached the end of the arena */

	if ( where == ep385.max_seq_len )
		where = -1;

	return ret;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
