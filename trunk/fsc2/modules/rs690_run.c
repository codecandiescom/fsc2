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


#include "rs690.h"


static bool rs690_update_pulses( bool flag );
static void rs690_pulse_check( FUNCTION *f );
static void rs690_defense_shape_check( FUNCTION *shape );
static void rs690_commit( bool flag );
static void rs690_make_fs( FS *start_fs );
static void rs690_populate_fs( FS *start_fs );
static void rs690_check_fs( void );
static void	rs690_correct_fs_for_8ns( void );
static void	rs690_correct_fs_for_4ns( void );
static FS *rs690_append_fs( Ticks pos );
static FS *rs690_insert_fs( FS *at, Ticks pos );
static void rs690_delete_fs_successor( FS *n );



/*-------------------------------------------------------------------------*/
/* Function is called in the experiment after pulses have been changed to  */
/* update the pulser accordingly. No checking has to be done except in the */
/* test run.                                                               */
/*-------------------------------------------------------------------------*/

bool rs690_do_update( void )
{
	bool restart = UNSET;
	bool state;


	/* Resort the pulses, check that the new pulse settings are reasonable
	   and finally commit all changes */

	if ( rs690.is_running )
	{
		restart = SET;
		if ( FSC2_MODE == EXPERIMENT )
			rs690_run( STOP );
	}

	state = rs690_update_pulses( FSC2_MODE == TEST );

	rs690.needs_update = UNSET;
	if ( restart && FSC2_MODE == EXPERIMENT )
		rs690_run( START );

	return state;
}


/*--------------------------------------------------------------------------*/
/* This function sorts the pulses and checks that the pulses don't overlap. */
/*--------------------------------------------------------------------------*/

static bool rs690_update_pulses( bool flag )
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
		f = rs690.function + i;

		/* Nothing to be done for unused functions */

		if ( ! f->is_used )
			continue;

		/* Check that pulses don't start to overlap */

		rs690_pulse_check( f );

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
				   sizeof *ch->pulse_params, rs690_pulse_compare );

			rs690_shape_padding_check_1( ch );
			if ( ch->function->self == PULSER_CHANNEL_TWT )
				rs690_twt_padding_check( ch );

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
			rs690.needs_update = SET;

			/* If the following tests fail stop immediately when doing a test
			   run but during a real experiment instead just reset the pulses
			   to their old values and return FAIL.*/

			TRY
			{
				/* Check that defense and shape pulses don't get to near to
				   each other (this checks are done when either one of the
				   minimum distances has been set or TWT or TWT_GATE pulses
				   are used) */

				if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
					 rs690.function[ PULSER_CHANNEL_DEFENSE ].is_used &&
					 ( rs690.is_shape_2_defense || rs690.is_defense_2_shape ||
					   rs690.function[ PULSER_CHANNEL_TWT ].is_used ||
					   rs690.function[ PULSER_CHANNEL_TWT_GATE ].is_used ) )
					rs690_defense_shape_check( f );

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

				for ( p = rs690_Pulses; p != NULL; p = p->next )
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

	rs690_shape_padding_check_2( );

	if ( rs690.needs_update )
		rs690_commit( flag );

	return OK;
}


/*-------------------------------------------------------------------*/
/* This function simply checks that no pulses of a function overlap. */
/*-------------------------------------------------------------------*/

