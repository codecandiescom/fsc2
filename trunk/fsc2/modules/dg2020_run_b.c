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


#include "dg2020_b.h"



#define type_ON( f )   ( ( f )->is_inverted ? LOW : HIGH )
#define type_OFF( f )  ( ( f )->is_inverted ? HIGH : LOW )

static void dg2020_shape_padding_check_1( FUNCTION *f );
static void dg2020_shape_padding_check_2( void );
static void dg2020_twt_padding_check( FUNCTION *f );
static void dg2020_defense_shape_check( FUNCTION *shape );


/*-------------------------------------------------------------------------*/
/* Function is called in the experiment after pulses have been changed to  */
/* update the pulser accordingly. Before pulses are set the new values are */
/* checked. If the check fails in the test run the program aborts while in */
/* the real experiment an error message is printed and the pulses are      */
/* reset to their original positions.                                      */
/*-------------------------------------------------------------------------*/

bool dg2020_do_update( void )
{
	bool state;


	if ( ! dg2020_is_needed )
		return OK;

	/* Resort the pulses and check that the new pulse settings are
	   reasonable and finally commit all changes */

	if ( ( state = dg2020_reorganize_pulses( FSC2_MODE == TEST ) ) &&
		 FSC2_MODE == EXPERIMENT )
		dg2020_update_data( );

	if ( FSC2_MODE == TEST )
	{
		if ( dg2020.dump_file != NULL )
			dg2020_dump_channels( dg2020.dump_file );
		if ( dg2020.show_file != NULL )
			dg2020_dump_channels( dg2020.show_file );
	}

	dg2020.needs_update = UNSET;
	return state;
}


/*---------------------------------------------------------------------*/
/* Function sorts the pulses and checks that the pulses don't overlap. */
/*---------------------------------------------------------------------*/

bool dg2020_reorganize_pulses( bool flag )
{
	static int i;
	int j;
	static FUNCTION *f;
	PULSE *p;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;

		/* Nothing to be done for unused functions */

		if ( ! f->is_used )
			continue;

		if ( f->num_pulses > 1 )
			qsort( f->pulses, f->num_pulses, sizeof( PULSE * ),
				   dg2020_start_compare );

		/* Check the new pulse positions and lengths, if they're not ok stop
		   program while doing the test run. For the real run just reset the
		   pulses to their previous positions and lengths and return FAIL. */

		TRY
		{
			dg2020_do_checks( f );
			TRY_SUCCESS;
		}
		CATCH( EXCEPTION )
		{
			for ( j = 0; j <= i; j++ )
			{
				f = dg2020.function + j;
				f->pulse_params = PULSE_PARAMS_P T_free( f->pulse_params );
			}

			if ( flag )
				THROW( EXCEPTION );

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

	}

	/* Send all the changes to the pulser */

	dg2020_shape_padding_check_2 ( );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		dg2020_commit( dg2020.function + i, flag );

	return OK;
}


/*-------------------------------------------------------------------------*/
/* Function is called after pulses have been changed to check if the new   */
/* settings are still ok, i.e. that they still fit into the pulsers memory */
/* and don't overlap.                                                      */
/*-------------------------------------------------------------------------*/

