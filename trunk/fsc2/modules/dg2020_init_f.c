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


#include "dg2020_f.h"


static void dg2020_basic_pulse_check( void );
static void dg2020_basic_functions_check( void );
static void dg2020_distribute_channels( void );
static void dg2020_pulse_start_setup( void );
static void dg2020_create_phase_pulses( FUNCTION *f );
static PULSE *dg2020_new_phase_pulse( FUNCTION *f, PULSE *p, int nth,
									  int pos, int pod );
static void dg2020_set_phase_pulse_pos_and_len( FUNCTION *f, PULSE *np,
												PULSE *p, int nth );


/*---------------------------------------------------------------------------
  Function does everything that needs to be done for checking and completing
  the internal representation of the pulser at the start of a test run.
---------------------------------------------------------------------------*/

void dg2020_init_setup( void )
{
	dg2020_basic_pulse_check( );
	dg2020_basic_functions_check( );
	dg2020_distribute_channels( );
	dg2020_pulse_start_setup( );
}


/*--------------------------------------------------------------------------
  Function runs through all pulses and checks that at least:
  1. the function is set and the function itself has been declared in the
     ASSIGNMENTS section
  2. the start position is set
  3. the length is set (only exception: if pulse function is DETECTION
     and no length is set it's more or less silently set to one tick)
  4. the sum of function delay, pulse start position and length does not
     exceed the pulsers memory
--------------------------------------------------------------------------*/

