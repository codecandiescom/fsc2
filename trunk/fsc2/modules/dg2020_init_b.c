/*
  $Id$
*/


#include "dg2020_b.h"


static void dg2020_basic_pulse_check( void );
static void dg2020_basic_functions_check( void );
static int dg2020_calc_channels_needed( FUNCTION *f );
static void dg2020_phase_setup_check( FUNCTION *f );
static void dg2020_distribute_channels( void );
static void dg2020_setup_phase_matrix( FUNCTION *f );
static void dg2020_pulse_start_setup( void );
static Phase_Sequence *dg2020_create_dummy_phase_seq( void );


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
  1. a pulse function is set and the function itself has been declared in
     the ASSIGNMENTS section
  2. the start position is set
  3. the length is set (only exception: if pulse function is DETECTION
     and no length is set it's more or less silently set to one tick)
  4. the sum of function delay, pulse start position and length does not
     exceed the pulsers memory
--------------------------------------------------------------------------*/

void dg2020_basic_pulse_check( void )
{
	PULSE *p;
	int i, j;
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

		/* Check start position and pulse length */

		if ( ! p->is_pos || ! p->is_len )
			p->is_active = UNSET;

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
			if ( p->function->num_pods == 1 )
			{
				eprint( FATAL, "%s: Phase %ld needs phase cycling but its "
						"function `%s' has only one pod assigned to it.",
						pulser_struct.name, p->num,
						Function_Names[ p->function->self ] );
				THROW( EXCEPTION );
			}

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
		else if ( p->function->num_pods > 1 )
		{
			p->pc = dg2020_create_dummy_phase_seq( );
			p->function->phase_setup->is_needed[ PHASE_PLUS_X ] = SET;
		}

		/* For functions with more than 1 pod (i.e. functions with phase
		   cycling) we need to set up a matrix that stores for which which
		   phase types are needed in each element of the phase cycle - this is
		   needed for calculating the number of channels we will need */

		if ( p->function->num_pods > 1 )
		{
			if ( p->function->pm == NULL )      /* if it doesn't exist yet */
			{
				p->function->pm = T_malloc( p->pc->len * 
											( PHASE_CW - PHASE_PLUS_X + 1 ) *
											sizeof( bool ) );
				for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
					for ( j = 0; j < p->pc->len; j++ )
						p->function->pm[ i * p->pc->len + j ] = UNSET;
				p->function->pc_len = p->pc->len;
			}

			for ( i = 0; i < p->pc->len; i++ )
				p->function->pm[ ( p->pc->sequence[ i ] - PHASE_PLUS_X )
							     * p->pc->len + i ] = SET;
		}

		if ( p->is_active )
			p->was_active = p->has_been_active = SET;
	}
}


/*--------------------------------------------------------------------------
--------------------------------------------------------------------------*/

static void dg2020_basic_functions_check( void )
{
	FUNCTION *f;
	int i, j;
	PULSE *cp;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Phase functions not supported in this driver... */

		assert( ! f->is_used || ( f->is_used &&
								  i != PULSER_CHANNEL_PHASE_1 &&
								  i != PULSER_CHANNEL_PHASE_2    ) );

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

		/* Assemble a list of all pulses assigned to the function */

		f->num_pulses = 0;
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
		
		f->num_needed_channels = dg2020_calc_channels_needed( f );

/*!!!*/
		if ( f->pm )
		{
			int i1, j1;
			printf( "pm for function `%s'\n", Function_Names[ i ] );
			for ( i1 = 0; i1 < 5; i1++ )
			{
				printf( "%s ", Phase_Types[ i1 ] );
				for ( j1 = 0; j1 < f->pc_len; j1++ )
					printf( " %1c", '0' + f->pm[ i1 * f->pc_len + j1 ] );
				printf( "\n" );
			}
		}
/*!!!*/
	}
}


/*--------------------------------------------------------------------------
  This function tries to figure out how many channels are going to be needed.
  It's very simple for functions that only have one pod - here one channel
  will do. On the other hand, for functions with phase cycling we have to
  consult the phase matrix (set up in dg2020_basic_pulse_check()). We need one
  channel for each used phase type for each stage of the phase cycle and (if
  not all phase types are used in all stages) one extra channel for the
  constant voltage.
--------------------------------------------------------------------------*/

