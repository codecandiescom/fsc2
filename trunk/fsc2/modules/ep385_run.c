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


#include "ep385.h"


static bool ep385_update_pulses( bool flag );
static void ep385_pulse_check( FUNCTION *f );
static void ep385_defense_shape_check( FUNCTION *shape );
static PULSE *ep385_delete_pulse( PULSE *p );
static void ep385_commit( bool flag );


/*-------------------------------------------------------------------------*/
/* Function is called in the experiment after pulses have been changed to  */
/* update the pulser accordingly. No checking has to be done except in the */
/* test run.                                                               */
/*-------------------------------------------------------------------------*/

bool ep385_do_update( void )
{
	bool restart = UNSET;
	bool state;


	if ( ! ep385_is_needed )
		return OK;

	/* Resort the pulses, check that the new pulse settings are reasonable
	   and finally commit all changes */

	if ( ep385.is_running )
	{
		restart = SET;
		if ( FSC2_MODE == EXPERIMENT )
			ep385_run( STOP );
	}

	state = ep385_update_pulses( FSC2_MODE == TEST );

	ep385.needs_update = UNSET;
	if ( restart && FSC2_MODE == EXPERIMENT )
		ep385_run( START );

	return state;
}


/*--------------------------------------------------------------------------*/
/* This function sorts the pulses and checks that the pulses don't overlap. */
/*--------------------------------------------------------------------------*/

static bool ep385_update_pulses( bool flag )
{
	static int i, j;
	int l, m;
	FUNCTION *f;
	PULSE *p;
	CHANNEL *ch;
	PULSE **pm_entry;
	PULSE_PARAMS *pp;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;

		/* Nothing to be done for unused functions */

		if ( ! f->is_used )
			continue;

		/* Check that pulses don't start to overlap */

		ep385_pulse_check( f );

		/* Set up and check pulses for each channel assoiciated with the
		   function according to the current phase. */

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			if ( ch->num_pulses == 0 )
				continue;

			/* Copy the old pulse list for the channel so the new state can
			   be compared to the old state and an update is done only when
			   needed. */

			memcpy( ch->old_pulse_params, ch->pulse_params,
					ch->num_active_pulses * sizeof *ch->pulse_params );
			ch->old_num_active_pulses = ch->num_active_pulses;

			/* Now we set up a new list of pulse parameters and sort it */

			pm_entry = f->pm[ f->next_phase * f->num_channels + j ];

			ch->num_active_pulses = 0;
			for ( m = 0; ( p = pm_entry[ m ] ) != NULL; m++ )
				if ( p->is_active )
				{
					pp = ch->pulse_params + ch->num_active_pulses++;

					pp->pos = p->pos + f->delay;
					pp->len = p->len;
					pp->pulse = p;

					if ( p->function->self != PULSER_CHANNEL_PULSE_SHAPE &&
						 p->sp != NULL )
					{
						pp->pos -= p->function->left_shape_padding;
						pp->len +=   p->function->left_shape_padding
								   + p->function->right_shape_padding;
					}
				}			

			qsort( ch->pulse_params, ch->num_active_pulses,
				   sizeof *ch->pulse_params, ep385_pulse_compare );

			ep385_shape_padding_check( ch );

			/* Compare old and new state, if nothing changed, i.e. the number
			   of active pulses is the same and all positions and lengths are
			   identical, we're already done and the channel does not need to
			   be updated */

			if ( ch->num_active_pulses == ch->old_num_active_pulses )
			{
				for ( m = 0; m < ch->num_active_pulses; m++ )
				if ( ch->pulse_params[ m ].pos !=
					 ch->old_pulse_params[ m ].pos ||
					 ch->pulse_params[ m ].len !=
					 ch->old_pulse_params[ m ].len )
					break;

				if ( m == ch->num_active_pulses )
				{
					ch->needs_update = UNSET;
					continue;
				}
			}

			ch->needs_update = SET;

			/* If the following tests fail stop immediately when doing a test
			   run but during a real experiment instead just reset the pulses
			   to their old values and return FAIL.*/

			TRY
			{
				/* Check that there aren't to many pulses for the chanel */

				if ( ch->num_active_pulses > MAX_PULSES_PER_CHANNEL )
				{
					print( FATAL, "More than %d pulses (%d) are required on "
						   "channel %d associated with function '%s'.\n",
						   MAX_PULSES_PER_CHANNEL, ch->num_active_pulses,
						   ch->self + CHANNEL_OFFSET,
						   Function_Names[ ch->function->self ] );
					THROW( EXCEPTION );
				}

				/* Check that defense and shape pulses don't get to near to
				   each other (this checks are done when either one of the
				   minimum distances has been set or TWT or TWT_GATE pulses
				   are used) */

				if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
					 ep385.function[ PULSER_CHANNEL_DEFENSE ].is_used &&
					 ( ep385.is_shape_2_defense || ep385.is_defense_2_shape ||
					   ep385.function[ PULSER_CHANNEL_TWT ].is_used ||
					   ep385.function[ PULSER_CHANNEL_TWT_GATE ].is_used ) )
					ep385_defense_shape_check( f );

				TRY_SUCCESS;
			}
			CATCH( EXCEPTION )
			{
				if ( flag )                 /* during test run */
					THROW( EXCEPTION );

				for ( l = 0; l <= i; i++ )
					for ( m = 0; m <= j; m++ )
					{
						memcpy( ch->pulse_params, ch->old_pulse_params,
								ch->old_num_active_pulses
								* sizeof *ch->pulse_params );
						ch->num_active_pulses = ch->old_num_active_pulses;
					}

				for ( p = ep385_Pulses; p != NULL; p = p->next )
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
		}
	}

	ep385_commit( flag );

	return OK;
}