static void dg2020_basic_pulse_check( void )
{
	PULSE *p;


	for ( p = dg2020_Pulses; p != NULL; p = p->next )
	{
		p->is_active = SET;

		/* Check the pulse function */

		if ( ! p->is_function )
		{
			print( FATAL, "Pulse %ld is not associated with a function.\n",
				   p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->function->is_used )
		{
			print( FATAL, "Function '%s' of pulse %ld hasn't been declared in "
				   "the ASSIGNMENTS section.\n",
				   Function_Names[ p->function->self ], p->num );
			THROW( EXCEPTION );
		}

		/* Check the start position */

		if ( ! p->is_pos )
			p->is_active = UNSET;

		/* Check the pulse length */

		if ( ! p->is_len )
		{
			if ( p->is_pos &&
				 p->function == &dg2020.function[ PULSER_CHANNEL_DET ] )
			{
				print( WARN, "Length of detection pulse %ld is being set to "
					   "%s.\n", p->num, dg2020_pticks( 1 ) );
				p->len = 1;
				p->is_len = SET;
			}
			else
				p->is_active = UNSET;
		}

		/* Check that the pulse fits into the pulsers memory
		   (If you check the following line real carefully, you will find that
		   one less than the number of bits in the pulser channel is allowed -
		   this is due to a bug in the pulsers firmware: if the very first bit
		   in any of the channels is set to high the pulser outputs a pulse of
		   250 us length before starting to output the real data in the
		   channel, thus the first bit has always to be set to low, i.e. must
		   be unused. But this is only the 'best' case when the pulser is used
		   in repeat mode, in single mode setting the first bit of a channel
		   leads to an obviously endless high pulse, while not setting the
		   first bit keeps the pulser from working at all...) */

		if ( p->is_pos && p->is_len &&
			 p->pos + p->len + p->function->delay >= MAX_PULSER_BITS )
		{
			print( FATAL, "Pulse %ld does not fit into the pulsers memory. "
				   "You could try a longer pulser time base.\n", p->num );
			THROW( EXCEPTION );
		}

		/* Check phase cycling of pulse */

		if ( p->pc && p->function->phase_func == NULL )
		{
			print( FATAL, "Pulse %ld needs phase cycling but its function "
				   "(%s) isn't associated with a phase function.\n",
				   p->num, Function_Names[ p->function->self ] );
			THROW( EXCEPTION );
		}

		if ( p->is_active )
			p->was_active = p->has_been_active = SET;
	}
}


/*-----------------------------------------------------------------------------
  Function does a consistency check concerning the functions:
  1. Each function mentioned in the ASSIGMENTS section should have pulses
     assigned to it, otherwise it's not really needed. The exceptions
	 are the PHASES functions where the pulses are created automatically. On
	 the other hand, the PHASES functions aren't needed if no phase sequences
	 have been defined.
  2. Each function needs a pod again with the exception of the PHASES
     functions that need two.
  Phase functions that are associated with useless functions or functions
  that don't have pulses with phase cycling have to be removed.
  Next, a list of pulses for each channel is set up. Now we also can count
  the number of channels needed for the function.
  Each time when we find that a function isn't needed we always have to put
  back all channels that might have been assigned to the function. Also, if
  there are more channels assigned than needed, the superfluous ones are put
  back into the free pool.
-----------------------------------------------------------------------------*/

static void dg2020_basic_functions_check( void )
{
	int i, j;
	FUNCTION *f;
	PULSE *cp;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Don't do anything if the function has never been mentioned */

		if ( ! f->is_used )
			continue;

		/* Check if the function that has been defined is really needed (i.e.
		   has pulses assigned to it) - exceptions are the PHASE functions
		   that get pulses assigned to them automatically and that are only
		   needed if pulse sequences have been defined. If a function isn't
		   needed also put back into the pool the channels that may have been
		   assigned to the function. */

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			/* Function has no pulses ? */

			if ( f->is_used && ! f->is_needed )
			{
				print( WARN, "No pulses have been assigned to function "
					   "'%s'.\n", Function_Names[ i ] );
				f->is_used = UNSET;

				for ( j = 0; j < f->num_channels; j++ )
				{
					f->channel[ j ]->function = NULL;
					f->channel[ j ] = NULL;
				}
				f->num_channels = 0;

				continue;
			}
		}
		else
		{
			f->num_pulses = 0;

			/* No phase sequences have been defined ? */

			if ( PSeq == NULL )
			{
				print( WARN, "Phase functions '%s' isn't needed, because no "
					   "phase sequences have been defined.\n",
					   Function_Names[ f->self ] );
				f->is_used = UNSET;

				for ( j = 0; j < f->num_channels; j++ )
				{
					f->channel[ j ]->function = NULL;
					f->channel[ j ] = NULL;
				}
				f->num_channels = 0;

				continue;
			}

			/* Phase function isn't associated with a function ? */

			if ( f->phase_func == NULL )
			{
				print( WARN, "Phase function '%s' isn't needed, because it's "
					   "not associated with a function.\n",
					   Function_Names[ f->self ] );
				f->is_used = UNSET;

				for ( j = 0; j < f->num_channels; j++ )
				{
					f->channel[ j ]->function = NULL;
					f->channel[ j ] = NULL;
				}
				f->num_channels = 0;

				continue;
			}

			/* No second pod assigned to phase function ? */

			if ( f->pod2 == NULL)
			{
				print( FATAL, "Function '%s' needs two pods assigned to it.\n",
					   Function_Names[ i ] );
				THROW( EXCEPTION );
			}

			/* No phase setup has been done for this phase function */

			if ( ! f->is_phs )
			{
				print( FATAL, "Missing data on how to convert the pulse "
					   "phases into pod outputs for function '%s'. Add a "
					   "%s_SETUP command in the ASSIGNMENTS section.\n",
						Function_Names[ i ], Function_Names[ i ] );
				THROW( EXCEPTION );
			}
		}

		/* Make sure there's a pod assigned to the function */

		if ( f->pod == NULL )
		{
			print( FATAL, "No pod has been assigned to function '%s'.\n",
				   Function_Names[ i ] );
			THROW( EXCEPTION );
		}

		/* Assemble a list of all pulses assigned to the function and, while
		   doing so, also count the number of channels really needed. Finally
		   set a flag that says if the function needs phase cycling */

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			f->num_needed_channels = 1;
			f->needs_phases = UNSET;
			f->num_active_pulses = 0;

			for ( cp = dg2020_Pulses; cp != NULL; cp = cp->next )
			{
				if ( cp->function != f )
					continue;

				f->num_pulses++;
				f->pulses = T_realloc( f->pulses,
									   f->num_pulses * sizeof( PULSE * ) );
				f->pulses[ f->num_pulses - 1 ] = cp;

				if ( cp->is_active )
					f->num_active_pulses++;

				if ( cp->pc )
					f->needs_phases = SET;
			}

			if ( ! f->needs_phases && f->phase_func != NULL )
			{
				print( WARN, "Function '%s' is associated with phase function "
					   "'%s' but none of its pulses need phase cycling.\n",
						Function_Names[ f->self ],
						Function_Names[ f->phase_func->self ] );

				for ( j = 0; j < f->phase_func->num_channels; j++ )
				{
					f->phase_func->channel[ j ]->function = NULL;
					f->phase_func->channel[ j ] = NULL;
				}

				f->phase_func->is_used = UNSET;
				f->phase_func->num_channels = 0;
				f->phase_func = NULL;
			}

			fsc2_assert( f->num_pulses > 0 );    /* paranoia ? */
		}
		else
		{
			f->needs_phases = UNSET;
			f->num_needed_channels = 2 * PSeq->len;
		}

		/* Put channels not needed back into the pool */

		if ( f->num_channels > f->num_needed_channels )
			print( WARN, "For function '%s' only %d channel%s needed instead "
				   "of the %d assigned to it.\n",
				   Function_Names[ i ], f->num_needed_channels,
				   f->num_needed_channels == 1 ? " is" : "s are",
				   f->num_channels );

		while ( f->num_channels > f->num_needed_channels )
		{
			f->channel[ --f->num_channels ]->function = NULL;
			f->channel[ f->num_channels ] = NULL;
		}
	}

	/* Now we've got to run a second time through the pulses. We put back
       channels for functions that we found to be useless and also the
       channels for phase functions associated with useless functions */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		if ( f->is_used )
			continue;

		for ( j = 0; j < f->num_channels; j++ )
		{
			f->channel[ j ]->function = NULL;
			f->channel[ j ] = NULL;
		}
		f->num_channels = 0;

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 &&
			 f->phase_func != NULL )
		{
			print( WARN, "Phase function '%s' isn't needed because function "
				   "'%s' it is associated with is not used.\n",
					Function_Names[ f->phase_func->self ],
					Function_Names[ f->self ] );

			for ( j = 0; j < f->phase_func->num_channels; j++ )
			{
				f->phase_func->channel[ j ]->function = NULL;
				f->phase_func->channel[ j ] = NULL;
			}
			f->phase_func->num_channels = 0;

			f->phase_func->is_used = UNSET;
		}
	}
}