void dg2020_do_checks( FUNCTION *f )
{
	PULSE *p;
	PULSE_PARAMS *pp, *ppn;
	int i, j;


	if ( f->num_active_pulses == 0 )
		return;

	f->old_pulse_params = PULSE_PARAMS_P T_free( f->old_pulse_params );
	f->old_pulse_params = f->pulse_params;

	f->pulse_params = PULSE_PARAMS_P T_malloc( f->num_active_pulses *
											   sizeof *f->pulse_params );
	
	for ( j = 0, i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		if ( ! p->is_active )
			continue;

		pp = f->pulse_params + j++;

		pp->pulse = p;
		p->old_pp = p->pp;
		p->pp = pp;
		pp->pos = p->pos + f->delay;
		pp->len = p->len;

		if ( f->uses_auto_shape_pulses )
		{
			pp->pos -= f->left_shape_padding;
			pp->len += f->left_shape_padding + f->right_shape_padding;
		}

		if ( f->self == PULSER_CHANNEL_TWT && p->tp != NULL )
		{
			pp->pos -= p->tp->function->left_twt_padding;
			pp->len +=   p->tp->function->left_twt_padding
				       + p->tp->function->right_twt_padding;
		}
	}

	dg2020_shape_padding_check_1( f );
	dg2020_twt_padding_check( f );

	for ( i = 0; i < f->num_active_pulses; i++ )
	{
		pp = f->pulse_params + i;

		/* Check that pulses still fit into the pulser memory (while in
		   test run) or the maximum sequence length (in the real run) */

		f->max_seq_len = Ticks_max( f->max_seq_len, pp->pos + pp->len );
		if ( f->max_seq_len >
				 ( FSC2_MODE == TEST ? MAX_PULSER_BITS : dg2020.max_seq_len ) )
		{
			if ( FSC2_MODE == TEST )
				print( FATAL, "Pulse sequence for function '%s' does not fit "
					   "into the pulsers memory.\n", f->name );
			else
				print( FATAL, "Pulse sequence for function '%s' is too long. "
					   "Perhaps you should try the MAXIMUM_PATTERN_LENGTH "
					   "command.\n", f->name );
			THROW( EXCEPTION );
		}

		/* Check for overlap of pulses */

		if ( i + 1 < f->num_active_pulses )
			ppn = f->pulse_params + i + 1;
		else
			ppn = NULL;

		if ( ppn != NULL && pp->pulse->pos + pp->pulse->len > ppn->pulse->pos )
		{
			if ( dg2020.auto_shape_pulses &&
				 f->self == PULSER_CHANNEL_PULSE_SHAPE )
			{
				if ( pp->pulse->sp != NULL && ppn->pulse->sp != NULL &&
					 pp->pulse->sp->function != ppn->pulse->sp->function )
					print( FATAL, "Shape pulses for pulses #%ld function "
						   "'%s') and #%ld (function '%s') %s"
						   "overlap.\n", pp->pulse->sp->num,
						   pp->pulse->sp->function->name, ppn->pulse->sp->num,
						   ppn->pulse->sp->function->name,
						   dg2020_IN_SETUP ? "" : "start to " );
				if ( pp->pulse->sp != NULL && ppn->pulse->sp == NULL )
					print( FATAL, "Automatically created shape pulse for "
						   "pulse #%ld (function '%s') and SHAPE pulse "
						   "#%ld %soverlap.\n", pp->pulse->sp->num,
						   pp->pulse->sp->function->name, ppn->pulse->num,
						   dg2020_IN_SETUP ? "" : "start to " );
				else if ( pp->pulse->sp == NULL && ppn->pulse->sp != NULL )
					print( FATAL, "Automatically created shape pulse for "
						   "pulse #%ld (function '%s') and SHAPE pulse "
						   "#%ld %soverlap.\n", ppn->pulse->sp->num,
						   ppn->pulse->sp->function->name, pp->pulse->num,
						   dg2020_IN_SETUP ? "" : "start to " );
			}
			else
				print( FATAL, "Pulses #%ld and #%ld of function '%s' %s"
					   "overlap.\n", pp->pulse->num, ppn->pulse->num,
					   f->name, dg2020_IN_SETUP ? "" : "start to "  );
			THROW( EXCEPTION );
		}
	}

	if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
		 dg2020.function[ PULSER_CHANNEL_DEFENSE ].is_used &&
		 ( dg2020.is_shape_2_defense || dg2020.is_defense_2_shape ||
		   dg2020.function[ PULSER_CHANNEL_TWT ].is_used ||
		   dg2020.function[ PULSER_CHANNEL_TWT_GATE ].is_used ) )
		dg2020_defense_shape_check( f );
}


/*------------------------------------------------*/
/*------------------------------------------------*/