/*-------------------------------------------------------------------*/
/* This function simply checks that no pulses of a function overlap. */
/*-------------------------------------------------------------------*/

static void ep385_pulse_check( FUNCTION *f )
{
	PULSE *p1, *p2;
	int i, j;


	if ( f->num_pulses < 2 )
		return;

	for ( i = 0; i < f->num_pulses - 1; i++ )
	{
		p1 = f->pulses[ i ];
		if ( ! p1->is_active )
			continue;

		for ( j = i + 1; j < f->num_pulses; j++ )
		{
			p2 = f->pulses[ j ];
			if ( ! p2->is_active )
				continue;

			if ( ( p1->pos == p2->pos ) ||
				 ( p1->pos < p2->pos && p1->pos + p1->len > p2->pos ) ||
				 ( p2->pos < p1->pos && p2->pos + p2->len > p1->pos ) )
			{
				if ( ep385.auto_shape_pulses &&
					 p1->function->self == PULSER_CHANNEL_PULSE_SHAPE )
				{
					if ( p1->function != p2->function )
						print( FATAL, "Shape pulses for pulses #%ld function "
							   "'%s') and #%ld (function '%s') start to "
							   "overlap.\n", p1->sp->num,
							   Function_Names[ p1->sp->function->self ],
							   p2->sp->num,
							   Function_Names[ p2->sp->function->self ] );
				}
				else
					print( FATAL, "Pulses #%ld and #%ld of function '%s' "
						   "start to overlap.\n", p1->num, p2->num,
						   Function_Names[ f->self ] );

				THROW( EXCEPTION );
			}
		}
	}
}


/*------------------------------------------------------------------------*/
/* Function checks if the distance between pulse shape pulses and defense */
/* pulses is large enough. The minimum lengths the shape_2_defense and    */
/* defense_2_shape members of the ep395 structure. Both are set to rather */
/* large values at first but can be customized by calling the EDL         */
/* functions pulser_shape_to_defense_minimum_distance() and               */
/* pulser_defense_to_shape_minimum_distance() (names are intentionally    */
/* that long).                                                            */
/* The function is called only if pulse shape and defense pulses are used */
/* and either also TWT or TWT_GATE pulses or at least one of both the     */
/* mentioned EDL functions have been called.                              */
/*------------------------------------------------------------------------*/

