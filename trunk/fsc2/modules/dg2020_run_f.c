/*
  $Id$
*/



#include "dg2020.h"



/*---------------------------------------------------------------------------
  Function is called in the experiment after pulses have been changed to
  update the pulser accordingly. No checking has to be done except in the
  test run.
----------------------------------------------------------------------------*/

void dg2020_do_update( void )
{
	/* Resort the pulses and, while in a test run, we also have to check that
	   the new pulse settings are reasonable */

	dg2020_reorganize_pulses( TEST_RUN );


	{
		PULSE *p = dg2020_Pulses;

		while ( p != NULL )
		{
			if ( p->is_active )
			{
				printf( "%4ld: %6ld %6ld", p->num, p->pos, p->len );
				if ( p->num < 0 )
					printf( " -> %4ld", p->for_pulse->num );
				printf( "\n" );
			}
			p = p->next;
		}
		printf( "\n" );
	}

	/* Finally commit all changes */

	if ( ! TEST_RUN )
		dg2020_update_data( );
}


/*---------------------------------------------------------------------------
  This function sorts the pulses, and if called with 'flag' set, also checks
  that the pulses don't overlap.
---------------------------------------------------------------------------*/

void dg2020_reorganize_pulses( bool flag )
{
	int i, j;
	FUNCTION *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ),
			   dg2020_start_compare );

		if ( flag )
			dg2020_do_checks( f );

		if ( f->needs_phases )
			dg2020_reorganize_phases( f->phase_func, flag );

		/* In a test run reset the flags of all pulses that say that the
		   pulse needs updating. But keep it in real runs, because it is
		   used when the changes are finally send to the pulser */

		if ( flag )
			for ( j = 0; j < f->num_pulses; j++ )
			{
				f->pulses[ j ]->needs_update = UNSET;
				f->pulses[ j ]->is_old_pos = UNSET;
				f->pulses[ j ]->is_old_len = UNSET;
			}
	}

	if ( flag )            // all is done for a test run
		return;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];
		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		dg2020_commit( f );
	}
}


/*---------------------------------------------------------------------------
  Function can be called after pulses have been changed to check if the new
  settings are still ok.
----------------------------------------------------------------------------*/

void dg2020_do_checks( FUNCTION *f )
{
	PULSE *p;
	int i;


	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		if ( i + 1 == f->num_pulses || ! f->pulses[ i + 1 ]->is_active )
		{
			f->max_seq_len = Ticks_max( f->max_seq_len, p->pos + p->len );
			if ( f->delay + f->max_seq_len >= MAX_PULSER_BITS )
			{
				eprint( FATAL, "%s:%ld: Pulse sequence for function `%s' does "
						"not fit into the pulsers memory. Maybe, you could "
						"try a longer pulser time base.\n", Fname, Lc,
						Function_Names[ f->self ] );
				THROW( EXCEPTION );
			}

			f->num_active_pulses = i + 1;
			break;
		}

		if ( p->pos + p->len > f->pulses[ i + 1 ]->pos )
		{
			if ( ! TEST_RUN )
				eprint( FATAL, "DG2020: Pulses %ld and %ld overlap.\n",
						p->num, f->pulses[ i + 1 ]->num );
			else
				eprint( FATAL, "%s:%ld: DG2020: Pulses %ld and %ld begin to "
						"overlap.\n", Fname, Lc, p->num,
						f->pulses[ i + 1 ]->num );
			THROW( EXCEPTION );
		}
	}
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

void dg2020_reorganize_phases( FUNCTION *f, bool flag )
{
	int i, j;
	PULSE *p;
	bool need_update = UNSET;


	assert( ( f->self == PULSER_CHANNEL_PHASE_1 ||
			  f->self == PULSER_CHANNEL_PHASE_2 ) &&
			f->is_used );

	/* First check which of the phase pulses will become active or inactive */

	for ( i = 0; ! need_update && i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];
		if ( ( p->is_active && ! p->for_pulse->is_active ) ||
			 ( ! p->is_active && p->for_pulse->is_active ) ||
			 ( p->for_pulse->is_old_pos || p->for_pulse->is_old_len ) )
			need_update = SET;
	}

	/* If nothing changed at all we're already finished... */

	if ( ! need_update )
		return;

	/* Now run through the phase pulses associated with each of the functions
	   pulse */

	for ( i = 0; i < f->phase_func->num_pulses; i++ )
	{
		p = f->phase_func->pulses[ i ];

		for ( j = 0; j < f->num_pulses; j++ )
			if ( f->pulses[ j ]->for_pulse == p )
				dg2020_recalc_phase_pulse( f, f->pulses[ j ], p, i, flag );
	}

	if ( ! flag )
		dg2020_commit( f );
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

