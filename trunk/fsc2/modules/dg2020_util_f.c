/*
  $Id$
*/


#include "dg2020.h"



/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. a integer multiple of the time base                        */
/*-----------------------------------------------------------------*/

Ticks double2ticks( double time )
{
	double ticks;


	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: DG2020: Can't set a time because no pulser "
				"time base has been set.\n",Fname, Lc );
		THROW( EXCEPTION );
	}

	ticks = time / dg2020.timebase;

	if ( fabs( ticks - lround( ticks ) ) > 1.0e-2 )
	{
		char *t = get_string_copy( ptime( time ) );
		eprint( FATAL, "%s:%ld: DG2020: Specified time of %s is not an "
				"integer multiple of the pulser time base of %s.\n",
				Fname, Lc, t, ptime( dg2020.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return ( Ticks ) lround( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

double ticks2double( Ticks ticks )
{
	assert( dg2020.is_timebase );
	return ( double ) ( dg2020.timebase * ticks );
}


/*----------------------------------------------------------------------*/
/* Checks if the difference of the levels specified for a pod connector */
/* are within the valid limits.                                         */
/*----------------------------------------------------------------------*/

void check_pod_level_diff( double high, double low )
{
	if ( low > high )
	{
		eprint( FATAL, "%s:%ld: DG2020: Low voltage level is above high "
				"level, instead use keyword INVERT to invert the polarity.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING + 0.01 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Difference between high and low "
				"voltage of %g V is too big, maximum is %g V.\n", Fname, Lc,
				high - low, MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING - 0.01 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Difference between high and low "
				"voltage of %g V is too small, minimum is %g V.\n", Fname, Lc,
				high - low, MIN_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

PULSE *get_pulse( long pnum )
{
	PULSE *cp = Pulses;


	while ( cp != NULL )
	{
		if ( cp->num == pnum )
			break;
		cp = cp->next;
	}

	if ( cp == NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Referenced pulse %ld does not "
				"exist.\n", Fname, Lc, pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *ptime( double time )
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

const char *pticks( Ticks ticks )
{
	return ptime( ticks2double( ticks ) );
}


/*-----------------------------------------------------------------------------
   Functions returns a pointer to a free channel - the channels with the
   highest numbers are used first since most users probably tend to use the
   low number channels for storing test pulse sequences that they don't like
   too much being overwritten just because they forgot to set a channel-to-
   function-assignment in their EDL program.
-----------------------------------------------------------------------------*/

CHANNEL *get_next_free_channel( void )
{
	int i = MAX_CHANNELS - 1;

	while ( dg2020.channel[ i ].function != NULL )
		i--;

	assert( i >= 0 );                 /* this can't happen ;-) */

	return &dg2020.channel[ i ];
}


/*---------------------------------------------------------------------------
  Comparison function for two pulses: returns 0 if both pulses are replace-
  ment pulses, -1 if only the second pulse is a replacement pulse or starts
  at a late time and 1 if only the first pulse is a replacement pulse or the
  second pulse starts earlier.
---------------------------------------------------------------------------*/

int init_compare( const void *A, const void *B )
{
	PULSE *a = *( PULSE ** ) A,
		  *b = *( PULSE ** ) B;

	if ( a->is_a_repl )
	{
		if ( b->is_a_repl )
			return 0;
		else
			return 1;
	}

	if ( b->is_a_repl || a->pos <= b->pos )
		return -1;

	return 1;
}