static void ep385_defense_shape_check( FUNCTION *shape )
{
	FUNCTION *defense = ep385.function + PULSER_CHANNEL_DEFENSE;
	PULSE *shape_p, *defense_p;
	long i, j;


	for ( i = 0; i < shape->num_pulses; i++ )
	{
		shape_p = shape->pulses[ i ];

		if ( ! shape_p->is_active )
			continue;

		for ( j = 0; j < defense->num_pulses; j++ )
		{
			defense_p = defense->pulses[ j ];

			if ( ! defense_p->is_active )
				continue;

			if ( shape_p->pos < defense_p->pos &&
				 shape_p->pos + shape_p->len + ep385.shape_2_defense >
				 defense_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					if ( shape_p->sp == NULL )
						print( FATAL, "Distance between PULSE_SHAPE pulse "
							   "#%ld and DEFENSE pulse #%ld got shorter than "
							   "%s.\n", shape_p->num, defense_p->num,
							   ep385_pticks( ep385.shape_2_defense ) );
					else
						print( FATAL, "Distance between shape pulse for pulse "
							   "#%ld (function '%s') and DEFENSE pulse #%ld "
							   "got shorter than %s.\n", shape_p->sp->num,
							   Function_Names[ shape_p->sp->function->self ],
							   defense_p->num,
							   ep385_pticks( ep385.shape_2_defense ) );
					THROW( EXCEPTION );
				}

				if ( ep385.shape_2_defense_too_near == 0 )
				{
					if ( shape_p->sp == NULL )
						print( SEVERE, "Distance between PULSE_SHAPE pulse "
							   "#%ld and DEFENSE pulse #%ld got shorter than "
							   "%s.\n", shape_p->num, defense_p->num,
							   ep385_pticks( ep385.shape_2_defense ) );
					else
						print( SEVERE, "Distance between shape pulse for "
							   "pulse #%ld (function '%s') and DEFENSE pulse "
							   "#%ld got shorter than %s.\n", shape_p->sp->num,
							   Function_Names[ shape_p->sp->function->self ],
							   defense_p->num,
							   ep385_pticks( ep385.shape_2_defense ) );
				}

				ep385.shape_2_defense_too_near++;
			}

			if ( defense_p->pos < shape_p->pos &&
				 defense_p->pos + defense_p->len + ep385.defense_2_shape >
				 shape_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					if ( shape_p->sp == NULL )
						print( FATAL, "Distance between DEFENSE pulse #%ld "
							   "and PULSE_SHAPE pulse #%ld got shorter than "
							   "%s.\n", defense_p->num, shape_p->num,
							   ep385_pticks( ep385.defense_2_shape ) );
					else
						print( FATAL, "Distance between DEFENSE pulse #%ld "
							   "and shape pulse for pulse #%ld (function "
							   "'%s') got shorter than %s.\n", defense_p->num,
							   shape_p->sp->num,
							   Function_Names[ shape_p->sp->function->self ],
							   ep385_pticks( ep385.defense_2_shape ) );
					THROW( EXCEPTION );
				}

				if ( ep385.defense_2_shape_too_near == 0 )
				{
					if ( shape_p->sp == NULL )
						print( SEVERE, "Distance between DEFENSE pulse #%ld "
							   "and PULSE_SHAPE pulse #%ld got shorter than "
							   "%s.\n", defense_p->num, shape_p->num,
							   ep385_pticks( ep385.defense_2_shape ) );
					else
						print( SEVERE, "Distance between DEFENSE pulse #%ld "
							   "and shape pulse for pulse #%ld (function "
							   "'%s') got shorter than %s.\n", defense_p->num,
							   shape_p->sp->num,
							   Function_Names[ shape_p->sp->function->self ],
							   ep385_pticks( ep385.defense_2_shape ) );
				}

				ep385.defense_2_shape_too_near++;
			}
		}
	}
}


/*------------------------------------------------------------------*/
/* Function is called after the test run to reset all the variables */
/* describing the state of the pulser to their initial values.      */
/*------------------------------------------------------------------*/

void ep385_full_reset( void )
{
	int i, j, m;
	PULSE *p = ep385_Pulses;
	FUNCTION *f;
	CHANNEL *ch;
	PULSE **pm_entry;
	PULSE_PARAMS *pp;


	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't (unless we haven't ben told to keep
		   all pulses, even unused ones) */

		if ( ! p->has_been_active && ! ep385.keep_all )
		{
			print( WARN, "Pulse #%ld is never used.\n", p->num );
			p = ep385_delete_pulse( p );
			continue;
		}

		/* Reset pulse properies to their initial values */

		p->pos     = p->initial_pos;
		p->is_pos  = p->initial_is_pos;
		p->len     = p->initial_len;
		p->is_len  = p->initial_is_len;
		p->dpos    = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;
		p->dlen    = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->needs_update = UNSET;
		p->is_old_pos = p->is_old_len = UNSET;
		p->is_active = ( p->is_pos && p->is_len && p->len > 0 );

		p = p->next;
	}

	ep385.is_running = ep385.has_been_running;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;
		f->next_phase = 0;

		if ( ! f->is_used )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			ch->needs_update = SET;

			if ( ch->num_pulses == 0 )
				continue;

			/* Copy the old pulse list for the channel so the new state can
			   be compared to the old state and an update is done only when
			   needed. */

			memcpy( ch->old_pulse_params, ch->pulse_params,
					ch->num_active_pulses * sizeof *ch->pulse_params );
			ch->old_num_active_pulses = ch->num_active_pulses;

			pm_entry = f->pm[ f->next_phase * f->num_channels + j ];

			ch->num_active_pulses = 0;
			for ( m = 0; ( p = pm_entry[ m ] ) != NULL; m++ )
				if ( p->is_active )
				{
					pp = ch->pulse_params + ch->num_active_pulses++;

					pp->pos = p->pos + f->delay;
					pp->len = p->len;
					pp->pulse = p;

					if ( p->function->self != PULSER_CHANNEL_PULSE_SHAPE &&
						 p->sp != NULL )
					{
						pp->pos -= p->function->left_shape_padding;
						pp->len +=   p->function->left_shape_padding
								   + p->function->right_shape_padding;
					}
				}

			qsort( ch->pulse_params, ch->num_active_pulses,
				   sizeof *ch->pulse_params, ep385_pulse_compare );

			ep385_shape_padding_check( ch );
		}
	}
}


