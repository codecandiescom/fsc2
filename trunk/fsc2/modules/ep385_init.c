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


static void ep385_basic_pulse_check( void );
static void ep385_basic_functions_check( void );
static void ep385_create_phase_matrix( FUNCTION *f );
static void ep385_setup_channels( void );
static void ep385_pulse_start_setup( void );
static void ep385_channel_start_check( CHANNEL *ch );
static void ep385_defense_shape_init_check( FUNCTION *shape );


/*-----------------------------------------------------------------*/
/* Function does everything that needs to be done for checking and */
/* completing the internal representation of the pulser at the     */
/* start of a test run.                                            */
/*-----------------------------------------------------------------*/

void ep385_init_setup( void )
{
	FUNCTION *f;
	int i, j;


	TRY
	{
		ep385_basic_pulse_check( );
		ep385_basic_functions_check( );
		ep385_setup_channels( );
		ep385_pulse_start_setup( );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		/* Free memory already allocated when the error happend */

		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		{
			f = &ep385.function[ i ];

			if ( f->pulses != NULL )
				f->pulses = PULSE_PP T_free( f->pulses );

			if ( f->pm != NULL )
			{
				for ( j = 0; j < f->pc_len * f->num_channels; j++ )
					if ( f->pm[ j ] != NULL )
						T_free( f->pm[ j ] );
				f->pm = PULSE_PPP T_free( f->pm );
			}
		}

		for ( i = 0; i < MAX_CHANNELS; i++ )
			if ( ep385.channel[ i ].pulse_params != NULL )
				ep385.channel[ i ].pulse_params =
					PULSE_PARAMS_P T_free( ep385.channel[ i ].pulse_params );

		RETHROW( );
	}
}


/*--------------------------------------------------------------------------*/
/* Function checks for all pulses that they have an initialized function,   */
/* sets the channel(s) for the pulses and checks all other pulse parameters */
/*--------------------------------------------------------------------------*/

