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
	static FUNCTION *f;
	PULSE *p;
	static CHANNEL *ch;
	PULSE **pm_entry;
	PULSE_PARAMS *pp;


	ep385.needs_update = UNSET;

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

					/* Extend pulses for which a shape pulse has been defined
					   a bit */

					if ( p->function->self != PULSER_CHANNEL_PULSE_SHAPE &&
						 p->sp != NULL )
					{
						pp->pos -= p->function->left_shape_padding;
						pp->len +=   p->function->left_shape_padding
								   + p->function->right_shape_padding;
					}

					/* Extend TWT pulses that were automatically created */

					if ( p->function->self == PULSER_CHANNEL_TWT &&
						 p->tp != NULL )
					{
						pp->pos -= p->tp->function->left_twt_padding;
						pp->len +=   p->tp->function->left_twt_padding
								   + p->tp->function->right_twt_padding;
					}
				}			

			qsort( ch->pulse_params, ch->num_active_pulses,
				   sizeof *ch->pulse_params, ep385_pulse_compare );

			ep385_shape_padding_check_1( ch );
			if ( ch->function->self == PULSER_CHANNEL_TWT )
				ep385_twt_padding_check( ch );

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
			ep385.needs_update = SET;

			/* If the following tests fail stop immediately when doing a test
			   run but during a real experiment instead just reset the pulses
			   to their old values and return FAIL.*/

			TRY
			{
				/* Check that there aren't too many pulses for the chanel */

				if ( ch->num_active_pulses > MAX_PULSES_PER_CHANNEL )
				{
					print( FATAL, "More than %d pulses (%d) are required on "
						   "channel %d associated with function '%s'.\n",
						   MAX_PULSES_PER_CHANNEL, ch->num_active_pulses,
						   ch->self + CHANNEL_OFFSET, ch->function->name );
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

	ep385_shape_padding_check_2( );

	/* Now we still need a final check that the distance between the last
	   defense pulse and the first shape pulse isn't too short - this is
	   only relevant for very fast repetitions of the pulse sequence but
	   needs to be tested - thanks to Celine Elsaesser for pointing this
	   out. */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;
		if ( ( ! f->is_used && f->num_channels == 0 ) ||
			 i == PULSER_CHANNEL_PHASE_1 ||
			 i == PULSER_CHANNEL_PHASE_2 )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];
			if ( ch->num_active_pulses == 0 )
				continue;

			f->max_seq_len = Ticks_max( f->max_seq_len,
						   ch->pulse_params[ ch->num_active_pulses - 1 ].pos +
						   ch->pulse_params[ ch->num_active_pulses - 1 ].len );
		}
	}

	if ( ep385.function[ PULSER_CHANNEL_DEFENSE ].is_used &&
		 ep385.function[ PULSER_CHANNEL_PULSE_SHAPE ].is_used )
	{
		Ticks add;
		CHANNEL *cs =
					 ep385.function[ PULSER_CHANNEL_PULSE_SHAPE ].channel[ 0 ],
				*cd = ep385.function[ PULSER_CHANNEL_DEFENSE ].channel[ 0 ];
		PULSE_PARAMS *shape_p, *defense_p;


		if ( cd->num_active_pulses != 0 && cs->num_active_pulses != 0 )
		{
			shape_p = cs->pulse_params;
			defense_p = cd->pulse_params + cd->num_active_pulses - 1;
			add = Ticks_min( ep385.defense_2_shape, shape_p->pos
							 + Ticks_max( cd->function->max_seq_len,
										  cs->function->max_seq_len )
							 - defense_p->pos - defense_p->len );

			if ( add < ep385.defense_2_shape )
				cd->function->max_seq_len = cs->function->max_seq_len =
									Ticks_max( cd->function->max_seq_len,
											   cs->function->max_seq_len )
									+ ep385.defense_2_shape - add;

			shape_p = cs->pulse_params + cs->num_active_pulses - 1;
			defense_p = cd->pulse_params;
			add = Ticks_min( ep385.shape_2_defense, defense_p->pos
							 + Ticks_max( cd->function->max_seq_len,
										  cs->function->max_seq_len )
							 - shape_p->pos - shape_p->len );

			if ( add < ep385.shape_2_defense )
				cd->function->max_seq_len = cs->function->max_seq_len =
									Ticks_max( cd->function->max_seq_len,
											   cs->function->max_seq_len )
									+ ep385.shape_2_defense - add;
		}
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;
		if ( ( ! f->is_used && f->num_channels == 0 ) ||
			 i == PULSER_CHANNEL_PHASE_1 ||
			 i == PULSER_CHANNEL_PHASE_2 )
			continue;

		if ( f->max_seq_len > MAX_PULSER_BITS )
		{
			print( FATAL, "Pulse sequence for function '%s' is too long.\n",
				   f->name );
			THROW( EXCEPTION );
		}
	}

	/* Now really send the new pulse settings to the device - an update is
	   also required if no pulses have been defined, because in this case
	   we only will come to this place when the pulser is initialized and
	   then we need to set up the empty channels */

	if ( ep385.needs_update || ! ep385_is_needed )
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

	/* Loop over all active pulses of the function and check that they don't
	   overlap. A few things have to be taken into account:
	   1. Automatically created shape pulses for pulses of the same function
	      should not be reported - they can only clash if also the pulses they
		  were created for do clash. Thus for automatically created shape
		  pulses we only need to check for overlap if the pulses they belong
		  to are from different functions.
	   2. We do have to check for overlaps between automatically generated
	      shape pulses and user defined shape pulses.
	   3. Automatically created TWT pulses can't overlap - if they do we
	      will automatically reduce their lengths to avoid overlaps.
	   4. Also user defined TWT pulses and automatically created ones can't
	      overlap. Again, the automatically generated ones will shrink if
		  necessary.
	   5. User defined TWT pulses can overlap and thus must be tested.
	*/

	for ( i = 0; i < f->num_pulses - 1; i++ )
	{
		p1 = f->pulses[ i ];

		/* Skip checks for inactive pulses and automatically generated
		   TWT pulses */

		if ( ! p1->is_active ||
			 ( f->self == PULSER_CHANNEL_TWT && p1->tp != NULL ) )
			continue;

		for ( j = i + 1; j < f->num_pulses; j++ )
		{
			p2 = f->pulses[ j ];

			/* Skip checks for inactive pulses */

			if ( ! p2->is_active ||
				 ( f->self == PULSER_CHANNEL_TWT && p2->tp != NULL ) )
				continue;

			if ( p1->pos == p2->pos ||
				 ( p1->pos < p2->pos && p1->pos + p1->len > p2->pos ) ||
				 ( p2->pos < p1->pos && p2->pos + p2->len > p1->pos ) )
			{
				if ( ep385.auto_shape_pulses &&
					 f->self == PULSER_CHANNEL_PULSE_SHAPE )
				{
					if ( p1->sp != NULL && p2->sp != NULL &&
						 p1->sp->function != p2->sp->function )
						print( FATAL, "Shape pulses for pulses #%ld (function "
							   "'%s') and #%ld (function '%s') start to "
							   "overlap.\n", p1->sp->num,
							   p1->sp->function->name, p2->sp->num,
							   p2->sp->function->name );
					if ( p1->sp != NULL && p2->sp == NULL )
						print( FATAL, "Automatically created shape pulse for "
							   "pulse #%ld (function '%s') and SHAPE pulse "
							   "#%ld start to overlap.\n", p1->sp->num,
							   p1->sp->function->name, p2->num );
					else if ( p1->sp == NULL && p2->sp != NULL )
						print( FATAL, "Automatically created shape pulse for "
							   "pulse #%ld (function '%s') and SHAPE pulse "
							   "#%ld start to overlap.\n", p2->sp->num,
							   p2->sp->function->name, p1->num );
				}
				else
					print( FATAL, "Pulses #%ld and #%ld (function '%s') "
						   "start to overlap.\n", p1->num, p2->num, f->name );

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

			if ( shape_p->pos <= defense_p->pos &&
				 shape_p->pos + shape_p->len + ep385.shape_2_defense >
				 defense_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					print( FATAL, "Distance between PULSE_SHAPE pulse %s#%ld "
						   "and DEFENSE pulse #%ld got shorter than %s.\n",
						   shape_p->sp ? "for pulse " : "",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   ep385_pticks( ep385.shape_2_defense ) );
					THROW( EXCEPTION );
				}

				if ( ep385_IN_SETUP )
					print( SEVERE, "Distance between PULSE_SHAPE pulse %s#%ld "
						   "and DEFENSE pulse #%ld is shorter than %s.\n",
						   shape_p->sp ? "for pulse " : "",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   defense_p->num,
						   ep385_pticks( ep385.shape_2_defense ) );
				else if ( ep385.shape_2_defense_too_near == 0 )
					print( SEVERE, "Distance between PULSE_SHAPE pulse %s#%ld "
						   "and DEFENSE pulse #%ld got shorter than %s.\n",
						   shape_p->sp ? "for pulse " : "",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   defense_p->num,
						   ep385_pticks( ep385.shape_2_defense ) );

				ep385.shape_2_defense_too_near++;
			}

			if ( defense_p->pos < shape_p->pos &&
				 defense_p->pos + defense_p->len + ep385.defense_2_shape >
				 shape_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					print( FATAL, "Distance between DEFENSE pulse #%ld and "
						   "PULSE_SHAPE pulse %s#%ld got shorter than %s.\n",
						   defense_p->num,
						   shape_p->sp ? "for pulse " : "",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   ep385_pticks( ep385.defense_2_shape ) );
					THROW( EXCEPTION );
				}

				if ( ep385_IN_SETUP )
					print( SEVERE, "Distance between DEFENSE pulse #%ld and "
						   "PULSE_SHAPE pulse %s#%ld is shorter than %s.\n",
						   defense_p->num,
						   shape_p->sp ? "for pulse " : "",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   ep385_pticks( ep385.defense_2_shape ) );
				else if ( ep385.defense_2_shape_too_near == 0 )
					print( SEVERE, "Distance between DEFENSE pulse #%ld and "
						   "PULSE_SHAPE pulse %s#%ld got shorter than %s.\n",
						   defense_p->num,
						   shape_p->sp ? "for pulse " : "",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   ep385_pticks( ep385.defense_2_shape ) );

				ep385.defense_2_shape_too_near++;
			}
		}
	}
}