static void rs690_pulse_check( FUNCTION *f )
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

			/* Skip checks for inactive pulses and automatically generated
			   TWT pulses */

			if ( ! p2->is_active ||
				( f->self == PULSER_CHANNEL_TWT && p2->tp != NULL ) )
				continue;

			if ( p1->pos == p2->pos ||
				 ( p1->pos < p2->pos && p1->pos + p1->len > p2->pos ) ||
				 ( p2->pos < p1->pos && p2->pos + p2->len > p1->pos ) )
			{
				if ( rs690.auto_shape_pulses &&
					 f->self == PULSER_CHANNEL_PULSE_SHAPE )
				{
					if ( p1->sp != NULL && p2->sp != NULL &&
						 p1->sp->function != p2->sp->function )
						print( FATAL, "Shape pulses for pulses #%ld function "
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
					print( FATAL, "Pulses #%ld and #%ld of function '%s' "
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

static void rs690_defense_shape_check( FUNCTION *shape )
{
	FUNCTION *defense = rs690.function + PULSER_CHANNEL_DEFENSE;
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
				 shape_p->pos + shape_p->len + rs690.shape_2_defense >
				 defense_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					if ( shape_p->sp == NULL )
						print( FATAL, "Distance between PULSE_SHAPE pulse "
							   "#%ld and DEFENSE pulse #%ld got shorter than "
							   "%s.\n", shape_p->num, defense_p->num,
							   rs690_pticks( rs690.shape_2_defense ) );
					else
						print( FATAL, "Distance between shape pulse for pulse "
							   "#%ld (function '%s') and DEFENSE pulse #%ld "
							   "got shorter than %s.\n", shape_p->sp->num,
							   shape_p->sp->function->name, defense_p->num,
							   rs690_pticks( rs690.shape_2_defense ) );
					THROW( EXCEPTION );
				}

				if ( rs690.shape_2_defense_too_near == 0 )
				{
					if ( shape_p->sp == NULL )
						print( SEVERE, "Distance between PULSE_SHAPE pulse "
							   "#%ld and DEFENSE pulse #%ld got shorter than "
							   "%s.\n", shape_p->num, defense_p->num,
							   rs690_pticks( rs690.shape_2_defense ) );
					else
						print( SEVERE, "Distance between shape pulse for "
							   "pulse #%ld (function '%s') and DEFENSE pulse "
							   "#%ld got shorter than %s.\n", shape_p->sp->num,
							   shape_p->sp->function->name, defense_p->num,
							   rs690_pticks( rs690.shape_2_defense ) );
				}

				rs690.shape_2_defense_too_near++;
			}

			if ( defense_p->pos < shape_p->pos &&
				 defense_p->pos + defense_p->len + rs690.defense_2_shape >
				 shape_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					if ( shape_p->sp == NULL )
						print( FATAL, "Distance between DEFENSE pulse #%ld "
							   "and PULSE_SHAPE pulse #%ld got shorter than "
							   "%s.\n", defense_p->num, shape_p->num,
							   rs690_pticks( rs690.defense_2_shape ) );
					else
						print( FATAL, "Distance between DEFENSE pulse #%ld "
							   "and shape pulse for pulse #%ld (function "
							   "'%s') got shorter than %s.\n", defense_p->num,
							   shape_p->sp->num, shape_p->sp->function->name,
							   rs690_pticks( rs690.defense_2_shape ) );
					THROW( EXCEPTION );
				}

				if ( rs690.defense_2_shape_too_near == 0 )
				{
					if ( shape_p->sp == NULL )
						print( SEVERE, "Distance between DEFENSE pulse #%ld "
							   "and PULSE_SHAPE pulse #%ld got shorter than "
							   "%s.\n", defense_p->num, shape_p->num,
							   rs690_pticks( rs690.defense_2_shape ) );
					else
						print( SEVERE, "Distance between DEFENSE pulse #%ld "
							   "and shape pulse for pulse #%ld (function "
							   "'%s') got shorter than %s.\n", defense_p->num,
							   shape_p->sp->num, shape_p->sp->function->name,
							   rs690_pticks( rs690.defense_2_shape ) );
				}

				rs690.defense_2_shape_too_near++;
			}
		}
	}
}


/*------------------------------------------------------------------*/
/* Function is called after the test run to reset all the variables */
/* describing the state of the pulser to their initial values.      */
/*------------------------------------------------------------------*/