static void ep385_basic_pulse_check( void )
{
	PULSE *p;
	FUNCTION *f;


	if ( ep385_Pulses == NULL )
	{
		print( SEVERE, "No pulses have been defined.\n" );
		return;
	}

	for ( p = ep385_Pulses; p != NULL; p = p->next )
	{
		p->is_active = SET;

		/* Check the pulse function */

		if ( ! p->is_function )
		{
			print( FATAL, "Pulse #%ld is not associated with a function.\n",
				   p->num );
			THROW( EXCEPTION );
		}

		f = p->function;

		/* Check that there's at least one channel associated with the
		   pulses function */

		if ( f->num_channels == 0 )
		{
			print( FATAL, "No channel has been set for function '%s' used for "
				   "pulse #%ld.\n",
				   Function_Names[ p->function->self ], p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->is_pos || ! p->is_len || p->len == 0 )
			p->is_active = UNSET;

		if ( p->is_active )
			p->was_active = p->has_been_active = SET;
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void ep385_basic_functions_check( void )
{
	FUNCTION *f;
	PULSE *p;
	int i;
	Ticks delay;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;

		/* Don't do anything if the function has never been mentioned */

		if ( ! f->is_used )
			continue;

		/* Phase functions are not supported in this driver... */

		fsc2_assert( i != PULSER_CHANNEL_PHASE_1 &&
					 i != PULSER_CHANNEL_PHASE_2 );

		/* Check if the function has pulses assigned to it */

		if ( ! f->is_needed )
		{
			print( WARN, "No pulses have been assigned to function '%s'.\n",
				   Function_Names[ i ] );
			f->is_used = UNSET;

			continue;
		}

		/* Assemble a list of all pulses assigned to the function and
		   determine the length of the phase cycle for the function */

		f->num_pulses = 0;

		f->pc_len = 1;

		for ( p = ep385_Pulses; p != NULL; p = p->next )
		{
			if ( p->function != f )
				continue;

			f->num_pulses++;
			f->pulses = PULSE_PP T_realloc( f->pulses,
										   f->num_pulses * sizeof *f->pulses );
			f->pulses[ f->num_pulses - 1 ] = p;

			if ( p->pc != NULL )
				f->pc_len = i_max( f->pc_len, p->pc->len );
		}

	}

	if ( ep385.neg_delay )
	{
		delay = MAX_PULSER_BITS;

		for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			if ( ep385.function[ i ].is_used &&
				 delay > ep385.function[ i ].delay )
				delay = ep385.function[ i ].delay;
		if ( delay != 0 )
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
				ep385.function[ i ].delay -= delay;
	}

	/* Create a matrix of pulse lists that contain the pulses for each
	   combination of phase and channel */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
		if ( ep385.function[ i ].is_needed )
			ep385_create_phase_matrix( ep385.function + i );
}


/*-----------------------------------------------------------------------*/
/* Function creates a matrix of pulse lists for each phase (columns) and */
/* each channel (rows) of the function (in case of functions not set up  */
/* for phase cycling this will be just a 1x1 matrix). The pulse lists    */
/* are unsorted, i.e. when using the pulse lists we still have to sort   */
/* them according to there start times and check if they are switched on */
/* and don't overlap.                                                    */
/*-----------------------------------------------------------------------*/

static void ep385_create_phase_matrix( FUNCTION *f )
{
	PULSE *p;
	PULSE **pm_entry;
	int phase_type;
	CHANNEL *ch;
	int i, j, k, l, m;


	f->pm = PULSE_PPP T_malloc( f->pc_len * f->num_channels * sizeof *f->pm );

	for ( j = 0; j < f->pc_len * f->num_channels; j++ )
		f->pm[ j ] = NULL;

	for ( j = 0; j < f->pc_len * f->num_channels; j++ )
	{
		f->pm[ j ] = PULSE_PP T_malloc( ( f->num_pulses + 1 )
										* sizeof **f->pm );

		*f->pm[ j ] = NULL;         /* list is still empty */
	}

	for ( i = 0; i < f->num_pulses; i++ )
	{
		p = f->pulses[ i ];

		/* For pulses without phase cycling we always use the +x-phase */

		if ( p->pc == NULL )
		{
			/* Find the index of the channel for +x-phase in the list of
			   channels of the function (if there is no phase setup for
			   the function this is automatically the only channel of the
			   function) */

			if ( f->phase_setup != NULL )
			{
				/* Check that a +x-phase channel has been set for the
				   function */

				if ( ! f->phase_setup->is_set[ PHASE_PLUS_X ] )
				{
					print( FATAL, "Phase type '%s' is needed for pulse #%ld "
						   "but it hasn't been not defined in a PHASE_SETUP "
						   "command for its function '%s'.\n",
						   Phase_Types[ PHASE_PLUS_X ], p->num,
						   Function_Names[ f->self ] );
					THROW( EXCEPTION );
				}

				f->phase_setup->is_needed[ PHASE_PLUS_X ] = SET;

				for ( l = 0; l < f->num_channels; l++ )
					if ( f->phase_setup->channels[ PHASE_PLUS_X ] ==
						 f->channel[ l ] )
						break;

				fsc2_assert( l < f->num_channels );
			}
			else
				l = 0;

			/* For each of the phase rows add the pulse to the lists
			   of pulses of the +x-channels (i.e. the l.th row)*/

			for ( k = 0; k < f->pc_len; k++ )
			{
				pm_entry = f->pm[ k * f->num_channels + l ];

				/* Find the end of the current pulse list, indicated
				   by a NULL pointer */

				for ( m = 0; pm_entry[ m ] != NULL && m < f->num_pulses;
					  m++ )
					/* empty */ ;

				fsc2_assert( m < f->num_pulses );

				pm_entry[ m ] = p;
				pm_entry[ m + 1 ] = NULL;
			}

			continue;
		}

		/* Now comes the more complicated case: pulses with phase cycling.
		   We loop over all phases of the pulse, i.e. all rows of the phase
		   matrix. */

		for ( k = 0; k < p->pc->len; k++ )
		{
			/* Figure out the current phase of the pulse */

			phase_type = p->pc->sequence[ k ];

			/* Check that a phase setup for this phase has happended */

			if ( ! f->phase_setup->is_set[ phase_type ] )
			{
				print( FATAL, "Phase type '%s' is needed for pulse #%ld "
					   "but it hasn't been not defined in a PHASE_SETUP "
					   "command for its function '%s'.\n",
					   Phase_Types[ phase_type ], p->num,
					   Function_Names[ f->self ] );
				THROW( EXCEPTION );
			}

			/* Figure out in which row the pulse must be appended to
			   the pulse list */

			ch = f->phase_setup->channels[ phase_type ];
			for ( l = 0; l < f->num_channels; l++ )
				if ( ch == f->channel[ l ] )
					break;

			fsc2_assert( l < f->num_channels );

			pm_entry = f->pm[ k * f->num_channels + l ];

			/* Find the end of the current pulse list, indicated by a NULL
			   pointer */

			for ( m = 0; pm_entry[ m ] != NULL && m < f->num_pulses;
				  m++ )
				/* empty */ ;

			fsc2_assert( m < f->num_pulses );

			/* Append the pulse to the current pulse list */

			pm_entry[ m ] = p;
			pm_entry[ m + 1 ] = NULL;
		}
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void ep385_setup_channels( void )
{
	CHANNEL *ch;
	FUNCTION *f;
	PULSE **pm_entry;
	int i, j, k, l, m;


	for ( i = 0; i < MAX_CHANNELS; i++ )
	{
		ch = &ep385.channel[ i ];

		if ( ( f = ch->function ) == NULL )
			continue;

		/* Count the maximum number of pulses that could be appear in the
		   channel by evaluating the phase matrix of the functions the
		   channel is associated with */

		for ( l = 0; l < f->num_channels; l++ )
			if ( ch == f->channel[ l ] )
				break;

		fsc2_assert( l < f->num_channels );

		for ( k = 0; k < f->pc_len; k++ )
		{
			/* Get phase matrix entry for current phase */

			pm_entry = f->pm[ k * f->num_channels + l ];

			/* Count number of pulses for current phase */

			for ( m = 0; pm_entry[ m ] != NULL; m++ )
				/* empty */ ;

			ch->num_pulses = i_max( ch->num_pulses, m );
		}

		/* Create two lists of pulse parameters for the channel */

		if ( ch->num_pulses > 0 )
		{
			ch->pulse_params =
				PULSE_PARAMS_P T_malloc( 2 * ch->num_pulses *
										 sizeof *ch->pulse_params );
			ch->old_pulse_params = ch->pulse_params + ch->num_pulses;

			for ( j = 0; j < ch->num_pulses; j++ )
			{
				ch->pulse_params->pos = ch->pulse_params->len =
					ch->old_pulse_params->pos = ch->old_pulse_params->len = 0;
				ch->pulse_params->pulse = ch->old_pulse_params->pulse = NULL;
			}
		}
		else
		{
			print( WARN, "Channel %d associated with function '%s' is not "
				   "used.\n", ch->self, Function_Names[ ch->function->self ] );
			ch->pulse_params = ch->old_pulse_params = NULL;
		}
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void ep385_pulse_start_setup( void )
{
	FUNCTION *f;
	CHANNEL *ch;
	PULSE **pm_entry;
	PULSE *p;
	int i, j, m;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = ep385.function + i;

		/* Nothing to be done for unused functions and the phase functions */

		if ( ( ! f->is_used && f->num_channels == 0 ) ||
			 i == PULSER_CHANNEL_PHASE_1 ||
			 i == PULSER_CHANNEL_PHASE_2 )
			continue;

		/* Run over all channels associated with the current function and
		   set the pulse pointers, positions and lengths for the current
		   phase */

		for ( j = 0; j < f->num_channels; j++ )
		{
			ch = f->channel[ j ];

			ch->needs_update = SET;

			if ( ch->num_pulses == 0 )
			{
				ch->num_active_pulses = ch->old_num_active_pulses = 0;
				continue;
			}

			pm_entry = f->pm[ f->next_phase * f->num_channels + j ];

			ch->num_active_pulses = 0;
			for ( m = 0; ( p = pm_entry[ m ] ) != NULL; m++ )
				if ( p->is_active )
				{
					ch->pulse_params[ ch->num_active_pulses ].pos =
					ch->old_pulse_params[ ch->num_active_pulses ].pos =
															 p->pos + f->delay;
					ch->pulse_params[ ch->num_active_pulses ].len =
					ch->old_pulse_params[ ch->num_active_pulses ].len = p->len;
					ch->pulse_params[ ch->num_active_pulses ].pulse =
					ch->old_pulse_params[ ch->num_active_pulses ].pulse = p;
					ch->num_active_pulses++;
				}

			ch->old_num_active_pulses = ch->num_active_pulses;

			ep385_channel_start_check( ch );
		}

		if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
			 ep385.function[ PULSER_CHANNEL_DEFENSE ].is_used &&
			 ( ep385.is_shape_2_defense || ep385.is_defense_2_shape ||
			   ep385.function[ PULSER_CHANNEL_TWT ].is_used ||
			   ep385.function[ PULSER_CHANNEL_TWT_GATE ].is_used ) )
			ep385_defense_shape_init_check( f );
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void ep385_channel_start_check( CHANNEL *ch )
{
	PULSE_PARAMS *pp;
	int i;


	/* Check that there aren't more pulses per channel than the pulser can
	   deal with */

	if ( ch->num_active_pulses > MAX_PULSES_PER_CHANNEL )
	{
		print( FATAL, "More than %d pulses (%d) are required on channel %d "
			   "associated with function '%s'.\n", MAX_PULSES_PER_CHANNEL,
			   ch->num_active_pulses, ch->self,
			   Function_Names[ ch->function->self ] );
		THROW( EXCEPTION );
	}

	qsort( ch->pulse_params, ch->num_active_pulses,
		   sizeof *ch->pulse_params, ep385_pulse_compare );

	for ( i = 0; i < ch->num_active_pulses; i++ )
	{
		pp = ch->pulse_params + i;
		if ( pp->pos + pp->len > MAX_PULSER_BITS )
		{
			print( FATAL, "Pulse #%ld of function '%s' does not fit into the "
				   "pulsers memory.\n",
				   pp->pulse->num, Function_Names[ ch->function->self ] );
			THROW( EXCEPTION );
		}

		if ( i == ch->num_active_pulses - 1 )
			continue;

		if ( pp->pos + pp->len == ch->pulse_params[ i + 1 ].pos )
		{
			print( FATAL, "Pulses %ld and %ld of function '%s' are not "
				   "separated.\n", pp->pulse->num,
				   ch->pulse_params[ i + 1 ].pulse->num,
				   Function_Names[ ch->function->self ] );
			THROW( EXCEPTION );
		}

		if ( pp->pos + pp->len > ch->pulse_params[ i + 1 ].pos )
		{
			print( FATAL, "Pulses %ld and %ld of function '%s' overlap.\n",
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

static void ep385_defense_shape_init_check( FUNCTION *shape )
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
				print( SEVERE, "Distance between PULSE_SHAPE pulse #%ld "
					   "and DEFENSE pulse #%ld is shorter than %s.\n",
					   shape_p->num, defense_p->num, ep385_ptime(
						   ep385_ticks2double( ep385.shape_2_defense ) ) );
				ep385.shape_2_defense_too_near = SET;

			}

			if ( defense_p->pos < shape_p->pos &&
				 defense_p->pos + defense_p->len + ep385.defense_2_shape >
				 shape_p->pos )
			{
				print( SEVERE, "Distance between DEFENSE pulse #%ld and "
					   "PULSE_SHAPE pulse #%ld is shorter than %s.\n",
					   defense_p->num, shape_p->num, ep385_ptime(
						   ep385_ticks2double( ep385.defense_2_shape ) ) );
				ep385.defense_2_shape_too_near = SET;
			}
		}
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