static void dg2020_shape_padding_check_1( FUNCTION *f )
{
	PULSE_PARAMS *pp, *ppp;
	int i;


	if ( f->self == PULSER_CHANNEL_PULSE_SHAPE ||
		 ! f->uses_auto_shape_pulses ||
		 f->num_active_pulses == 0 )
		return;

	/* Check that first pulse don't starts to early */

	pp = f->pulse_params;
	if ( pp->pos < 0 )
	{
		if ( ! pp->pulse->left_shape_warning )
		{
			print( SEVERE, "Pulse #%ld too early to set left shape padding "
				   "of %s.\n", pp->pulse->num,
				   dg2020_pticks( f->left_shape_padding ) );
			pp->pulse->left_shape_warning = SET;
		}

		dg2020.left_shape_warning++;
		pp->pulse->function->min_left_shape_padding =
					l_min( pp->pulse->function->min_left_shape_padding,
						   pp->pulse->function->left_shape_padding + pp->pos );
		pp->len += pp->pos;
		pp->pos = 0;
	}

	/* Shorten intermediate pulses if they would overlap */

	for ( i = 1; i < f->num_active_pulses; i++ )
	{
		POD *pod_pp, *pod_ppp;

		ppp = pp;
		pp = pp + 1;

		/* Don't complain about manually generated pulses - the user has to
		   take care of this */

		if ( ppp->pulse->sp == NULL && pp->pulse->sp == NULL )
			continue;

		/* If the pulses appear on different pods they may not overlap, but
		   when they appear on the same pod we simply shorten on of them. */

		pod_pp = pp->pulse->function->num_pods == 1 ?
			pp->pulse->function->pod[ 0 ] :
			pp->pulse->function->phase_setup->pod[
				pp->pulse->pc->sequence[ f->next_phase ] - PHASE_PLUS_X ];
		pod_ppp = ppp->pulse->function->num_pods == 1 ?
			ppp->pulse->function->pod[ 0 ] :
			ppp->pulse->function->phase_setup->pod[
				ppp->pulse->pc->sequence[ f->next_phase ] - PHASE_PLUS_X ];

		if ( ppp->pos + ppp->len > pp->pos )
		{
			if ( pod_pp == pod_ppp )
				ppp->len -= ppp->pos + ppp->len - pp->pos;
			else
			{
				print( FATAL, "Distance between pulses #%ld and #%ld %stoo "
					   "small to set shape padding.\n", ppp->pulse->num,
					   pp->pulse->num, dg2020_IN_SETUP ? "" : "becomes " );
				THROW( EXCEPTION );
			}
		}

	}

	/* Check that last pulse isn't too long */

	pp = f->pulse_params + f->num_active_pulses - 1;

	if ( pp->pos + pp->len >
				 ( FSC2_MODE == TEST ? MAX_PULSER_BITS : dg2020.max_seq_len ) )
	{
		if ( ! pp->pulse->right_shape_warning )
		{

			if ( FSC2_MODE == TEST )
				print( SEVERE, "Pulse #%ld too long to set right shape "
					   "padding of %s.\n", pp->pulse->num,
					   dg2020_pticks( f->right_shape_padding ) );
			else
			{
				print( SEVERE, "Pulse #%ld too long to set right shape "
					   "padding of %s. Perhaps you should try the "
					   "MAXIMUM_PATTERN_LENGTH command.\n", pp->pulse->num,
					   dg2020_pticks( f->right_shape_padding ) );
				THROW( EXCEPTION );
			}

			pp->pulse->right_shape_warning = SET;
		}

		dg2020.right_shape_warning++;
		pp->pulse->function->min_right_shape_padding =
			l_min( pp->pulse->function->min_right_shape_padding,
				   pp->pulse->function->right_shape_padding + pp->pos );
		pp->len = MAX_PULSER_BITS - pp->pos;
	}
}


/*------------------------------------------------*/
/*------------------------------------------------*/

static void dg2020_shape_padding_check_2( void )
{
	FUNCTION *f1, *f2;
	PULSE_PARAMS *pp1, *pp2;
	int i, j, k, l;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f1 = dg2020.function + i;

		if ( ! f1->uses_auto_shape_pulses )
			continue;

		for ( j = i + 1; j < PULSER_CHANNEL_NUM_FUNC; j++ )
		{
			f2 = dg2020.function + j;

			if ( ! f2->uses_auto_shape_pulses )
				continue;

			for ( k = 0; k < f1->num_active_pulses; k++ )
			{
				pp1 = f1->pulse_params + k;

				for ( l = 0; l < f2->num_active_pulses; l++ )
				{
					pp2 = f2->pulse_params + l;

					if ( ( pp1->pos <= pp2->pos &&
						   pp1->pos + pp1->len > pp2->pos ) ||
						 ( pp1->pos > pp2->pos &&
						   pp1->pos < pp2->pos + pp2->len ) )
					{
						print( FATAL, "Distance between pulses #%ld and #%ld "
							   "too short to set shape padding.\n",
							   pp1->pulse->num, pp2->pulse->num );
						THROW( EXCEPTION );
					}
				}
			}
		}
	}
}


