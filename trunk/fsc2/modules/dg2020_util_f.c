/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "dg2020_f.h"


/*-----------------------------------------------------------------*
 * Converts a time into the internal type of a time specification,
 * i.e. a integer multiple of the time base
 *-----------------------------------------------------------------*/

Ticks dg2020_double2ticks( double p_time )
{
	double ticks;


	if ( ! dg2020.is_timebase )
	{
		print( FATAL, "Can't set a time because no pulser time base has been "
			   "set.\n" );
		THROW( EXCEPTION );
	}

	ticks = p_time / dg2020.timebase;

	if ( ticks > TICKS_MAX || ticks < TICKS_MIN )
	{
		print( FATAL, "Specified time is too long for time base of %s.\n",
			   dg2020_ptime( dg2020.timebase ) );
		THROW( EXCEPTION );
	}

	if ( fabs( Ticksrnd( ticks ) - p_time / dg2020.timebase ) > 1.0e-2 ||
		 ( p_time > 0.99e-9 && Ticksrnd( ticks ) == 0 ) )
	{
		char *t = T_strdup( dg2020_ptime( p_time ) );
		print( FATAL, "Specified time of %s is not an integer multiple of "
			   "the pulser time base of %s.\n",
			   t, dg2020_ptime( dg2020.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return Ticksrnd( ticks );
}


/*-----------------------------------------------------*
 * Does the exact opposite of the previous function...
 *-----------------------------------------------------*/

double dg2020_ticks2double( Ticks ticks )
{
	fsc2_assert( dg2020.is_timebase );
	return ( double ) ( dg2020.timebase * ticks );
}


/*----------------------------------------------------------------------*
 * Checks if the difference of the levels specified for a pod connector
 * are within the valid limits.
 *----------------------------------------------------------------------*/

void dg2020_check_pod_level_diff( double high,
								  double low )
{
	if ( low > high )
	{
		print( FATAL, "Low voltage level is above high level, use keyword "
			   "INVERT to invert the polarity instead.\n" );
		THROW( EXCEPTION );
	}

	if ( high - low > MAX_POD_VOLTAGE_SWING + 0.1 * VOLTAGE_RESOLUTION )
	{
		print( FATAL, "Difference between high and low voltage of %g V is too "
			   "big, maximum is %g V.\n", high - low, MAX_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}

	if ( high - low < MIN_POD_VOLTAGE_SWING - 0.1 * VOLTAGE_RESOLUTION )
	{
		print( FATAL, "Difference between high and low voltage of %g V is too "
			   "small, minimum is %g V.\n",
			   high - low, MIN_POD_VOLTAGE_SWING );
		THROW( EXCEPTION );
	}
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Pulse_T *dg2020_get_pulse( long pnum )
{
	Pulse_T *cp = dg2020.pulses;


	if ( pnum < 0 )
	{
		print( FATAL, "Invalid pulse number: %ld.\n", pnum );
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
		print( FATAL, "Referenced pulse #%ld does not exist.\n", pnum );
		THROW( EXCEPTION );
	}

	return cp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

const char *dg2020_ptime( double p_time )
{
	static char buffer[ 128 ];

	if ( fabs( p_time ) >= 1.0 )
		sprintf( buffer, "%g s", p_time );
	else if ( fabs( p_time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * p_time );
	else if ( fabs( p_time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * p_time );
	else
		sprintf( buffer, "%g ns", 1.e9 * p_time );

	return buffer;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

const char *dg2020_pticks( Ticks ticks )
{
	return dg2020_ptime( dg2020_ticks2double( ticks ) );
}


/*-------------------------------------------------------------------------*
 * Functions returns a pointer to a free channel - the channels with the
 * lowest numbers are used first since most users probably tend to use the
 * high number channels for storing test pulse sequences that they don't
 * like too much being overwritten just because they didn't set a channel-
 * to-function-assignment in their EDL program.
 *-------------------------------------------------------------------------*/

Channel_T *dg2020_get_next_free_channel( void )
{
	int i = 0;


	while ( dg2020.channel[ i ].function != NULL )
		i++;

	fsc2_assert( i < MAX_CHANNELS );              /* this can't happen ;-) */

	return dg2020.channel + i;
}


/*---------------------------------------------------------------------------*
 * Comparison function for two pulses: returns 0 if both pulses are inactive,
 * -1 if only the second pulse is inactive or starts at a late time and 1 if
 * only the first pulse is inactive pulse or the second pulse starts earlier.
 *---------------------------------------------------------------------------*/

int dg2020_start_compare( const void * A,
						  const void * B )
{
	Pulse_T *a = *( Pulse_T ** ) A,
		    *b = *( Pulse_T ** ) B;

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


/*--------------------------------------------------------------------------*
 * Functions searches for the phase pulses associated with the pulse passed
 * as the first argument and belonging to the channels passed as the second
 * and third argument. If pulses are found they are returned in the last to
 * arguments. If at least one was found a postive result is returned.
 *--------------------------------------------------------------------------*/

bool dg2020_find_phase_pulse( Pulse_T *   p,
							  Pulse_T *** pl,
							  int *       num )
{
	Pulse_T *pp = dg2020.pulses;


	fsc2_assert( p->num >= 0 );    /* is it really a normal pulse ? */

	*pl = NULL;
	*num = 0;

	while ( pp != NULL )
	{
		if ( pp->num < 0 && pp->for_pulse == p )
		{
			*pl = PULSE_PP T_realloc( *pl, ++( *num ) * sizeof **pl );
			( *pl )[ *num - 1 ] = pp;
		}

		pp = pp->next;
	}

	return *num != 0;
}


/*-----------------------------------------------------------------------*
 * This function finds all the pulses of a phase function that belong to
 * the same channel and returns them as a sorted list.
 *-----------------------------------------------------------------------*/

int dg2020_get_phase_pulse_list( Function_T * f,
								 Channel_T *  channel,
								 Pulse_T ***  list )
{
	int i;
	int num_pulses = 0;


	*list = NULL;
	for ( i = 0; i < f->num_pulses; i++ )
	{
		if ( f->pulses[ i ]->channel != channel )
			continue;
		*list = PULSE_PP T_realloc( *list,
									( num_pulses + 1 ) * sizeof **list );
		*( *list + num_pulses++ ) = f->pulses[ i ];
	}

	qsort( *list, num_pulses, sizeof( Pulse_T * ), dg2020_start_compare );

	return num_pulses;
}


/*-------------------------------------------------------------------------*
 *-------------------------------------------------------------------------*/

Ticks dg2020_get_max_seq_len( void )
{
	int i;
	Ticks max = 0;
	Function_T *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		max = Ticks_max( max, f->max_seq_len + f->delay );
	}

	if ( dg2020.is_max_seq_len )
		max = Ticks_max( max, dg2020.max_seq_len );

	return max + dg2020.grace_period;
}


/*-------------------------------------------------------------------------*
 * Function calculates the amount of memory needed for padding to achieve
 * the requested repetition time. It then sets up the blocks if they are
 * needed. The additional bit in the memory size is needed because, due to
 * a bug in the pulsers firmware, the first bit can't be used.
 *-------------------------------------------------------------------------*/

void dg2020_calc_padding( void )
{
	Ticks padding, block_length;
	long block_repeat;


	/* Make sure sequence length has at least the minimum possible size */

	dg2020.max_seq_len = Ticks_max( dg2020.max_seq_len, MIN_BLOCK_SIZE );

	/* Make sure the sequences fit into the pulser memory */

	if ( dg2020.max_seq_len >= MAX_PULSER_BITS )
	{
		print( FATAL, "The requested pulse sequences don't fit into the "
			   "pulsers memory. You could try a longer pulser time base.\n" );
		THROW( EXCEPTION );
	}

	/* If no repeat time or frequency has been set we're done */

	if ( ! dg2020.is_repeat_time )
	{
		if ( dg2020.trig_in_mode == EXTERNAL )
		{
			strcpy( dg2020.block[ 0 ].blk_name, "B0" );
			dg2020.block[ 0 ].start = 0;
			dg2020.block[ 0 ].repeat = 1;
			dg2020.block[ 0 ].is_used = SET;
		}

		dg2020.mem_size = dg2020.max_seq_len + 1;
		return;
	}

	/* Check how much padding is needed - if the size of the pulse pattern is
	   already long enough just set its size and return */

	padding = dg2020.repeat_time - dg2020.max_seq_len;

	if ( padding <= 0 )
	{
		if ( padding < 0 )
		{
			char *t = T_strdup( dg2020_pticks( dg2020.max_seq_len ) );
			print( SEVERE, "Pulse pattern is %s long and thus longer than the "
				   "repeat time of %s.\n",
				   t, dg2020_pticks( dg2020.repeat_time ) );
			T_free( t );
		}
		dg2020.mem_size = dg2020.max_seq_len + 1;
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
		print( FATAL, "Can't set the repetition rate for the experiment "
			   "because this wouldn't fit into the pulsers memory.\n" );
		THROW( EXCEPTION );
	}

	/* Calculate and set new size of pulse pattern */

	dg2020.max_seq_len += padding;

	/* Return if no repetition of the last block is needed, otherwise set
	   block boundaries and repetition counts */

	if ( block_repeat < 2 )
	{
		dg2020.mem_size = dg2020.max_seq_len + 1;
		return;
	}

	dg2020.mem_size = dg2020.max_seq_len + block_length + 1;

	strcpy( dg2020.block[ 0 ].blk_name, "B0" );
	dg2020.block[ 0 ].start = 0;
	dg2020.block[ 0 ].repeat = 1;
	dg2020.block[ 0 ].is_used = SET;

	strcpy( dg2020.block[ 1 ].blk_name, "B1" );
	dg2020.block[ 1 ].start = dg2020.max_seq_len + 1;
	dg2020.block[ 1 ].repeat = block_repeat;
	dg2020.block[ 1 ].is_used = SET;
}


/*----------------------------------------------------------*
 * dg2020_prep_cmd() does some preparations in assembling
 * a command string for setting a pulse pattern. It first
 * checks the supplied parameters, allocates memory for the
 * command string and finally assembles the start of the
 * command string. If the function returns succesfully one
 * should not forget to free the memory allocated for the
 * command string later on!
 * ->
 *  * pointer to pointer to the command string
 *  * number of the pulser channel
 *  * address of the start of the pulse pattern to be set
 *  * length of the pulse pattern to be set
 * <-
 *  * 1: ok, 0: error
 *----------------------------------------------------------*/

bool dg2020_prep_cmd( char ** cmd,
					  int     channel,
					  Ticks   address,
					  Ticks   length )
{
	char dummy[ 10 ];


	/* Check the parameters */

	if ( channel < 0 || channel > MAX_CHANNELS ||
		 address < 0 || address + length > MAX_PULSER_BITS ||
		 length <= 0 || length > MAX_PULSER_BITS )
		return FAIL;

	/* Get enough memory for the command string */

	*cmd = CHAR_P T_malloc( length + 50L );

	/* Set up the command string */

	sprintf( dummy, "%ld", length );
	sprintf( *cmd, ";DATA:PATT:BIT %d,%ld,%ld,#%ld%s", channel,
			 ( long ) address, ( long ) length, ( long ) strlen( dummy ),
			 dummy );

	return OK;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

void dg2020_set( char * arena,
				 Ticks  start,
				 Ticks  len,
				 Ticks  offset )
{
	fsc2_assert( start + len + offset <= dg2020.max_seq_len );

	memset( arena + offset + start, SET, len );
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

int dg2020_diff( char *  old_p,
				 char *  new_p,
				 Ticks * start,
				 Ticks * length )
{
	static Ticks where = 0;
	int ret;
	char *a = old_p + where,
		 *b = new_p + where;
	char cur_state;


	/* If we reached the end of the arena in the last call return 0 */

	if ( where == -1 )
	{
		where = 0;
		return 0;
	}

	/* Search for next difference in the arena */

	while ( where < dg2020.max_seq_len && *a == *b )
	{
		where++;
		a++;
		b++;
	}

	/* If none was found anymore */

	if ( where == dg2020.max_seq_len )
	{
		where = 0;
		return 0;
	}

	/* Store the start position (including the offset and the necessary one
	   due to the pulsers firmware bug) and remember if we have to reset (-1)
	   or to set (1) */

	*start = where;
	ret = *a ? -1 : 1;
	cur_state = *a;

	/* Now figure out the length of the area we have to set or reset */

	*length = 0;
	while ( where < dg2020.max_seq_len && *a != *b && *a == cur_state )
	{
		where++;
		a++;
		b++;
		( *length )++;
	}

	/* Set a marker that we reached the end of the arena */

	if ( where == dg2020.max_seq_len )
		where = -1;

	return ret;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

void dg2020_dump_channels( FILE * fp )
{
	Function_T *f;
	Pulse_T *p;
	int i, k;
	int next_phase;
	const char *plist[ ] = { "+X", "-X", "+Y", "-Y" };


	if ( fp == NULL )
		return;

	fprintf( fp, "===\n" );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;

		if ( ! f->is_used || f->pulses == NULL )
			continue;

		if ( f->self != PULSER_CHANNEL_PHASE_1 &&
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			fprintf( fp, "%s:%d", f->name, f->pod->self );

			for ( k = 0; k < f->num_pulses; k++ )
			{
				p = f->pulses[ k ];
				if ( ! p->is_active )
					continue;

				fprintf( fp, " %ld %ld %ld", p->num, p->pos, p->len );

				if ( f->needs_phases )
				{
					if ( p->pc == NULL )
						fprintf( fp, " +X" );
					else
						fprintf( fp, " %s",
								 plist[ p->pc->sequence[ f->next_phase ] ] );
				}
			}

			fprintf( fp, "\n" );
		}
		else
		{
			fprintf( fp, "%s:%d", f->name, f->pod->self );

			next_phase = f->next_phase;
			if ( next_phase >= f->num_channels )
			next_phase = 0;

			for ( k = 0; k < f->num_pulses; k++ )
			{
				p = f->pulses[ k ];
				if ( ! p->is_active || f->channel[ next_phase ] != p->channel )
					continue;
				if ( p->num < 0 )
					fprintf( fp, " (%ld) %ld %ld",
							 p->for_pulse->num, p->pos, p->len );
				else
					fprintf( fp, " %ld %ld %ld", p->num, p->pos, p->len );
			}

			fprintf( fp, "\n" );

			fprintf( fp, "%s:%d", f->name, f->pod2->self );
			next_phase++;

			for ( k = 0; k < f->num_pulses; k++ )
			{
				p = f->pulses[ k ];
				if ( ! p->is_active || f->channel[ next_phase ] != p->channel )
					continue;
				if ( p->num < 0 )
					fprintf( fp, " (%ld) %ld %ld",
							 p->for_pulse->num, p->pos, p->len );
				else
					fprintf( fp, " %ld %ld %ld", p->num, p->pos, p->len );
			}

			fprintf( fp, "\n" );

		}
	}
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

long dg2020_ch_to_num( long channel )
{
	switch ( channel )
	{
		case CHANNEL_CH0 :
			return 0;

		case CHANNEL_CH1 :
			return 1;

		case CHANNEL_CH2 :
			return 2;

		case CHANNEL_CH3 :
			return 3;

		case CHANNEL_CH4 :
			return 4;

		case CHANNEL_CH5 :
			return 5;

		case CHANNEL_CH6 :
			return 6;

		case CHANNEL_CH7 :
			return 7;

		case CHANNEL_CH8 :
			return 8;

		case CHANNEL_CH9 :
			return 9;

		case CHANNEL_CH10 :
			return 10;

		case CHANNEL_CH11 :
			return 11;

		case CHANNEL_CH12 :
			return 12;

		case CHANNEL_CH13 :
			return 13;

		case CHANNEL_CH14 :
			return 14;

		case CHANNEL_CH15 :
			return 15;

		case CHANNEL_CH16 :
			return 16;

		case CHANNEL_CH17 :
			return 17;

		case CHANNEL_CH18 :
			return 18;

		case CHANNEL_CH19 :
			return 19;

		case CHANNEL_CH20 :
			return 20;

		case CHANNEL_CH21 :
			return 21;

		case CHANNEL_CH22 :
			return 22;

		case CHANNEL_CH23 :
			return 23;

		case CHANNEL_CH24 :
			return 24;

		case CHANNEL_CH25 :
			return 25;

		case CHANNEL_CH26 :
			return 26;

		case CHANNEL_CH27 :
			return 27;

		case CHANNEL_CH28 :
			return 28;

		case CHANNEL_CH29 :
			return 29;

		case CHANNEL_CH30 :
			return 30;

		case CHANNEL_CH31 :
			return 31;

		case CHANNEL_CH32 :
			return 32;

		case CHANNEL_CH33 :
			return 33;

		case CHANNEL_CH34 :
			return 34;

		case CHANNEL_CH35 :
			return 35;

		default :
			print( FATAL, "Pulser has no channel %s.\n",
				   Channel_Names[ channel ] );
			THROW( EXCEPTION );
	}

	return -1;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