void dg2020_recalc_phase_pulse( FUNCTION *f, PULSE *phase_p,
								PULSE *p, int nth, bool flag )
{
	PULSE **pppl;                 // list of phase pulses for previous pulse
	int ppp_num;                  // and the length of this list
	PULSE *pp, *pn;
	int i;
	static PULSE *for_pulse;


	/* If the pulse the phase pulse is associated with has become inactive
	   also the phase pulse becomes inactive and has to be removed */

	if ( ! p->is_active && phase_p->is_active )
	{
		phase_p->is_active = UNSET;
		if ( ! phase_p->is_old_pos )
		{
			phase_p->old_pos = phase_p->pos;
			phase_p->is_old_pos = SET;
		}
		if ( ! phase_p->is_old_len )
		{
			phase_p->old_len = phase_p->len;
			phase_p->is_old_len = SET;
		}

		phase_p->is_pos = phase_p->is_len = UNSET;
		phase_p->len = 0;
		phase_p->is_active = UNSET;
		phase_p->needs_update = SET;
		return;
	}
			
	/* If neither the phase pulse nor the pulse its associated with is active
	   nothing has to be done */

	if ( ! p->is_active && ! phase_p->is_active )
		return;

	/* All remaining phase pulses and the pulses they are associated with got
       to be active */

	phase_p->is_active = SET;

	/* Now lets start to check the positions */

	if ( nth == 0 )                 /* for the first phase pulse */
	{
		/* If the position isn't already set and is set to the first
		   possible position we've got to set it otherwise its already
		   at the place it belongs */

		if ( ! phase_p->is_pos || phase_p->pos != - f->delay )
		{
			if ( p->pos - f->delay < f->psd )
			{
				eprint( FATAL, "%s:%ld: DG2020: Pulse %ld now starts too "
						"early to allow setting of a phase pulse.\n",
						Fname, Lc, p->num );
				THROW( EXCEPTION );
			}

			/* Store the old position if there's one and hasn't been set yet */

			if ( phase_p->is_pos && ! phase_p->is_old_pos )
			{
				phase_p->old_pos = phase_p->pos;
				phase_p->is_old_pos = SET;
			}

			phase_p->pos = - f->delay;
			phase_p->is_pos = SET;
			phase_p->needs_update = SET;
		}
	}
	else                                 /* if its not the first pulse */
	{
		/* Find the position of the preceeding pulse */
		
		pp = p->function->pulses[ nth - 1 ];

		/* If the phase switch delay does not fit between the pulse it gets
		   associated with and its predecessor we have to complain (except
		   when both pulses use the same phase sequence or none at all) */

		if ( p->pos - ( pp->pos + pp->len ) < f->psd && p->pc != pp->pc )
		{
			eprint( FATAL, "%s:%ld: DG2020: Distance between pulses %ld and "
					"%ld becomes too small to allow setting of phase "
					"pulses.\n", Fname, Lc, p->num, pp->num );
			THROW( EXCEPTION );
		}

		/* If the phase pulse has a position and it hasn't been stored yet
           store it now */

		if ( phase_p->is_pos && ! phase_p->is_old_pos )
		{
			phase_p->old_pos = phase_p->pos;
			phase_p->is_old_pos = SET;
		}

		/* If the phase pulse had no position yet or it got dangerously near
		   to the previous pulse set it to the point in the middle between the
		   pulse it belongs to and its predecessor */

		if ( ! phase_p->is_pos ||
			 phase_p->pos < pp->pos + pp->len + dg2020.grace_period )
			phase_p->pos = ( p->pos + pp->pos + pp->len ) / 2;

		/* Now all phase pulses have a position and we check if this position
		   is ok. Ok means on the one hand that the phase pulse must start at
		   least that much earlier than the pulse it belongs to that the phase
		   switch delay is still maintained. Otherwise we have to make the
		   phase pulse at least that much earlier. But that also means that we
		   have to shorten all the phase pulses associated with the previous
		   pulse (if there are any). Again, there's the exception for the case
		   that the pulse and its predecessor use the same phase sequence and
		   thus both their phase pulses get merged into one pulse anyway. */
		   
		if ( p->pos - phase_p->pos < f->psd && p->pc != pp->pc )
			phase_p->pos = p->pos - f->psd;

		if ( dg2020_find_phase_pulse( pp, &pppl, &ppp_num ) )
		{
			for ( i = 0; i < ppp_num; i++ )
			{
				if ( pppl[ i ]->is_len &&
					 pppl[ i ]->pos + pppl[ i ]->len == phase_p->pos )
					continue;

				if ( ! pppl[ i ]->is_old_len )
				{
					pppl[ i ]->old_len = pppl[ i ]->len;
					pppl[ i ]->is_old_len = SET;
				}
				pppl[ i ]->len = phase_p->pos - pppl[ i ]->pos;
				pppl[ i ]->needs_update = SET;
			}

			T_free( pppl );
		}

		/* Now we can be sure that we have a legal position for the phase
		   pulse - if got a new position it has to be updated */

		if ( ! phase_p->is_pos ||
			 ( phase_p->is_pos && phase_p->pos != phase_p->old_pos ) )
		{
			phase_p->is_pos = SET;
			phase_p->needs_update = SET;
		}

		/* Finally, if we find in the test run that the phase pulse gets too
		   near to the end of the previous (real) pulse, we utter a warning */

		if ( flag && p != for_pulse && p->pc != pp->pc &&
			 phase_p->pos - pp->pos - pp->len < dg2020.grace_period )
			eprint( SEVERE, "%s:%ld: DG2020: Pulses %ld and %ld become so "
					"close that problems with phase switching may result.\n",
					Fname, Lc, pp->num, p->num );
	}


	for_pulse = p;

	/* Now that we're done with setting the position we can continue with
	   setting the phase pulses length. We don't have to do anything if the
	   phase pulse has a length set and the lenght exceeds the pulse it
	   belongs to by the grace period */

	if ( phase_p->is_len &&
		 phase_p->pos + phase_p->len > p->pos + p->len + dg2020.grace_period )
		return;

	/* Store the old length of the phase pulse (if there's one) */

	if ( phase_p->is_len )
	{
		phase_p->old_len = phase_p->len;
		phase_p->is_old_len = SET;
	}

	if ( nth == p->function->num_active_pulses - 1 ) // last active pulse ?
	{
		if ( flag )                // in test run
			phase_p->len = -1;
		else
			/* Take care: the variable `dg2020.max_seq_len' already
			   includes the delay for the function... */

			phase_p->len = dg2020.max_seq_len - f->delay - phase_p->pos;
	}
	else
	{
		pn = p->function->pulses[ nth + 1 ];
		phase_p->len = ( p->pos + p->len + pn->pos ) / 2 - phase_p->pos;
	}

	if ( ! phase_p->is_len ||
		 ( phase_p->len != phase_p->old_len ) )
		 phase_p->needs_update = SET;

	phase_p->is_len = SET;
}


