#include "fsc2.h"


/* Creates a new pulse and appends it to the pulse list. */

Pulse *pulse_new( long num )
{
	Pulse *p;


	/* check that the pulse number is in the allowed range */

	if (num < 0 ||  num >= MAX_PULSE_NUM )
	{
		eprint( "%s:%ld: Pulse number %ld out of range (0-%d).\n",
				Fname, Lc, num, MAX_PULSE_NUM - 1 );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* check that the pulse does not already exists */

	if ( pulse_find_pulse( ( int ) num ) != NULL )
	{
		eprint( "%s:%ld: Pulse with number %ld already exists.\n",
				Fname, Lc, num );
		THROW( PREPARATIONS_EXCEPTION );
	}

	/* allocate memory for the new pulse, set its number  and link it
	   to the top of the pulse list */

	p = ( Pulse * ) T_malloc( sizeof( Pulse ) );

	p->num = ( int ) num;

	p->prev = NULL;
	p->next = Plist;

	if ( Plist != NULL )
		Plist->prev = p;

	Plist = p;

	return( p );
}



Pulse *pulse_find_pulse( int num )
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
