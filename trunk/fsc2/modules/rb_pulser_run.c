/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


/*----------------------------------------------------------------------*
 * Function sets the delay for the very first delay card, i.e. the card
 * controllng the delay of the first microwave pulse. If there's no MW
 * pulse the delay for this card is set to the shortest possible time.
 *----------------------------------------------------------------------*/

void rb_pulser_init_delay( void )
{
	FUNCTION *f = rb_pulser.function + PULSER_CHANNEL_MW;
	PULSE *p;
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

	p = f->pulses[ 0 ];

	pos = p->pos + p->function->delay * rb_pulser.timebase - 2 * MIN_DELAY;

	if ( pos < 0.0 )
	{
		print( FATAL, "First MW pulse starts too early.\n" );
		THROW( EXCEPTION );
	}

	shift = Ticks_rnd( pos / rb_pulser.timebase ) * rb_pulser.timebase - pos;

	if ( fabs( shift ) > PRECISION * rb_pulser.timebase )
	{
		print( SEVERE, "Position of first MW pulse not possible, must shift "
			   "it by %s.\n", rb_pulser_ptime( shift ) );
		pos += shift;
	}

	delay = Ticks_rnd( pos / rb_pulser.timebase );

	if( card->delay != delay || ! card->was_active )
		card->needs_update = SET;

	card->delay = delay;
	card->is_active = SET;
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

void rb_pulser_delay_card_setup( void )
{
	RULBUS_DELAY_CARD *card;
	FUNCTION *f;
	PULSE *p;
	volatile double start, delta, shift;
	Ticks dT;
	int i, j;


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

		start =   delay_card[ INIT_DELAY ].delay * rb_pulser.timebase
			    + MIN_DELAY;

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
				/* Find out the first possible moment the following pulse
				   could start at - if there's no following delay card
				   creating the pulse we must not include the delay for this
				   non-existent card. But, on the other hand, for the RF
				   channel we have to include the intrinsic delay of the
				   synthesizer creating the pulse... */

				start += MIN_DELAY;
				if ( card->next != NULL )
					start += MIN_DELAY;
				else if ( f->self == PULSER_CHANNEL_RF )
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

				dT = Ticks_rnd( delta / rb_pulser.timebase );
				shift = dT * rb_pulser.timebase - delta;
			
				if ( fabs( shift ) > PRECISION * rb_pulser.timebase )
					print( SEVERE, "Position of pulse #%ld of function '%s' "
						   "not possible, must shift it by %s.\n", p->num,
						   f->name, rb_pulser_ptime( shift ) );

				if ( card->delay != dT || ! card->was_active )
					card->needs_update = SET;
				card->delay = dT;
				card->is_active = SET;
				
				start += card->delay * rb_pulser.timebase;

				/* Some channels have no card for the pulse itself, here we
				   just store the length and lets deal with this later */

				if ( ( card = card->next ) == NULL )
				{
					f->last_pulse_len = p->len * rb_pulser.timebase;
					continue;
				}
			}
			else
				start += MIN_DELAY;

			if ( card->delay != p->len || ! card->was_active )
				card->needs_update = SET;
			card->delay = p->len;
			start += card->delay * rb_pulser.timebase;
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


/*------------------------------------------------*
 *------------------------------------------------*/

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

	for ( card = delay_card + INIT_DELAY, i = INIT_DELAY; i < NUM_DELAY_CARDS;
		  card++, i++ )
	{
/*
		if ( ! card->needs_update )
			continue;
*/
		if ( ! card->is_active )
		{
			rb_pulser_delay_card_state( card->handle, STOP );
			continue;
		}

		rb_pulser_delay_card_delay( card->handle, card->delay );

//		if ( ! card->was_active )
			rb_pulser_delay_card_state( card->handle, START );
	}
}


/*----------------------------------------------------------------*
 * Function for telling the synthesizer about the RF pulse length
 *----------------------------------------------------------------*/

static void rb_pulser_rf_pulse( void )
{
	FUNCTION *f = rb_pulser.function + PULSER_CHANNEL_RF;
	PULSE *p;
	Var *Func_ptr;
	int acc;


	if ( ! f->is_used )
		return;

	p = f->pulses[ 0 ];

	if ( p->is_active )
	{
		if ( ( Func_ptr = func_get( rb_pulser.synth_pulse_width, &acc ) )
																	  == NULL )
		{
			print( FATAL, "Function for setting synthesizer pulse length is "
				   "not available.\n" );
			THROW( EXCEPTION );
		}

		vars_push( FLOAT_VAR, f->last_pulse_len );
		vars_pop( func_call( Func_ptr ) );
	}

	/* Switch synthesizer output on or off if the pulse just became active or
	   inactive */

	if ( ( p->was_active && ! p->is_active ) ||
		 ( ! p->was_active && p->is_active ) )
	{
		if ( ( Func_ptr = func_get( rb_pulser.synth_state, &acc ) ) == NULL )
		{
			print( FATAL, "Function for setting synthesizer output state is "
				   "not available.\n" );
			THROW( EXCEPTION );
		}

		if ( p->was_active && ! p->is_active )
			vars_push( STR_VAR, "OFF" );
		else
			vars_push( STR_VAR, "ON" );

		vars_pop( func_call( Func_ptr ) );

		p->was_active = p->is_active;
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