/*------------------------------------------------------------------------*/
/* Function is called after the test run and experiments to reset all the */
/* variables describing the state of the pulser to their initial values.  */
/*------------------------------------------------------------------------*/

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
		/* After the test run check if the pulse has been used at all, send
		   a warning and delete it if it hasn't (unless we haven't ben told
		   to keep all pulses, even unused ones) */

		if ( FSC2_MODE != EXPERIMENT &&
			 ! p->has_been_active && ! ep385.keep_all )
		{
			print( WARN, "Pulse #%ld is never used.\n", p->num );
			p = ep385_delete_pulse( p, SET );
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

			for ( ch->num_active_pulses = 0, m = 0;
				  ( p = pm_entry[ m ] ) != NULL; m++ )
			{
				if ( p->is_active )
					continue;

				pp = ch->pulse_params + ch->num_active_pulses++;

				pp->pos = p->pos + f->delay;
				pp->len = p->len;
				pp->pulse = p;

				/* Automatically extend pulses for which a shape pulse has
				   been created a bit */

				if ( p->function->self != PULSER_CHANNEL_PULSE_SHAPE &&
					 p->sp != NULL )
				{
					pp->pos -= p->function->left_shape_padding;
					pp->len +=   p->function->left_shape_padding
							   + p->function->right_shape_padding;
				}

				/* Extend TWT pulses that were automatically created */

				if ( p->function->self == PULSER_CHANNEL_TWT &&
					 p->tp != NULL )
				{
					pp->pos -= p->tp->function->left_twt_padding;
					pp->len +=   p->tp->function->left_twt_padding
							   + p->tp->function->right_twt_padding;
				}
			}

			qsort( ch->pulse_params, ch->num_active_pulses,
				   sizeof *ch->pulse_params, ep385_pulse_compare );

			ep385_shape_padding_check_1( ch );
			if ( ch->function->self == PULSER_CHANNEL_TWT )
				ep385_twt_padding_check( ch );
		}
	}

	ep385_shape_padding_check_2( );
}


