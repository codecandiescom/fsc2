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


/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. an integer multiple of the time base                       */
/*-----------------------------------------------------------------*/

Ticks rs690_double2ticks( double p_time )
{
	double ticks;


	if ( ! rs690.is_timebase )
	{
		print( FATAL, "Can't set a time because no pulser time base has been "
			   "set.\n" );
		THROW( EXCEPTION );
	}

	ticks = p_time / rs690.timebase;

	if ( ticks > TICKS_MAX || ticks < TICKS_MIN )
	{
		print( FATAL, "Specified time is too long for time base of %s.\n",
			   rs690_ptime( rs690.timebase ) );
		THROW( EXCEPTION );
	}

	if ( fabs( Ticksrnd( ticks ) - p_time / rs690.timebase ) > 1.0e-2 ||
		 ( p_time > 0.99e-9 && Ticksrnd( ticks ) == 0 ) )
	{
		char *t = T_strdup( rs690_ptime( p_time ) );
		print( FATAL, "Specified time of %s is not an integer multiple of the "
			   "fixed pulser the time base of %s.\n",
			   t, rs690_ptime( rs690.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return Ticksrnd( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

double rs690_ticks2double( Ticks ticks )
{
	if ( ! rs690.is_timebase )
	{
		print( FATAL, "Can't set a time because no pulser time base has been "
			   "set.\n" );
		THROW( EXCEPTION );
	}

	return rs690.timebase * ticks;
}


/*-----------------------------------------------*/
/* Returns the structure for pulse numbered pnum */
/*-----------------------------------------------*/

PULSE *rs690_get_pulse( long pnum )
{
	PULSE *cp = rs690_Pulses;


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
		print( FATAL, "Referenced pulse #%ld does not exist.\n", pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *rs690_ptime( double p_time )
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

const char *rs690_pticks( Ticks ticks )
{
	return rs690_ptime( rs690_ticks2double( ticks ) );
}


/*-------------------------------------------------------------------*/
/* Comparison function for two pulses: returns 0 if both pulses are  */
/* inactive, -1 if only the second pulse is inactive or starts at a  */
/* later time and 1 if only the first pulse is inactive pulse or the */
/* second pulse starts earlier.                                      */
/*-------------------------------------------------------------------*/

int rs690_pulse_compare( const void *A, const void *B )
{
	PULSE_PARAMS *a = ( PULSE_PARAMS * ) A,
		         *b = ( PULSE_PARAMS * ) B;

	return a->pos <= b->pos ? -1 : 1;
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

void rs690_dump_channels( FILE *fp )
{
	FUNCTION *f;
	CHANNEL *ch;
	int i, j, k;


	if ( fp == NULL )
		return;

	fprintf( fp, "===\n" );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;

		if ( ! f->is_needed && f->num_channels == 0 )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			if ( ! ch->needs_update )
				continue;

			fprintf( fp, "%s:%s", f->name, rs690_num_2_channel( ch->self ) );
			for ( k = 0; k < ch->num_active_pulses; k++ )
				if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
					 ch->pulse_params[ k ].pulse->sp != NULL )
					fprintf( fp, " (%ld) %ld %ld",
							 ch->pulse_params[ k ].pulse->sp->num,
							 ch->pulse_params[ k ].pos,
							 ch->pulse_params[ k ].len );
				else if ( f->self == PULSER_CHANNEL_TWT &&
						  ch->pulse_params[ k ].pulse->tp != NULL )
					fprintf( fp, " (%ld) %ld %ld",
							 ch->pulse_params[ k ].pulse->tp->num,
							 ch->pulse_params[ k ].pos,
							 ch->pulse_params[ k ].len );
				else
					fprintf( fp, " %ld %ld %ld",
							 ch->pulse_params[ k ].pulse->num,
							 ch->pulse_params[ k ].pos,
							 ch->pulse_params[ k ].len );
			fprintf( fp, "\n" );
		}
	}
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

void rs690_calc_max_length( FUNCTION *f )
{
	int i, j;
	CHANNEL *ch;
	Ticks max_len = 0;


	if ( ! f->is_needed || f->num_channels == 0 )
		return;

	for ( j = 0; j < f->num_channels; j++ )
	{
		ch = f->channel[ j ];

		for ( i = 0; i < ch->num_active_pulses; i++ )
			max_len += ch->pulse_params[ i ].len;
	}

	f->max_len = l_max( f->max_len, max_len );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

char *rs690_num_2_channel( int num )
{
	static char name[ 4 ];


	sprintf( name, "%c%d", num / 16 + 'A', num % 16 );
	return name;
}

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