/*------------------------------------------------*/
/*------------------------------------------------*/

static void dg2020_twt_padding_check( FUNCTION *f )
{
	PULSE_PARAMS *pp, *ppp;
	int i;


	if ( f->self != PULSER_CHANNEL_TWT || f->num_active_pulses == 0 )
		return;

	/* Check that first TWT pulse doesn't start too early (this only can
	   happen for automatically created pulses) */

	pp = f->pulse_params;
	if ( pp->pulse->tp != NULL && pp->pos < 0 )
	{
		if ( ! pp->pulse->left_twt_warning )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Pulse #%ld too early to set left padding of "
					   "%s for its TWT pulse.\n", pp->pulse->tp->num,
					   dg2020_pticks(
						   		 pp->pulse->tp->function->left_twt_padding ) );
			pp->pulse->left_twt_warning = SET;
		}

		dg2020.left_twt_warning++;
		pp->pulse->function->min_left_twt_padding =
					  l_min( pp->pulse->function->min_left_twt_padding,
							 pp->pulse->function->left_twt_padding + pp->pos );
		pp->len += pp->pos;
		pp->pos = 0;

		if ( ! pp->pulse->needs_update )
		{
			pp->pulse->is_old_len = pp->pulse->is_old_pos = UNSET;
			pp->pulse->was_active = pp->pulse->is_active;
		}

		pp->pulse->needs_update = SET;
	}

	/* Shorten intermediate pulses so they don't overlap - if necessary even
	   remove pulses if a pulse is completely within the area of its
	   predecessor */

	for ( i = 1; i < f->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		/* Don't complain about manually generated pulses - the user has to
		   take care of this */

		if ( ppp->pulse->tp == NULL && pp->pulse->tp == NULL )
			continue;

		if ( pp->pos < 0 )
		{
			if ( ! pp->pulse->left_twt_warning )
			{
				print( SEVERE, "Pulse #%ld too early to set left padding of "
					   "%s for its TWT pulse.\n",
					   pp->pulse->tp->num, dg2020_pticks(
								 pp->pulse->tp->function->left_twt_padding ) );
				pp->pulse->left_twt_warning = SET;
			}

			dg2020.left_twt_warning++;
			pp->pulse->function->min_left_twt_padding =
					  l_min( pp->pulse->function->min_left_twt_padding,
							 pp->pulse->function->left_twt_padding + pp->pos );
			pp->len += pp->pos;
			pp->pos = 0;

			if ( ! pp->pulse->needs_update )
			{
				pp->pulse->is_old_len = pp->pulse->is_old_pos = UNSET;
				pp->pulse->was_active = pp->pulse->is_active;
			}

			pp->pulse->needs_update = SET;
		}

		if ( ppp->pos == pp->pos )
		{
			if ( ppp->len <= pp->len )
				memmove( ppp, pp,
						 ( f->num_active_pulses-- - i-- ) * sizeof *pp );
			else
				memmove( pp, pp + 1,
						 ( f->num_active_pulses-- - --i ) * sizeof *pp );

			if ( ! pp->pulse->needs_update )
			{
				pp->pulse->is_old_len = pp->pulse->is_old_pos = UNSET;
				pp->pulse->was_active = pp->pulse->is_active;
			}

			pp->pulse->needs_update = SET;

			if ( ! ppp->pulse->needs_update )
			{
				ppp->pulse->is_old_len = ppp->pulse->is_old_pos = UNSET;
				ppp->pulse->was_active = ppp->pulse->is_active;
			}

			ppp->pulse->needs_update = SET;
			continue;
		}

		if ( ppp->pos + ppp->len > pp->pos )
		{
			if ( ppp->pos + ppp->len >= pp->pos + pp->len )
			{
				memmove( pp, pp + 1,
					  ( --f->num_active_pulses - --i ) * sizeof *pp );

				if ( ! pp->pulse->needs_update )
				{
					pp->pulse->is_old_len = pp->pulse->is_old_pos = UNSET;
					pp->pulse->was_active = pp->pulse->is_active;
				}

				pp->pulse->needs_update = SET;

				if ( ! ppp->pulse->needs_update )
				{
					ppp->pulse->is_old_len = ppp->pulse->is_old_pos = UNSET;
					ppp->pulse->was_active = ppp->pulse->is_active;
				}

				ppp->pulse->needs_update = SET;
			}

			else
			{
				ppp->len -= ppp->pos + ppp->len - pp->pos;

				if ( ! ppp->pulse->needs_update )
				{
					ppp->pulse->is_old_len = ppp->pulse->is_old_pos = UNSET;
					ppp->pulse->was_active = ppp->pulse->is_active;
				}

				ppp->pulse->needs_update = SET;
			}
		}
	}

	/* Check that the last TWT pulse isn't too long (can only happen for
	   automatically created pulses) */

	pp = f->pulse_params + f->num_active_pulses - 1;
	if ( pp->pos + pp->len >
				 ( FSC2_MODE == TEST ? MAX_PULSER_BITS : dg2020.max_seq_len ) )
	{
		if ( ! pp->pulse->right_twt_warning )
		{
			if ( FSC2_MODE == TEST )
				print( SEVERE, "Pulse #%ld too long to set right padding of "
					   "%s for its TWT pulse.\n", pp->pulse->tp->num,
					   dg2020_pticks(
								pp->pulse->tp->function->right_twt_padding ) );
			else
			{
				print( FATAL, "Pulse #%ld too long to set right padding of "
					   "%s for its TWT pulse. Perhaps you should try the "
					   "MAXIMUM_PATTERN_LENGTH command.\n", pp->pulse->tp->num,
					   dg2020_pticks(
								pp->pulse->tp->function->right_twt_padding ) );
				THROW( EXCEPTION );
			}

			pp->pulse->right_twt_warning = SET;
		}

		dg2020.right_twt_warning++;
		pp->pulse->function->min_right_twt_padding =
			l_min( pp->pulse->function->min_right_twt_padding,
				   pp->pulse->function->right_twt_padding + pp->pos );
		pp->len = MAX_PULSER_BITS - pp->pos;

		if ( ! pp->pulse->needs_update )
		{
			pp->pulse->is_old_len = pp->pulse->is_old_pos = UNSET;
			pp->pulse->was_active = pp->pulse->is_active;
		}

		pp->pulse->needs_update = SET;
	}

	/* Finally check if the pulses are far enough apart - for automatically
	   created pulses lengthen them if necessary, for user created pulses
	   print a warning */

	pp = f->pulse_params;

	for ( i = 1; i < f->num_active_pulses; i++ )
	{
		ppp = pp;
		pp = pp + 1;

		if ( pp->pos - ( ppp->pos + ppp->len )
			 < dg2020.minimum_twt_pulse_distance )
		{
			if ( pp->pulse->tp != NULL || ppp->pulse->tp != NULL )
				ppp->len = pp->pos - ppp->pos;
			else
			{
				if ( dg2020.twt_distance_warning++ != 0 )
					print( SEVERE, "Distance between TWT pulses #%ld and #%ld "
						   "is smaller than %s.\n", ppp->pulse->num,
						   pp->pulse->num, dg2020_pticks(
										 dg2020.minimum_twt_pulse_distance ) );
			}
		}
	}
}


