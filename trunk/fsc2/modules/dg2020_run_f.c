/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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



#include "dg2020_f.h"



#define type_ON( f )           ( ( f )->is_inverted ? LOW : HIGH )
#define type_OFF( f )          ( ( f )->is_inverted ? HIGH : LOW )



/*-------------------------------------------------------------------------*/
/* Function is called in the experiment after pulses have been changed to  */
/* update the pulser accordingly. Before pulses are set the new values are */
/* checked. If the check fails in the test run the program aborts while in */
/* the real experiment an error message is printed and the pulses are      */
/* reset to their original positions.                                      */
/*-------------------------------------------------------------------------*/

bool dg2020_do_update( void )
{
	if ( ! dg2020_is_needed )
		return OK;

	/* Resort the pulses and, while in a test run, we also have to check that
	   the new pulse settings are reasonable */

	if ( ! dg2020_reorganize_pulses( TEST_RUN ) )
	{
		dg2020.needs_update = UNSET;
		return FAIL;
	}

	/* Finally commit all changes */

	if ( ! TEST_RUN && ! dg2020_update_data( ) )
	{
		eprint( FATAL, UNSET, "%s: Setting the pulser failed badly.\n",
				pulser_struct.name );
		THROW( EXCEPTION )
	}

	dg2020.needs_update = UNSET;
	return OK;
}


/*----------------------------------------------------------------*/
/* This function sorts the pulses, and if called with 'flag' set, */
/* also checks that the pulses don't overlap.                     */
/*----------------------------------------------------------------*/

bool dg2020_reorganize_pulses( bool flag )
{
	static int i;
	FUNCTION *f;
	PULSE *p;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		qsort( f->pulses, ( size_t ) f->num_pulses, sizeof( PULSE * ),
			   dg2020_start_compare );

		/* Check the pulse positions and lengths, if test fails in test run
		   the program is stopped, while in real run all pulses are reset to
		   their original position and length */

		TRY
		{

			dg2020_do_checks( f );

			/* Reorganize the phase pulses of the phase function associated
			   with the current function */

			if ( f->needs_phases )
				dg2020_reorganize_phases( f->phase_func, flag );

			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			if ( flag )
				THROW( EXCEPTION )

			for ( p = dg2020_Pulses; p != NULL; p = p->next )
			{
				if ( p->is_old_pos )
					p->pos = p->old_pos;
				if ( p->is_old_len )
					p->len = p->old_len;
				p->is_active = IS_ACTIVE( p );
				p->needs_update = UNSET;
			}

			return FAIL;
		}

		/* Send all the changes to the pulser */

		dg2020_commit( f, flag );
	}

	return OK;
}


/*----------------------------------------------------------------*/
/* Function can be called after pulses have been changed to check */
/* if the new settings are still ok.                              */
/*----------------------------------------------------------------*/

void dg2020_do_checks( FUNCTION *f )
{
	PULSE *p;
	int i;


	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		if ( p->is_active )
		{
			/* Check that pulses still fit into the pulser memory (while in
			   test run) or the maximum sequence length (in the real run) */

			f->max_seq_len = Ticks_max( f->max_seq_len, p->pos + p->len );
			if ( f->delay + f->max_seq_len >
				 		  ( TEST_RUN ? MAX_PULSER_BITS : dg2020.max_seq_len ) )
			{
				if ( TEST_RUN )
					eprint( FATAL, SET, "%s: Pulse sequence for function "
							"`%s' does not fit into the pulsers memory. "
							"Maybe, you could try a longer pulser time "
							"base.\n", pulser_struct.name,
							Function_Names[ f->self ] );
				else
					eprint( FATAL, SET, "%s: Pulse sequence for function "
							"`%s' is too long. Perhaps you should try the "
							"MAXIMUM_PATTERN_LENGTH command.\n",
							pulser_struct.name, Function_Names[ f->self ] );
				THROW( EXCEPTION )
			}

			f->num_active_pulses = i + 1;
		}

		if ( i + 1 < f->num_pulses && f->pulses[ i + 1 ]->is_active &&
			 p->pos + p->len > f->pulses[ i + 1 ]->pos )
		{
			if ( dg2020_IN_SETUP )
				eprint( TEST_RUN ? FATAL : SEVERE, UNSET,
						"%s: Pulses %ld and %ld overlap.\n",
						pulser_struct.name, p->num,
						f->pulses[ i + 1 ]->num );
			else
				eprint( FATAL, SET, "%s: Pulses %ld and %ld begin to "
						"overlap.\n", pulser_struct.name, p->num,
						f->pulses[ i + 1 ]->num );
			THROW( EXCEPTION )
		}
	}
}


