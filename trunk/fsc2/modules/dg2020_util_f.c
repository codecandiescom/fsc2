/*
  $Id$
*/


#include "dg2020.h"



/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. a integer multiple of the time base                        */
/*-----------------------------------------------------------------*/

Ticks dg2020_double2ticks( double time )
{
	double ticks;


	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: DG2020: Can't set a time because no pulser "
				"time base has been set.\n",Fname, Lc );
		THROW( EXCEPTION );
	}

	ticks = time / dg2020.timebase;

	if ( fabs( ticks - lround( ticks ) ) > 1.0e-2 )
	{
		char *t = get_string_copy( dg2020_ptime( time ) );
		eprint( FATAL, "%s:%ld: DG2020: Specified time of %s is not an "
				"integer multiple of the pulser time base of %s.\n",
				Fname, Lc, t, dg2020_ptime( dg2020.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return ( Ticks ) lround( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

double dg2020_ticks2double( Ticks ticks )
{
	assert( dg2020.is_timebase );
	return ( double ) ( dg2020.timebase * ticks );
}


/*----------------------------------------------------------------------*/
/* Checks if the difference of the levels specified for a pod connector */
/* are within the valid limits.                                         */
/*----------------------------------------------------------------------*/

void dg2020_check_pod_level_diff( double high, double low )
{
	if ( low > high )
	{
		eprint( FATAL, "%s:%ld: DG2020: Low voltage level is above high "
				"level, instead use keyword INVERT to invert the polarity.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING + 0.01 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Difference between high and low "
				"voltage of %g V is too big, maximum is %g V.\n", Fname, Lc,
				high - low, MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING - 0.01 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Difference between high and low "
				"voltage of %g V is too small, minimum is %g V.\n", Fname, Lc,
				high - low, MIN_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

PULSE *dg2020_get_pulse( long pnum )
{
	PULSE *cp = dg2020_Pulses;


	if ( pnum < 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid pulse number: %ld.\n",
				Fname, Lc, pnum );
		THROW( EXCEPTION );
	}

	while ( cp != NULL )
	{
		if ( cp->num == pnum )
			break;
		cp = cp->next;
	}

	if ( cp == NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Referenced pulse %ld does not "
				"exist.\n", Fname, Lc, pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *dg2020_ptime( double time )
{
	static char buffer[ 128 ];

	if ( fabs( time ) >= 1.0 )
		sprintf( buffer, "%g s", time );
	else if ( fabs( time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * time );
	else if ( fabs( time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * time );
	else
		sprintf( buffer, "%g ns", 1.e9 * time );

	return buffer;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *dg2020_pticks( Ticks ticks )
{
	return dg2020_ptime( dg2020_ticks2double( ticks ) );
}


/*-----------------------------------------------------------------------------
   Functions returns a pointer to a free channel - the channels with the
   highest numbers are used first since most users probably tend to use the
   low number channels for storing test pulse sequences that they don't like
   too much being overwritten just because they forgot to set a channel-to-
   function-assignment in their EDL program.
-----------------------------------------------------------------------------*/

CHANNEL *dg2020_get_next_free_channel( void )
{
	int i = MAX_CHANNELS - 1;

	while ( dg2020.channel[ i ].function != NULL )
		i--;

	assert( i >= 0 );                 /* this can't happen ;-) */

	return &dg2020.channel[ i ];
}


/*---------------------------------------------------------------------------
  Comparison function for two pulses: returns 0 if both pulses are inactive,
  -1 if only the second pulse is inactive or starts at a late time and 1 if
  only the first pulse is inactive pulse or the second pulse starts earlier.
---------------------------------------------------------------------------*/

int dg2020_start_compare( const void *A, const void *B )
{
	PULSE *a = *( PULSE ** ) A,
		  *b = *( PULSE ** ) B;

	if ( ! a->is_active )
	{
		if ( ! b->is_active )
			return 0;
		else
			return 1;
	}

	if ( ! b->is_active || a->pos <= b->pos )
		return -1;

	return 1;
}


/*---------------------------------------------------------------------------
  Functions searches for the phase pulses associated with the pulse passed as
  the first argument and belonging to the channels passed as the second and
  third argument. If pulses are found they are returned in the last to
  arguments. If at least one was found a postive result is returned.
---------------------------------------------------------------------------*/

bool dg2020_find_phase_pulse( PULSE *p, PULSE ***pl, int *num )
{
	PULSE *pp = dg2020_Pulses;


	assert( p->num >= 0 );    // is it really a normal pulse ?

	*pl = NULL;
	*num = 0;

	while ( pp != NULL )
	{
		if ( pp->num < 0 && pp->for_pulse == p )
		{
			*pl = T_realloc( *pl, ++( *num ) * sizeof( PULSE * ) );
			( *pl )[ *num - 1 ] = pp;
		}

		pp = pp->next;
	}

	return *num != 0;
}


/*---------------------------------------------------------------------------
---------------------------------------------------------------------------*/

Ticks dg2020_get_max_seq_len( void )
{
	int i;
	Ticks max = 0;
	FUNCTION *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		max = Ticks_max( max, f->max_seq_len );
	}

	return max + dg2020.grace_period;
}


/*---------------------------------------------------------------------------
  Function calculates the amount of memory needed for padding to achieve the
  requested repetition time. It then sets up the blocks if they are needed.
---------------------------------------------------------------------------*/

void dg2020_calc_padding( void )
{
	Ticks padding, block_length;
	long block_repeat;


	/* Calculate the grace period and append it to the maximum sequence
	   length */

	dg2020.max_seq_len = Ticks_min( dg2020.max_seq_len,	MAX_PULSER_BITS - 1 );

	/* If no repeat time or frequency has been set nothing is to be done */

	if ( ! dg2020.is_repeat_time )
	{
		dg2020.mem_size = dg2020.max_seq_len;
		return;
	}

	/* Check how much padding is needed - if the size of the pulse pattern is
	   already long enough just set its size and return */

	padding = dg2020.repeat_time - dg2020.max_seq_len;

	if ( padding <= 0 )
	{
		if ( padding < 0 )
			eprint( SEVERE, "DG2020: Pulse pattern is %s long and thus longer "
					"than the repeat time of %s.\n",
					dg2020_pticks( dg2020.max_seq_len ),
					dg2020_pticks( dg2020.repeat_time ) );
		dg2020.mem_size = dg2020.max_seq_len;
		return;
	}

	/* Just setting an area long enough for padding in the pulser memory to
	   a constant value would be a waste of space, so instead the padding
	   area is split into two parts: 1. an empty area after the last pulse,
	   2. an empty block which is repeated. Both lengths as well as the
	   number of repetitions of the empty block are calculated while trying
	   to minimize the length of the empty block and doing as much looping
	   as possible while keeping in mind that there is a lower limit of 64
	   bits (i.e. MIN_BLOCK_SIZE) for the block size as well as a maximum
	   number of repetitions of 65536 (i.e. MAX_BLOCK_REPEATS). */

	block_length = Ticks_max( MIN_BLOCK_SIZE, padding / MAX_BLOCK_REPEATS + 
							 ( ( padding % MAX_BLOCK_REPEATS > 0 ) ? 1 : 0 ) );
	block_repeat = padding / block_length;
	padding %= block_length;

	if ( dg2020.max_seq_len + padding + block_length >= MAX_PULSER_BITS )
	{
		eprint( FATAL, "DG2020: Can't set the repetition rate for the "
				"experiment because this wouldn't fit into the pulsers "
				"memory.\n" );
		THROW( EXCEPTION );
	}

	/* Calculate and set new size of pulse pattern */

	dg2020.max_seq_len += padding;

	/* Return if no repetition of the last block is needed, otherwise set
	   block boundaries and repetition counts */

	if ( block_repeat < 2 )
	{
		dg2020.mem_size = dg2020.max_seq_len;
		return;
	}

	dg2020.mem_size = dg2020.max_seq_len + block_length;

	strcpy( dg2020.block[ 0 ].blk_name, "B1" );
	dg2020.block[ 0 ].start = 0;
	dg2020.block[ 0 ].repeat = 1;
	dg2020.block[ 0 ].is_used = SET;

	strcpy( dg2020.block[ 1 ].blk_name, "B2" );
	dg2020.block[ 1 ].start = dg2020.max_seq_len;
	dg2020.block[ 1 ].repeat = block_repeat;
	dg2020.block[ 1 ].is_used = SET;
}