/*------------------------------------------------------------------------*/
/* Function checks if the distance between pulse shape pulses and defense */
/* pulses is large enough. The minimum lengths the shape_2_defense and    */
/* defense_2_shape members of the dg2020 structure. Both are set to       */
/* rather large values at first but can be customized by calling the EDL  */
/* functions pulser_shape_to_defense_minimum_distance() and               */
/* pulser_defense_to_shape_minimum_distance() (names are intentionally    */
/* that long).                                                            */
/* The function is called only if pulse shape and defense pulses are used */
/* and either also TWT or TWT_GATE pulses or at least one of both the     */
/* mentioned EDL functions have been called.                              */
/*------------------------------------------------------------------------*/

static void dg2020_defense_shape_check( FUNCTION *shape )
{
	FUNCTION *defense = dg2020.function + PULSER_CHANNEL_DEFENSE;
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
				 shape_p->pos + shape_p->len + dg2020.shape_2_defense >
				 defense_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					print( FATAL, "Distance between PULSE_SHAPE pulse %s#%ld "
						   "and DEFENSE pulse #%ld got shorter than %s.\n",
						   shape_p->sp ? "for pulse " : "", 
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   defense_p->num,
						   dg2020_pticks( dg2020.shape_2_defense ) );
					THROW( EXCEPTION );
				}

				if ( dg2020_IN_SETUP )
					print( SEVERE, "Distance between PULSE_SHAPE pulse %s"
						   "#%ld and DEFENSE pulse #%ld is shorter than "
						   "%s.\n", shape_p->sp ? "for pulse " : "", 
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   defense_p->num,
						   dg2020_pticks( dg2020.shape_2_defense ) );
				else if ( ! dg2020.shape_2_defense_too_near )
					print( SEVERE, "Distance between PULSE_SHAPE pulse %s"
						   "#%ld and DEFENSE pulse #%ld got shorter than "
						   "%s.\n",
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   defense_p->num,
						   dg2020_pticks( dg2020.shape_2_defense ) );
				dg2020.shape_2_defense_too_near++;
			}

			if ( defense_p->pos < shape_p->pos &&
				 defense_p->pos + defense_p->len + dg2020.defense_2_shape >
				 shape_p->pos )
			{
				if ( FSC2_MODE == EXPERIMENT )
				{
					print( FATAL, "Distance between DEFENSE pulse #%ld and "
						   "PULSE_SHAPE pulse %s#%ld got shorter than %s.\n",
						   defense_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   dg2020_pticks( dg2020.defense_2_shape ) );
					THROW( EXCEPTION );
				}

				if ( dg2020_IN_SETUP )
				{
					print( SEVERE, "Distance between DEFENSE pulse #%ld "
						   "and PULSE_SHAPE pulse #%ld is shorter than "
						   "%s.\n", defense_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   dg2020_pticks( dg2020.defense_2_shape ) );
				}
				else if ( ! dg2020.defense_2_shape_too_near )
					print( SEVERE, "Distance between DEFENSE pulse #%ld "
						   "and PULSE_SHAPE pulse #%ld got shorter than "
						   "%s.\n", defense_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   shape_p->sp ? shape_p->sp->num : shape_p->num,
						   dg2020_pticks( dg2020.defense_2_shape ) );
				dg2020.defense_2_shape_too_near++;
			}
		}
	}
}


