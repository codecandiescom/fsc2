/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#include "rb_pulser.h"


static void rb_pulser_commit( bool flag );
static void rb_pulser_rf_pulse( void );


/*---------------------------------------------------------------------*
 * Function is called in the experiment after pulses have been changed
 * to update the pulser accordingly.
 *---------------------------------------------------------------------*/

bool rb_pulser_do_update( void )
{
	bool restart = UNSET;
	bool state;


	/* Resort the pulses, check that the new pulse settings are reasonable
	   and finally commit all changes */

	if ( rb_pulser.is_running )
	{
		restart = SET;
		if ( FSC2_MODE == EXPERIMENT )
			rb_pulser_run( STOP );
	}

	state = rb_pulser_update_pulses( FSC2_MODE == TEST );

	if ( restart && FSC2_MODE == EXPERIMENT )
		rb_pulser_run( START );

	return state;
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

bool rb_pulser_update_pulses( bool flag )
{
	rb_pulser_function_init( );
	rb_pulser_init_delay( );
	rb_pulser_delay_card_setup( );
	rb_pulser_rf_pulse( );

	/* Now figure out the pulse sequence length */

	rb_pulser_seq_length_check( );

	rb_pulser_commit( flag );

	return OK;
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

void rb_pulser_function_init( void )
{
	int i, j;
	FUNCTION *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rb_pulser.function + i;

		if ( ! f->is_used )
			continue;

		for ( f->num_active_pulses = 0, j = 0; j < f->num_pulses; j++ )
			if ( f->pulses[ j ]->is_active )
				f->num_active_pulses++;

		if ( f->num_pulses > 1 )
			qsort( f->pulses, f->num_pulses, sizeof *f->pulses,
				   rb_pulser_start_compare );
	}
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

void rb_pulser_init_delay( void )
{
	FUNCTION *f = rb_pulser.function + PULSER_CHANNEL_MW;
	PULSE *p = f->pulses[ 0 ];
	PULSE *cp;
	double pos, shift;
	Ticks delay;
	RULBUS_DELAY_CARD *card = delay_card + INIT_DELAY;


	card->was_active = card->is_active;
	card->is_active = card->needs_update = UNSET;

	/* If there's no active MW pulse the initial delay card always gets
	   set to the shortest possible delay, otherwise to the delay required
	   for the first MW pulse */

	if ( f->num_active_pulses == 0 )
	{
		if ( card->delay != 0 || ! card->was_active )
			card->needs_update = SET;
		card->delay = 0;
		return;
	}

	pos = p->pos + p->function->delay - 2 * MIN_DELAY;

	if ( pos < 0.0 )
	{
		print( FATAL, "First MW pulse starts too early.\n" );
		THROW( EXCEPTION );
	}

	shift = Ticks_rnd( pos / rb_pulser.timebase ) * rb_pulser.timebase - pos;

	if ( fabs( shift ) > PRECISION * rb_pulser.timebase )
	{
		print( SEVERE, "Position of first MW pulse can't be realized, "
			   "must shift all pulses by %s.\n", rb_pulser_ptime( shift ) );

		for ( cp = rb_pulser_Pulses; cp != NULL; cp = cp->next )
			cp->pos += shift;
	}

	delay = Ticks_rnd( p->pos / rb_pulser.timebase );

	if( card->delay != delay || ! card->was_active )
		card->needs_update = SET;

	card->delay = delay;
	card->is_active = SET;
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

void rb_pulser_delay_card_setup( void )
{
	RULBUS_DELAY_CARD *card, *cc;
	FUNCTION *f;
	PULSE *p;
	double start, delta, shift;
	Ticks dT;
	int i, j, k;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rb_pulser.function + i;

		if ( ! f->is_used )
			continue;

		/* Declare all delay cards associated with the function as inactive */

		for ( card = f->delay_card; card != NULL; card = card->next )
		{
			card->was_active = card->is_active;
			card->needs_update = card->is_active = UNSET;
		}

		/* Loop over all active pulses of the function (which are already
		   ordered by start position */

		for ( card = f->delay_card, j = 0; j < f->num_active_pulses; j++ )
		{
			p = f->pulses[ j ];

			/* If no cards are left for the function we have too many pulses */

			if ( card == NULL )
			{
				print( FATAL, "Too many active pulses for function '%s'.\n",
					   f->name );
				THROW( EXCEPTION );
			}

			/* All execpt the first MW pulse have a previous delay card which
			   isn't the delay card for the inital delay and which must be set
			   to make the pulse start at the correct moment */

			if ( f->self != PULSER_CHANNEL_MW ||
				 ( f->self == PULSER_CHANNEL_MW && j != 0 ) )
			{
				/* Find out the first possible moment the pulse could start
				   at */

				for ( cc = card; cc != NULL; cc = cc->prev )
					start = cc->delay * rb_pulser.timebase + MIN_DELAY;
				start += 2 * MIN_DELAY;

				/* The synthesizer adds another intrinsic delay of about 50 ns
				   for the RF channel */

				if ( f->self == PULSER_CHANNEL_RF )
					start += SYNTHESIZER_DELAY;

				/* Check that the pulse doesn't start earlier */

				delta = p->pos + f->delay - start;

				if ( delta < 0.0 )
				{
					if ( j == 0 )
						print( FATAL, "Pulse #%ld of function '%s' starts too "
							   "early.\n", p->num, f->name );
					else
						print( FATAL, "Pulse #%ld of function '%s' gets too "
							   "near to pulse #%ld.\n", p->num, f->name,
							   f->pulses[ j - 1 ]->num );
					THROW( EXCEPTION );
				}

				/* Now calculate how we have to set up the card, if necessary
				   shift the pulse position */

				delta -= MIN_DELAY;

				if ( f->self == PULSER_CHANNEL_RF )
					delta -= SYNTHESIZER_DELAY;

				dT = Ticks_rnd( delta / rb_pulser.timebase );
				shift = dT * rb_pulser.timebase - delta;
			
				if ( fabs( shift ) > PRECISION * rb_pulser.timebase )
				{
					print( SEVERE, "Position of pulse #%ld of function '%s' "
						   "can't be realized, must shift it (and following "
						   "pulses) by %s.\n", rb_pulser_ptime( shift ) );

					for ( k = j; j < f->num_active_pulses; k++ )
						f->pulses[ k ]->pos += shift;
				}

				if ( card->delay != dT || ! card->was_active )
					card->needs_update = SET;
				card->delay = dT;
				card->is_active = SET;
				
				/* Some channels have no card for the pulse itself, here we
				   just store the length and lets deal with this later */

				if ( ( card = card->next ) == NULL )
				{
					f->last_pulse_len = p->len * rb_pulser.timebase;
					continue;
				}
			}

			if ( card->delay != p->len || ! card->was_active )
				card->needs_update = SET;
			card->delay = p->len;
			card->is_active = SET;
			card = card->next;
		}
	}
}


/*------------------------------------------------------------------------
 * Function is called after the test run and experiments to reset all the
 * variables describing the state of the pulser to their initial values.
 *------------------------------------------------------------------------*/

void rb_pulser_full_reset( void )
{
	PULSE *p = rb_pulser_Pulses;


	while ( p != NULL )
	{
		p->pos     = p->initial_pos;
		p->is_pos  = p->initial_is_pos;
		p->len     = p->initial_len;
		p->is_len  = p->initial_is_len;
		p->dpos    = p->initial_dpos;
		p->is_dpos = p->initial_is_dpos;
		p->dlen    = p->initial_dlen;
		p->is_dlen = p->initial_is_dlen;

		p->is_active = IS_ACTIVE( p );

		p = p->next;
	}
}


/*------------------------------------------------*/
/*------------------------------------------------*/

void rb_pulser_seq_length_check( void )
{
	int i;
	FUNCTION *f;
	RULBUS_DELAY_CARD *card;
	double max_seq_len = 0.0, seq_len;


	if ( rb_pulser.trig_in_mode == EXTERNAL )
		return;

	/* Determine length of sequence lengths of the individual functions */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = rb_pulser.function + i;

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used )
			continue;

		seq_len = 0.0;
		for ( card = f->delay_card; card != NULL && card->is_active;
			  card = card->next )
			seq_len += card->delay * rb_pulser.timebase + MIN_DELAY;

		max_seq_len = d_max( max_seq_len, seq_len );
	}

	max_seq_len +=   delay_card[ INIT_DELAY ].delay * rb_pulser.timebase
				   + MIN_DELAY;

	if ( max_seq_len > rb_pulser.rep_time )
	{
		print( FATAL, "Pulse sequence is longer than the experiment "
			   "repetition time.\n" );
		THROW( EXCEPTION );
	}
}


