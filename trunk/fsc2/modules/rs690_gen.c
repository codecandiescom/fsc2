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

#include "rs690.h"


static int Cur_PHS = -1;                 /* used for internal sanity checks */


/*------------------------------------------------------------------*/
/* Function is called via the TIMEBASE command to set the timebase  */
/* used with the pulser - got to be called first because all nearly */
/* all other functions depend on the timebase setting !             */
/*------------------------------------------------------------------*/

bool rs690_store_timebase( double timebase )
{
	int i;


	if ( rs690.is_timebase )
	{
		print( FATAL, "Time base has already been set to %s.\n",
			   rs690_ptime( rs690.timebase ) );
		THROW( EXCEPTION );
	}

	/* Check for too short time bases */

	if ( timebase < rs690_fixed_timebases[ 0 ] * 0.999 )
	{
		static char *min = NULL;

		TRY
		{
			min = T_strdup( rs690_ptime( rs690_fixed_timebases[ 0 ] ) );
			print( FATAL, "Invalid time base of %s, must be at least  %s.\n",
				   rs690_ptime( timebase ), min );
			THROW( EXCEPTION );
		}
		OTHERWISE
		{
			if ( min )
				T_free( min );
			RETHROW( );
		}
	}

	/* Figure out if we can use the internal clock (with time bases of
	   4 ns, 8 ns or 16 ns) or if we have to expect an external clock
	   input. The same holds if an input level type for an external clock
	   aleady has been set. */

	if ( ! rs690.is_timebase_level )
		for ( i = 0; i < NUM_FIXED_TIMEBASES; i++ )
		{
			if ( timebase >= rs690_fixed_timebases[ i ] * 0.999 &&
				 timebase <= rs690_fixed_timebases[ i ] * 1.001 )
			{
				rs690.timebase_type = i;
				rs690.timebase = rs690_fixed_timebases[ i ];
				rs690.timebase_mode = INTERNAL;
				rs690.is_timebase = SET;
				break;
			}

			if ( timebase > rs690_fixed_timebases[ i ] )
				continue;

			rs690.timebase_type = i - 1;
			rs690.timebase = timebase;
			rs690.timebase_mode = EXTERNAL;
			rs690.is_timebase = SET;
			break;
		}

	/* External time bases are handled like an internal time base of 4 ns */

	if ( ! rs690.is_timebase )
	{
		rs690.timebase_type = TIMEBASE_4_NS;
		rs690.timebase = timebase;
		rs690.timebase_mode = EXTERNAL;
		rs690.is_timebase = SET;
	}

	/* Remind the user that with the choosen time base an external clock
	   might be needed */

	if ( rs690.timebase_mode == EXTERNAL && ! rs690.is_timebase_level )
		print( NO_ERROR, "Time base of %s requires external clock.\n",
			   rs690_ptime( timebase ) );

	return OK;
}


/*------------------------------------------------*/
/*------------------------------------------------*/

bool rs690_store_timebase_level( int level_type )
{
	fsc2_assert( level_type == TTL_LEVEL || level_type == ECL_LEVEL );

	if ( rs690.is_timebase_level )
	{
		print( FATAL, "External input for clock already has been set to "
			   "%s.\n", rs690.timebase_level == TTL_LEVEL ? "TTL" : "ECL" );
		THROW( EXCEPTION );
	}

	rs690.timebase_level = level_type;
	rs690.is_timebase_level = SET;

	if ( rs690.is_timebase && rs690.timebase_mode == INTERNAL )
	{
		rs690.timebase_mode = EXTERNAL;
		rs690.timebase_type = TIMEBASE_4_NS;
	}

	return OK;
}


/*------------------------------------------------*/
/* Function for assigning a channel to a function */
/*------------------------------------------------*/