/*-----------------------------------------------------------*/
/* Function creates all active pulses in the channels of the */
/* pulser assigned to the function passed as argument.       */
/*-----------------------------------------------------------*/

void dg2020_set_pulses( FUNCTION *f )
{
	PULSE *p;
	Ticks start, end;
	int i, j;


	fsc2_assert( f->self != PULSER_CHANNEL_PHASE_1 &&
				 f->self != PULSER_CHANNEL_PHASE_2 );

	/* Always set the very first bit to LOW state, see the rant about the bugs
	   in the pulser firmware at the start of dg2020_gpib_b.c. Then set the
	   rest of the channels to off state. */

	for ( i = 0; i < f->num_needed_channels; i++ )
	{
		dg2020_set_constant( f->channel[ i ]->self, -1, 1, LOW );
		dg2020_set_constant( f->channel[ i ]->self, 0, dg2020.mem_size - 1,
							 type_OFF( f ) );
	}

	/* Now simply run through all active pulses of the channel */

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses && p->is_active;
		  p = f->pulses[ ++i ] )
	{
		/* Set the area of the pulse itself */

		start = p->pp->pos;
		end = p->pp->pos + p->pp->len;

		for ( j = 0; j < f->pc_len; j++ )
		{
			if ( p->channel[ j ] == NULL )        /* skip unused channels */
				continue;

			if ( start != end )
				dg2020_set_constant( p->channel[ j ]->self, start, end - start,
									 type_ON( f ) );
		}
	}

	for ( p = f->pulses[ 0 ], i = 0; i < f->num_pulses; p = f->pulses[ ++i ] )
		p->was_active = p->is_active;
}