/*------------------------------------------------*/
/*------------------------------------------------*/

void ep385_shape_padding_check( CHANNEL *ch )
{
	PULSE_PARAMS *pp, *ppp;
	int i;


	if ( ch->function->self == PULSER_CHANNEL_PULSE_SHAPE ||
		 ! ch->function->uses_auto_shape_pulses ||
		 ch->num_active_pulses == 0 )
		return;

	pp = ch->pulse_params;
	if ( pp->pos < 0 )
	{
		if ( ! pp->pulse->left_shape_warning )
		{
			print( SEVERE, "Pulse #%ld too early to set left padding of %s.\n",
				   pp->pulse->num,
				   ep385_pticks( ch->function->left_shape_padding ) );
			pp->pulse->left_shape_warning = SET;
		}

		ep385.left_shape_warning++;
		pp->pulse->function->min_left_shape_padding =
					l_min( pp->pulse->function->min_left_shape_padding,
						   pp->pulse->function->left_shape_padding + pp->pos );
		pp->len += pp->pos;
		pp->pos = 0;
	}

	for ( i = 1; i < ch->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		if ( ppp->pos + ppp->len > pp->pos )
			ppp->len -= ppp->pos + ppp->len - pp->pos;

		if ( pp->pos + pp->len > MAX_PULSER_BITS )
		{
			if ( ! pp->pulse->right_shape_warning )
			{
				print( SEVERE, "Pulse #%ld too long to set right padding of "
					   "%s.\n", pp->pulse->num,
					   ep385_pticks( ch->function->right_shape_padding ) );
				pp->pulse->right_shape_warning = SET;
			}

			ep385.right_shape_warning++;
			pp->pulse->function->min_right_shape_padding =
				   l_min( pp->pulse->function->min_right_shape_padding,
						  pp->pulse->function->right_shape_padding + pp->pos );
			pp->len = MAX_PULSER_BITS - pp->pos;
		}
	}
}


/*------------------------------------------------*/
/* Function deletes a pulse and returns a pointer */
/* to the next pulse in the pulse list.           */
/*------------------------------------------------*/

static PULSE *ep385_delete_pulse( PULSE *p )
{
	PULSE *pp;
	int i;


	/* If the pulse has an associated shape pulse delete it */

	if ( p->sp && p->sp->function->self == PULSER_CHANNEL_PULSE_SHAPE )
		ep385_delete_pulse( p->sp );

	/* First we've got to remove the pulse from its functions pulse list */

	for ( i = 0; i < p->function->num_pulses; i++ )
		if ( p->function->pulses[ i ] == p )
			break;

	fsc2_assert( i < p->function->num_pulses );             /* Paranoia */

	/* Put the last of the functions pulses into the slot for the pulse to
	   be deleted and shorten the list by one element */

	if ( i != p->function->num_pulses - 1 )
		p->function->pulses[ i ] =
			p->function->pulses[ p->function->num_pulses - 1 ];

	/* Now delete the pulse - if the deleted pulse was the last pulse of
	   its function send a warning and mark the function as useless */

	if ( p->function->num_pulses-- > 1 )
		p->function->pulses = PULSE_PP
					  T_realloc( p->function->pulses,
								 p->function->num_pulses *
								 sizeof *p->function->pulses );
	else
	{
		p->function->pulses = PULSE_PP T_free( p->function->pulses );

		print( SEVERE, "Function '%s' isn't used at all because all its "
			   "pulses are never used.\n",
			   Function_Names[ p->function->self ] );
		p->function->is_used = UNSET;
	}

	/* Now remove the pulse from the pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;

	/* Special care has to be taken if this is the very last pulse... */

	if ( p == ep385_Pulses && p->next == NULL )
		ep385_Pulses = PULSE_P T_free( ep385_Pulses );
	else
		T_free( p );

	return pp;
}


/*-------------------------------------------------------------------*/
/* Changes the pulse pattern in all channels so that the data in the */
/* pulser get in sync with the its internal representation.          */
/*-------------------------------------------------------------------*/

static void ep385_commit( bool flag )
{
	PULSE *p;
	int i, j;
	FUNCTION *f;


	/* Only really set the pulses while doing an experiment */

	if ( ! flag )
		ep385_set_channels( );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;

		/* Nothing to be done for unused functions */

		if ( ! f->is_used )
			continue;

		for ( j = 0; j < f->num_pulses; j++ )
		{
			p = f->pulses[ j ];
			p->needs_update = UNSET;
			p->was_active = p->is_active;
			p->is_old_pos = p->is_old_len = UNSET;
		}

		for ( j = 0; j < f->num_channels; j++ )
			f->channel[ j ]->needs_update = UNSET;
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