/*---------------------------------------------------------------------------*/
/* The function readjusts the phase pulses of a phase function. An important */
/* aim is to keep the number of changes to the phase pulses at a minimum     */
/* because changing pulses takes quite some time and thus slows down the     */
/* experiment. Therefore the phase pulses are only adjusted if they come too */
/* near to the pulses in the neighborhood of the pulse they're associated    */
/* with.                                                                     */
/*---------------------------------------------------------------------------*/

void dg2020_reorganize_phases( FUNCTION *f, bool flag )
{
	int i, j;
	PULSE *p;
	bool need_update = UNSET;


	fsc2_assert( f->is_used && ( f->self == PULSER_CHANNEL_PHASE_1 ||
								 f->self == PULSER_CHANNEL_PHASE_2 ) );

	/* First check if any of the phase pulses needs to be updated */

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];
		if ( TEST_RUN && p->is_active )
			f->max_seq_len = Ticks_max( f->max_seq_len, p->pos + p->len );
		if ( p->for_pulse->needs_update )
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


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

void dg2020_recalc_phase_pulse( FUNCTION *f, PULSE *phase_p,
								PULSE *p, int nth, bool flag )
{
	PULSE **pppl;                 // list of phase pulses for previous pulse
	int ppp_num;                  // and the length of this list
	PULSE *pp, *pn;
	int i;
	static PULSE *for_pulse = NULL;


	/* If the pulse the phase pulse is associated with has become inactive
	   also the phase pulse becomes inactive and has to be removed */

	p->was_active = p->is_active;

	if ( ! p->is_active && phase_p->is_active )
	{
		phase_p->is_active = UNSET;

		phase_p->old_pos = phase_p->pos;
		phase_p->is_old_pos = SET;

		phase_p->old_len = phase_p->len;
		phase_p->is_old_len = SET;

		phase_p->is_len = UNSET;
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

	phase_p->has_been_active = phase_p->is_active = SET;

	/* Now lets start to check the positions */

	if ( nth == 0 )                 /* for the first phase pulse */
	{
		/* If the position is set and is already at the first possible moment
		   do nothing about it */

		if ( phase_p->is_pos && phase_p->pos == - f->delay )
			goto set_length;

		if ( p->pos - f->delay < f->psd )
		{
			eprint( FATAL, SET, "%s: Pulse %ld now starts too early to allow "
					"setting of a phase pulse.\n",
					pulser_struct.name, p->num );
			THROW( EXCEPTION )
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
	else                                 /* if its not the first pulse */
	{
		/* Find the position of the preceeding pulse */
		
		pp = p->function->pulses[ nth - 1 ];

		/* Don't do anything if the position is already set and the pulse
		   starts at a reasonable position, i.e. it starts long enough before
		   the pulse its associated with and not too near to the previous
		   pulse */

		if ( phase_p->is_pos &&
			 ( ( phase_p->pos <= p->pos - f->psd &&
				 phase_p->pos >= pp->pos + pp->len + dg2020.grace_period ) ||
			   p->pc == pp->pc ) )
			goto set_length;

		/* If the phase switch delay does not fit between the pulse it's
		   associated with and its predecessor we have to complain (except
		   when both pulses use the same phase sequence or none at all) */

		if ( p->pos - ( pp->pos + pp->len ) < f->psd && p->pc != pp->pc )
		{
			eprint( FATAL, SET, "%s: Distance between pulses %ld and %ld "
					"becomes too small to allow setting of phase pulses.\n",
					pulser_struct.name, p->num, pp->num );
			THROW( EXCEPTION )
		}

		/* If the phase pulse has a position and it hasn't been stored yet
           store it now */

		if ( phase_p->is_pos && ! phase_p->is_old_pos )
		{
			phase_p->old_pos = phase_p->pos;
			phase_p->is_old_pos = SET;
		}

		/* So, obviously, we have to reset the pulse position: Try to start the
		   phase pulse as late as possible, i.e. just the phase switch delay
		   plus a grace period before the associated pulse, because the
		   probablity is high that the preceeding pulse is going to be shifted
		   to later times or is lenghtened */

		phase_p->pos = p->pos - f->psd;

		/* If this is too near to the preceeding pulse leave out the grace
		   period, and if this still is too near complain (except when both
		   pulses use the same phase sequence since than both phase pulses get
		   merged into one) */

		if ( phase_p->pos < pp->pos + pp->len + dg2020.grace_period )
		{
			phase_p->pos += dg2020.grace_period;

			if ( phase_p->pos < pp->pos + pp->len + dg2020.grace_period &&
				 p->pc != pp->pc && for_pulse != p )
			{
				eprint( SEVERE, SET, "%s: Pulses %ld and %ld become so "
						"close that problems with phase switching may "
						"result.\n", pulser_struct.name, pp->num,
						p->num );
				for_pulse = p;
			}
		}

		/* Adjust the length of all phase pulses associated with the
		   precceding pulse */

		if ( dg2020_find_phase_pulse( pp, &pppl, &ppp_num ) )
		{
			for ( i = 0; i < ppp_num; i++ )
			{
				if ( pppl[ i ]->is_len )
				{
					if ( ! pppl[ i ]->is_old_len )
					{
						pppl[ i ]->is_old_len = SET;
						pppl[ i ]->old_len = pppl[ i ]->len;
					}

					pppl[ i ]->len = phase_p->pos - pppl[ i ]->pos;

					if ( pppl[ i ]->old_len == pppl[ i ]->len )
						pppl[ i ]->is_old_len = UNSET;

					pppl[ i ]->needs_update = NEEDS_UPDATE( pppl[ i ] );
				}

				if ( pppl[ i ]->pos + pppl[ i ]->len <
					 pppl[ i ]->for_pulse->pos + pppl[ i ]->for_pulse->len &&
					 pppl[ i ]->for_pulse->pc != pppl[ i ]->for_pulse->pc )
				{
					eprint( FATAL, SET, "%s: Distance between pulses "
							"%ld and %ld becomes too small to allow setting "
							"of phase pulses.\n", pulser_struct.name, p->num,
							pppl[ i ]->for_pulse->num );
					THROW( EXCEPTION )
				}

				if ( pppl[ i ]->pos + pppl[ i ]->len <
					 pppl[ i ]->for_pulse->pos + pppl[ i ]->for_pulse->len
					 + dg2020.grace_period  &&
					 pppl[ i ]->for_pulse->pc != pppl[ i ]->for_pulse->pc &&
					 p != for_pulse )
				{
					eprint( SEVERE, SET, "%s: Pulses %ld and %ld become so "
							"close that problems with phase switching may "
							"result.\n", pulser_struct.name, p->num,
							pppl[ i ]->for_pulse->num );
					for_pulse = p;
				}
			}

			T_free( pppl );
		}

		/* Now we can be sure that we have a legal position for the phase
		   pulse and as it got a new position it has to be updated */

		phase_p->is_pos = SET;
		phase_p->needs_update = SET;
	}

set_length:

	if ( phase_p->is_old_pos && phase_p->old_pos == phase_p->pos )
		phase_p->is_old_pos = UNSET;

	/* Store the old length of the phase pulse (if there's one) */

	if ( phase_p->is_len )
	{
		phase_p->old_len = phase_p->len;
		phase_p->is_old_len = SET;
	}

	if ( nth == p->function->num_pulses - 1 ||
		 ! p->function->pulses[ nth  + 1 ]->is_active )  // last active pulse ?
	{
		if ( flag )                // in test run
			phase_p->len = -1;
		else
			/* Take care: the variable `dg2020.max_seq_len' already
			   includes the delay for the functions... */

			phase_p->len = dg2020.max_seq_len - f->delay - phase_p->pos;
	}
	else
	{
		pn = p->function->pulses[ nth + 1 ];

		/* This length is only tentatively and may become shorter when the
		   following phase pulse is set */

		if ( phase_p->is_len &&
			 ( ( phase_p->pos + phase_p->len >=
				 p->pos + p->len + dg2020.grace_period || p->pc == pn->pc ) &&
			   phase_p->pos + phase_p->len <= dg2020.max_seq_len - f->delay &&
			   ( phase_p->pos + phase_p->len <= pn->pos - f->psd ||
				 p->pc == pn->pc ) ) )
			goto done_setting;

		phase_p->len = pn->pos - f->psd - phase_p->pos;

		if ( phase_p->pos + phase_p->len < p->pos + p->len && p->pc != pn->pc )
		{
			eprint( FATAL, SET, "%s: Distance between pulses %ld and %ld "
					"becomes too small to allow setting of phase pulses.\n",
					pulser_struct.name, p->num, pn->num );
			THROW( EXCEPTION )
		}

		if ( phase_p->pos + phase_p->len <
			 p->pos + p->len + dg2020.grace_period &&
			 p->pc != pn->pc && p != for_pulse )
		{
			eprint( SEVERE, SET, "%s: Pulses %ld and %ld become so close "
					"that problems with phase switching may result.\n",
					pulser_struct.name, p->num, pn->num );
			for_pulse = p;
		}
	}

done_setting:

	phase_p->is_len = SET;

	/* Make sure the flags for an old position or length are only set if the
	   old and the new value really differ */

	if ( phase_p->is_old_len && phase_p->old_len == phase_p->len )
		phase_p->is_old_len = UNSET;

	phase_p->needs_update = NEEDS_UPDATE( phase_p );
}


/*------------------------------------------------------------------*/
/* Function is called after the test run to reset all the variables */
/* describing the state of the pulser to their initial values       */
/*------------------------------------------------------------------*/

void dg2020_full_reset( void )
{
	PULSE *p = dg2020_Pulses;


	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't (unless we haven't ben told to keep
		   all pulses, even unused ones) */

		if ( ! p->has_been_active && ! dg2020.keep_all )
		{
			if ( p->num >=0 )
				eprint( WARN, UNSET, "%s: Pulse %ld is never used.\n",
						pulser_struct.name, p->num );
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

		p->needs_update = UNSET;

		p->is_old_pos = p->is_old_len = UNSET;

		p->is_active = ( p->is_pos && p->is_len &&
						 ( p->len > 0 || p->len == -1 ) );

		p = p->next;
	}
}


/*-------------------------------------------------------------------------*/
/* Function deletes a pulse and returns a pointer to the next pulse in the */
/* pulse list.                                                             */
/*-------------------------------------------------------------------------*/

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

	fsc2_assert( i < p->function->num_pulses );  /* Paranoia */

	/* Put the last of the functions pulses into the slot for the pulse to
	   be deleted and shorten the list by one element */

	if ( i != p->function->num_pulses - 1 )
		p->function->pulses[ i ] = 
			p->function->pulses[ p->function->num_pulses - 1 ];

	/* Now delete the pulse - if the deleted pulse was the last pulse of
	   its function send a warning and mark the function as useless */

	if ( p->function->num_pulses-- > 1 )
		p->function->pulses =
					  T_realloc( p->function->pulses,
								 p->function->num_pulses * sizeof( PULSE * ) );
	else
	{
		p->function->pulses = T_free( p->function->pulses );

		eprint( SEVERE, UNSET, "%s: Function `%s' isn't used at all because "
				"all its pulses are never used.\n", pulser_struct.name,
				Function_Names[ p->function->self ] );
		p->function->is_used = UNSET;
	}

	/* Now remove the pulse from the real pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;

	/* Special care has to be taken if this is the very last pulse... */

	if ( p == dg2020_Pulses && p->next == NULL )
		dg2020_Pulses = T_free( dg2020_Pulses );
	else
		T_free( p );

	return pp;
}


/*-----------------------------------------------------------------------*/
/* After the test run, when all things have been checked and the maximum */
/* length of the pulse sequence is known, we can finally calculate the   */
/* lengths of the last phase pulses of a phase function - they simply    */
/* last to the end of the pulse sequence.                                */
/*-----------------------------------------------------------------------*/

void dg2020_finalize_phase_pulses( int func )
{
	FUNCTION *f;
	int i;
	PULSE *p;


	fsc2_assert( func == PULSER_CHANNEL_PHASE_1 ||
				 func == PULSER_CHANNEL_PHASE_2 );

	f = &dg2020.function[ func ];
	if ( ! f->is_used )
		return;

	/* Find the last active phase pulses and set its length to the maximum
	   posible amount, i.e. to the maximum sequence length - take care, the
	   variable `dg2020.max_seq_len' already includes the delay for the
	   function... */

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];
		if ( p->is_active && p->is_pos && p->is_len && p->len == -1 )
			p->len = dg2020.max_seq_len - f->delay - p->pos;
	}
}