/*------------------------------------------------------------------*/
/* Function is called after the test run to reset all the variables */
/* describing the state of the pulser to their initial values.      */
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
				print( WARN, "Pulse #%ld is never used.\n", p->num );
			p = dg2020_delete_pulse( p, SET );
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

	if ( dg2020_phs[ 0 ].function != NULL )
		dg2020_phs[ 0 ].function->next_phase = 0;
	if ( dg2020_phs[ 1 ].function != NULL )
		dg2020_phs[ 1 ].function->next_phase = 0;
}


/*------------------------------------------------*/
/* Function deletes a pulse and returns a pointer */
/* to the next pulse in the pulse list.           */
/*------------------------------------------------*/

PULSE *dg2020_delete_pulse( PULSE *p, bool warn )
{
	PULSE *pp;
	int i;


	/* If the pulse has an associated shape pulse delete it */

	if ( p->sp )
	{
		if ( p->sp->function->self == PULSER_CHANNEL_PULSE_SHAPE )
		{
			dg2020_delete_pulse( p->sp, warn );
			p->sp = NULL;
		}
		else
			p->sp->sp = NULL;
	}

	/* If the pulse has an associated TWT pulse also delete it */

	if ( p->tp )
	{
		if ( p->tp->function->self == PULSER_CHANNEL_TWT )
		{
			dg2020_delete_pulse( p->tp, warn );
			p->sp = NULL;
		}
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

		fsc2_assert( i < p->function->num_pulses );  /* Paranoia */

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
					   "its pulses are unused.\n", p->function->name );
			p->function->is_used = UNSET;
		}
	}

	/* Now remove the pulse from the real pulse list */

	if ( p->next != NULL )
		p->next->prev = p->prev;
	pp = p->next;
	if ( p->prev != NULL )
		p->prev->next = p->next;
	T_free( p->channel );

	/* Special care has to be taken if this is the very last pulse... */

	if ( p == dg2020_Pulses && p->next == NULL )
		dg2020_Pulses = PULSE_P T_free( dg2020_Pulses );
	else
		T_free( p );

	return pp;
}


/*---------------------------------------------------------------------*/
/* Changes the pulse pattern in the channels belonging to function 'f' */
/* so that the data in the pulser get in sync with the its internal    */
/* representation. Some care has taken to minimize the number of       */
/* commands and their length.                                          */
/*---------------------------------------------------------------------*/