void rs690_full_reset( void )
{
	int i, j, m;
	PULSE *p = rs690_Pulses;
	FUNCTION *f;
	CHANNEL *ch;
	PULSE **pm_entry;
	PULSE_PARAMS *pp;


	rs690_cleanup_fs( );

	while ( p != NULL )
	{
		/* First we check if the pulse has been used at all, send a warning
           and delete it if it hasn't (unless we haven't ben told to keep
		   all pulses, even unused ones) */

		if ( ! p->has_been_active && ! rs690.keep_all )
		{
			print( WARN, "Pulse #%ld is never used.\n", p->num );
			p = rs690_delete_pulse( p, SET );
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

	rs690.is_running = rs690.has_been_running;

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;
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
			{
				if ( p->is_active )
					continue;

				pp = ch->pulse_params + ch->num_active_pulses++;

				pp->pos = p->pos + f->delay;
				pp->len = p->len;
				pp->pulse = p;

				/* Extend pulses for which a shape pulse has been created
				   automatically a bit */

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
				   sizeof *ch->pulse_params, rs690_pulse_compare );

			rs690_shape_padding_check_1( ch );
			if ( ch->function->self == PULSER_CHANNEL_TWT )
				rs690_twt_padding_check( ch );
		}
	}

	rs690_shape_padding_check_2( );
}


/*--------------------------------------------------------------------------*/
/* Checks if shape padding can be set correctly for all pulses of a channel */
/*--------------------------------------------------------------------------*/

void rs690_shape_padding_check_1( CHANNEL *ch )
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
				   rs690_pticks( ch->function->left_shape_padding ) );
			pp->pulse->left_shape_warning = SET;
		}

		rs690.left_shape_warning++;
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
}


/*-----------------------------------------------------------------------*/
/* In the shape padding checks fo single channels it could not be tested */
/* if pulses with automatic shape padding in different channels would    */
/* overlap, which needs to be done here.                                 */
/*-----------------------------------------------------------------------*/

void rs690_shape_padding_check_2( void )
{
	CHANNEL *ch1, *ch2;
	FUNCTION *f1, *f2;
	int i, j, k, l, m, n;
	PULSE_PARAMS *pp1, *pp2;


	/* We need to check all pulses of functions that expect automatically
	   created shape pulses */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f1 = rs690.function + i;

		if ( ! f1->uses_auto_shape_pulses )
			continue;

		for ( j = 0; j < PULSER_CHANNEL_NUM_FUNC; j++ )
		{
			f2 = rs690.function + j;

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
						}     /* loop over all pulses of inner channel */
					}     /* loop over all pulses of outer channel */
				}     /* loop over all channels of inner function */
			}     /* loop over all channels of outer function */
		}     /* inner loop over all functions */
	}     /* outer loop over all functions */
}


/*-----------------------------------------------------------------*/
/* Here we check that TWT pulses don't overlap (if at least one of */
/* them has been created automatically), if necessary shortening   */
/* or even eliminating TWT pulses, Then we again lengthen pulses   */
/* if the time between two TWT gets too short.                     */
/*-----------------------------------------------------------------*/