/*---------------------------------------------------------------------------
  Function checks first if there are enough pulser channels and than assigns
  channels to functions that haven't been assigned as many as they need
----------------------------------------------------------------------------*/

static void dg2020_distribute_channels( void )
{
	int i;
	FUNCTION *f;


	/* First count the number of channels we really need for the experiment */

	dg2020.needed_channels = 0;
	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];
		if ( ! f->is_used )
			continue;
		dg2020.needed_channels += f->num_needed_channels;
	}

	/* Now we've got to check if there are at least as many channels as are
	   needed for the experiment */

	if ( dg2020.needed_channels > MAX_CHANNELS )
	{
		print( FATAL, "The experiment would require %d pulser channels but "
			   "only %d are available.\n",
				dg2020.needed_channels, ( int ) MAX_CHANNELS );
		THROW( EXCEPTION );
	}

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Don't do anything for unused functions */

		if ( ! f->is_used )
			continue;

		/* If the function needs more channels than it got assigned get them
		   now from the pool of free channels */

		while ( f->num_channels < f->num_needed_channels )
		{
			f->channel[ f->num_channels ] = dg2020_get_next_free_channel( );
			f->channel[ f->num_channels ]->function = f;
			f->num_channels++;
		}
	}
}


/*-----------------------------------------------------------------------------
  In this function the pulses for all the functions are sorted and further
  consistency checks are done.
-----------------------------------------------------------------------------*/

static void dg2020_pulse_start_setup( void )
{
	FUNCTION *f;
	int i, j;


	/* Sort the pulses and check that they don't overlap */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ),
			   dg2020_start_compare );

		dg2020_do_checks( f );

		for ( j = 0; j < f->num_pulses; j++ )
			f->pulses[ j ]->channel = f->channel[ 0 ];

		if ( f->needs_phases )
			dg2020_create_phase_pulses( f->phase_func );
	}
}