/*------------------------------------------------------------------*/
/* Function creates all active pulses in the channels of the pulser */
/* assigned to the function passed as argument.                     */
/*------------------------------------------------------------------*/

void dg2020_set_pulses( FUNCTION *f )
{
	PULSE *p,
		  *pp;
	Ticks start, end = 0;
	int i;


	/* As usual we need a special treatment of phase pulses... */

	if ( f->self == PULSER_CHANNEL_PHASE_1 ||
		 f->self == PULSER_CHANNEL_PHASE_2 )
		return;

	/* Always set the very first bit to LOW state, see the rant about the bugs 
	   in the pulser firmware at the start of dg2020_gpib.c... */

	dg2020_set_constant( f->channel[ 0 ]->self, -1, 1, LOW );

	/* Now simply run through all active pulses of the channel */

	for ( pp = NULL, p = f->pulses[ 0 ], i = 0;
		  i < f->num_pulses && p->is_active;
		  pp = p, p = f->pulses[ ++i ] )
	{
		/* Set the area from the end of the previous pulse up to the start
		   of the current pulse (exception: for the first pulse we start
		   at the first possible position */

		start = ( pp == NULL ) ? 0 : pp->pos + pp->len + f->delay;
		end = p->pos + f->delay;
		if ( start != end )
			dg2020_set_constant( p->channel->self, start, end - start,
								 type_OFF( f ) );

		/* Set the area of the pulse itself */

		start = end;
		end = p->pos + p->len + f->delay;
		if ( start != end )
			dg2020_set_constant( p->channel->self, start, end - start,
								   type_ON( f ) );
	}

	/* Finally set the area following the last active pulse to the end
	   of the sequence (take care: dg2020.max_seq_len already includes the
	   maximum delay of all functions) */

	if ( pp != NULL )
	{
		start = end;
		end = dg2020.max_seq_len;
		if ( start != end )
			dg2020_set_constant( pp->channel->self, start, end - start,
								   f->is_inverted ? HIGH : LOW );
	}
	else
		dg2020_set_constant( f->channel[ 0 ]->self, -1,
							 dg2020.max_seq_len + 1, type_OFF( f ) );

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses; p = f->pulses[ ++i ] )
		p->was_active = p->is_active;

	if ( f->needs_phases )
		dg2020_set_phase_pulses( f->phase_func );		
}