/*-------------------------------------------------------------------*
 * Changes the pulse pattern in all channels so that the data in the
 * pulser get in sync with the its internal representation.
 * 'flag' is set during the test run.
 *-------------------------------------------------------------------*/

static void rb_pulser_commit( bool flag )
{
	RULBUS_DELAY_CARD *card;
	int i;


	if ( flag )
	{
		if ( rb_pulser.dump_file != NULL )
			rb_pulser_write_pulses( rb_pulser.dump_file );
		if ( rb_pulser.show_file != NULL )
			rb_pulser_write_pulses( rb_pulser.show_file );
		return;
	}

	for ( card = delay_card + INIT_DELAY, i = 0; i < NUM_DELAY_CARDS;
		  card++, i++ )
	{
		if ( ! card->needs_update )
			continue;

		if ( ! card->is_active )
		{
			rb_pulser_delay_card_state( card->handle, STOP );
			continue;
		}

		rb_pulser_delay_card_delay( card->handle, card->delay );

		if ( ! card->was_active )
			rb_pulser_delay_card_state( card->handle, START );
	}
}


/*----------------------------------------------------------------*
 * Function for telling the synthesizer about the RF pulse length
 *----------------------------------------------------------------*/

static void rb_pulser_rf_pulse( void )
{
	FUNCTION *f = rb_pulser.function + PULSER_CHANNEL_RF;
	PULSE *p = f->pulses[ 0 ];
	Var *Func_ptr;
	int acc;


	if ( ! f->is_used || ! p->is_active )
		return;

	if ( ( Func_ptr = func_get( rb_pulser.synth_pulse_width, &acc ) ) == NULL )
	{
		print( FATAL, "Function for setting synthesizer pulse length is not "
			   "available.\n" );
		THROW( EXCEPTION );
	}

	vars_push( FLOAT_VAR, f->last_pulse_len );
	vars_pop( func_call( Func_ptr ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