/*-----------------------------------------------------------------------------
  In this function the additional pulses needed for phase cycling in one of
  the phase functions are created. As argument only pointers to the phase
  functions are allowed.
-----------------------------------------------------------------------------*/

static void dg2020_create_phase_pulses( FUNCTION *f )
{
	int i, j, l;
	PULSE *p;


	for ( i = 0; i < f->phase_func->num_pulses; i++ )
	{
		f->num_active_pulses = 0;

		for ( j = 0; j < PSeq->len; j++ )
			for ( l = 0; l < 2; l++ )
			{
				p = dg2020_new_phase_pulse( f, f->phase_func->pulses[ i ], i,
											j, l );
				if ( p != NULL )
				{
					f->pulses = T_realloc( f->pulses, ++f->num_pulses
										   * sizeof( PULSE * ) );
					f->pulses[ f->num_pulses - 1 ] = p;

					if ( p->is_active )
					{
						p->was_active = p->has_been_active = SET;
						f->num_active_pulses++;
					}
				}
			}
	}
}


/*---------------------------------------------------------------------------
  f = pointer to the phase function the new pulse belongs to
  p = pointer to pulse the new phase pulse is associated with
  pos = position of p in the list of pulses of this function
  pod = 0 / 1: the pod for the new phase pulse
---------------------------------------------------------------------------*/

static PULSE *dg2020_new_phase_pulse( FUNCTION *f, PULSE *p, int nth,
									  int pos, int pod )
{
	PULSE *np, *cp;
	int type;


	/* Figure out the phase type - its stored in the Phase_Sequence the entry
	   in the referenced pulses points to. For pulses that don't need phase
	   cycling but get it anyway, because they belong to a function that has
	   other pulses that need phase cycling, the X-phase is used. */

	type = p->pc != NULL ? p->pc->sequence[ pos ] : PHASE_PLUS_X;

	/* No pulse is needed if for the current phase type / pod combination no
	   voltage is needed */

	if ( f->phs.var[ type ][ pod ] == 0 )
		return NULL;

	/* Get memory for a new pulse and append it to the list of pulses */

	for ( cp = np = dg2020_Pulses; np != NULL; np = np->next )
		cp = np;

	np = T_malloc( sizeof( PULSE ) );

	np->prev = cp;
	cp->next = np;

	np->next = NULL;
	np->pc = NULL;

	/* These 'artifical' pulses get negative numbers */

	np->num = ( np->prev->num >= 0 ) ? -1 : np->prev->num - 1;

	/* Its function is the phase function it belongs to */

	np->is_function = SET;
	np->function = f;

	/* Set the channel it belongs to */

	np->channel = f->channel[ 2 * pos + pod ];

	/* Its only active if the pulse it belongs to is active */

	np->was_active = np->is_active = p->is_active;
	np->for_pulse = p;

	/* Calculate its position and length if possible */

	if ( np->is_active )
		dg2020_set_phase_pulse_pos_and_len( f, np, p, nth );

	/* it doesn't needs updates yet */

	np->is_old_pos = np->is_old_len = UNSET;
	np->needs_update = UNSET;

	np->initial_pos = np->pos;
	np->initial_is_pos = SET;
	np->initial_pos = np->pos;
	np->initial_is_len = SET;

	return np;
}


/*---------------------------------------------------------------------------
---------------------------------------------------------------------------*/