bool rs690_assign_channel_to_function( int function, long channel )
{
	FUNCTION *f = rs690.function + function;
	CHANNEL *c = rs690.channel + channel;


	fsc2_assert( function >= 0 && function < PULSER_CHANNEL_NUM_FUNC );


	if ( function == PULSER_CHANNEL_PHASE_1 ||
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "Phase pulse functions can't be used with this "
			   "driver.\n" );
		THROW( EXCEPTION );
	}

	/* Prelimary test for completely bogus channel numbers */

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel, valid are [A-%c][0-15].\n", 
			   NUM_HSM_CARDS == 1 ? 'D' : 'H' );
		THROW( EXCEPTION );
	}

	/* The channels that can be used depend on the time base - so if it hasn't
	   been set complain and bail out */

	if ( ! rs690.is_timebase )
	{
		print( FATAL, "Can't assign channels because no pulser time base has "
			   "been set.\n" );
		THROW( EXCEPTION );
	}

	/* For time bases between 4 and 8 ns only the lowest four channels of each
	   connector can be used, for time bases between 8 and 16 ns only the
	   lower 8 channels */

	switch ( rs690.timebase_type )
	{
		case TIMEBASE_4_NS :
			if ( channel % 16 >= 4 )
			{
				print( FATAL, "For time bases below 8 ns only channels "
					   "[A-%c][0-3] can be used\n",
					   NUM_HSM_CARDS == 1 ? 'D' : 'H' );
				THROW( EXCEPTION );
			}
			break;

		case TIMEBASE_8_NS :
			if ( channel % 16 >= 8 )
			{
				print( FATAL, "For time bases below 16 ns only channels "
					   "[A-%c][0-7] can be used\n",
					   NUM_HSM_CARDS == 1 ? 'D' : 'H' );
				THROW( EXCEPTION );
			}
			break;

		case TIMEBASE_16_NS :
			break;

		default :
			fsc2_assert( 1 == 0 );
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

bool rs690_set_function_delay( int function, double delay )
{
	Ticks Delay = rs690_double2ticks( delay );
	int i;


	if ( rs690.function[ function ].is_delay )
	{
		print( FATAL, "Delay for function '%s' has already been set.\n",
			   Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* Check that the delay value is reasonable */

	if ( Delay < 0 )
	{
		if ( rs690.is_trig_in_mode && rs690.trig_in_mode == EXTERNAL )
		{
			print( FATAL, "Negative delays are impossible in EXTERNAL trigger "
				   "mode.\n" );
			THROW( EXCEPTION );
		}

		if ( Delay < rs690.neg_delay )
		{
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			{
				if ( i == function )
					continue;
				rs690.function[ i ].delay -= rs690.neg_delay + Delay;
			}
			rs690.neg_delay = - Delay;
			rs690.function[ function ].delay = 0;
		}
		else
			rs690.function[ function ].delay += rs690.neg_delay + Delay;
	}
	else
		rs690.function[ function ].delay += Delay;

	rs690.function[ function ].is_used = SET;
	rs690.function[ function ].is_delay = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool rs690_invert_function( int function )
{
	rs690.function[ function ].is_inverted = SET;
	return OK;
}


/*--------------------------------------------------------------------*/
/* Function for setting the trigger mode, either INTERNAL or EXTERNAL */
/*--------------------------------------------------------------------*/

bool rs690_set_trigger_mode( int mode )
{
	fsc2_assert( mode == INTERNAL || mode == EXTERNAL );


	if ( rs690.is_trig_in_mode && rs690.trig_in_mode != mode )
	{
		print( FATAL, "Trigger mode has already been set to %sTERNAL.\n",
			   rs690.trig_in_mode == INTERNAL ? "IN" : "EX" );
		THROW( EXCEPTION );
	}

	if ( mode == EXTERNAL && rs690.is_neg_delay )
	{
		print( FATAL, "EXTERNAL trigger mode and using negative delays "
			   "for functions isn't possible.\n" );
		THROW( EXCEPTION );
	}

	rs690.trig_in_mode = mode;
	rs690.is_trig_in_mode = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool rs690_set_trig_in_slope( int slope )
{
	fsc2_assert( slope == POSITIVE || slope == NEGATIVE );

	if ( rs690.is_trig_in_slope && rs690.trig_in_slope != slope )
	{
		print( FATAL, "A different trigger slope (%s) has already been set.\n",
			   slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_trig_in_mode && rs690.trig_in_mode == INTERNAL )
	{
		print( SEVERE, "Setting a trigger slope is useless in INTERNAL "
			   "trigger mode.\n" );
		return FAIL;
	}

	if ( rs690.is_repeat_time )
	{
		print( FATAL, "Setting a trigger slope and requesting a fixed "
			   "repetition time or frequency is mutually exclusive.\n" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_neg_delay &&
		 ! ( ( rs690.is_trig_in_mode && rs690.trig_in_mode == INTERNAL ) ||
			 rs690.is_repeat_time ) )
	{
		print( FATAL, "Setting a trigger slope (requiring EXTERNAL "
			   "trigger mode) and using negative delays for functions is "
			   "impossible.\n" );
		THROW( EXCEPTION );
	}

	rs690.trig_in_mode = EXTERNAL;
	rs690.trig_in_slope = slope;
	rs690.is_trig_in_slope = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool rs690_set_trig_in_level_type( double type )
{
	int level_type;


	level_type = ( int ) type;

	if ( level_type != TTL_LEVEL && level_type != ECL_LEVEL )
	{
		print( FATAL, "Trigger input levels can only be set via the "
			   "'TTL' or 'ECL' keywords for this device.\n" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_trig_in_level_type &&
		 rs690.trig_in_level_type != level_type )
	{
		print( FATAL, "A different trigger level type of '%s' has already "
			   "been set.\n",
			   rs690.trig_in_level_type == TTL_LEVEL ? "TTL" : "ECL" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_trig_in_mode && rs690.trig_in_mode == INTERNAL )
	{
		print( SEVERE, "Setting a trigger level type is useless in INTERNAL "
			   "trigger mode.\n" );
		return FAIL;
	}

	if ( rs690.is_repeat_time )
	{
		print( FATAL, "Setting a trigger input level type and requesting a "
			   "fixed repetition time or frequency is mutually exclusive.\n" );
		THROW( EXCEPTION );
	}

	if ( rs690.is_neg_delay &&
		 ! ( ( rs690.is_trig_in_mode && rs690.trig_in_mode == INTERNAL ) ||
			 rs690.is_repeat_time ) )
	{
		print( FATAL, "Setting a trigger level type (thus implicitly "
			   "selecting EXTERNAL trigger mode) and using negative delays "
			   "for functions or a fixed repetition time or frequency "
			   "mutually exclusive.\n" );
		THROW( EXCEPTION );
	}

	rs690.trig_in_mode = EXTERNAL;
	rs690.trig_in_level_type = level_type;
	rs690.is_trig_in_level_type = SET;

	return OK;
}


/*------------------------------------------------------------------*/
/* Function for setting the repetition time for the pulse sequences */
/*------------------------------------------------------------------*/

bool rs690_set_repeat_time( double rep_time )
{
	/* Complain if a different repetition time has already been set */

	if ( rs690.is_repeat_time &&
		 rs690.repeat_time != rs690_double2ticks( rep_time ) )
	{
		print( FATAL, "A different repeat time/frequency of %s/%g Hz has "
			   "already been set.\n", rs690_pticks( rs690.repeat_time ),
			   1.0 / rs690_ticks2double( rs690.repeat_time ) );
		THROW( EXCEPTION );
	}

	/* We can't set a repetition time when using an external trigger */

	if ( rs690.is_trig_in_mode && rs690.trig_in_mode == EXTERNAL )
	{
		print( FATAL, "Setting a repeat time/frequency and trigger mode to "
			   "EXTERNAL isn't possible.\n" );
		THROW( EXCEPTION );
	}

	/* Check that the repetition time is within the allowed limits */

	if ( rep_time <= 0 )
	{
		print( FATAL, "Invalid zero or negative repeat time: %s.\n",
			   rs690_ptime( rep_time ) );
		THROW( EXCEPTION );
	}

	rs690.repeat_time = rs690_double2ticks( rep_time );

	if ( rs690.timebase_type == TIMEBASE_4_NS && rs690.repeat_time % 4 )
	{
		static char *o;

		TRY
		{
			o = T_strdup( rs690_pticks( rs690.repeat_time ) );
			rs690.repeat_time = ( rs690.repeat_time / 4 + 1 ) * 4;
			print( WARN, "Adjusting repeat time/frequency from %s/%g Hz to "
				   "%s/%g Hz.\n", o, 1.0 / rep_time,
				   rs690_pticks( rs690.repeat_time ),
				   1.0 / rs690_ticks2double( rs690.repeat_time ) );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			o = CHAR_P T_free( o );
			RETHROW( );
		}

		o = CHAR_P T_free( o );
	}
	else if ( rs690.timebase_type == TIMEBASE_4_NS && rs690.repeat_time % 2 )
	{
		static char *o;

		TRY
		{
			o = T_strdup( rs690_pticks( rs690.repeat_time ) );
			rs690.repeat_time = ( rs690.repeat_time / 2 + 1 ) * 2;
			print( WARN, "Adjusting repeat time/frequency from %s/%g Hz to "
				   "%s/%g Hz.\n", o, 1.0 / rep_time,
				   rs690_pticks( rs690.repeat_time ),
				   1.0 / rs690_ticks2double( rs690.repeat_time ) );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			o = CHAR_P T_free( o );
			RETHROW( );
		}

		o = CHAR_P T_free( o );
	}
			   
	rs690.is_repeat_time = SET;

	return OK;
}


/*----------------------------------------------------------------*/
/* Fucntion associates a phase sequence with one of the functions */
/*----------------------------------------------------------------*/

bool rs690_set_phase_reference( int phs, int function )
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

	if ( rs690_phs[ phs ].function != NULL )
	{
		print( FATAL, "PHASE_SETUP_%d has already been assoiated with "
			   "function %s.\n",
			   phs, rs690_phs[ phs ].function->name );
		THROW( EXCEPTION );
	}

	f = rs690.function + function;

	/* Check if a phase setup has been already assigned to the function */

	if ( f->phase_setup != NULL )
	{
		print( FATAL, "Phase setup for function %s has already been done.\n",
			   f->name );
		THROW( EXCEPTION );
	}

	rs690_phs[ phs ].is_defined = SET;
	rs690_phs[ phs ].function = f;
	f->phase_setup = rs690_phs + phs;

	return OK;
}


/*-------------------------------------------------------------*/
/* This funcion gets called for setting a phase type - channel */
/* association in a PHASE_SETUP commmand.                      */
/*-------------------------------------------------------------*/

bool rs690_phase_setup_prep( int phs, int type, int dummy, long channel )
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

	if ( rs690_phs[ phs ].is_set[ type ] )
	{
		fsc2_assert( rs690_phs[ phs ].channels[ type ] != NULL );

		print( SEVERE, "Channel for controlling phase type '%s' has already "
			   "been set to %s.\n", Phase_Types[ type ],
			   rs690_num_2_channel(
								   rs690_phs[ phs ].channels[ type ]->self ) );
		return FAIL;
	}

	/* Check that channel number isn't comletely bogus */

	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel, valid are [A-%c][0-15].\n", 
			   NUM_HSM_CARDS == 1 ? 'D' : 'H' );
		THROW( EXCEPTION );
	}

	/* The channels that can be used depend on the time base - so if it hasn't
	   been set complain and bail out */

	if ( ! rs690.is_timebase )
	{
		print( FATAL, "Can't do phase setup because no pulser time base has "
			   "been set.\n" );
		THROW( EXCEPTION );
	}

	/* For time bases between 4 and 8 ns only the lowest four channels of each
	   connector can be used, for time bases between 8 and 16 ns only the
	   lower 8 channels */

	switch ( rs690.timebase_type )
	{
		case TIMEBASE_4_NS :
			if ( channel % 16 >= 4 )
			{
				print( FATAL, "For time bases below 8 ns only channels "
					   "[A-%c][0-3] can be used\n",
					   NUM_HSM_CARDS == 1 ? 'D' : 'H' );
				THROW( EXCEPTION );
			}
			break;

		case TIMEBASE_8_NS :
			if ( channel % 16 >= 8 )
			{
				print( FATAL, "For time bases below 16 ns only channels "
					   "[A-%c][0-7] can be used\n",
					   NUM_HSM_CARDS == 1 ? 'D' : 'H' );
				THROW( EXCEPTION );
			}
			break;

		case TIMEBASE_16_NS :
			break;

		default :
			fsc2_assert( 1 == 0 );
	}

	rs690_phs[ phs ].is_set[ type ] = SET;
	rs690_phs[ phs ].channels[ type ] = rs690.channel + channel;

	return OK;
}


/*------------------------------------------------------------------*/
/* Function does a primary sanity check after a PHASE_SETUP command */
/* has been parsed.                                                 */
/*------------------------------------------------------------------*/

bool rs690_phase_setup( int phs )
{
	bool is_set = UNSET;
	int i, j;
	FUNCTION *f;


	fsc2_assert( Cur_PHS != -1 && Cur_PHS == phs );
	Cur_PHS = -1;

	if ( rs690_phs[ phs ].function == NULL )
	{
		print( SEVERE, "No function has been associated with "
			   "PHASE_SETUP_%1d.\n", phs + 1 );
		return FAIL;
	}

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
	{
		if ( ! rs690_phs[ phs ].is_set[ i ] )
			 continue;

		is_set = SET;
		f = rs690_phs[ phs ].function;

		/* Check that the channel needed for the current phase is reserved
		   for the function that we're going to phase-cycle */

		for ( j = 0; j < f->num_channels; j++ )
			if ( rs690_phs[ phs ].channels[ i ] == f->channel[ j ] )
				break;

		if ( j == f->num_channels )
		{
			print( FATAL, "Channel %s needed for phase '%s' is not reserved "
				   "for function '%s'.\n",
				   rs690_num_2_channel( rs690_phs[ phs ].channels[ i ]->self ),
				   Phase_Types[ i ], f->name );
			THROW( EXCEPTION );
		}

		/* Check that the channel isn't already used for a different phase */

		for ( j = 0; j < i; j++ )
			if ( rs690_phs[ phs ].is_set[ j ] &&
				 rs690_phs[ phs ].channels[ i ] ==
				 							   rs690_phs[ phs ].channels[ j ] )
			{
				print( FATAL, "The same channel %d is used for phases '%s' "
					   "and '%s'.\n", rs690_phs[ phs ].channels[ i ],
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

bool rs690_keep_all( void )
{
	rs690.keep_all = SET;
	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
