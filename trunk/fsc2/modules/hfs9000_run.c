/*
  $Id$
*/


#include "hfs9000.h"


static PULSE *hfs9000_delete_pulse( PULSE *p );


/*------------------------------------------------------------------------
  Function is called after pulses have been changed to check if the new
  settings are still ok, i.e. that they still fit into the pulsers memory
  and don't overlap.
--------------------------------------------------------------------------*/

void hfs9000_do_checks( FUNCTION *f )
{
	PULSE *p;
	int i;

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		if ( p->is_active )
		{
			f->max_seq_len = Ticks_max( f->max_seq_len, p->pos + p->len );
			if ( f->delay + f->max_seq_len > MAX_PULSER_BITS )
			{
				eprint( FATAL, "%s:%ld: %s: Pulse sequence for function "
						"`%s' does not fit into the pulsers memory. Maybe, "
						"you could try a longer pulser time base.\n", Fname,
						Lc, pulser_struct.name, Function_Names[ f->self ] );
				THROW( EXCEPTION );
			}

			f->num_active_pulses = i + 1;
		}

		if ( i + 1 < f->num_pulses && f->pulses[ i + 1 ]->is_active &&
			 p->pos + p->len > f->pulses[ i + 1 ]->pos )
		{
			if ( hfs9000_IN_SETUP )
				eprint( FATAL, "%s: Pulses %ld and %ld overlap.\n",
						pulser_struct.name, p->num, f->pulses[ i + 1 ]->num );
			else
				eprint( FATAL, "%s:%ld: %s: Pulses %ld and %ld begin to "
						"overlap.\n", Fname, Lc, pulser_struct.name, p->num,
						f->pulses[ i + 1 ]->num );
			THROW( EXCEPTION );
		}
	}
}


/*----------------------------------------------------------------------------
  Function creates all active pulses in the channels of the pulser assigned
  to the function passed as argument.
----------------------------------------------------------------------------*/

void hfs9000_set_pulses( FUNCTION *f )
{
	PULSE *p;
	Ticks start, end;
	int i;


	assert( f->self != PULSER_CHANNEL_PHASE_1 &&
			f->self != PULSER_CHANNEL_PHASE_2 );

	if ( f->channel->self == HFS9000_TRIG_OUT )
	{
		if ( f->pulses != NULL )
		{
			hfs9000_set_trig_out_pulse( );
			f->pulses[ 0 ]->was_active = f->pulses[ 0 ]->is_active;
		}
		return;
	}

	/* Set the channels to zero. */

	hfs9000_set_constant( f->channel->self, 0, hfs9000.mem_size, 0 );

	/* Now simply run through all active pulses of the channel */

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses && p->is_active;
		  p = f->pulses[ ++i ] )
	{
		/* Set the area of the pulse itself */

		start = p->pos + f->delay;
		end = p->pos + p->len + f->delay;

		if ( start != end )
			hfs9000_set_constant( p->channel->self, start, end - start, 1 );
	}

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses; p = f->pulses[ ++i ] )
		p->was_active = p->is_active;
}


/*----------------------------------------------------------------------------
  Function is called after the test run to reset all the variables describing
  the state of the pulser to their initial values
----------------------------------------------------------------------------*/

void hfs9000_full_reset( void )
{
	PULSE *p = hfs9000_Pulses;


	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't */

		if ( ! p->has_been_active )
		{
			if ( p->num >=0 )
				eprint( WARN, "%s: Pulse %ld is never used.\n",
						pulser_struct.name, p->num );
			p = hfs9000_delete_pulse( p );
			continue;
		}

		/* Reset pulse properies to their initial values */

		p->pos = p->initial_pos;
		p->is_pos = p->initial_is_pos;
		p->len = p->initial_len;
		p->is_len = p->initial_is_len;
		p->dpos = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;
		p->dlen = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->needs_update = UNSET;

		p->is_old_pos = p->is_old_len = UNSET;

		p->is_active = ( p->is_pos && p->is_len &&
						 ( p->len > 0 || p->len == -1 ) );

		p = p->next;
	}
}


/*----------------------------------------------------------------------------
  Function deletes a pulse and returns a pointer to the next pulse in the
  pulse list.
----------------------------------------------------------------------------*/

static PULSE *hfs9000_delete_pulse( PULSE *p )
{
	PULSE *pp;
	int i;


	/* First we've got to remove the pulse from its functions pulse list */

	for ( i = 0; i < p->function->num_pulses; i++ )
		if ( p->function->pulses[ i ] == p )
			break;

	assert( i < p->function->num_pulses );  /* Paranoia */

	/* Put the last of the functions pulses into the slot for the pulse to
	   be deleted and shorten the list by one element */

	if ( i != p->function->num_pulses - 1 )
		p->function->pulses[ i ] = 
			p->function->pulses[ p->function->num_pulses - 1 ];
	p->function->pulses = T_realloc( p->function->pulses,
									--p->function->num_pulses
									* sizeof( PULSE * ) );

	/* If the deleted pulse was the last pulse of its function send a warning
	   and mark the function as useless */

	if ( p->function->num_pulses == 0 )
	{
		eprint( SEVERE, "%s: Function `%s' isn't used at all because all its "
				"pulses are never used.\n", pulser_struct.name,
				Function_Names[ p->function->self ] );
		p->function->is_used = UNSET;
	}

	/* Now remove the pulse from the real pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;
	T_free( p->channel );
	T_free( p );

	return pp;
}
