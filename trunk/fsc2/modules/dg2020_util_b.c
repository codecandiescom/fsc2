/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


#include "dg2020_b.h"



/*-----------------------------------------------------------------*/
/* Converts a time into the internal type of a time specification, */
/* i.e. a integer multiple of the time base                        */
/*-----------------------------------------------------------------*/

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
		print( FATAL, "Specified time of %s is not an integer multiple of the "
			   "pulser time base of %s.\n",
			   t, dg2020_ptime( dg2020.timebase ) );
		T_free( t );
		THROW( EXCEPTION );
	}

	return Ticksrnd( ticks );
}


/*-----------------------------------------------------*/
/* Does the exact opposite of the previous function... */
/*-----------------------------------------------------*/

double dg2020_ticks2double( Ticks ticks )
{
	fsc2_assert( dg2020.is_timebase );
	return ( double ) ( dg2020.timebase * ticks );
}


/*------------------------------------------------------*/
/* Checks if the difference of the levels specified for */
/* a pod connector are within the valid limits.         */
/*------------------------------------------------------*/

void dg2020_check_pod_level_diff( double high, double low )
{
	if ( low > high )
	{
		print( FATAL, "Low voltage level is above high level, use keyword "
			   "INVERT to invert the polarity.\n" );
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


/*------------------------------------------------------------------------*/
/* Returns pointer to the pulses structure if given a valid pulse number. */
/*------------------------------------------------------------------------*/

PULSE *dg2020_get_pulse( long pnum )
{
	PULSE *cp = dg2020_Pulses;


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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

const char *dg2020_pticks( Ticks ticks )
{
	return dg2020_ptime( dg2020_ticks2double( ticks ) );
}


/*-------------------------------------------------------------------------*/
/* Functions returns a pointer to a free channel - the channels with the   */
/* lowest numbers are used first since most users probably tend to use the */
/* high number channels for storing test pulse sequences that they don't   */
/* like too much being overwritten just because they didn't set a channel- */
/* to-function-assignment in their EDL program.                            */
/*-------------------------------------------------------------------------*/

CHANNEL *dg2020_get_next_free_channel( void )
{
	int i = 0;


	while ( dg2020.channel[ i ].function != NULL )
		i++;

	fsc2_assert( i < MAX_CHANNELS );               /* this can't happen ;-) */

	return dg2020.channel + i;
}


/*------------------------------------------------------------------*/
/* Comparison function for two pulses: returns 0 if both pulses are */
/* inactive, -1 if only the second pulse is inactive or starts at a */
/* later time and 1 if only the first pulse is inactive pulse or    */
/* the second pulse starts earlier.                                 */
/*------------------------------------------------------------------*/

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


/*---------------------------------------------------------*/
/* Determines the longest sequence of all pulse functions. */
/*---------------------------------------------------------*/

Ticks dg2020_get_max_seq_len( void )
{
	int i;
	Ticks max = 0;
	FUNCTION *f;


	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;

		/* Nothing to be done for unused functions and the phase functions */

		if ( ! f->is_used ||
			 f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
			continue;

		max = Ticks_max( max, f->max_seq_len ); 
	}

	if ( dg2020.is_max_seq_len )
		max = Ticks_max( max, dg2020.max_seq_len );

	return max;
}


/*------------------------------------------------------------------------*/
/* Function calculates the amount of memory needed for padding to achieve */
/* the requested repetition time. It then sets up the blocks if they are  */
/* needed. The additional bit in the memory size is needed because, due   */
/* to a bug in the pulsers firmware, the first bit can't be used          */
/*------------------------------------------------------------------------*/

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
				   "requested repeat time of %s.\n",
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
			   "because it wouldn't fit into the pulsers memory.\n" );
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


/*----------------------------------------------------------*/
/* dg2020_prep_cmd() does some preparations in assembling   */
/* a command string for setting a pulse pattern. It first   */
/* checks the supplied parameters, allocates memory for the */
/* command string and finally assembles the start of the    */
/* command string. If the function returns succesfully one  */
/* should not forget to free the memory allocated for the   */
/* command string later on!                                 */
/* ->                                                       */
/*  * pointer to pointer to the command string              */
/*  * number of the pulser channel                          */
/*  * address of the start of the pulse pattern to be set   */
/*  * length of the pulse pattern to be set                 */
/* <-                                                       */
/*  * 1: ok, 0: error                                       */
/*----------------------------------------------------------*/

bool dg2020_prep_cmd( char **cmd, int channel, Ticks address, Ticks length )
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
	sprintf( *cmd, ":DATA:PATT:BIT %d,%ld,%ld,#%ld%s", channel,
			 ( long ) address, ( long ) length, ( long ) strlen( dummy ),
			 dummy );

	return OK;
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void dg2020_set( char *arena, Ticks start, Ticks len )
{
	fsc2_assert( start + len <= dg2020.max_seq_len );

	memset( arena + start, SET, len );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

void dg2020_clear( char *arena, Ticks start, Ticks len )
{
	fsc2_assert( start + len <= dg2020.max_seq_len );

	memset( arena + start, UNSET, len );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

int dg2020_diff( char *old_p, char *new_p, Ticks *start, Ticks *length )
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


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

void dg2020_duty_check( void )
{
	FUNCTION *f;
	int i;
	int fns[ ] = { PULSER_CHANNEL_TWT, PULSER_CHANNEL_TWT_GATE };

	if ( ! dg2020.is_repeat_time )
		return;


	for ( i = 0; i < 2; i++ )
	{
		f = dg2020.function + fns[ i ];
		if ( f->is_used && f->num_channels > 0 &&
			 dg2020_calc_max_length( f ) > 
			 						MAX_TWT_DUTY_CYCLE * dg2020.repeat_time &&
			 f->max_duty_warning++ == 0 ) 
			print( SEVERE, "Duty cycle of TWT exceeded due to length of %s "
				   "pulses.\n", f->name );
	}
}


/*-------------------------------------------------------------------------*/
/* Function calculates the total length of all active pulses of a function */
/*-------------------------------------------------------------------------*/

Ticks dg2020_calc_max_length( FUNCTION *f )
{
	int i;
	Ticks max_len = 0;


	if ( f->num_pods == 0 || f->num_channels == 0 || f->num_params == 0 )
		return 0;

	for ( i = 0; i < f->num_params; i++ )
		max_len += f->pulse_params[ i ].len;

	return max_len;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_show_pulses( void )
{
	int pd[ 2 ];
	pid_t pid;


	if ( pipe( pd ) == -1 )
	{
		if ( errno == EMFILE || errno == ENFILE )
			print( FATAL, "Failure, running out of system resources.\n" );
		return;
	}

	if ( ( pid = fork( ) ) < 0 )
	{
		if ( errno == ENOMEM || errno == EAGAIN )
			print( FATAL, "Failure, running out of system resources.\n" );
		return;
	}

	/* Here's the childs code */

	if ( pid == 0 )
	{
		char *cmd = NULL;


		CLOBBER_PROTECT( cmd );

		close( pd[ 1 ] );

		if ( dup2( pd[ 0 ], STDIN_FILENO ) == -1 )
		{
			goto filter_failure;
			close( pd[ 0 ] );
		}

		close( pd[ 0 ] );

		TRY
		{
			cmd = get_string( "%s%sfsc2_pulses", bindir, slash( bindir ) );
			TRY_SUCCESS;
		}
		OTHERWISE
			goto filter_failure;

		execl( cmd, "fsc2_pulses", NULL );

	filter_failure:

		T_free( cmd );
		_exit( EXIT_FAILURE );
	}

	/* And finally the code for the parent */

	close( pd[ 0 ] );
	dg2020.show_file = fdopen( pd[ 1 ], "w" );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void dg2020_dump_pulses( void )
{
	char *name;
	char *m;
	struct stat stat_buf;


	do
	{
		TRY
		{
			name = T_strdup( fl_show_fselector( "File for dumping pulses:",
												"./", "*.pls", NULL ) );
			TRY_SUCCESS;
		}
		OTHERWISE
			return;

		if ( name == NULL || *name == '\0' )
		{
			T_free( name );
			return;
		}

		if  ( 0 == stat( name, &stat_buf ) )
		{
			m = get_string( "The selected file does already exist:\n%s\n"
							"\nDo you really want to overwrite it?", name );
			if ( 1 != show_choices( m, 2, "Yes", "No", NULL, 2 ) )
			{
				T_free( m );
				name = CHAR_P T_free( name );
				continue;
			}
			T_free( m );
		}

		if ( ( dg2020.dump_file = fopen( name, "w+" ) ) == NULL )
		{
			switch( errno )
			{
				case EMFILE :
					show_message( "Sorry, you have too many open files!\n"
								  "Please close at least one and retry." );
					break;

				case ENFILE :
					show_message( "Sorry, system limit for open files "
								  "exceeded!\n Please try to close some "
								  "files and retry." );
				break;

				case ENOSPC :
					show_message( "Sorry, no space left on device for more "
								  "file!\n    Please delete some files and "
								  "retry." );
					break;

				default :
					show_message( "Sorry, can't open selected file for "
								  "writing!\n       Please select a "
								  "different file." );
			}

			name = CHAR_P T_free( name );
			continue;
		}
	} while ( dg2020.dump_file == NULL );

	T_free( name );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

void dg2020_dump_channels( FILE *fp )
{
	FUNCTION *f;
	POD *pod;
	PULSE_PARAMS *pp;
	int i, j, k;


	if ( fp == NULL )
		return;

	fprintf( fp, "===\n" );

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;

		if ( f->num_pods == 0 )
			continue;

		for ( j = 0; j < f->num_pods; j++ )
		{
			pod = f->pod[ j ];

			fprintf( fp, "%s:%d", f->name, pod->self );
			for ( k = 0; k < f->num_params; k++ )
			{
				pp = f->pulse_params + k;
				if ( f->self == PULSER_CHANNEL_PULSE_SHAPE &&
					 pp->pulse->sp != NULL )
					fprintf( fp, " (%ld) %ld %ld",
							 pp->pulse->sp->num, pp->pos, pp->len );
				else if ( f->self == PULSER_CHANNEL_TWT &&
						  pp->pulse->tp != NULL )
					fprintf( fp, " (%ld) %ld %ld",
							 pp->pulse->tp->num, pp->pos, pp->len );
				else if ( pp->pulse->pc == NULL ||
						  f->phase_setup->pod[
							  pp->pulse->pc->sequence[ f->next_phase ] ]
						  == pod )
					fprintf( fp, " %ld %ld %ld",
							 pp->pulse->num, pp->pos, pp->len );
			}
			fprintf( fp, "\n" );
		}
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