/*---------------------------------------------------------------------------*/
/* Function creates the active pulses in the diverse channels assigned to a  */
/* phase function. Here the problem is that the pulses are not well sorted   */
/* in the pulse list of the function and belong to different channels. To    */
/* make live simple we create a dummy function that gets 'assigned' a sorted */
/* list of the pulses belonging to one of the phase functions channels and   */
/* can than call dg2020_set_pulses() on this dummy function to do all the    */
/* real work.                                                                */
/*---------------------------------------------------------------------------*/

void dg2020_set_phase_pulses( FUNCTION *f )
{
	FUNCTION dummy_f;
	int i;


	dummy_f.self = PULSER_CHANNEL_NO_TYPE;
	dummy_f.is_inverted = f->is_inverted;
	dummy_f.delay = f->delay;
	dummy_f.needs_phases = UNSET;

	for ( i = 0; i < f->num_channels; i++ )
	{
		dummy_f.pulses = NULL;
		dummy_f.channel[ 0 ] = f->channel[ i ];

		/* Get a sorted list of all phase pulses in this channel */

		dummy_f.num_pulses = dg2020_get_phase_pulse_list( f, f->channel[ i ],
														  &dummy_f.pulses );
		if ( dummy_f.pulses != NULL )        /* there are pulses */
		{
			dg2020_set_pulses( &dummy_f );
			T_free( dummy_f.pulses );
		}
		else                                 /* no pulses in this channel */
		{
			if ( ! f->is_inverted )
				dg2020_set_constant( f->channel[ i ]->self, -1,
									 dg2020.max_seq_len + 1, type_OFF( f ) );
			else
			{
				dg2020_set_constant( f->channel[ i ]->self, -1, 1, LOW );
				dg2020_set_constant( f->channel[ i ]->self, 0,
									 dg2020.max_seq_len, type_OFF( f ) );
			}
		}
	}
}


