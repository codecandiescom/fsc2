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
static void ep385_channel_check( CHANNEL *ch );
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


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &ep385.function[ i ];

		/* Nothing to be done for unused functions */

		if ( ! f->is_used )
			continue;

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
					ch->pulse_params[ ch->num_active_pulses ].pos =
															 p->pos + f->delay;
					ch->pulse_params[ ch->num_active_pulses ].len = p->len;
					ch->pulse_params[ ch->num_active_pulses++ ].pulse = p;
				}			

			qsort( ch->pulse_params, ch->num_active_pulses,
				   sizeof *ch->pulse_params, ep385_pulse_compare );

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

			/* Otherwise we have to check that the pulses still fit within the
			   pulsers memory and don't start to overlap - if they do stop
			   immediately when doing a test run but during a real experiment
			   instead just reset the pulses to their old values and return
			   FAIL.*/

			TRY
			{
				ep385_channel_check( ch );
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

		/* Check that defense and shape pulses don't get to near to each
		   other (this checks are done when either one of the minimum
		   distances has been set or TWT or TWT_GATE pulses are used) */

		if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
			 ep385.function[ PULSER_CHANNEL_DEFENSE ].is_used &&
			 ( ep385.is_shape_2_defense || ep385.is_defense_2_shape ||
			   ep385.function[ PULSER_CHANNEL_TWT ].is_used ||
			   ep385.function[ PULSER_CHANNEL_TWT_GATE ].is_used ) )
			ep385_defense_shape_check( f );
	}

	ep385_commit( flag );

	return OK;
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void ep385_channel_check( CHANNEL *ch )
{
	PULSE_PARAMS *pp;
	int i;


	/* Check that there aren't more pulses per channel than the pulser can
	   deal with */

	if ( ch->num_active_pulses > MAX_PULSES_PER_CHANNEL )
	{
		print( FATAL, "More than %d pulses (%d) are required on channel %d "
			   "associated with function '%s'.\n", MAX_PULSES_PER_CHANNEL,
			   ch->num_active_pulses, ch->self + CHANNEL_OFFSET,
			   Function_Names[ ch->function->self ] );
		THROW( EXCEPTION );
	}

	for ( i = 0; i < ch->num_active_pulses; i++ )
	{
		pp = ch->pulse_params + i;
		if ( pp->pos + pp->len > MAX_PULSER_BITS )
		{
			print( FATAL, "Pulse %ld of function '%s' does not fit into the "
				   "pulsers memory.\n",
				   pp->pulse->num, Function_Names[ ch->function->self ] );
			THROW( EXCEPTION );
		}

		if ( i == ch->num_active_pulses - 1 )
			continue;

		if ( pp->pos + pp->len == ch->pulse_params[ i + 1 ].pos )
		{
			print( FATAL, "Pulses %ld and %ld of function '%s' are not "
				   "separated anymore.\n", pp->pulse->num,
				   ch->pulse_params[ i + 1 ].pulse->num,
				   Function_Names[ ch->function->self ] );
			THROW( EXCEPTION );
		}

		if ( pp->pos + pp->len > ch->pulse_params[ i + 1 ].pos )
		{
			print( FATAL, "Pulses %ld and %ld of function '%s' start to "
				   "overlap.\n",
				   pp->pulse->num, ch->pulse_params[ i + 1 ].pulse->num,
				   Function_Names[ ch->function->self ] );
			THROW( EXCEPTION );
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
					print( FATAL, "Distance between PULSE_SHAPE pulse %ld "
						   "and DEFENSE pulse %ld got shorter than %s.\n",
						   shape_p->num, defense_p->num, ep385_ptime(
							   ep385_ticks2double( ep385.shape_2_defense ) ) );
					THROW( EXCEPTION );
				}

				print( SEVERE, "Distance between PULSE_SHAPE pulse %ld "
					   "and DEFENSE pulse %ld got shorter than %s.\n",
					   shape_p->num, defense_p->num, ep385_ptime(
						   ep385_ticks2double( ep385.shape_2_defense ) ) );
				ep385.shape_2_defense_too_near = SET;

			}

			if ( defense_p->pos < shape_p->pos &&
				 defense_p->pos + defense_p->len + ep385.defense_2_shape >
				 shape_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					print( FATAL, "Distance between DEFENSE pulse %ld and "
						   "PULSE_SHAPE pulse %ld got shorter than %s.\n",
						   defense_p->num, shape_p->num, ep385_ptime(
							   ep385_ticks2double( ep385.defense_2_shape ) ) );
					THROW( EXCEPTION );
				}

				print( FATAL, "Distance between DEFENSE pulse %ld and "
					   "PULSE_SHAPE pulse %ld got shorter than %s.\n",
					   defense_p->num, shape_p->num, ep385_ptime(
						   ep385_ticks2double( ep385.defense_2_shape ) ) );
				ep385.defense_2_shape_too_near = SET;
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


	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't (unless we haven't ben told to keep
		   all pulses, even unused ones) */

		if ( ! p->has_been_active && ! ep385.keep_all )
		{
			print( WARN, "Pulse %ld is never used.\n", p->num );
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
					ch->pulse_params[ ch->num_active_pulses ].pos =
															 p->pos + f->delay;
					ch->pulse_params[ ch->num_active_pulses ].len = p->len;
					ch->pulse_params[ ch->num_active_pulses ].pulse = p;
					ch->num_active_pulses++;
				}

			qsort( ch->pulse_params, ch->num_active_pulses,
				   sizeof *ch->pulse_params, ep385_pulse_compare );

			if ( ch->num_active_pulses == ch->old_num_active_pulses )
			{
				for ( m = 0; m < ch->num_active_pulses; m++ )
				if ( ch->pulse_params[ m ].pos !=
					 ch->old_pulse_params[ m ].pos ||
					 ch->pulse_params[ m ].len !=
					 ch->old_pulse_params[ m ].len )
					break;

				if ( m == ch->num_active_pulses )
					continue;
			}
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
		f = &ep385.function[ i ];

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
