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
	int i;
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

	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];
		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		dg2020_commit( f, flag );
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
		dg2020_commit_phases( f, flag );
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


/*----------------------------------------------------------------------------
  Function creates all active pulses in the channels of the pulser assigned
  to the function passed as argument.
----------------------------------------------------------------------------*/


void dg2020_set_pulses( FUNCTION *f )
{
	PULSE *p,
		  *pp;
	Ticks start, end;
	int i;


	/* As usual we need a special treatment of phase pulses... */

	if ( f->self == PULSER_CHANNEL_PHASE_1 ||
		 f->self == PULSER_CHANNEL_PHASE_2 )
	{
		dg2020_set_phase_pulses( f );
		return;
	}

	/* Always set the very first bit to LOW state, see the rant about the bugs 
	   in the pulser firmware at the start of dg2020_gpib.c... */

	dg2020_set_constant( p->channel->self, 0, 1, LOW );

	/* Now simply run through all active pulses of the channel */

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses && p->is_active;
		  p = f->pulses[ ++i ] )
	{
		pp = i == 0 ? NULL : f->pulses[ i - 1 ];

		/* Set the area from the end of the previous pulse up to the start
		   of the current pulse (exception: for the first pulse we start
		   at the first possible position */

		start = pp == NULL ? 1 : pp->pos + pp->len + f->delay + 1;
		end = p->pos + f->delay + 1;
		dg2020_set_constant( p->channel->self, start, end - start,
							 f->is_inverted ? HIGH : LOW );

		/* Set the area of the pulse itself */

		start = end;
		end = p->pos + p->len + f->delay + 1;
		dg2020_set_constant( p->channel->self, start, end - start,
							 f->is_inverted ? LOW : HIGH );
		pp = p;
		p = p->next;
	}

	/* Finally set the area following the last active pulse to the end
	   of the sequence */

	start = end;
	end = dg2020.max_seq_len + f->delay + 1;
	dg2020_set_constant( pp->channel->self, start, end - start,
						 f->is_inverted ? HIGH : LOW );
}


/*----------------------------------------------------------------------------
  Function creates the active pulses in the diverse channels assigned to a
  phase function. Here the problem is that the pulses are not well sorted
  in the pulse list of the function and belong to different channels. To
  make live simple we create a dummy function that gets 'assigned' a sorted
  list of the pulses belonging to one of the phase functions channels and
  can than call dg2020_set_pulses() on this dummy function to do all the
  real work.
----------------------------------------------------------------------------*/

void dg2020_set_phase_pulses( FUNCTION *f )
{
	FUNCTION dummy_f;
	int i;


	dummy_f.self = PULSER_CHANNEL_NO_TYPE;
	dummy_f.is_inverted = f->is_inverted;
	dummy_f.delay = f->delay;

	for ( i = 0; i < f->num_channels; i++ )
	{
		dummy_f.pulses = NULL;
		dummy_f.num_pulses = dg2020_get_phase_pulse_list( f, f->channel[ i ],
														  &dummy_f.pulses );
		dg2020_set_pulses( &dummy_f );
		T_free( dummy_f.pulses );
	}
}


/*----------------------------------------------------------------------------
  Function does all the needed changes to the pulses in the channels of a
  function.
----------------------------------------------------------------------------*/