/*--------------------------------------------------------------------*/
/* Changes the pulse pattern in the channel belonging to function 'f' */
/* so that the data in the pulser get in sync with the its internal   */
/* representation. Care has taken to minimize the number of commands  */
/* and their length.                                                  */
/*--------------------------------------------------------------------*/

void dg2020_commit( FUNCTION * f, bool flag )
{
	PULSE *p;
	int i;
	Ticks start, len;
	char *old, *new;
	int what;
	bool needs_changes = UNSET;
	int ch;


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
			p = f->pulses[ i ];
			p->needs_update = UNSET;
			p->was_active = p->is_active;
			p->is_old_pos = p->is_old_len = UNSET;
		}

		return;
	}

	/* In a real run we have to change the pulses. The only way to keep the
	   number and length of commands to be sent to the pulser at a minimum
	   while getting it right in every imaginable case is to create two images
	   of the pulser channels state, one of the current stateand a second one
	   of the state after the changes. These images are compared and only that
	   parts where differences are found are either SET or RESET. Of course,
	   that needs quite some computer time but probable is faster, or at least
	   easier to understand and to debug, than any alternative I came up
	   with... */

	old = T_calloc( 2 * ( size_t ) dg2020.max_seq_len, 1 );
	new = old + dg2020.max_seq_len;

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];
		if ( ! p->needs_update )
		{
			p->is_old_len = p->is_old_pos = UNSET;
			p->was_active = p->is_active;
			continue;
		}

		needs_changes = SET;

		if ( p->is_old_pos || p->is_old_len )
			dg2020_set( old, p->is_old_pos ? p->old_pos : p->pos,
						p->is_old_len ? p->old_len : p->len, f->delay );
		dg2020_set( new, p->pos, p->len, f->delay );

		p->is_old_len = p->is_old_pos = UNSET;
		if ( p->is_active )
			p->was_active = SET;
		p->needs_update = UNSET;
	}

	/* Find all different areas by repeatedly calling dg2020_diff() - it
       returns +1 or -1 for setting or resetting plus the start and length of
       the different area or 0 if no differences are found anymore */

	if ( needs_changes )
	{
		ch = f->pulses[ 0 ]->channel->self;
		while ( ( what = dg2020_diff( old, new, &start, &len ) ) != 0 )
			dg2020_set_constant( ch, start, len,
								 what == -1 ? type_OFF( f ) : type_ON( f ) );
	}

	T_free( old );
}


