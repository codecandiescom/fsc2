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


static int Cur_PHS = -1;


/*------------------------------------------------------------------*/
/* Function is called via the TIMEBASE command to set the timebase  */
/* used with the pulser - got to be called first because all nearly */
/* all other functions depend on the timebase setting !             */
/*------------------------------------------------------------------*/

bool ep385_store_timebase( double timebase )
{
	if ( ep385.is_timebase )
	{
		if ( ep385.timebase_mode == EXTERNAL )
			print( FATAL, "Time base (external) has already been set to %s.\n",
				   ep385_ptime( ep385.timebase ) );
		else
			print( FATAL, "Time base has already automatically set to 8 ns. "
				   "Make sure the TIMEBASE command comes first in the "
				   "ASSIGNMENTS section.\n" );
		THROW( EXCEPTION );
	}

	if ( timebase < FIXED_TIMEBASE )
	{
		static char *min = NULL;

		TRY
		{
			min = T_strdup( ep385_ptime( ( double ) FIXED_TIMEBASE ) );
			print( FATAL, "Invalid time base of %s, must be at least  %s.\n",
				   ep385_ptime( timebase ), min );
			THROW( EXCEPTION );
		}
		OTHERWISE
		{
			if ( min )
				T_free( min );
			RETHROW( );
		}
					
	}

	ep385.is_timebase = SET;
	ep385.timebase = timebase;
	ep385.timebase_mode = EXTERNAL;

	return OK;
}


/*------------------------------------------------*/
/* Function for assigning a channel to a function */
/*------------------------------------------------*/