void rs690_twt_padding_check( CHANNEL *ch )
{
	PULSE_PARAMS *pp, *ppp;
	int i;


	fsc2_assert( ch->function->self == PULSER_CHANNEL_TWT );

	if ( ch->num_active_pulses == 0 )
		return;

	/* Check that first TWT pulse doesn't start too early (this only can
	   happen for automatically created pulses) */

	pp = ch->pulse_params;
	if ( pp->pulse->tp != NULL && pp->pos < 0 )
	{
		if ( ! pp->pulse->left_twt_warning )
		{
			print( SEVERE, "Pulse #%ld too early to set left padding of "
				   "%s for its TWT pulse.\n",
				   pp->pulse->tp->num,
				   rs690_pticks( pp->pulse->tp->function->left_twt_padding ) );
			pp->pulse->left_twt_warning = SET;
		}

		rs690.left_twt_warning++;
		pp->pulse->function->min_left_twt_padding =
					  l_min( pp->pulse->function->min_left_twt_padding,
							 pp->pulse->function->left_twt_padding + pp->pos );
		pp->len += pp->pos;
		pp->pos = 0;
	}

	/* Shorten intermediate pulses so they don't overlap - if necessary even
	   remove pulses if a pulse is completely within the area of its
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
					   pp->pulse->tp->num, rs690_pticks(
								 pp->pulse->tp->function->left_twt_padding ) );
				pp->pulse->left_twt_warning = SET;
			}

			rs690.left_twt_warning++;
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
				ppp->len = pp->pos - ppp->pos;
		}
	}

	/* Finally check if the pulses are too far enough apart - for automatically
	   created pulses lengthen them if necessary, for user created pulses
	   print a warning */

	pp = ch->pulse_params;

	for ( i = 1; i < ch->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		if ( pp->pos - ( ppp->pos + ppp->len )
			 							   < rs690.minimum_twt_pulse_distance )
		{
			if ( pp->pulse->tp == NULL && ppp->pulse->tp == NULL )
			{
				if ( rs690.twt_distance_warning++ != 0 )
					print( SEVERE, "Distance between TWT pulses #%ld and #%ld "
						   "is smaller than %s.\n", ppp->pulse->num,
						   pp->pulse->num, rs690_pticks(
										  rs690.minimum_twt_pulse_distance ) );
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

PULSE *rs690_delete_pulse( PULSE *p, bool warn )
{
	PULSE *pp;
	int i;


	/* If the pulse has an associated shape pulse delete it */

	if ( p->sp )
	{
		if ( p->sp->function->self == PULSER_CHANNEL_PULSE_SHAPE )
			rs690_delete_pulse( p->sp, warn );
		else
			p->sp->sp = NULL;
	}

	/* If the pulse has an associated TWT pulse also delete it */

	if ( p->tp )
	{
		if ( p->tp->function->self == PULSER_CHANNEL_TWT )
			rs690_delete_pulse( p->tp, warn );
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

	if ( p == rs690_Pulses )
		rs690_Pulses = p->next;
	T_free( p );

	return pp;
}


/*-------------------------------------------------------------------*/
/* Changes the pulse pattern in all channels so that the data in the */
/* pulser get in sync with the its internal representation.          */
/* 'flag' is set during the test run.                                */
/*-------------------------------------------------------------------*/

static void rs690_commit( bool flag )
{
	PULSE *p;
	int i, j;
	FUNCTION *f;


	/* Only really set the pulses while doing an experiment */

	rs690_channel_setup( flag );

	if ( flag )
	{
		if ( rs690.dump_file != NULL )
			rs690_dump_channels( rs690.dump_file );
		if ( rs690.show_file != NULL )
			rs690_dump_channels( rs690.show_file );

		rs690_duty_check( );
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;

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


/*--------------------------------------------------------------------*/
/* We're using the pulser in timing simulator mode. In this mode each */
/* output state of the pulser during a pulse sequence is defined by   */
/* the data to be output during the state and the length of time the  */
/* the data have to be output. If e.g. there is just one pulse,       */
/* starting at a time T and with a length of L, we would have three   */
/* output states, the first one for the time between 0 and T, a       */
/* second one for the time between T and T and T + L (i.e. the time   */
/* where the pulse is on) and a third one for the time after the      */
/* pulse. More complicated pulse sequences can be split in the same   */
/* manner, with a new output state whenever one of the pulses gets    */
/* switched on or off.                                                */
/* In the following an internal representation of these states gets   */
/* created in the form of a linked list of pulser state structures of */
/* type FS. Each of these structures has an member for its position   */
/* and length and a set of fields that represent the data to be out-  */
/* put (each field corresponds to a 'Field' of the pulser of which    */
/* the individual bits can be assigned to the channels of the output  */
/* connectors).                                                       */
/* Things get a bit more complicated because the pulser needs a state */
/* of zero for all channels at the start or strange things happen (i. */
/* e. when starting the pulser but not triggering it yet all initial  */
/* states are output for a longer time etc.)                          */
/*--------------------------------------------------------------------*/

#define START_OFFSET 1

void rs690_channel_setup( bool flag )
{
	int i;


	/* Switch to a new set of FS structures */

	if ( rs690.old_fs != NULL )
	{
		while ( rs690.old_fs->next != NULL )
			rs690_delete_fs_successor( rs690.old_fs );
		T_free( rs690.old_fs );
	}

	rs690.old_fs = rs690.new_fs;
	rs690.old_fs_count = rs690.new_fs_count;
	rs690.last_old_fs = rs690.last_new_fs;

	rs690.new_fs = NULL;
	rs690.new_fs_count = 0;
	rs690.last_new_fs = NULL;

	if ( rs690.timebase_type != TIMEBASE_16_NS )
	{
		for ( i = 0; i < 4 * NUM_HSM_CARDS; i++ )
			if ( rs690.timebase_type == TIMEBASE_8_NS )
				rs690_default_fields[ i ] &= 0xFF;
			else
				rs690_default_fields[ i ] &= 0xF;
	}

	TRY
	{
		rs690.new_fs = rs690_append_fs( - START_OFFSET );
		rs690.new_fs->len = 1;

		rs690_append_fs( 0 );

		rs690_make_fs( rs690.new_fs->next );
		rs690_populate_fs( rs690.new_fs->next );

		rs690_check_fs( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		rs690_cleanup_fs( );
		RETHROW( );
	}

	if ( ! flag )
		rs690_set_channels( );
}


/*------------------------------------------------------------*/
/* Runs over all active pulses and creates a new FS structure */
/* for each change of the state of one of the pulses.         */
/*------------------------------------------------------------*/

static void rs690_make_fs( FS *start_fs )
{
	FUNCTION *f;
	CHANNEL *ch;
	int i, j, k;
	PULSE_PARAMS *pp;
	FS *n;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;

		if ( ! f->is_needed && f->num_channels == 0 )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			if ( ch->num_active_pulses == 0 )
				continue;

			/* We run twice over all pulses and FS structures - first for
			   the state change at the begin of a pulse, then for the end
			   of a pulse. */

			for ( n = start_fs, pp = ch->pulse_params, k = 0;
				  k < ch->num_active_pulses; pp++, k++ )
			{
				for ( ; n != NULL; n = n->next )
					if ( pp->pos > n->pos )
					{
						if ( n->next == NULL )
						{
							n = rs690_append_fs( pp->pos );
							break;
						}

						if ( n->next != NULL && n->next->pos <= pp->pos )
							continue;

						n = rs690_insert_fs( n, pp->pos );
						break;
					}
					else if ( pp->pos == n->pos )
						break;
			}

			for ( n = start_fs, pp = ch->pulse_params, k = 0;
				  k < ch->num_active_pulses; pp++, k++ )
			{
				for ( ; n != NULL; n = n->next )
					if ( pp->pos + pp->len > n->pos )
					{
						if ( n->next == NULL )
						{
							n = rs690_append_fs( pp->pos + pp->len );
							break;
						}

						if ( n->next != NULL &&
							 n->next->pos <= pp->pos + pp->len )
							continue;

						n = rs690_insert_fs( n, pp->pos + pp->len );
						break;
					}
					else if ( pp->pos + pp->len == n->pos )
						break;
			}
		}
	}

	/* Set the pulse lengths */

	for ( n = start_fs; n->next != NULL; n = n->next )
		n->len = n->next->pos - n->pos;
}


/*-----------------------------------------------------------------*/
/* Loops over all pulses a second time, setting the data fields of */
/* all the FS structures to reflect the pulse states.              */
/*-----------------------------------------------------------------*/

static void rs690_populate_fs( FS *start_fs )
{
	FUNCTION *f;
	CHANNEL *ch;
	int i, j, k;
	PULSE_PARAMS *pp;
	FS *n;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rs690.function + i;

		if ( ! f->is_needed && f->num_channels == 0 )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			if ( ch->num_active_pulses == 0 )
				continue;

			for ( n = start_fs, pp = ch->pulse_params, k = 0;
				  k < ch->num_active_pulses; pp++, k++ )
				for ( ; n != NULL; n = n->next )
				{
					if ( pp->pos > n->pos )
						continue;

					if ( pp->pos + pp->len <= n->pos )
						break;

					if ( ! f->is_inverted )
						n->fields[ ch->field ] |= 1 << ch->bit;
					else
						n->fields[ ch->field ] &= ~ ( 1 << ch->bit );
					continue;
				}
		}
	}
}


/*----------------------------------------------------------------*/
/* Runs through the list of FS structures and does some cosmetics */
/*----------------------------------------------------------------*/

void rs690_check_fs( void )
{
	FS *nn;
	static FS *n = NULL;
	static char *s;


	/* The very last FS, standing for the time following the pulses hasn't
	   got a length yet. If a repetition time has been given we set it to
	   its number of ticks, otherwise to 1 tick. While we're at it we
	   also need to check if the pulse sequence hasn't gotten longer than
	   the repetition time. */

	for ( n = rs690.new_fs; n->next != NULL; n = n->next )
		/* empty */ ;

	if ( rs690.is_repeat_time )
	{
		if ( n->pos + START_OFFSET > rs690.repeat_time )
		{
			TRY
			{
				s = T_strdup( rs690_pticks( n->pos + START_OFFSET ) );
				print( FATAL, "Pulse sequence is longer (%s) than the "
					   "repetition time of %s.\n",
					   s, rs690_pticks( rs690.repeat_time ) );
				s = CHAR_P T_free( s );
				THROW( EXCEPTION );
			}
			OTHERWISE
			{
				s = CHAR_P T_free( s );
				RETHROW( );
			}
		}
		else
			n->len = Ticks_max( 1, rs690.repeat_time - n->pos );
	}
	else      /* no repetition time has been set */
		n->len = 1;

	/* If the fields of two adjacent FS structures have the same bit pattern
	   combine them (this can happen if e.g. the distance between two pulses
	   is zero). */

	for ( n = rs690.new_fs; n != NULL && n->next != NULL; n = n->next )
		while ( n->next != NULL &&
				! memcmp( n->fields, n->next->fields, sizeof n->fields ) )
		{
			n->len += n->next->len;
			rs690_delete_fs_successor( n );
			rs690.new_fs_count--;
		}

	/* Now for the shorter time bases we have to make sure the FS structures
	   lengths aren't too short */

	if ( rs690.timebase_type == TIMEBASE_8_NS )
		rs690_correct_fs_for_8ns( );
	else if ( rs690.timebase_type == TIMEBASE_4_NS )
		rs690_correct_fs_for_4ns( );

	/* Check that none of the FS structures is longer than the maximum allowed
	   time (MAX_TICKS_PER_ENTRY), otherwise split them up in as many shorter
	   ones as needed. The only exception is the very last, it represents the
	   repetition time and with this we've got to deal when setting up the
	   pulser for real. */

	for ( n = rs690.new_fs; n->next != NULL; n = n->next )
		if ( n->len > MAX_TICKS_PER_ENTRY )
		{
			nn = rs690_insert_fs( n, n->pos + MAX_TICKS_PER_ENTRY );
			nn->len = n->len - MAX_TICKS_PER_ENTRY;
			n->len = MAX_TICKS_PER_ENTRY;
			memcpy( nn->fields, n->fields, sizeof n->fields );
		}

	rs690.last_new_fs = n;

	/* Finally, if the pulse sequence is that complicated that more than the
	   maximum number of words we have for storing table entries would be
	   needed we have to bail out. */

	if ( rs690.new_fs_count > TS_MAX_WORDS - 2 )
	{
		print( FATAL, "Pulse sequence is too complicated to create with "
			   "this device.\n" );
		THROW( EXCEPTION );
	}
}


/*--------------------------------------------------------------------------*/
/* The basic unit of table entries are 16 bit words, where for a time base  */
/* of 16 ns each bit represents a channel of one of the output connectors.  */
/* For a time base of 8 ns only 8 channels can be associated with a word,   */
/* and first the high and then the low byte of a word is output to these 8  */
/* channels) to be able to clock out data at a 125 MHz rate while new words */
/* can be loaded only in 16 ns).                                            */
/* Until now the fields in the FS structures were treated as needed for a   */
/* 16 ns time base. But now we have adjust for structures that are only 1   */
/* Tick long - they need to be merged with another Tick from the following  */
/* structure.                                                               */
/*--------------------------------------------------------------------------*/

static void rs690_correct_fs_for_8ns( void )
{
	FS *n, *p;
	int i;


	for ( p = rs690.new_fs, n = p->next; n != NULL; n = n->next )
	{
		/* If there was still room in the previous FS move the first slice
		   of the current FS into it */

		if ( p->len < 2 )
		{
			for ( i = 0; i <= rs690.last_used_field; i++ )
				p->fields[ i ] = ( p->fields[ i ] << 8 ) | 
								 ( n->fields[ i ] & 0xff );
			p->len++;
			p->is_composite = SET;
			n->pos++;
			n->len--;
		}

		/* If the current FS was just one Tick long and this Tick got shifted
		   into the previous FS structure it's now empty, so delete it (we're
		   guaranteed that now FS strcuture will never have a length of 0
		   for any other reasons). */

		if ( n->len == 0 )
		{
			rs690_delete_fs_successor( p );
			rs690.new_fs_count--;
			n = p;
			continue;
		}

		p = n;
	}

	/* Make sure the last FS has an even length */

	if ( p->len & 1 )
		p->len++;
}


/*------------------------------------------------------------------*/
/* For a time base of 4 ns we have a similar problem as for an 8 ns */
/* time base (see above), but it gets even a bit more complicated.  */
/* For a 4 ns second time base each 16 bit word can be split into 4 */
/* nibbles for 4 consecutive output states of 4 channels (you can't */
/* have more than 4 channels per connector with a 4 ns time base to */
/* have a clock out frequency of 125 MHz while word loading happens */
/* at 62.5 MHz only).                                               */
/* If the length of an FS structure is at least 4 Ticks long we do  */
/* not have to do anything. But for shorter structures we need to   */
/* fill it with Ticks from the following structure.                 */
/*------------------------------------------------------------------*/

static void	rs690_correct_fs_for_4ns( void )
{
	FS *n, *p;
	int i, j;


	for ( p = rs690.new_fs, n = p->next; n != NULL; n = n->next )
	{
		/* If there is still room in the previous FS move as many Ticks
		   of the current FS into it as possible (but only while there
		   are still Ticks left in the current FS). */

		while ( p->len < 4 && n->len > 0 )
		{
			for ( i = 0; i <= rs690.last_used_field; i++ )
				p->fields[ i ] = ( p->fields[ i ] << 4 ) |
								 ( n->fields[ i ] & 0xf );
			p->len++;
			p->is_composite = SET;
			n->pos++;
			n->len--;
		}

		/* If all the Ticks of the current FS have been moved into the
		   previous FS it's now empty, so delete it and continue with
		   the next FS in the list. */

		if ( n->len == 0 )
		{
			rs690_delete_fs_successor( p );
			rs690.new_fs_count--;
			n = p;
			continue;
		}

		/* If the current FS is now shorter than 4 Ticks we have to prepare
		   it to become the next predecessor into which we're going to shift
		   Ticks from the next FS. */

		if ( n->len < 4 )
			for ( i = 0; i <= rs690.last_used_field; i++ )
				for ( j = 0; j < n->len - 1; j++ )
					n->fields[ i ] = ( n->fields[ i ] << 4 ) |
									 ( n->fields[ i ] & 0xF );

		p = n;
	}

	/* If very the last FS wasn't completely filled (i.e. hadn't at least 4
	   Ticks) fill it up with time slices with data for no pulses - this
	   might lead to a lengthening of the repetition time of up to three
	   clock cycles. */

	if ( p->len == 1 )             /* no pulses in this one anyway */
		p->len = 4;

	while ( p->len < 4 )
	{
		for ( i = 0; i <= rs690.last_used_field; i++ )
			p->fields[ i ] = ( p->fields[ i ] << 4 ) |
							 ( rs690_default_fields[ i ] & 0xf );	
		p->len++;
		p->is_composite = SET;
	}

	if ( p->len % 4 )
		p->len = ( p->len / 4 + 1 ) * 4;
}


/*------------------------------------------------------------------*/
/* Inserts a new FS structure into the linked list of FS structures */
/* immediately after the FS structure passed to the function. The   */
/* position mmeberof the new structure is initialized to the value  */
/* of the second argument and the field members are set to the      */
/* values of the rs690_default_fields array (which are the bit      */
/* patterns one needs for no pulses at all, taking into account the */
/* polarity of the channels).                                       */
/*------------------------------------------------------------------*/

static FS *rs690_insert_fs( FS *at, Ticks pos )
{
	FS *n;


	if ( rs690.new_fs == NULL )
		return rs690_append_fs( pos );

	n = FS_P T_malloc( sizeof *n );
	rs690.new_fs_count++;

	n->next = at->next;
	at->next = n;

	n->len = 0;
	n->pos = pos;
	n->is_composite = UNSET;
	memcpy( n->fields, rs690_default_fields, sizeof n->fields );

	return n;
}


/*----------------------------------------------------------------*/
/* Appends a new FS structure to the end of the linked list of FS */
/* structures. Everything else as in the previous function.       */
/*----------------------------------------------------------------*/

static FS *rs690_append_fs( Ticks pos )
{
	FS *n, *p;


	if ( rs690.new_fs == NULL )
	{
		n = rs690.new_fs = FS_P T_malloc( sizeof *n );
		rs690.new_fs_count = 1;
	}
	else
	{
		for ( p = rs690.new_fs; p->next != NULL; p = p->next )
			/* empty */ ;

		n = p->next = FS_P T_malloc( sizeof *n );
		rs690.new_fs_count++;
	}

	n->next = NULL;
	n->len = 0;
	n->pos = pos;
	n->is_composite = UNSET;
	memcpy( n->fields, rs690_default_fields, sizeof n->fields );

	return n;
}


/*------------------------------------------------------------------*/
/* Deletes the FS structure pointed to by the next member of the FS */
/* structure passed to the function. Take care: the number of       */
/* structures as stored in rs690_new_fs_count or rs690.old_fs_count */
/* does *not* get changed!                                          */
/*------------------------------------------------------------------*/

static void rs690_delete_fs_successor( FS *n )
{
	FS *nn;

	if ( n->next == NULL )
		return;

	nn = n->next->next;
	T_free( n->next );
	n->next = nn;
}


/*-----------------------------------------------------*/
/* Removes all FS structures allocated for the program */
/*-----------------------------------------------------*/

void rs690_cleanup_fs( void )
{
	if ( rs690.new_fs != NULL )
	{
		while ( rs690.new_fs->next != NULL )
			rs690_delete_fs_successor( rs690.new_fs );
		rs690.new_fs = FS_P T_free( rs690.new_fs );
	}
	rs690.new_fs_count = 0;


	if ( rs690.old_fs != NULL )
	{
		while ( rs690.old_fs->next != NULL )
			rs690_delete_fs_successor( rs690.old_fs );
		rs690.old_fs = FS_P T_free( rs690.old_fs );
	}
	rs690.old_fs_count = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
