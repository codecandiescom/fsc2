/*
  $Id$
*/



#include "dg2020_f.h"


static void dg2020_function_check( void );


/*---------------------------------------------------------------------------
  Function does everything that needs to be done for checking and completing
  the internal representation of the pulser at the start of a test run.
---------------------------------------------------------------------------*/

void dg2020_init_setup( void )
{
	dg2020_function_check( );

	/* Check that all functions that have more than one pod assigned to it
	   also need phase cycling (and the other way round) */



}


void dg2020_function_check( void )
{
	FUNCTION *f;
	int i, j;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
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

		/* If the function has more than one pod assigned to it make sure it
		   also does phase cycling */

		if ( f->num_pods > 1 && ! f->needs_phases )
		{
			eprint( FATAL, "%s: Too many pods assigned to function `%s'.",
					pulser_struct.name, Function_Names[ i ] );
			THROW( EXCEPTION );
		}

		if ( f->needs_phases && f->phase_setup == NULL )
		{
			eprint( FATAL, "%s: Missim#ng phase setup for function `%s'.",
					pulser_struct.name, Function_Names[ i ] );
			THROW( EXCEPTION );
		}
	}
}