bool ep385_assign_channel_to_function( int function, long channel )
{
	FUNCTION *f = ep385.function + function;
	CHANNEL *c = ep385.channel + channel;


	fsc2_assert( function >= 0 && function < PULSER_CHANNEL_NUM_FUNC );


	if ( function == PULSER_CHANNEL_PHASE_1 ||
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "Phase pulse functions can't be used with this "
			   "driver.\n" );
		THROW( EXCEPTION );
	}

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel, valid are CH[%d-%d].\n", 
			   0, MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	/* Check that the channel hasn't already been assigned to a function */

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			print( SEVERE, "Channel %ld gets assigned more than once to "
				   "function '%s'.\n", channel, c->function->name );
			return FAIL;
		}

		print( FATAL, "Channel %ld is already used for function '%s'.\n",
			   channel, c->function->name );
		THROW( EXCEPTION );
	}

	/* The PULSE_SHAPE and TWT function can only have one channel assigned
	   to it */

	if ( ( function == PULSER_CHANNEL_PULSE_SHAPE ||
		   function == PULSER_CHANNEL_TWT ) &&
		 f->num_channels > 0 )
	{
		print( FATAL, "Only one channel can be assigned to function '%s'.\n",
			   Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* Append the channel to the list of channels assigned to the function */

	f->is_used = SET;
	f->channel[ f->num_channels++ ] = c;
	c->function = f;

	return OK;
}


/*---------------------------------------------------------------*/
/* Function for setting a delay for a function - negative delays */
/* are only possible for INTERNAL trigger mode!                  */
/*---------------------------------------------------------------*/

bool ep385_set_function_delay( int function, double delay )
{
	Ticks Delay = ep385_double2ticks( delay );
	int i;


	if ( ep385.function[ function ].is_delay )
	{
		print( FATAL, "Delay for function '%s' has already been set.\n",
			   Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* Check that the delay value is reasonable */

	if ( Delay < 0 )
	{
		if ( ep385.is_trig_in_mode && ep385.trig_in_mode == EXTERNAL )
		{
			print( FATAL, "Negative delays are impossible in EXTERNAL trigger "
				   "mode.\n" );
			THROW( EXCEPTION );
		}

		if ( Delay < ep385.neg_delay )
		{
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			{
				if ( i == function )
					continue;
				ep385.function[ i ].delay -= ep385.neg_delay + Delay;
			}
			ep385.neg_delay = - Delay;
			ep385.function[ function ].delay = 0;
		}
		else
			ep385.function[ function ].delay += ep385.neg_delay + Delay;
	}
	else
		ep385.function[ function ].delay += Delay;

	ep385.function[ function ].is_used = SET;
	ep385.function[ function ].is_delay = SET;

	return OK;
}


/*--------------------------------------------------------------------*/
/* Function for setting the trigger mode, either INTERNAL or EXTERNAL */
/*--------------------------------------------------------------------*/

bool ep385_set_trigger_mode( int mode )
{
	fsc2_assert( mode == INTERNAL || mode == EXTERNAL );


	if ( ep385.is_trig_in_mode )
	{
		print( FATAL, "Trigger mode has already been set.\n" );
		THROW( EXCEPTION );
	}

	if ( mode == EXTERNAL && ep385.is_neg_delay )
	{
		print( FATAL, "EXTERNAL trigger mode and using negative delays "
			   "for functions isn't possible.\n" );
		THROW( EXCEPTION );
	}

	return OK;
}


/*------------------------------------------------------------------*/
/* Function for setting the repetition time for the pulse sequences */
/*------------------------------------------------------------------*/

bool ep385_set_repeat_time( double rep_time )
{
	static double new_rep_time;
	static double min_repeat_time;
	static double max_repeat_time;


	/* For the following we need the pulsers time base. If it hasn't been
	   set yet we default to the built-in clock running at 125 MHz. */

	if ( ! ep385.is_timebase )
	{
		ep385.timebase = FIXED_TIMEBASE;
		ep385.timebase_mode = INTERNAL;
		ep385.is_timebase = SET;
	}

	/* Complain if a different time base already has been set */

	if ( ep385.is_repeat_time &&
		 rep_time != 
		 	( new_rep_time = ( 12800 * ep385.repeat_time 
								+ BITS_PER_MEMORY_BLOCK * MAX_MEMORY_BLOCKS )
							 * ep385.timebase ) )
	{
		print( FATAL, "A different repeat time/frequency of %s/%g Hz has "
			   "already been set.\n", ep385_ptime( new_rep_time ),
			   1.0 / new_rep_time );
		THROW( EXCEPTION );
	}

	/* We can't set a repetition time with external trigger */

	if ( ep385.is_trig_in_mode && ep385.trig_in_mode == EXTERNAL )
	{
		print( FATAL, "Setting a repeat time/frequency and trigger mode to "
			   "EXTERNAL isn't possible.\n" );
		THROW( EXCEPTION );
	}

	/* Check that the repetition time is within the allowed limits */

	if ( rep_time <= 0 )
	{
		print( FATAL, "Invalid zero or negative repeat time: %s.\n",
			   ep385_ptime( rep_time ) );
		THROW( EXCEPTION );
	}

	min_repeat_time	= ( BITS_PER_MEMORY_BLOCK * MAX_MEMORY_BLOCKS
						+ MIN_REPEAT_TIMES * REPEAT_TICKS ) * ep385.timebase;
	max_repeat_time	= ( BITS_PER_MEMORY_BLOCK * MAX_MEMORY_BLOCKS
						+ MAX_REPEAT_TIMES * REPEAT_TICKS ) * ep385.timebase;

	if ( rep_time < min_repeat_time * 0.99 ||
		 rep_time > max_repeat_time * 1.01 )
	{
		static char *tmin = NULL, *tmax = NULL;

		TRY
		{
			tmin = T_strdup( ep385_ptime( min_repeat_time ) );
			tmin = T_strdup( ep385_ptime( max_repeat_time ) );

			print( FATAL, "Repeat time/frequency of %s/%g Hz is not within "
				   "range of %s/%g Hz to %s/%g Hz.\n", ep385_ptime( rep_time ),
				   tmin, 1.0 / min_repeat_time, tmax, 1.0 / max_repeat_time );
			THROW( EXCEPTION );
		}
		OTHERWISE
		{
			if ( tmin )
				T_free( tmin );
			if ( tmax )
				T_free( tmax );
			RETHROW( );
		}
	}

	ep385.repeat_time = lrnd( ceil( ( ep385_double2ticks( rep_time )
								  - BITS_PER_MEMORY_BLOCK * MAX_MEMORY_BLOCKS )
									/ (double ) REPEAT_TICKS ) );
	new_rep_time = ( ep385.repeat_time * REPEAT_TICKS 
					 + BITS_PER_MEMORY_BLOCK * MAX_MEMORY_BLOCKS )
				   * ep385.timebase;

	/* If the adjusted repetition time doesn't fit within 1% with the
	   requested time print a warning */

	if ( fabs( new_rep_time - rep_time ) > 0.01 * rep_time )
		print( WARN, "Adjusting repeat time/frequency to %s/%g Hz.\n",
			   ep385_ptime( new_rep_time ), 1.0 / new_rep_time );

	ep385.is_repeat_time = SET;

	return OK;
}


/*----------------------------------------------------------------*/
/* Fucntion associates a phase sequence with one of the functions */
/*----------------------------------------------------------------*/

bool ep385_set_phase_reference( int phs, int function )
{
	FUNCTION *f;


	fsc2_assert ( Cur_PHS != - 1 ? ( Cur_PHS == phs ) : 1 );

	Cur_PHS = phs;

	/* Phase function can't be used with this driver... */

	if ( function == PULSER_CHANNEL_PHASE_1 ||
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "PHASE function can't be used with this driver.\n" );
		THROW( EXCEPTION );
	}

	/* The PULSE_SHAPE and TWT functions can't be phase-cycled */

	if ( function == PULSER_CHANNEL_PULSE_SHAPE ||
		 function == PULSER_CHANNEL_TWT )
	{
		print( FATAL, "Function '%s' can't be phase-cycled.\n",
			   Function_Names[ PULSER_CHANNEL_PULSE_SHAPE ] );
		THROW( EXCEPTION );
	}

	/* Check if a function has already been assigned to the phase setup */

	if ( ep385_phs[ phs ].function != NULL )
	{
		print( FATAL, "PHASE_SETUP_%d has already been assoiated with "
			   "function %s.\n",
			   phs, ep385_phs[ phs ].function->name );
		THROW( EXCEPTION );
	}

	f = ep385.function + function;

	/* Check if a phase setup has been already assigned to the function */

	if ( f->phase_setup != NULL )
	{
		print( SEVERE, "Phase setup for function %s has already been done.\n",
			   f->name );
		return FAIL;
	}

	ep385_phs[ phs ].is_defined = SET;
	ep385_phs[ phs ].function = f;
	f->phase_setup = ep385_phs + phs;

	return OK;
}


/*-------------------------------------------------------------*/
/* This funcion gets called for setting a phase type - channel */
/* association in a PHASE_SETUP commmand.                      */
/*-------------------------------------------------------------*/

bool ep385_phase_setup_prep( int phs, int type, int dummy, long channel )
{
	fsc2_assert ( Cur_PHS != - 1 ? ( Cur_PHS == phs ) : 1 );
	fsc2_assert ( phs == 0 || phs == 1 );


	dummy = dummy;

	Cur_PHS = phs;

	/* Make sure the phase type is supported */

	if  ( type < PHASE_PLUS_X || type > PHASE_CW )
	{
		print( FATAL, "Unknown phase type.\n" );
		THROW( EXCEPTION );
	}

	type -= PHASE_PLUS_X;

	/* Complain if a channel has already been assigned for this phase type */

	if ( ep385_phs[ phs ].is_set[ type ] )
	{
		fsc2_assert( ep385_phs[ phs ].channels[ type ] != NULL );

		print( SEVERE, "Channel for controlling phase type '%s' has already "
			   "been set to %d.\n", Phase_Types[ type ],
			   ep385_phs[ phs ].channels[ type ]->self );
		return FAIL;
	}

	/* Check that channel number is in valid range */

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel number %ld.\n", channel );
		THROW( EXCEPTION );
	}

	ep385_phs[ phs ].is_set[ type ] = SET;
	ep385_phs[ phs ].channels[ type ] = ep385.channel + channel;

	return OK;
}


/*------------------------------------------------------------------*/
/* Function does a primary sanity check after a PHASE_SETUP command */
/* has been parsed.                                                 */
/*------------------------------------------------------------------*/

bool ep385_phase_setup( int phs )
{
	bool is_set = UNSET;
	int i, j;
	FUNCTION *f;


	fsc2_assert( Cur_PHS != -1 && Cur_PHS == phs );

	if ( ep385_phs[ phs ].function == NULL )
	{
		print( SEVERE, "No function has been associated with "
			   "PHASE_SETUP_%1d.\n", phs + 1 );
		return FAIL;
	}

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
	{
		if ( ! ep385_phs[ phs ].is_set[ i ] )
			 continue;

		is_set = SET;
		f = ep385_phs[ phs ].function;

		/* Check that the channel needed for the current phase is reserved
		   for the function that we're going to phase-cycle */

		for ( j = 0; j < f->num_channels; j++ )
			if ( ep385_phs[ phs ].channels[ i ] == f->channel[ j ] )
				break;

		if ( j == f->num_channels )
		{
			print( FATAL, "Channel %d needed for phase '%s' is not reserved "
				   "for function '%s'.\n",
				   ep385_phs[ phs ].channels[ i ]->self,
				   Phase_Types[ i ], f->name );
			THROW( EXCEPTION );
		}

		/* Check that the channel isn't already used for a different phase */

		for ( j = 0; j < i; j++ )
			if ( ep385_phs[ phs ].is_set[ j ] &&
				 ep385_phs[ phs ].channels[ i ] ==
				 							   ep385_phs[ phs ].channels[ j ] )
			{
				print( FATAL, "The same channel %d is used for phases '%s' "
					   "and '%s'.\n", ep385_phs[ phs ].channels[ i ],
					   Phase_Types[ j ], Phase_Types[ i ] );
				THROW( EXCEPTION );
			}
	}

	/* Complain if no channels were declared for phase cycling */

	if ( ! is_set )
	{
		print( SEVERE, "No channels have been assigned to phase in "
			   "PHASE_SETUP_%1d.\n", phs + 1 );
		return FAIL;
	}

	return OK;
}


/*----------------------------------------------*/
/* Function that gets called to tell the pulser */
/* driver to keep all pulses, even unused ones. */
/*----------------------------------------------*/

bool ep385_keep_all( void )
{
	ep385.keep_all = SET;
	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