void dg2020_commit( FUNCTION * f, bool flag )
{
	PULSE *p;
	int i;
	Ticks amount;


	/* As so often the phase functions need some special treatment */

	if ( f->self == PULSER_CHANNEL_PHASE_1 ||
		 f->self == PULSER_CHANNEL_PHASE_2 )
	{
		dg2020_commit_phases( f, flag );
		return;
	}

	/* In a test run just reset the flags of all pulses that say that the
	   pulse needs updating */

	if ( flag )
	{
		for ( i = 0; i < f->num_pulses; i++ )
		{
			f->pulses[ i ]->needs_update = UNSET;
			f->pulses[ i ]->is_old_pos = UNSET;
			f->pulses[ i ]->is_old_len = UNSET;
		}

		return;
	}

	/* In a real run we have to change the pulses. We run twice trough all
	   the pulses: In the first run we delete all pulses that have become
	   inactive, while in the second we change or set the active ones */

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];
		if ( ! p->needs_update || p->is_active )
			continue;

		assert( p->is_old_pos && p->is_old_len );
		dg2020_set_constant( p->channel->self , p->old_pos + f->delay + 1,
							 p->old_len, f->is_inverted ? HIGH : LOW );
		p->needs_update = p->old_pos = p->old_len = UNSET;
	}

	for ( i = 0; i < f->num_pulses; i++ )
	{
		if ( ! p->needs_update )
			continue;
		if ( ! p->is_active )
			break;

		assert( p->pos != p->old_pos );
		assert( p->len != p->old_len );

		if ( p->is_old_pos )          /* position did change */
		{
			if ( p->is_old_len )      /* both position and length did change */
			{
				if ( p->pos + p->len <= p->old_pos ||
					 p->pos >= p->old_pos + p->old_len )
				{
					dg2020_set_constant( p->channel->self,
										 p->pos + f->delay + 1, p->len,
										 f->is_inverted ? LOW : HIGH );
					dg2020_set_constant( p->channel->self,
										 p->old_pos + f->delay + 1, p->old_len,
										 f->is_inverted ? HIGH : LOW );
				}
				else 
				{
					if ( p->pos < p->old_pos )
						dg2020_set_constant( p->channel->self,
											 p->pos + f->delay + 1,
											 p->old_pos - p->pos,
											 f->is_inverted ? LOW : HIGH );
					else
						dg2020_set_constant( p->channel->self,
											 p->old_pos + f->delay + 1,
											 p->pos - p->old_pos,
											 f->is_inverted ? HIGH : LOW );

					if ( p->pos + p->len > p->old_pos + p->old_len )
						dg2020_set_constant( p->channel->self,
											 p->old_pos + p->old_len
											 + f->delay + 1,
											 p->pos + p->len
											 - p->old_pos - p->old_len,
											 f->is_inverted ? LOW : HIGH );
					else if ( p->pos + p->len < p->old_pos + p->old_len )
						dg2020_set_constant( p->channel->self,
											 p->pos + p->len + f->delay + 1,
											 p->old_pos + p->old_len
											 - p->pos - p->len,
											 f->is_inverted ? HIGH : LOW );
				}
			}
			else                      /* length didn't change */
			{
				if ( p->pos + p->len < p->old_pos ||
					 p->pos > p->old_pos + p->len )
				{
					amount = p->len;

					dg2020_set_constant( p->channel->self,
										 p->old_pos + f->delay + 1, amount,
										 f->is_inverted ? HIGH : LOW );
					dg2020_set_constant( p->channel->self,
										 p->pos + f->delay + 1, amount,
										 f->is_inverted ? LOW : HIGH );
				}
				else if ( p->pos < p->old_pos )
				{
					amount = p->old_pos - p->pos;

					dg2020_set_constant( p->channel->self,
										 p->pos + f->delay + 1, amount,
										 f->is_inverted ? LOW : HIGH );
					dg2020_set_constant( p->channel->self,
										 p->pos + p->len + f->delay + 1,
										 amount,
										 f->is_inverted ? HIGH : LOW );
				}
				else if ( p->pos < p->old_pos + p->len )
				{
					amount = p->pos - p->old_pos;

					dg2020_set_constant( p->channel->self,
										 p->old_pos + f->delay + 1, amount,
										 f->is_inverted ? HIGH : LOW );
					dg2020_set_constant( p->channel->self,
										 p->old_pos + p->len + f->delay + 1,
										 amount, f->is_inverted ? LOW : HIGH );
				}
			}
		}
		else                          /* only length changed */
		{
			if ( p->old_len < p->len )    /* pulse became longer */
				dg2020_set_constant( p->channel->self,
									 p->old_pos + p->old_len + f->delay + 1,
									 p->len - p->old_len,
									 f->is_inverted ? LOW : HIGH );
			else                  /* pulse became shorter */
				dg2020_set_constant( p->channel->self,
									 p->pos + p->len + f->delay + 1,
									 p->old_len - p->len,
									 f->is_inverted ? HIGH  : LOW);
		}

		p->needs_update = p->is_old_pos = p->is_old_len = UNSET;
	}
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

void dg2020_commit_phases( FUNCTION * f, bool flag )
{
	FUNCTION dummy_f;
	int i;

	dummy_f.self = PULSER_CHANNEL_NO_TYPE;
	dummy_f.is_inverted = f->is_inverted;
	dummy_f.delay = f->delay;

	for ( i = 0; i < f->num_channels; i++ )
	{
		dummy_f.pulses = NULL;
		dummy_f.num_pulses = dg2020_get_phase_pulse_list( f, f->channel[ i ],
														  &dummy_f.pulses );
		dg2020_commit( &dummy_f, flag );
		T_free( dummy_f.pulses );
	}
}