static int dg2020_calc_channels_needed( FUNCTION *f )
{
	int i, j;
	bool is_all = SET;
	int num_channels;


	if ( f->num_pods < 2 )
		return 1;

	assert( f->pm != NULL );

	num_channels = 0;
	for ( i = 0; i < f->pc_len; i++ )
		for ( j = 0; j <= PHASE_CW - PHASE_PLUS_X; j++ )
		{
			if ( f->pm[ j * f->pc_len + i ] )
				num_channels++;
			else
				is_all = UNSET;
		}
			
	if ( ! is_all )            /* if we needed a constant voltage */
		num_channels++;

/*!!!*/
	printf( "Number of needed channels: %d\n", num_channels );
/*!!!*/

	return num_channels;
}


/*--------------------------------------------------------------------------
--------------------------------------------------------------------------*/

static void dg2020_phase_setup_check( FUNCTION *f )
{
	int i, j;
	POD *free_pod_list[ MAX_PODS_PER_FUNC ];
	int free_pods;


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
	   needed for the current function. Now we can check if these phase types
	   are also defined in the phase setup. */

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
	{
		if ( f->phase_setup->is_needed[ i ] && ! f->phase_setup->is_set[ i ] )
		{
			eprint( FATAL, "%s: Phase type `%s' is needed for function `%s' "
					"but it hasn't been not defined in a PHASE_SETUP "
					"command.", pulser_struct.name,
					Phase_Types[ i + PHASE_PLUS_X ],
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}
	}

	/* Now we distribute pods of the function that aren't used for pulse types
	   as set in the PHASE_SETUP command to phase types that aren't needed for
	   real pulses (this way the unused pod is automatically set to a constant
	   voltages as needed by the Berlin pulse bridge) */

	free_pods = 0;
	for ( i = 0; i < f->num_pods; i++ )
	{
		for ( j = 0; j <= PHASE_CW - PHASE_PLUS_X; j++ )
			if ( f->pod[ i ] == f->phase_setup->pod[ j ] )
				break;
		if ( j == PHASE_CW - PHASE_PLUS_X + 1 )
			free_pod_list[ free_pods++ ] = f->pod[ i ];
	}

	if ( free_pods != 0 )
		for ( i = j = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
			if ( ! f->phase_setup->is_set[ i ] )
			{
				f->phase_setup->is_set[ i ]= SET;
				f->phase_setup->pod[ i ] = free_pod_list[ j ];
				j = ( j + 1 ) % free_pods;
			}

	/* We also have to test that different phase types are not associated with
       the same pod (exception: the different phase types are never actually
       used for a pulse - that's ok). */

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
		for ( j = 0; j <= PHASE_CW - PHASE_PLUS_X; j++ )
		{
			if ( i == j )
				continue;

			if ( f->phase_setup->is_needed[ i ] &&
				 f->phase_setup->is_set[ j ] &&
				 f->phase_setup->pod[ i ] == f->phase_setup->pod[ j ] )
			{

				/* Distinguish between the cases that either both phase types
				   are really needed by pulses or that one is needed by a
				   pulse and the other is defined but not actually needed for
				   a pulse but to just to create a constant voltage. */

				if ( f->phase_setup->is_needed[ j ] )
					eprint( FATAL, "%s: Pod %ld can't be used for phase type "
							"`%s' and `%s' at the same time.",
							pulser_struct.name,
							f->phase_setup->pod[ i ]->self,
							Phase_Types[ i + PHASE_PLUS_X ],
							Phase_Types[ j + PHASE_PLUS_X ] );
				else
					eprint( FATAL, "%s: Pod %ld is needed for phase type `%s' "
							"and can't be also used for other phase types.",
							pulser_struct.name,
							f->phase_setup->pod[ i ]->self,
							Phase_Types[ i + PHASE_PLUS_X ] );
				THROW( EXCEPTION );
			}
		}
}


/*----------------------------------------------------------------------------
  Function checks first if there are enough pulser channels and than assigns
  channels to funcions that haven't been assigned as many channels as needed.
-----------------------------------------------------------------------------*/

static void dg2020_distribute_channels( void )
{
	int i;
	FUNCTION *f;


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
		eprint( FATAL, "%s: Running the experiment would require %d pulser "
				"channels but only %d are available.", pulser_struct.name, 
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
			f->channel[ f->num_channels++ ]->function = f;
		}

		if ( f->num_pods > 1 )
			dg2020_setup_phase_matrix( f );
	}
}


/*--------------------------------------------------------------------------
  The phase matrix created here has as many rows as there are phase types
  (5 for the Berlin pulser) and as many columns as there are 'stages' in
  the phase cycle. Its elements are the numbers of the channels to be used
  for the corresponding phase type in the corresponding stage of the phase
  cycle. As lang as there are no repetitions of the same phase pattern in
  the stages of the phase cycle this is the minimum number of channels.
--------------------------------------------------------------------------*/

static void dg2020_setup_phase_matrix( FUNCTION *f )
{
	int i, j;
	int cur_channel;


	assert( f->pm != NULL && f->pcm == NULL );

	/* If the number of needed channels is smaller than the size of the phase
	   type / phase sequence matrix we will need one extra channel for the
	   constant voltage */

	if ( f->num_needed_channels < f->pc_len * ( PHASE_CW - PHASE_PLUS_X + 1 ) )
		cur_channel = 1;
	else
		cur_channel = 0;

	f->pcm = T_malloc( f->pc_len * ( PHASE_CW - PHASE_PLUS_X + 1 ) *
					   sizeof( CHANNEL * ) );

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
		for ( j = 0; j < f->pc_len; j++ )
			if ( f->pm[ i * f->pc_len + j ] )
				f->pcm[ i * f->pc_len + j ] = f->channel[ cur_channel++ ];
			else
				f->pcm[ i * f->pc_len + j ] = f->channel[ 0 ];

	T_free( f->pm );
	f->pm = NULL;

/*!!!*/
	if ( f->pcm )
	{
		int i1, j1;
		printf( "pcm for function `%s'\n", Function_Names[ f->self ] );
		for ( i1 = 0; i1 < 5; i1++ )
		{
			printf( "%s ", Phase_Types[ i1 ] );
			for ( j1 = 0; j1 < f->pc_len; j1++ )
				printf( " %2d", f->pcm[ i1 * f->pc_len + j1 ]->self );
			printf( "\n" );
		}
	}
/*!!!*/
}


/*--------------------------------------------------------------------------
--------------------------------------------------------------------------*/

static void dg2020_pulse_start_setup( void )
{
	FUNCTION *f;
	int i, j, k;


	/* Sort the pulses and check that they don't overlap */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 i == PULSER_CHANNEL_PHASE_1 ||
			 i == PULSER_CHANNEL_PHASE_2 )
			continue;

		/* Sort the pulses of current the function */

		qsort( f->pulses, f->num_pulses, sizeof( PULSE * ),
			   dg2020_start_compare );

		/* Check that they don't overlap and fit into the pulsers memory */

		dg2020_do_checks( f );

		/* Set for each pulse the channel(s) it belongs to */

		for ( j = 0; j < f->num_pulses; j++ )
		{
			f->pulses[ j ]->channel = T_malloc( f->pc_len *
												sizeof( CHANNEL * ) );
			for ( k = 0; k < f->pc_len; k++ )
				f->pulses[ j ]->channel[ k ] = NULL;

			if ( f->num_needed_channels == 1 )
				f->pulses[ j ]->channel[ 0 ] = f->channel[ 0 ];
			else
				for ( k = 0; k < f->pc_len; k++ )
					f->pulses[ j ]->channel[ k ] =
						f->pcm[ ( f->pulses[ j ]->pc->sequence[ k ]
								  - PHASE_PLUS_X ) * f->pc_len + k ];
		}
	}
}


