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


#include "dg2020_b.h"


static void dg2020_defense_twt_check( void );


#define type_ON( f )   ( ( f )->is_inverted ? LOW : HIGH )
#define type_OFF( f )  ( ( f )->is_inverted ? HIGH : LOW )


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

	if ( ( state = dg2020_reorganize_pulses( TEST_RUN ) ) && ! TEST_RUN )
		dg2020_update_data( );

	dg2020.needs_update = UNSET;
	return state;
}


/*--------------------------------------------------------------------------*/
/* This function sorts the pulses and checks that the pulses don't overlap. */
/*--------------------------------------------------------------------------*/

bool dg2020_reorganize_pulses( bool flag )
{
	static int i;
	FUNCTION *f;
	PULSE *p;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

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


/*-------------------------------------------------------------------------*/
/* Function is called after pulses have been changed to check if the new   */
/* settings are still ok, i.e. that they still fit into the pulsers memory */
/* and don't overlap.                                                      */
/*-------------------------------------------------------------------------*/

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
							"`%s' does not fit into the pulsers memory.\n",
							pulser_struct.name, Function_Names[ f->self ] );
				else
					eprint( FATAL, ! dg2020_IN_SETUP, "%s: Pulse sequence for "
							"function `%s' is too long. Perhaps you should "
							"try the MAXIMUM_PATTERN_LENGTH command.\n",
							pulser_struct.name, Function_Names[ f->self ] );
				THROW( EXCEPTION )
			}

			f->num_active_pulses = i + 1;
		}

		/* Check for overlap of pulses */

		if ( i + 1 < f->num_pulses && f->pulses[ i + 1 ]->is_active &&
			 p->pos + p->len > f->pulses[ i + 1 ]->pos )
		{
			if ( dg2020_IN_SETUP )
				eprint( TEST_RUN ? FATAL : SEVERE, UNSET,
						"%s: Pulses %ld and %ld overlap.\n",
						pulser_struct.name, p->num, f->pulses[ i + 1 ]->num );
			else
				eprint( FATAL, SET, "%s: Pulses %ld and %ld begin to "
						"overlap.\n", pulser_struct.name,
						p->num, f->pulses[ i + 1 ]->num );
			THROW( EXCEPTION )
		}
	}

	if ( f->self == PULSER_CHANNEL_TWT_GATE &&
		 dg2020.function[ PULSER_CHANNEL_TWT_GATE ].is_used &&
		 dg2020.function[ PULSER_CHANNEL_DEFENSE ].is_used )
		dg2020_defense_twt_check( );
}


/*-----------------------------------------------------------------*/
/* If there are both TWT_GATE pulses and DEFENSE pulses check that */
/* they won't get to near to each other to avoid destroying the    */
/* diode or the mixer inadvertently.                               */
/*-----------------------------------------------------------------*/

