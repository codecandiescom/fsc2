/*
  $Id$
*/


#include "hfs9000.h"


#define ON( f )           ( ( f )->is_inverted ? LOW : HIGH )
#define OFF( f )          ( ( f )->is_inverted ? HIGH : LOW )



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

	/* Set the channels to off state. */

	hfs9000_set_constant( f->channel->self, 0, hfs9000.mem_size, OFF( f ) );

	/* Now simply run through all active pulses of the channel */

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses && p->is_active;
		  p = f->pulses[ ++i ] )
	{
		/* Set the area of the pulse itself */

		start = p->pos + f->delay;
		end = p->pos + p->len + f->delay;

		if ( start != end )
			hfs9000_set_constant( p->channel->self, start, end - start,
								 ON( f ) );
	}

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses; p = f->pulses[ ++i ] )
		p->was_active = p->is_active;
}
