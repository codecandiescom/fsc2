#include "fsc2.h"


/* Creates a new pulse and appends it to the pulse list. */

Pulse *pulse_new( int num )
{
	Pulse *p;


	/* check that the pulse number is in the allowed range */

	if ( num < 0 || num >= MAX_PULSE_NUM )
	{
		eprint( FATAL, "%s:%ld: Pulse number %d out of range (0-%d).\n",
				Fname, Lc, num, MAX_PULSE_NUM - 1 );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* check that the pulse does not already exists */

	if ( pulse_find( num ) != NULL )
	{
		eprint( FATAL, "%s:%ld: Pulse with number %d already exists.\n",
				Fname, Lc, num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* allocate memory for the new pulse, set its number and link it
	   to the top of the pulse list */

	p = ( Pulse * ) T_malloc( sizeof( Pulse ) );

	p->prev = NULL;
	p->next = Plist;

	if ( Plist != NULL )
		Plist->prev = p;

	p->num = num;
	p->func = PULSER_CHANNEL_NO_TYPE;

	Plist = p;

	return( p );
}



Pulse *pulse_find( int num )
{
	Pulse *p = Plist;

	while ( p != NULL )
	{
		if ( p->num == num )
			return( p );
		p = p->next;
	}

	return( NULL );
}


void pulse_set_func( Pulse *p, long func )
{
	if ( p->func != PULSER_CHANNEL_NO_TYPE )
	{
		eprint( FATAL, "%s:%ld: Function of pulse %d is already set.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	if ( func < PULSER_CHANNEL_MW || func > PULSER_CHANNEL_RF_GATE )
	{
		eprint( FATAL, "%s:%ld: Invalid function type for pulse %d.\n",
				Fname, Lc, p->num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	p->func = func;
}