/*--------------------------------------------------------------------------*/
/* Changes the pulse pattern in the channel belonging to the phase function */
/* 'f' so that the data in the pulser get in sync with the its internal     */
/* representation. Actually, the real work is done in dg2020_commit().      */
/*--------------------------------------------------------------------------*/

void dg2020_commit_phases( FUNCTION * f, bool flag )
{
	FUNCTION dummy_f;
	int i;

	dummy_f.self = PULSER_CHANNEL_NO_TYPE;
	dummy_f.is_inverted = f->is_inverted;
	dummy_f.delay = f->delay;
	dummy_f.max_seq_len = f->max_seq_len;

	for ( i = 0; i < f->num_channels; i++ )
	{
		dummy_f.pulses = NULL;
		dummy_f.num_pulses = dg2020_get_phase_pulse_list( f, f->channel[ i ],
														  &dummy_f.pulses );
		if ( dummy_f.pulses != NULL )
		{
			dg2020_commit( &dummy_f, flag );
			T_free( dummy_f.pulses );
		}
	}
}


/*---------------------------------------------------------------------------*/
/* Sets the additional bock used for maintaining the requested repetion time */
/* to the low state                                                          */
/*---------------------------------------------------------------------------*/

void dg2020_clear_padding_block( FUNCTION *f )
{
	int i;


	if ( ! f->is_used ||
		 ! dg2020.block[ 1 ].is_used )
		return;

	for ( i = 0; i < f->num_channels; i++ )
		dg2020_set_constant( f->channel[ i ]->self,
							 dg2020.block[ 1 ].start - 1,
							 dg2020.mem_size - dg2020.block[ 1 ].start,
							 type_OFF( f ) );
}
