/*
  $Id$
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
		eprint( FATAL, "%s:%ld: %s: Can't set a time because no pulser time "
				"base has been set.", Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	ticks = time / hfs9000.timebase;

	if ( fabs( ticks - lround( ticks ) ) > 1.0e-2 )
	{
		char *t = get_string_copy( hfs9000_ptime( time ) );
		eprint( FATAL, "%s:%ld: %s: Specified time of %s is not an integer "
				"multiple of the pulser time base of %s.", Fname, Lc,
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
	assert( hfs9000.is_timebase );
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
		eprint( FATAL, "%s:%ld: %s: Low voltage level is above high level, "
				"use keyword INVERT to invert the polarity.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING + 0.1 * VOLTAGE_RESOLUTION )
	{
		eprint( FATAL, "%s:%ld: %s: Difference between high and low "
				"voltage of %g V is too big, maximum is %g V.", Fname, Lc,
				pulser_struct.name, high - low, MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING - 0.1 * VOLTAGE_RESOLUTION )
	{
		eprint( FATAL, "%s:%ld: %s: Difference between high and low "
				"voltage of %g V is too small, minimum is %g V.", Fname, Lc,
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
		eprint( FATAL, "%s:%ld: %s: Invalid pulse number: %ld.",
				Fname, Lc, pulser_struct.name, pnum );
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
		eprint( FATAL, "%s:%ld: %s: Referenced pulse %ld does not exist.",
				Fname, Lc, pulser_struct.name, pnum );
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
