/*
  $Id$

  Copyright (c) 2001 Jens Thoms Toerring

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


#include "hfs9000.h"


static void hfs9000_basic_pulse_check( void );
static void hfs9000_basic_functions_check( void );
static void hfs9000_pulse_start_setup( void );


/*---------------------------------------------------------------------------
  Function does everything that needs to be done for checking and completing
  the internal representation of the pulser at the start of a test run.
---------------------------------------------------------------------------*/

void hfs9000_init_setup( void )
{
	hfs9000_basic_pulse_check( );
	hfs9000_basic_functions_check( );
	hfs9000_pulse_start_setup( );
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void hfs9000_basic_pulse_check( void )
{
	PULSE *p;
	FUNCTION *f;



	if ( hfs9000_Pulses == NULL )
	{
		eprint( SEVERE, UNSET, "%s: No pulses have been defined.\n",
				pulser_struct.name );
		return;
	}

	for ( p = hfs9000_Pulses; p != NULL; p = p->next )
	{
		p->is_active = SET;

		/* Check the pulse function */

		if ( ! p->is_function )
		{
			eprint( FATAL, UNSET, "%s: Pulse %ld is not associated with a "
					"function.\n", pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->function->is_used )
		{
			eprint( FATAL, UNSET, "%s: The function `%s' of pulse %ld hasn't "
					"been declared in the ASSIGNMENTS section.\n",
					pulser_struct.name, Function_Names[ p->function->self ],
					p->num );
			THROW( EXCEPTION );
		}

		f = p->function;

		if ( p->function->channel == NULL )
		{
			eprint( FATAL, UNSET, "%s: No channel has been set for function "
					"`%s' used for pulse %ld.\n", pulser_struct.name,
					Function_Names[ p->function->self ], p->num );
			THROW( EXCEPTION );
		}
		else
			p->channel = p->function->channel;

		/* Check start position and pulse length */

		if ( ! p->is_pos || ! p->is_len || p->len == 0 )
			p->is_active = UNSET;

		if ( p->is_pos && p->is_len && p->len != 0 &&
			 p->pos + p->len + p->function->delay > MAX_PULSER_BITS )
		{
			eprint( FATAL, UNSET, "%s: Pulse %ld does not fit into the "
					"pulsers memory. You could try a longer pulser time "
					"base.\n", pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( p->is_active )
			p->was_active = p->has_been_active = SET;

		f->pulses = T_realloc( f->pulses,
							   ( f->num_pulses + 1 ) * sizeof( PULSE * ) );
		f->pulses[ f->num_pulses++ ] = p;
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void hfs9000_basic_functions_check( void )
{
	FUNCTION *f;
	int i;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &hfs9000.function[ i ];

		/* Don't do anything if the function has never been mentioned */

		if ( ! f->is_used )
			continue;

		/* Phase functions are not supported in this driver... */

		fsc2_assert( i != PULSER_CHANNEL_PHASE_1 &&
					 i != PULSER_CHANNEL_PHASE_2 );

		/* Check if the function has pulses assigned to it */

		if ( ! f->is_needed )
		{
			eprint( WARN, UNSET, "%s: No pulses have been assigned to "
					"function `%s'.\n", pulser_struct.name,
					Function_Names[ i ] );
			f->is_used = UNSET;

			if ( f->channel != NULL )
			{
				f->channel->function = NULL;
				f->channel = NULL;
			}

			continue;
		}

		if ( f->delay < 0 &&
			 ( hfs9000.is_trig_in_mode || hfs9000.trig_in_mode == EXTERNAL ) )
		{
			eprint( FATAL, UNSET, "%s: Negative delay for function `%s' can't "
					"be used with external triggert mode.\n",
					pulser_struct.name, Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}
	}
}


/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

static void hfs9000_pulse_start_setup( void )
{
	FUNCTION *f;
	int i;


	/* Sort the pulses and check that they don't overlap */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &hfs9000.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 i == PULSER_CHANNEL_PHASE_1 ||
			 i == PULSER_CHANNEL_PHASE_2 )
			continue;

		/* Sort the pulses of current the function */

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ),
			   hfs9000_start_compare );

		hfs9000_do_checks( f );
	}
}