static void dg2020_defense_twt_check( void )
{
	FUNCTION *twt = &dg2020.function[ PULSER_CHANNEL_TWT_GATE ],
		     *defense = &dg2020.function[ PULSER_CHANNEL_DEFENSE ];
	PULSE *twt_p, *defense_p;
	Ticks defense_2_twt, twt_2_defense;
	long i, j;


	defense_2_twt = ( Ticks ) lrnd( ceil( DEFENSE_2_TWT_MIN_DISTANCE /
										  dg2020.timebase ) );
	twt_2_defense = ( Ticks ) lrnd( ceil( TWT_2_DEFENSE_MIN_DISTANCE /
										  dg2020.timebase ) );

	for ( i = 0; i < defense->num_pulses; i++ )
	{
		defense_p = defense->pulses[ i ];

		if ( ! defense_p->is_active )
			continue;

		for ( j = 0; j < twt->num_pulses; j++ )
		{
			twt_p = twt->pulses[ j ];
			if ( ! twt_p->is_active )
				continue;

			if ( twt_p->pos < defense_p->pos &&
				 twt_p->len + twt_p->pos + twt_2_defense > defense_p->pos )
				eprint( SEVERE, ! dg2020_IN_SETUP, "%s: TWT_GATE pulse %ld "
						"gets dangerously near to DEFENSE pulse %ld.\n",
						pulser_struct.name, twt_p->num, defense_p->num );

			if ( twt_p->pos > defense_p->pos &&
				 defense_p->pos + defense_p->len + defense_2_twt > twt_p->pos )
				eprint( SEVERE, ! dg2020_IN_SETUP, "%s: DEFENSE pulse %ld "
						"gets dangerously near to TWT_GATE pulse %ld.\n",
						pulser_struct.name, defense_p->num, twt_p->num );
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

		start = p->pos + f->delay;
		end = p->pos + p->len + f->delay;

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


/*------------------------------------------------*/
/* Function deletes a pulse and returns a pointer */
/* to the next pulse in the pulse list.           */
/*------------------------------------------------*/

PULSE *dg2020_delete_pulse( PULSE *p )
{
	PULSE *pp;
	int i;


	/* First we've got to remove the pulse from its functions pulse list */

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
				"all its pulses are unused.\n", pulser_struct.name,
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

	/* Special care has to be taken if this is the very last pulse... */

	if ( p == dg2020_Pulses && p->next == NULL )
		dg2020_Pulses = T_free( dg2020_Pulses );
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
		f->channel[ i ]->old = T_calloc( 2 * dg2020.max_seq_len, 1 );
		f->channel[ i ]->new = f->channel[ i ]->old + dg2020.max_seq_len;
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
				dg2020_set( p->channel[ j ]->old,
							p->is_old_pos ? p->old_pos : p->pos,
							p->is_old_len ? p->old_len : p->len, f->delay );
			if ( p->is_pos && p->is_len && p->len != 0 )
				dg2020_set( p->channel[ j ]->new, p->pos, p->len, f->delay );

			p->channel[ j ]->needs_update = SET;
		}

		p->is_old_len = p->is_old_pos = UNSET;
		if ( p->is_active )
			p->was_active = SET;
		p->needs_update = UNSET;
	}

	/* Loop over all channels belonging to the function and for each channel
	   that needs to be changeda find all differences between the old and the
	   new state by repeatedly calling dg2020_diff() - it returns +1 or -1 for
	   setting or resetting plus the start and length of the different area or
	   0 if no differences are found anymore. For each difference set the real
	   pulser channel accordingly. */

	for ( i = 0; i < f->num_needed_channels; i++ )
	{
		if ( f->channel[ i ]->needs_update )
		{
			while ( ( what = dg2020_diff( f->channel[ i ]->old,
										  f->channel[ i ]->new, 
										  &start, &len ) ) != 0 )
				dg2020_set_constant( f->channel[ i ]->self, start, len,
								   what == -1 ? type_OFF( f ) : type_ON( f ) );
		}

		f->channel[ i ]->needs_update = UNSET;
		T_free( f->channel[ i ]->old );
	}
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void dg2020_cw_setup( void )
{
	int i;
	PHASE_SETUP *p;
	FUNCTION *f;


	f = &dg2020.function[ PULSER_CHANNEL_MW ];
	p = f->phase_setup;

	/* First, all non-cw channels get associated with the first pulser channel
	   and the cw channel with the second */

	for ( i = 0; i < f->num_pods; i++ )
		if ( f->pod[ i ] != p->pod[ PHASE_CW ] )
			dg2020_channel_assign( f->channel[ 0 ]->self, f->pod[ i ]->self );
		else
			dg2020_channel_assign( f->channel[ 1 ]->self, f->pod[ i ]->self );

	/* Now the the first channel is set to be always logical low, the second
	   (i.e. the cw channel) to be always logical high */

	if ( ! dg2020_set_constant( f->channel[ 0 ]->self, -1, 1, 0 ) ||
		 ! dg2020_set_constant( f->channel[ 0 ]->self, 0, 127,
								type_OFF( f ) ) ||
		 ! dg2020_set_constant( f->channel[ 1 ]->self, -1, 1, 0 ) ||
		 ! dg2020_set_constant( f->channel[ 1 ]->self, 0, 127, type_ON( f ) ) )
	{
		eprint( FATAL, UNSET, "%s: Failed to setup pulser.\n",
				pulser_struct.name );
		THROW( EXCEPTION )
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
		eprint( FATAL, UNSET, "%s: Failed to setup pulser.\n",
				pulser_struct.name );
		THROW( EXCEPTION )
	}

	/* Now that everything is done the pulser is told to really set the
	   channels */

	dg2020_update_data( );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