/*----------------------------------------------------------------------------
  Function is called after the test run to reset all the variables describing
  the state of the pulser to their initial values
----------------------------------------------------------------------------*/

void dg2020_full_reset( void )
{
	PULSE *p = dg2020_Pulses;


	/* First we reset all the pulses */

	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't */

		if ( ! p->is_active && ! p->has_been_active )
		{
			eprint( WARN, "DG2020: Pulse %ld is never used.\n", p->num );
			p = dg2020_delete_pulse( p );
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

		p->is_old_pos = p->is_old_len = UNSET;

		p->is_active = ( p->is_pos && p->is_len && p->len > 0 );

		p = p->next;
	}

	/* Now we resort all the functions pulse lists (no checking needed) */

	dg2020_reorganize_pulses( TEST_RUN );
}


/*----------------------------------------------------------------------------
  Function deletes a pulse and returns a pointer to the next pulse in the
  pulse list.
----------------------------------------------------------------------------*/

PULSE *dg2020_delete_pulse( PULSE *p )
{
	PULSE *pp;
	int i;


	/* If the pulse belongs to a function that needs phase cycling we try to
	   find the phase pulses and delete them first */

	if ( p->num >= 0 && p->function->needs_phases )
	{
		pp = dg2020_Pulses;
		while ( pp != NULL )
			if ( pp->num < 0 && pp->for_pulse == p )
				pp = dg2020_delete_pulse( pp );
			else
				pp = pp->next;
	}

	/* Now delete the pulse. First we've got to remove it from its functions
       pulse list */

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
		eprint( SEVERE, "DG2020: Function `%s' isn't used at all because all "
				"its pulses are never used.\n",
				Function_Names[ p->function->self ] );
		p->function->is_used = UNSET;
	}

	/* Now remove the pulse from the real pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;
	T_free( p );

	return pp;
}


/*----------------------------------------------------------------------------
  After the test run, when all things have been checkes and the maximum length
  of the pulse sequence is known, we can finally calculate the lengths of the
  last phase pulses of a phase function - they simply last to the end of the
  pulse sequence.
----------------------------------------------------------------------------*/

void dg2020_finalize_phase_pulses( int func )
{
	FUNCTION *f;
	int i;
	PULSE *p;


	assert( func == PULSER_CHANNEL_PHASE_1 || func == PULSER_CHANNEL_PHASE_2 );

	f = &dg2020.function[ func ];
	if ( ! f->is_used )
		return;

	/* Find the last active pahse pulses and set their length to the maximum
	   posible amount, i.e. to the maximum sequence length - take care,
	   the variable `dg2020.max_seq_len' already includes the delay for
	   the function... */


	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];
		if ( p->is_active && p->is_pos && p->is_len && p->len < 0 )
			p->len = dg2020.max_seq_len - f->delay - p->pos;
	}
}


void dg2020_commit( FUNCTION * f )
{
}
