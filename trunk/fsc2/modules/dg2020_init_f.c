/*
  $Id$
*/



#include "dg2020.h"



/*---------------------------------------------------------------------------
  Function does everything that needs to be done for checking and completing
  the internal representation of the pulser at the start of a test run.
---------------------------------------------------------------------------*/

void init_setup( void )
{
	basic_pulse_check( );
	basic_functions_check( );
	distribute_channels( );
	pulse_start_setup( );
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

void basic_pulse_check( void )
{
	PULSE *p;


	for ( p = Pulses; p != NULL; p = p->next )
	{
		p->is_active = SET;

		/* Check the pulse function */

		if ( ! p->is_function )
		{
			eprint( FATAL, "DG2020: Pulse %ld has no function assigned to "
					"it.\n", p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->function->is_used )
		{
			eprint( FATAL, "DG2020: The function `%s' of pulse %ld hasn't "
					"been declared in the ASSIGNMENTS section.\n",
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
				eprint( WARN, "DG2020: Length of detection pulse %ld is being "
						"set to %s\n", p->num, ptime( 1 ) );
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

		if ( p->pos + p->len + p->function->delay >= MAX_PULSER_BITS )
		{
			eprint( FATAL, "DG2020: Pulse %ld does not fit into the pulsers "
					"memory. Maybe, you could try a longer pulser time "
					"base.\n", p->num );
			THROW( EXCEPTION );
		}

		/* Check phase cycling of pulse */

		if ( p->pc )
		{
			if ( p->function->phase_func == NULL )
			{
				eprint( FATAL, "DG2020: Pulse %ld needs phase cycling but its "
						"function (%s) isn't associated with a phase "
						"function.\n", p->num,
						Function_Names[ p->function->self ] );
				THROW( EXCEPTION );
			}
		}

		if ( p->is_active )
			p->has_been_active = SET;
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

void basic_functions_check( void )
{
	int i, j;
	FUNCTION *f;
	PULSE *cp;
	bool need_phases;


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
				eprint( WARN, "DG2020: No pulses have been assigned to "
						"function `%s'.\n", Function_Names[ i ] );
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
			/* No phase sequences have been defined ? */

			if ( PSeq == NULL )
			{
				eprint( WARN, "DG2020: Phase functions `%s' isn't be needed, "
						"because no phase sequences have been defined.\n",
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
				eprint( WARN, "DG2020: Phase function `%s' isn't be needed, "
						"because it's not associated with a function.\n",
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

			/* No secoond pod assigned to phase function ? */

			if ( f->pod2 == NULL)
			{
				eprint( FATAL, "DG2020: Function `%s' needs two pods "
						"assigned to it.\n", Function_Names[ i ] );
				THROW( EXCEPTION );
			}

			/* No phase setup has been done for this phase function */

			if ( ! f->is_phs )
			{
				eprint( FATAL, "DG2020: Missing data on how to convert the "
						"pulse phases into pod outputs for function `%s'. "
						"Add a %s_SETUP command in the ASSIGNMENTS section.\n",
						Function_Names[ i ], Function_Names[ i ] );
				THROW( EXCEPTION );
			}
		}

		/* Make sure there's a pod assigned to the function */

		if ( f->pod == NULL )
		{
			eprint( FATAL, "DG2020: No pod has been assigned to "
					"function `%s'.\n", Function_Names[ i ] );
			THROW( EXCEPTION );
		}

		/* Assemble a list of all pulses assigned to the function and, while
		   doing so, also count the number of channels really needed */

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			f->num_needed_channels = 1;
			need_phases = UNSET;

			for ( cp = Pulses; cp != NULL; cp = cp->next )
			{
				if ( cp->function != f )
					continue;

				f->num_pulses++;
				f->pulses = T_realloc( f->pulses,
									   f->num_pulses * sizeof( PULSE * ) );
				f->pulses[ f->num_pulses - 1 ] = cp;
				
				if ( cp->pc )
					need_phases = SET;
			}

			if ( ! need_phases && f->phase_func != NULL )
			{
				eprint( WARN, "DG2020: Function `%s' is associated with phase "
						"function `%s' but none of its pulses needs phase "
						"cycling.\n", Function_Names[ f->self ],
						Function_Names[ f->phase_func->self ] );

				f->phase_func->is_used = UNSET;
				for ( j = 0; j < f->phase_func->num_channels; j++ )
				{
					f->phase_func->channel[ j ]->function = NULL;
					f->phase_func->channel[ j ] = NULL;
				}
				f->phase_func->num_channels = 0;
			}

			assert( f->num_pulses > 0 );    /* paranoia ? */
		}
		else
			f->num_needed_channels = PSeq->len;

		/* Put channels not needed back into the pool */

		if ( f->num_channels > f->num_needed_channels )
			eprint( WARN, "DG2020: For function `%s' only %d channel%s needed "
					"instead of the %d assigned ones.\n",
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
       channels for functons that we found to be useless and also the channels
	   for phase functions associated with useless functions */

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
			eprint( WARN, "DG2020: Phase function `%s' isn't needed because "
					"function `%s' it is associated with is not used.\n",
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


/*-----------------------------------------------------------------------------
  Function checks first if there are enough pulser channels and than assignes
  channels to funcions that haven't been assigned as many as needed
-----------------------------------------------------------------------------*/

void distribute_channels( void )
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
		eprint( FATAL, "DG2020: Running the experiment would require %d "
				"pulser channels but only %d are available.\n",
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
			f->channel[ f->num_channels ] = get_next_free_channel( );
			f->channel[ f->num_channels ]->function = f;
			f->num_channels++;
		}
	}
}


/*-----------------------------------------------------------------------------
  In this function the pulses for all the functions are sorted and further
  consistency checks are done.
-----------------------------------------------------------------------------*/

void pulse_start_setup( void )
{
	int i, j;
	FUNCTION *f;
	PULSE *p;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		/* Sort the pulses of this channel by their start times, move inactive
		   pulses to the end of the list */

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ), start_compare );

		/* Check that the relevant pulses are separated */

		for ( j = 0; j < f->num_pulses; j++ )
		{
			p = f->pulses[ j ];
			p->channel = f->channel[ 0 ];
			if ( j + 1 == f->num_pulses || ! f->pulses[ j + 1 ]->is_active )
			{
				f->num_active_pulses = j + 1;
				break;
			}
			if ( p->len + p->pos >= f->pulses[ j + 1 ]->pos )
			{
				eprint( FATAL, "DG2020: Pulses %ld and %ld %s.\n",
						p->num, f->pulses[ j + 1 ]->num,
						p->len + p->pos == f->pulses[ j + 1 ]->pos ?
						"are not separated" : "overlap");
				THROW( EXCEPTION );
			}
		}
	}

	/* Now all the normal pulses are setup and we can calculate the the pulses
	   needed for phase cycling */

	create_phase_pulses( PULSER_CHANNEL_PHASE_1 );
	create_phase_pulses( PULSER_CHANNEL_PHASE_2 );
}


/*-----------------------------------------------------------------------------
  In this function the additional pulses needed for phase cycling in one of
  the phase functions are created.
-----------------------------------------------------------------------------*/


void create_phase_pulses( int func )
{
	FUNCTION *f = &dg2020.function[ func ];
	int i, j, l;
	PULSE *p;


	if ( ! f->is_used )
		return;

	for ( i = 0; i < f->phase_func->num_pulses; i++ )
	{
		f->num_active_pulses = 0;

		for ( j = 0; j < ASeq[ 0 ].len; j++ )
			for ( l = 0; l < 2; l++ )
			{
				p = new_phase_pulse( f, f->phase_func->pulses[ i ], j, l );
				if ( p != NULL )
				{
					f->num_pulses++;
					f->pulses = T_realloc( f->pulses, f->num_pulses
										   * sizeof( PULSE * ) );
					f->pulses[ f->num_pulses - 1 ] = p;

					if ( p->is_active )
					{
						p->has_been_active = SET;
						f->num_active_pulses++;
					}
				}
			}

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ), start_compare );
	}

	/* Now that we have these preliminary pulses we need to add some padding
	   because the phase switches don't work instantaneously and check if the
	   phase pulses stay separated (here we have to be even more carefully
	   because to control one switch to pods are combined!)  */

	/* MORE TO COME when I now about the padding times needed...*/
}


/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/

PULSE *new_phase_pulse( FUNCTION *f, PULSE *p, int pos, int pod )
{
	PULSE *np, *cp = Pulses;
	int type;


	/* Figure out the phase type - its stored in the Phase_Sequence the
	   entry in the referenced pulses points to */

	type = p->pc->sequence[ pos ];

	/* No pulse needs creating if for the current phase type / pod combintion
	   no voltage is needed */

	if ( f->phs.var[ type ][ pod ] == 0 )
		return NULL;

	/* Get memory for a new pulse and append it to the list of pulses */

	for ( np = Pulses; np != NULL; np = np->next )
		cp = np;

	np = T_malloc( sizeof( PULSE ) );

	np->prev = cp;
	cp->next = np;

	np->next = NULL;
	np->pc = NULL;

	/* These artifical pulses get negative numbers */

	np->num = ( np->prev->num >= 0 ) ? -1 : np->prev->num - 1;

	/* Its function is the phase function it belongs to */

	np->is_function = SET;
	np->function = f;

	/* Position and length and the changes are tentatively set to the
	   properties of the pulse it is assigned to */

	np->is_pos = p->is_pos;
	np->pos = np->initial_pos = p->pos;

	np->is_len = p->is_len;
	np->len = np->initial_len = p->len;

	if ( ( np->is_dpos = p->is_dpos ) == SET )
		np->dpos = np->initial_dpos = p->dpos;
	if ( ( np->is_dlen = p->is_dlen ) == SET )
		np->dlen = np->initial_dlen = p->dlen;

	np->is_active = p->is_active;

	/* Set the channel it belongs to */

	np->channel = f->channel[ 2 * type + pod ];

	/* it doesn't needs updates yet */

	np->is_old_pos = np->is_old_len = UNSET;
	np->needs_update = UNSET;

	/* Finally set the pulse it's assigned to */

	np->for_pulse = p;

	return np;
}