void dg2020_commit( FUNCTION *f, bool flag )
{
	PULSE *p;
	int i, j;
	Ticks start, len;
	int what;


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

	/* In a real run we now have to change the pulses. The only way to keep
	   the number and length of commands to be sent to the pulser at a minimum
	   while getting it right in every imaginable case is to create two images
	   of the pulser channel states, one of the current state and a second one
	   of the state after the changes. These images are compared and only that
	   parts where differences are found are changed. Of course, that needs
	   quite some computer time but probable is faster, or at least easier to
	   understand and to debug, than any alternative I came up with...

	   First allocate memory for the old and the new states of the channels
	   used by the function */

	for ( i = 0; i < f->num_needed_channels; i++ )
	{
		f->channel[ i ]->old_d = CHAR_P T_calloc( 2 * dg2020.max_seq_len, 1 );
		f->channel[ i ]->new_d = f->channel[ i ]->old_d + dg2020.max_seq_len;
	}

	/* Now loop over all pulses and pick the ones that need changes */

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		if ( ! p->needs_update )
		{
			p->is_old_len = p->is_old_pos = UNSET;
			p->was_active = p->is_active;
			continue;
		}

		/* For the channels used by the pulse enter the old and new position
		   in the corresponding memory representations of the channels and
		   mark the channel as needing update */

		for ( j = 0; j < f->pc_len; j++ )
		{
			if ( p->channel[ j ] == NULL )        /* skip unused channels */
				continue;

			if ( p->is_old_pos || ( p->is_old_len && p->old_len != 0 ) )
				dg2020_set( p->channel[ j ]->old_d,
							p->is_old_pos ? p->old_pp->pos : p->pp->pos,
							p->is_old_len ? p->old_pp->len : p->pp->len );
			if ( p->is_pos && p->is_len && p->pp->len != 0 )
				dg2020_set( p->channel[ j ]->new_d, p->pp->pos, p->pp->len );

			p->channel[ j ]->needs_update = SET;
		}

		p->is_old_len = p->is_old_pos = UNSET;
		if ( p->is_active )
			p->was_active = SET;
		p->needs_update = UNSET;
	}

	/* Loop over all channels belonging to the function and for each channel
	   that needs to be changed find all differences between the old and the
	   new state by repeatedly calling dg2020_diff() - it returns +1 or -1 for
	   setting or resetting plus the start and length of the different area or
	   0 if no differences are found anymore. For each difference set the real
	   pulser channel accordingly. */

	for ( i = 0; i < f->num_needed_channels; i++ )
	{
		if ( f->channel[ i ]->needs_update )
		{
			while ( ( what = dg2020_diff( f->channel[ i ]->old_d,
										  f->channel[ i ]->new_d,
										  &start, &len ) ) != 0 )
				dg2020_set_constant( f->channel[ i ]->self, start, len,
								   what == -1 ? type_OFF( f ) : type_ON( f ) );
		}

		f->channel[ i ]->needs_update = UNSET;
		T_free( f->channel[ i ]->old_d );
	}
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void dg2020_cw_setup( void )
{
	int i;
	PHASE_SETUP *p;
	FUNCTION *f;


	f = dg2020.function + PULSER_CHANNEL_MW;
	p = f->phase_setup;

	if ( f->num_channels == 0 )
	{
		f->channel[ 0 ] = dg2020.channel + MAX_CHANNELS - 1;
		f->channel[ 1 ] = dg2020.channel + MAX_CHANNELS - 2;
	}

	if ( f->num_channels == 1 )
	{
		if ( f->channel[ 0 ]->self != MAX_CHANNELS - 1 )
			f->channel[ 1 ] = dg2020.channel + MAX_CHANNELS - 1;
		else
			f->channel[ 1 ] = dg2020.channel + MAX_CHANNELS - 2;
	}

	/* The first channel is set to be always logical low, the second (i.e.
	   the cw channel) to be always logical high */

	if ( ! dg2020_set_constant( f->channel[ 0 ]->self, -1, 1, 0 ) ||
		 ! dg2020_set_constant( f->channel[ 0 ]->self, 0, 127,
								type_OFF( f ) ) ||
		 ! dg2020_set_constant( f->channel[ 1 ]->self, -1, 1, 0 ) ||
		 ! dg2020_set_constant( f->channel[ 1 ]->self, 0, 127, type_ON( f ) ) )
	{
		print( FATAL, "Failed to setup pulser.\n" );
		THROW( EXCEPTION );
	}

	/* Finally, we've got to setup the blocks. The first one has not much
	   meaning (except allowing to start with a low voltage level, while
	   the second block is repeated infinitely. */

	strcpy( dg2020.block[ 0 ].blk_name, "B0" );
	dg2020.block[ 0 ].start = 0;
	dg2020.block[ 0 ].repeat = 1;
	dg2020.block[ 0 ].is_used = SET;

	strcpy( dg2020.block[ 1 ].blk_name, "B1" );
	dg2020.block[ 1 ].start = 64;
	dg2020.block[ 1 ].repeat = 1;
	dg2020.block[ 1 ].is_used = SET;

	if ( ! dg2020_make_blocks( 2, dg2020.block ) ||
		 ! dg2020_make_seq( 2, dg2020.block ) )
	{
		print( FATAL, "Failed to setup pulser.\n" );
		THROW( EXCEPTION );
	}

	/* Now that everything is done the pulser is told to really set the
	   channels */

	dg2020_update_data( );

	/* Finally, all non-cw channels get associated with the first pulser
	   channel and the cw channel with the second */

	for ( i = 0; i < f->num_pods; i++ )
		if ( f->pod[ i ] != p->pod[ PHASE_CW ] )
			dg2020_channel_assign( f->channel[ 0 ]->self, f->pod[ i ]->self );
		else
			dg2020_channel_assign( f->channel[ 1 ]->self, f->pod[ i ]->self );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