/*--------------------------------------------------------------------------*/
/* Checks if shape padding can be set correctly for all pulses of a channel */
/*--------------------------------------------------------------------------*/

void ep385_shape_padding_check_1( CHANNEL *ch )
{
	PULSE_PARAMS *pp, *ppp;
	int i;


	if ( ch->function->self == PULSER_CHANNEL_PULSE_SHAPE ||
		 ! ch->function->uses_auto_shape_pulses ||
		 ch->num_active_pulses == 0 )
		return;

	/* Check that first pulse don't starts to early */

	pp = ch->pulse_params;
	if ( pp->pos < 0 )
	{
		if ( ! pp->pulse->left_shape_warning )
		{
			print( SEVERE, "Pulse #%ld too early to set left shape padding "
				   "of %s.\n", pp->pulse->num,
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

	/* Shorten intermediate pulses if they would overlap */

	for ( i = 1; i < ch->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		if ( ppp->pos + ppp->len > pp->pos )
			ppp->len -= ppp->pos + ppp->len - pp->pos;
	}

	/* Check that last pulse isn't too long */

	pp = ch->pulse_params + ch->num_active_pulses - 1;
	if ( pp->pos + pp->len > MAX_PULSER_BITS )
	{
		if ( ! pp->pulse->right_shape_warning )
		{
			print( SEVERE, "Pulse #%ld too long to set right shape padding "
				   "of %s.\n", pp->pulse->num,
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


/*-----------------------------------------------------------------------*/
/* In the shape padding checks fo single channels it could not be tested */
/* if pulses with automatic shape padding in different channels would    */
/* overlap, which needs to be done here.                                 */
/*-----------------------------------------------------------------------*/

void ep385_shape_padding_check_2( void )
{
	CHANNEL *ch1, *ch2;
	FUNCTION *f1, *f2;
	int i, j, k, l, m, n;
	PULSE_PARAMS *pp1, *pp2;


	/* We need to check all pulses of functions that expect automatically
	   created shape pulses */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f1 = ep385.function + i;

		if ( ! f1->uses_auto_shape_pulses )
			continue;

		for ( j = 0; j < PULSER_CHANNEL_NUM_FUNC; j++ )
		{
			f2 = ep385.function + j;

			if ( ! f2->uses_auto_shape_pulses )
			continue;

			/* Single channels already have been checked */

			if ( f1 == f2 && f1->num_channels == 1 )
				continue;

			for ( k = 0; k < f1->num_channels; k++ )
			{
				ch1 = f1->channel[ k ];

				for ( l = ( f1 == f2 ) ? k + 1 : 0; l < f2->num_channels; l++ )
				{
					ch2 = f2->channel[ l ];

					for ( m = 0; m < ch1->num_active_pulses; m++ )
					{
						pp1 = ch1->pulse_params + m;

						for ( n = 0; n < ch2->num_active_pulses; n++ )
						{
							pp2 = ch2->pulse_params + n;

							if ( ( pp1->pos <= pp2->pos &&
								   pp1->pos + pp1->len > pp2->pos ) ||
								 ( pp1->pos > pp2->pos &&
								   pp1->pos < pp2->pos + pp2->len ) )
							{
								print( FATAL, "Distance between pulses #%ld "
									   "and #%ld too short to set shape "
									   "padding.\n", pp1->pulse->num,
									   pp2->pulse->num );
								THROW( EXCEPTION );
							}
						}
					}
				}
			}
		}
	}
}


/*-----------------------------------------------------------------*/
/* Here we check that TWT pulses don't overlap (if at least one of */
/* them has been created automatically), if necessary shortening   */
/* or even eliminating TWT pulses, Then we again lengthen pulses   */
/* if the time between two TWT gets too short.                     */
/*-----------------------------------------------------------------*/

void ep385_twt_padding_check( CHANNEL *ch )
{
	PULSE_PARAMS *pp, *ppp;
	int i;


	fsc2_assert( ch->function->self == PULSER_CHANNEL_TWT );

	if ( ch->num_active_pulses == 0 )
		return;

	/* Check that first TWT pulse doesn't start too early (this only can
	   happen for automatically created pulses) */

	pp = ch->pulse_params;
	if ( pp->pos < 0 )
	{
		if ( ! pp->pulse->left_twt_warning )
		{
			print( SEVERE, "Pulse #%ld too early to set left padding of "
				   "%s for its TWT pulse.\n",
				   pp->pulse->tp->num,
				   ep385_pticks( pp->pulse->tp->function->left_twt_padding ) );
			pp->pulse->left_twt_warning = SET;
		}

		ep385.left_twt_warning++;
		pp->pulse->function->min_left_twt_padding =
					  l_min( pp->pulse->function->min_left_twt_padding,
							 pp->pulse->function->left_twt_padding + pp->pos );
		pp->len += pp->pos;
		pp->pos = 0;
	}

	/* Shorten intermediate pulses so they don't overlap - if necessary even
	   remove a pulse if it's is completely within the area of its
	   predecessor */

	for ( i = 1; i < ch->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		/* Check for negative start times (can only happen for automatically
		   created pulses) */

		if ( pp->pos < 0 )
		{
			if ( ! pp->pulse->left_twt_warning )
			{
				print( SEVERE, "Pulse #%ld too early to set left padding of "
					   "%s for its TWT pulse.\n",
					   pp->pulse->tp->num, ep385_pticks(
								 pp->pulse->tp->function->left_twt_padding ) );
				pp->pulse->left_twt_warning = SET;
			}

			ep385.left_twt_warning++;
			pp->pulse->function->min_left_twt_padding =
					  l_min( pp->pulse->function->min_left_twt_padding,
							 pp->pulse->function->left_twt_padding + pp->pos );
			pp->len += pp->pos;
			pp->pos = 0;
		}

		if ( ppp->pos == pp->pos )
		{
			if ( ppp->len <= pp->len )
				memmove( ppp, pp,
						 ( ch->num_active_pulses-- - i-- ) * sizeof *pp );
			else
				memmove( pp, pp + 1,
						 ( ch->num_active_pulses-- - --i ) * sizeof *pp );
			pp = ppp;
		}
		else if ( ppp->pos + ppp->len > pp->pos )
		{
			if ( ppp->pos + ppp->len >= pp->pos + pp->len )
			{
				memmove( pp, pp + 1,
						 ( --ch->num_active_pulses - --i ) * sizeof *pp );
				pp = ppp;
			}
			else
				ppp->len = pp->pos - ppp->len;
		}
	}

	/* Check that the last TWT pulse isn't too long (can only happen for
	   automatically created pulses) */

	pp = ch->pulse_params + ch->num_active_pulses - 1;
	if ( pp->pos + pp->len > MAX_PULSER_BITS )
	{
		if ( pp->pulse->tp != NULL && ! pp->pulse->right_twt_warning )
		{
			print( SEVERE, "Pulse #%ld too long to set right padding of %s "
				   "for its TWT pulse.\n", pp->pulse->tp->num, ep385_pticks(
								pp->pulse->tp->function->right_twt_padding ) );
			pp->pulse->right_twt_warning = SET;
		}

		ep385.right_twt_warning++;
		pp->pulse->function->min_right_twt_padding =
			l_min( pp->pulse->function->min_right_twt_padding,
				   pp->pulse->function->right_twt_padding + pp->pos );
		pp->len = MAX_PULSER_BITS - pp->pos;
	}

	/* Finally check if the pulses are far enough apart - for automatically
	   created pulses lengthen them if necessary, for user created pulses
	   print a warning */

	pp = ch->pulse_params;

	for ( i = 1; i < ch->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		if ( pp->pos - ( ppp->pos + ppp->len )
			 							   < ep385.minimum_twt_pulse_distance )
		{
			if ( pp->pulse->tp == NULL && ppp->pulse->tp == NULL )
			{
				if ( ep385.twt_distance_warning++ != 0 )
					print( SEVERE, "Distance between TWT pulses #%ld and #%ld "
						   "is smaller than %s.\n", ppp->pulse->num,
						   pp->pulse->num, ep385_pticks(
										  ep385.minimum_twt_pulse_distance ) );
			}
			else
				ppp->len = pp->pos - ppp->pos;
		}
	}
}


/*------------------------------------------------*/
/* Function deletes a pulse and returns a pointer */
/* to the next pulse in the pulse list.           */
/*------------------------------------------------*/

PULSE *ep385_delete_pulse( PULSE *p, bool warn )
{
	PULSE *pp;
	int i;


	/* If the pulse has an associated shape pulse delete it */

	if ( p->sp )
	{
		if ( p->sp->function->self == PULSER_CHANNEL_PULSE_SHAPE )
			ep385_delete_pulse( p->sp, warn );
		else
			p->sp->sp = NULL;
	}

	/* If the pulse has an associated TWT pulse also delete it */

	if ( p->tp )
	{
		if ( p->tp->function->self == PULSER_CHANNEL_TWT )
			ep385_delete_pulse( p->tp, warn );
		else
			p->tp->tp = NULL;
	}

	/* First we've got to remove the pulse from its functions pulse list (if
	   it already made it into the functions pulse list) */

	if ( p->is_function && p->function->num_pulses > 0 &&
		 p->function->pulses != NULL )
	{
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
											   p->function->num_pulses
											   * sizeof *p->function->pulses );
		else
		{
			p->function->pulses = PULSE_PP T_free( p->function->pulses );

			if ( warn )
				print( SEVERE, "Function '%s' isn't used at all because all "
					   "its pulses are never used.\n", p->function->name );
			p->function->is_used = UNSET;
		}
	}

	/* Now remove the pulse from the pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;

	/* Special care has to be taken if this is the very last pulse... */

	if ( p == ep385_Pulses )
		ep385_Pulses = p->next;
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
	else
	{
		if ( ep385.dump_file != NULL )
			ep385_dump_channels( ep385.dump_file );
		if ( ep385.show_file != NULL )
			ep385_dump_channels( ep385.show_file );

		ep385_duty_check( );
	}

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