static void dg2020_set_phase_pulse_pos_and_len( FUNCTION *f, PULSE *np,
												PULSE *p, int nth )
{
	PULSE **pppl;                 /* list of phase pulses for previous pulse */
	int ppp_num;                  /* and the length of this list */
	PULSE *pp, *pn;
	int i;
	static PULSE *for_pulse;


	if ( nth == 0 )                           /* for first pulse ? */
	{
		/* We try to start the phase pulse for the first pulse as early as
		   possible, i.e. even within the delay for the phase function */

		if ( p->pos - f->delay < f->psd )
		{
			print( FATAL, "Pulse %ld starts too early to allow setting of a "
				   "phase pulse.\n", p->num );
			THROW( EXCEPTION );
		}

		np->pos = np->initial_pos = - f->delay;
	}
	else
	{
		pp = p->function->pulses[ nth - 1 ];

		/* If the phase switch delay does not fit between the pulse it gets
		   associated with and its predecessor we have to complain (except
		   when both pulses use the same phase sequence or none at all) */

		if ( p->pos - pp->pos - pp->len < f->psd && p->pc != pp->pc )
		{
			print( FATAL, "Distance between pulses %ld and %ld is too small "
				   "to allow setting of phase pulses.\n", pp->num, p->num );
			THROW( EXCEPTION );
		}

		/* Try to start the phase pulse as late as possible, i.e. just the
		   phase switch delay before the associated pulse, because the
		   probablity is high that the preceeding pulse is going to be
		   shifted to later times or is lenghtened */

		np->pos = np->initial_pos = p->pos - f->psd;

		/* If this is too near to the preceeding pulse leave out the grace
		   period, and if this still is too near to the previous pulse
		   complain (except when both pulses use the same phase sequence since
		   than both phase pulses get merged into one) */

		if ( np->pos < pp->pos + pp->len + dg2020.grace_period )
		{
			np->pos += dg2020.grace_period;

			if ( np->pos < pp->pos + pp->len + dg2020.grace_period &&
				 p->pc != pp->pc && for_pulse != p )
			{
				print( SEVERE, "Pulses %ld and %ld are so close that problems "
					   "with phase switching may result.\n", pp->num, p->num );
				for_pulse = p;
			}
		}

		/* Adjust the length of all phase pulses associated with the
		   preceeding pulse */

		if ( dg2020_find_phase_pulse( pp, &pppl, &ppp_num ) )
		{
			for ( i = 0; i < ppp_num; i++ )
			{
				if ( pppl[ i ]->is_len )
					pppl[ i ]->len = np->pos - pppl[ i ]->pos;

				if ( pppl[ i ]->pos + pppl[ i ]->len <
					 pppl[ i ]->for_pulse->pos + pppl[ i ]->for_pulse->len &&
					 pppl[ i ]->for_pulse->pc != pppl[ i ]->for_pulse->pc )
				{
					print( FATAL, "Distance between pulses %ld and %ld is too "
						   "small to allow setting of phase pulses.\n",
						   p->num, pppl[ i ]->for_pulse->num );
					THROW( EXCEPTION );
				}

				if ( pppl[ i ]->pos + pppl[ i ]->len <
					 pppl[ i ]->for_pulse->pos + pppl[ i ]->for_pulse->len
					 + dg2020.grace_period  &&
					 pppl[ i ]->for_pulse->pc != pppl[ i ]->for_pulse->pc &&
					 p != for_pulse )
				{
					print( SEVERE, "Pulses %ld and %ld are so close that "
						   "problems with phase switching may result.\n",
						   p->num, pppl[ i ]->for_pulse->num );
					for_pulse = p;
				}
			}

			T_free( pppl );
		}
	}
	np->is_pos = p->is_pos == SET;

	/* We can't know the maximum possible length of the last phase pulse yet,
	   this will only be known when we figured out the maximum sequence length
	   in the test run, thus we flag our missing knowledge by setting the
	   length to a negative value */

	if ( nth == p->function->num_active_pulses - 1 )  /* last active pulse ? */
		np->len = np->initial_len = -1;
	else
	{
		/* This length is only tentatively and may become shorter when the
		   following phase pulse is set */

		pn = p->function->pulses[ nth + 1 ];
		np->len = np->initial_len =	pn->pos - f->psd - np->pos;

		if ( np->pos + np->len < p->pos + p->len && p->pc != pn->pc )
		{
			print( FATAL, "Distance between pulses %ld and %ld is too small "
				   "to allow setting of phase pulses.\n", p->num, pn->num );
			THROW( EXCEPTION );
		}

		if ( np->pos + np->len < p->pos + p->len + dg2020.grace_period &&
			 p->pc != pn->pc && p != for_pulse )
		{
			print( SEVERE, "Pulses %ld and %ld are so close that problems "
				   "with phase switching may result.\n", p->num, pn->num );
			for_pulse = p;
		}
	}

	np->is_len = p->is_len == SET;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
