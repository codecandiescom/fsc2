/*
  $Id$
*/



#include "dg2020_b.h"


static void dg2020_basic_pulse_check( void );
static void dg2020_basic_functions_check( void );
static void dg2020_phase_setup_check( FUNCTION *f );


/*---------------------------------------------------------------------------
  Function does everything that needs to be done for checking and completing
  the internal representation of the pulser at the start of a test run.
---------------------------------------------------------------------------*/

void dg2020_init_setup( void )
{
	dg2020_basic_pulse_check( );
	dg2020_basic_functions_check( );
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

void dg2020_basic_pulse_check( void )
{
	PULSE *p;
	int i;
	int cur_type;


	for ( p = dg2020_Pulses; p != NULL; p = p->next )
	{
		p->is_active = SET;

		/* Check the pulse function */

		if ( ! p->is_function )
		{
			eprint( FATAL, "%s: Pulse %ld is not associated with a function.",
					pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		if ( ! p->function->is_used )
		{
			eprint( FATAL, "%s: The function `%s' of pulse %ld hasn't been "
					"declared in the ASSIGNMENTS section.",
					pulser_struct.name, Function_Names[ p->function->self ],
					p->num );
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
				eprint( WARN, "%s: Length of detection pulse %ld is being set "
						"to %s", pulser_struct.name, p->num,
						dg2020_ptime( 1 ) );
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
			eprint( FATAL, "%s: Pulse %ld does not fit into the pulsers "
					"memory. Maybe, you could try a longer pulser time "
					"base.", pulser_struct.name, p->num );
			THROW( EXCEPTION );
		}

		/* We need to know which phase types will be needed for this pulse */

		if ( p->pc )
		{
			if ( p->function->phase_setup == NULL )
			{
				eprint( FATAL, "%s: Pulse %ld needs phase cycling but a "
						"phase setup for its function `%s' is missing.",
						pulser_struct.name, p->num,
						Function_Names[ p->function->self ] );
				THROW( EXCEPTION );
			}

			for ( i = 0; i < p->pc->len; i++ )
			{
				cur_type = p->pc->sequence[ i ];
				if  ( cur_type < PHASE_PLUS_X || cur_type > PHASE_CW )
				{
					eprint( FATAL, "%s: Pulse %ld needs phase type `%s' but "
							"this type isn't possible with this driver.",
							pulser_struct.name, p->num,
							Phase_Types[ cur_type ] );
					THROW( EXCEPTION );
				}

				cur_type -= PHASE_PLUS_X;
				p->function->phase_setup->is_needed[ cur_type ] = SET;
			}
		}

		if ( p->is_active )
			p->was_active = p->has_been_active = SET;
	}
}



static void dg2020_basic_functions_check( void )
{
	FUNCTION *f;
	int i, j;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		/* Phase functions not supported in this driver... */

		if ( i == PULSER_CHANNEL_PHASE_1 ||
			 i == PULSER_CHANNEL_PHASE_2 )
			continue;

		f = &dg2020.function[ i ];

		/* Don't do anything if the function has never been mentioned */

		if ( ! f->is_used )
			continue;

		/* Check if the function has pulses assigned to it */

		if ( ! f->is_needed )
		{
			eprint( WARN, "%s: No pulses have been assigned to function "
					"`%s'.", pulser_struct.name, Function_Names[ i ] );
			f->is_used = UNSET;

			for ( j = 0; j < f->num_channels; j++ )
			{
				f->channel[ j ]->function = NULL;
				f->channel[ j ] = NULL;
			}
			f->num_channels = 0;

			continue;
		}

		/* Make sure there's at least one pod assigned to the function */

		if ( f->num_pods == 0 )
		{
			eprint( FATAL, "%s: No pod has been assigned to function `%s'.",
					pulser_struct.name, Function_Names[ i ] );
			THROW( EXCEPTION );
		}

		/* Check that for functions that need phase cycling there was also a
		   PHASE_SETUP command */

		if ( f->num_pods > 1 && f->phase_setup == NULL )
		{
			eprint( FATAL, "%s: Missing phase setup for function `%s'.",
					pulser_struct.name, Function_Names[ i ] );
			THROW( EXCEPTION );
		}

		/* Now its time to check the phase setup for functions which use phase
		   cycling */

		if ( f->num_pods > 1 )
			dg2020_phase_setup_check( f );
	}
}




static void dg2020_phase_setup_check( FUNCTION *f )
{
	int i, j;


	/* First let's see if the pods mentioned in the phase setup are really
	   assigned to the function */

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
	{
		if ( ! f->phase_setup->is_set[ i ] )
			continue;

		for ( j = 0; j < f->num_pods; j++ )
			if ( f->pod[ j ]->self == f->phase_setup->pod[ i ]->self )
				break;

		if ( j == f->num_pods )                 /* not found ? */
		{
			eprint( FATAL, "%s: According to the the phase setup pod %d is "
					"needed for function `%s' but it's not assigned to it.",
					pulser_struct.name, f->phase_setup->pod[ i ]->self,
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}
	}

	/* In the basic pulse check we created made a list of all phase types
	   needed for all functions. Now we can check if these phase types are
	   also defined in the phase setup. */

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
	{
		if ( f->phase_setup->is_needed[ i ] && ! f->phase_setup->is_set[ i ] )
		{
			eprint( FATAL, "%s: Phase type `%s' is needed for function `%s' "
					"but it hasn't been not defined in the PHASE_SETUP "
					"commmand.", pulser_struct.name,
					Phase_Types[ i + PHASE_PLUS_X ],
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}
	}
}