/*--------------------------------------------------------------------------
  Function creates a dummy phase sequence for pulses with no phase sequence
  defined but belonging to a function that has more than one pod assigned to
  it. This dummy phase sequence consists of just '+X' phases (length is 1 if
  there are no real phase sequences or equal to the the length of the normal
  phase sequences otherwise). If such a dummy phase sequence already exists
  the pulse becomes associated with this already existing dummy sequence.
--------------------------------------------------------------------------*/

static Phase_Sequence *dg2020_create_dummy_phase_seq( void )
{
	Phase_Sequence *np, *pn;
	int i;

	/* First check if there's a phase sequence consisting of just '+X' - if it
	   does use this as the phase sequence for the pulse */

	for ( np = PSeq; np != NULL; np = np->next )
	{
		for ( i = 0; i < np->len; i++ )
			if ( np->sequence[ i ] != PHASE_PLUS_X )
				break;

		if ( i == np->len )
			return np;
	}

	/* Create a new phase sequence */

	np = T_malloc( sizeof( Phase_Sequence ) );

	if ( PSeq == NULL )
	{
		PSeq = np;
		np->len = 1;
		np->sequence = T_malloc( sizeof( int ) );
		np->sequence[ 0 ] = PHASE_PLUS_X;
	}
	else
	{
		pn = PSeq;
		while ( pn->next != NULL )
			pn = pn->next;
		pn->next = np;
		np->len = PSeq->len;
		np->sequence = T_malloc( np->len * sizeof( int ) );
		for ( i = 0; i < np->len; i++ )
			np->sequence[ i ] = PHASE_PLUS_X;
	}

	np->next = NULL;
	return np;
}
