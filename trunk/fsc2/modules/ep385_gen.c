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
		char *min = T_strdup( ep385_ptime( ( double ) FIXED_TIMEBASE ) );

		print( FATAL, "Invalid time base of %s, must be at least  %s.\n",
			   ep385_ptime( timebase ), min );
		T_free( min );
		THROW( EXCEPTION );
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
	FUNCTION *f = &ep385.function[ function ];
	CHANNEL *c = &ep385.channel[ channel - CHANNEL_OFFSET ];


	channel -= CHANNEL_OFFSET;

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel, valid are CH[%d-%d].\n", 
			   CHANNEL_OFFSET, MAX_CHANNELS + CHANNEL_OFFSET - 1 );
		THROW( EXCEPTION );
	}

	/* Check that the channel hasn't already been assigned to a function */

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			print( SEVERE, "Channel %ld gets assigned more than once to "
				   "function '%s'.\n", channel + CHANNEL_OFFSET,
				   Function_Names[ c->function->self ] );
			return FAIL;
		}

		print( FATAL, "Channel %ld is already used for function '%s'.\n",
			   channel + CHANNEL_OFFSET, Function_Names[ c->function->self ] );
		THROW( EXCEPTION );
	}

	/* Append the channel to the list of channels assigned to the function */

	f->is_used = SET;
	f->channel[ f->num_channels++ ] = c;

	c->function = f;

	return OK;
}


/*-----------------------------------------------------------------*/
/* Function for setting a delay for a function - negative are only */
/* possible for INTERNAL trigger mode!                             */
/*-----------------------------------------------------------------*/

bool ep385_set_function_delay( int function, double delay )
{
	Ticks Delay = ep385_double2ticks( delay );
	int i;
	FUNCTION *f;


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
			print( FATAL, "Negative delays are invalid in EXTERNAL trigger "
				   "mode.\n" );
			THROW( EXCEPTION );
		}

		if ( Delay < ep385.neg_delay )
		{
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			{
				f = &ep385.function[ i ];
				if ( ! f->is_used || i == function )
					continue;
				f->delay += ep385.neg_delay - Delay;
			}
			ep385.neg_delay = ep385.function[ function ].delay = - Delay;
		}
		else
			ep385.function[ function ].delay = ep385.neg_delay - Delay;
	}
	else
		ep385.function[ function ].delay = Delay - ep385.neg_delay;

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
	if ( ep385.is_repeat_time &&
		 ep385.repeat_time != ep385_double2ticks( rep_time ) )
	{
		print( FATAL, "A different repeat time/frequency of %s/%g Hz has "
			   "already been set.\n", ep385_pticks( ep385.repeat_time ),
			   1.0 / ep385_ticks2double( ep385.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( ep385.is_trig_in_mode && ep385.trig_in_mode == EXTERNAL )
	{
		print( FATAL, "Setting a repeat time/frequency and trigger mode to "
			   "EXTERNAL isn't possible.\n" );
		THROW( EXCEPTION );
	}

	if ( rep_time <= 0 )
	{
		print( FATAL, "Invalid zero or negative repeat time: %s.\n",
			   ep385_ptime( rep_time ) );
		THROW( EXCEPTION );
	}


	ep385.repeat_time = ep385_double2ticks( rep_time );
	ep385.is_repeat_time = SET;

	return OK;
}


/*----------------------------------------------------------------------*/
/* Function allows to enforce a certain maximum sequence length in case */
/* the correct sequence length can't be determined during the test run  */
/*----------------------------------------------------------------------*/

bool ep385_set_max_seq_len( double seq_len )
{
	if ( ep385.is_max_seq_len &&
		 ep385.max_seq_len != ep385_double2ticks( seq_len ) )
	{
		print( FATAL, "A differrent minimum pattern length of %s has already "
			   "been set.\n", ep385_pticks( ep385.max_seq_len ) );
		THROW( EXCEPTION );
	}

	/* Check that the value is reasonable */

	if ( seq_len <= 0 )
	{
		print( FATAL, "Zero or negative minimum pattern length.\n" );
		THROW( EXCEPTION );
	}

	ep385.max_seq_len = ep385_double2ticks( seq_len );
	ep385.is_max_seq_len = SET;

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

	/* Check if a function has already been assigned to the phase setup */

	if ( ep385_phs[ phs ].function != NULL )
	{
		print( FATAL, "PHASE_SETUP_%d has already been assoiated with "
			   "function %s.\n",
			   phs, Function_Names[ ep385_phs[ phs ].function->self ] );
		THROW( EXCEPTION );
	}

	f = &ep385.function[ function ];

	/* Check if a phase setup has been already assigned to the function */

	if ( f->phase_setup != NULL )
	{
		print( SEVERE, "Phase setup for function %s has already been done.\n",
			   Function_Names[ f->self ] );
		return FAIL;
	}

	ep385_phs[ phs ].is_defined = SET;
	ep385_phs[ phs ].function = f;
	f->phase_setup = &ep385_phs[ phs ];

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
	channel -= CHANNEL_OFFSET;

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
			   ep385_phs[ phs ].channels[ type ]->self + CHANNEL_OFFSET );
		return FAIL;
	}

	/* Check that channel number is in valid range */

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel number %ld.\n",
			   channel + CHANNEL_OFFSET );
		THROW( EXCEPTION );
	}

	ep385_phs[ phs ].is_set[ type ] = SET;
	ep385_phs[ phs ].channels[ type ] = &ep385.channel[ channel ];

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
				   ep385_phs[ phs ].channels[ i ]->self + CHANNEL_OFFSET,
				   Phase_Types[ i ], Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		/* Check that the channel isn't already used for a different phase */

		for ( j = 0; j < i; j++ )
			if ( ep385_phs[ phs ].is_set[ j ] &&
				 ep385_phs[ phs ].channels[ i ] ==
				 							   ep385_phs[ phs ].channels[ j ] )
			{
				print( FATAL, "The same channel %d is used for phases '%s' "
					   "and '%s'.\n",
					   ep385_phs[ phs ].channels[ i ] + CHANNEL_OFFSET,
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
