/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


static int Cur_PHS = -1;                 /* used for internal sanity checks */


/*------------------------------------------------------------------*/
/* Function is called via the TIMEBASE command to set the timebase  */
/* used with the pulser - got to be called first because all nearly */
/* all other functions depend on the timebase setting !             */
/*------------------------------------------------------------------*/

bool dg2020_store_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		char *min = T_strdup( dg2020_ptime( ( double ) MIN_TIMEBASE ) );
		char *max = T_strdup( dg2020_ptime( ( double ) MAX_TIMEBASE ) );

		print( FATAL, "Invalid time base of %s, valid range is %s to %s.\n",
			   dg2020_ptime( timebase ), min, max );
		T_free( min );
		T_free( max );
		THROW( EXCEPTION );
	}

	dg2020.is_timebase = SET;
	dg2020.timebase = timebase;

	dg2020.minimum_twt_pulse_distance =
					 Ticksrnd( ceil( MINIMUM_TWT_PULSE_DISTANCE / timebase ) );

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_assign_function( int function, long pod )
{
	FUNCTION *f = dg2020.function + function;
	POD *p = dg2020.pod + pod;


	/* Check that pod number is in valid range */

	if ( pod < 0 || pod > NUM_PODS )
	{
		print( FATAL, "Invalid pod number: %ld, valid pod number are %d-%d.\n",
			   pod, 0, NUM_PODS - 1 );
		THROW( EXCEPTION );
	}

	/* Check pod isn't already in use */

	if ( p->function != NULL )
	{
		print( FATAL, "Pod number %ld has already been assigned to function "
			   "'%s'.\n", pod, p->function->name );
		THROW( EXCEPTION );
	}

	/* In this driver we don't have any use for phase functions */

	if ( f->self == PULSER_CHANNEL_PHASE_1 ||
		 f->self == PULSER_CHANNEL_PHASE_2 )
	{
		print( FATAL, "Phase functions can't be used with this driver.\n" );
		THROW( EXCEPTION );
	}

	/* Check that function doesn't get assigned too many pods */

	if ( f->num_pods >= MAX_PODS_PER_FUNC )
	{
		print( FATAL, "Function %s has already been assigned the maximum "
			   "number of %d pods.\n", f->name, ( int ) MAX_PODS_PER_FUNC );
		THROW( EXCEPTION );
	}

	f->is_used = SET;
	f->pod[ f->num_pods++ ] = p;
	p->function = f;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_assign_channel_to_function( int function, long channel )
{
	FUNCTION *f = dg2020.function + function;
	CHANNEL *c = dg2020.channel + channel;


	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		print( FATAL, "Invalid channel number: %ld, valid range is 0-%d.\n",
			   channel, ( int ) MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			print( SEVERE, "Channel %ld is assigned twice to function '%s'.\n",
				   channel, c->function->name );
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

	f->is_used = SET;
	f->channel[ f->num_channels++ ] = c;

	c->function = f;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_invert_function( int function )
{
	dg2020.function[ function ].is_inverted = SET;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_function_delay( int function, double delay )
{
	Ticks Delay = dg2020_double2ticks( delay );
	int i;


	if ( dg2020.function[ function ].is_delay )
	{
		print( FATAL, "Delay for function '%s' has already been set.\n",
			   Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* Check that the delay value is reasonable */

	if ( Delay < 0 )
	{
		if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode ) ||
			 dg2020.is_trig_in_slope || dg2020.is_trig_in_level ||
			 dg2020.is_trig_in_impedance )
		{
			print( FATAL, "Negative delays are invalid in EXTERNAL trigger "
				   "mode.\n" );
			THROW( EXCEPTION );
		}

		dg2020.is_neg_delay = SET;

		if ( Delay < - dg2020.neg_delay )
		{
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
				dg2020.function[ i ].delay -= dg2020.neg_delay + Delay;

			dg2020.neg_delay = - Delay;
			dg2020.function[ function ].delay = 0;
		}
		else
			dg2020.function[ function ].delay += Delay;
	}
	else
		dg2020.function[ function ].delay += Delay;

	dg2020.function[ function ].is_used = SET;
	dg2020.function[ function ].is_delay = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_function_high_level( int function, double voltage )
{
	long v;


	v = lrnd( voltage / VOLTAGE_RESOLUTION );

	if ( v < lrnd( MIN_POD_HIGH_VOLTAGE / VOLTAGE_RESOLUTION ) ||
		 v > lrnd( MAX_POD_HIGH_VOLTAGE / VOLTAGE_RESOLUTION ) )
	{
		print( FATAL, "Invalid high level of %g V for function '%s', valid "
			   "range is %g V to %g V.\n", voltage, Function_Names[ function ],
			   MIN_POD_HIGH_VOLTAGE, MAX_POD_HIGH_VOLTAGE );
		THROW( EXCEPTION );
	}

	voltage = VOLTAGE_RESOLUTION * v;

	if ( dg2020.function[ function ].is_low_level )
		dg2020_check_pod_level_diff( voltage,
									 dg2020.function[ function ].low_level );

	dg2020.function[ function ].high_level = voltage;
	dg2020.function[ function ].is_high_level = SET;
	dg2020.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_function_low_level( int function, double voltage )
{
	long v;


	v = lrnd( voltage / VOLTAGE_RESOLUTION );

	if ( v < lrnd( MIN_POD_LOW_VOLTAGE / VOLTAGE_RESOLUTION ) ||
		 v > lrnd( MAX_POD_LOW_VOLTAGE / VOLTAGE_RESOLUTION ) )
	{
		print( FATAL, "Invalid low level of %g V for function '%s', valid "
			   "range is %g V to %g V.\n", voltage, Function_Names[ function ],
			   MIN_POD_LOW_VOLTAGE, MAX_POD_LOW_VOLTAGE );
		THROW( EXCEPTION );
	}

	voltage = VOLTAGE_RESOLUTION * v;

	if ( dg2020.function[ function ].is_high_level )
		dg2020_check_pod_level_diff( dg2020.function[ function ].high_level,
									 voltage );

	dg2020.function[ function ].low_level = voltage;
	dg2020.function[ function ].is_low_level = SET;
	dg2020.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_trigger_mode( int mode )
{
	fsc2_assert( mode == INTERNAL || mode == EXTERNAL );

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode != mode )
	{
		print( FATAL, "Trigger mode has already been set to %s.\n",
			   mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			print( FATAL, "INTERNAL trigger mode and setting a trigger slope "
				   "isn't possible.\n" );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_level )
		{
			print( FATAL, "INTERNAL trigger mode and setting a trigger level "
				   "isn't possible.\n" );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_impedance )
		{
			print( FATAL, "INTERNAL trigger mode and setting a trigger "
				   "impedance is isn't possible.\n" );
			THROW( EXCEPTION );
		}
	}
	else if ( dg2020.is_neg_delay )
	{
		print( FATAL, "EXTERNAL trigger mode and using negative delays "
			   "for functions isn't possible.\n" );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_mode = mode;
	dg2020.is_trig_in_mode = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_trig_in_level( double voltage )
{
	long v;


	v = lrnd( voltage / VOLTAGE_RESOLUTION );

	voltage = VOLTAGE_RESOLUTION * v;

	if ( dg2020.is_trig_in_level && dg2020.trig_in_level != voltage )
	{
		print( FATAL, "A different level of %g V for the trigger has already "
			   "been set.\n", dg2020.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		print( SEVERE, "Setting a trigger level is useless in INTERNAL "
			   "trigger mode.\n" );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) )
	{
		print( FATAL, "Setting a trigger level (thus implicitly selecting "
			   "EXTERNAL trigger mode) and using negative delays for "
			   "functions is incompatible.\n" );
		THROW( EXCEPTION );
	}

	if ( v > lrnd( MAX_TRIG_IN_LEVEL / VOLTAGE_RESOLUTION ) ||
		 v < lrnd( MIN_TRIG_IN_LEVEL / VOLTAGE_RESOLUTION ) )
	{
		print( FATAL, "Invalid level for trigger of %g V, valid range is %g V "
			   "to %g V.\n", voltage, MIN_TRIG_IN_LEVEL, MAX_TRIG_IN_LEVEL );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_level = voltage;
	dg2020.is_trig_in_level = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_trig_in_slope( int slope )
{
	fsc2_assert( slope == POSITIVE || slope == NEGATIVE );

	if ( dg2020.is_trig_in_slope && dg2020.trig_in_slope != slope )
	{
		print( FATAL, "A different trigger slope (%s) has already been set.\n",
			   slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		print( SEVERE, "Setting a trigger slope is useless in INTERNAL "
			   "trigger mode.\n" );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) )
	{
		print( FATAL, "Setting a trigger slope (implicitly selecting EXTERNAL "
			   "trigger mode) and using negative delays for functions is "
			   "incompatible.\n" );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_slope = slope;
	dg2020.is_trig_in_slope = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_trig_in_impedance( int state )
{
	fsc2_assert( state == LOW || state == HIGH );

	if ( dg2020.is_trig_in_impedance && dg2020.trig_in_impedance != state )
	{
		print( FATAL, "A trigger impedance (%s) has already been set.\n",
			   state == LOW ? "LOW = 50 Ohm" : "HIGH = 1 kOhm" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		print( SEVERE, "Setting a trigger impedance is useless in INTERNAL "
			   "trigger mode.\n" );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) )
	{
		print( FATAL, "Setting a trigger impedance (implicitly selecting "
			   "EXTERNAL trigger mode) and using negative delays for "
			   "functions is incompatible.\n" );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_impedance = state;
	dg2020.is_trig_in_impedance = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_repeat_time( double rep_time )
{
	if ( dg2020.is_repeat_time &&
		 dg2020.repeat_time != dg2020_double2ticks( rep_time ) )
	{
		print( FATAL, "A different repeat time/frequency of %s/%g Hz has "
			   "already been set.\n", dg2020_pticks( dg2020.repeat_time ),
			   1.0 / dg2020_ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( rep_time <= 0 )
	{
		print( FATAL, "Invalid zero or negative repeat time: %s.\n",
			   dg2020_ptime( rep_time ) );
		THROW( EXCEPTION );
	}


	dg2020.repeat_time = dg2020_double2ticks( rep_time );
	dg2020.is_repeat_time = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_max_seq_len( double seq_len )
{
	if ( dg2020.is_max_seq_len &&
		 dg2020.max_seq_len != dg2020_double2ticks( seq_len ) )
	{
		print( FATAL, "A different minimum pattern length of %s has already "
			   "been set.\n", dg2020_pticks( dg2020.max_seq_len ) );
		THROW( EXCEPTION );
	}

	/* Check that the value is reasonable */

	if ( seq_len <= 0 )
	{
		print( FATAL, "Zero or negative minimum pattern length.\n" );
		THROW( EXCEPTION );
	}

	dg2020.max_seq_len = dg2020_double2ticks( seq_len );
	dg2020.is_max_seq_len = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_phase_reference( int phs, int function )
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

	if ( dg2020_phs[ phs ].function != NULL )
	{
		print( FATAL, "PHASE_SETUP_%d has already been assoiated with "
			   "function %s.\n", phs + 1, dg2020_phs[ phs ].function->name );
		THROW( EXCEPTION );
	}

	f = dg2020.function + function;

	/* Check if a phase setup has been already assigned to the function */

	if ( f->phase_setup != NULL )
	{
		print( FATAL, "Phase setup for function %s has already been done.\n",
			   f->name );
		THROW( EXCEPTION );
	}

	dg2020_phs[ phs ].is_defined = SET;
	dg2020_phs[ phs ].function = f;
	f->phase_setup = dg2020_phs + phs;

	return OK;
}


/*---------------------------------------------------------------------*/
/* This funcion gets called for setting a phase type - pod association */
/* in a PHASE_SETUP commmand.                                          */
/*---------------------------------------------------------------------*/

bool dg2020_phase_setup_prep( int phs, int type, int dummy, long pod )
{
	UNUSED_ARGUMENT( dummy );
	fsc2_assert( Cur_PHS != - 1 ? ( Cur_PHS == phs ) : 1 );
	fsc2_assert( phs == 0 || phs == 1 );

	Cur_PHS = phs;

	/* Make sure the phase type is supported */

	if  ( type < PHASE_PLUS_X || type >= NUM_PHASE_TYPES )
	{
		print( FATAL, "Unknown phase type.\n" );
		THROW( EXCEPTION );
	}

	/* Complain if a pod has already been assigned for this phase type */

	if ( dg2020_phs[ phs ].is_set[ type ] )
	{
		fsc2_assert( dg2020_phs[ phs ].pod[ type ] != NULL );

		print( SEVERE, "Pod for controlling phase type '%s' has already been "
			   "set to %d.\n",
			   Phase_Types[ type ], dg2020_phs[ phs ].pod[ type ]->self );
		return FAIL;
	}

	/* Check that pod number is in valid range */

	if ( pod < 0 || pod >= NUM_PODS )
	{
		print( FATAL, "Invalid pod number %ld, valid range is %ld-%ld.\n",
			   pod, 0, NUM_PODS );
		THROW( EXCEPTION );
	}

	dg2020_phs[ phs ].is_set[ type ] = SET;
	dg2020_phs[ phs ].pod[ type ] = dg2020.pod + pod;

	return OK;
}


/*-------------------------------------------------*/
/* Function just does a primary sanity check after */
/* a PHASE_SETUP command has been parsed.          */
/*-------------------------------------------------*/

bool dg2020_phase_setup( int phs )
{
	bool is_set = UNSET;
	int i, j;
	FUNCTION *f;


	fsc2_assert( Cur_PHS != -1 && Cur_PHS == phs );
	Cur_PHS = -1;

	if ( dg2020_phs[ phs ].function == NULL )
	{
		print( SEVERE, "No function has been associated with "
			   "PHASE_SETUP_%1d.\n", phs + 1 );
		return FAIL;
	}

	for ( i = 0; i < NUM_PHASE_TYPES; i++ )
	{
		if ( ! dg2020_phs[ phs ].is_set[ i ] )
			 continue;

		is_set = SET;
		f = dg2020_phs[ phs ].function;

		/* Check that the pod reserved for the current phase is reserved for
		   the function that we're going to phase-cycle */

		for ( j = 0; j < f->num_pods; j++ )
			if ( dg2020_phs[ phs ].pod[ i ] == f->pod[ j ] )
				break;

		if ( j == f->num_pods )
		{
			print( FATAL, "Pod %d needed for phase '%s' is not reserved for "
				   "function '%s'.\n", dg2020_phs[ phs ].pod[ i ]->self,
				   Phase_Types[ i ], f->name );
			THROW( EXCEPTION );
		}

		/* Check that the pod isn't already used for a different phase */

		for ( j = 0; j < i; j++ )
			if ( dg2020_phs[ phs ].is_set[ j ] &&
				 dg2020_phs[ phs ].pod[ i ] == dg2020_phs[ phs ].pod[ j ] )
			{
				print( FATAL, "The same pod %d is used for phases '%s' and "
					   "'%s'.\n", dg2020_phs[ phs ].pod[ i ]->self,
					   Phase_Types[ j ], Phase_Types[ i ] );
				THROW( EXCEPTION );
			}
	}

	/* Complain if no pods were declared for phase cycling */

	if ( ! is_set )
	{
		print( SEVERE, "No pods have been assigned to phases in "
			   "PHASE_SETUP_%1d.\n", phs + 1 );
		return FAIL;
	}

	return OK;
}


/*----------------------------------------------*/
/* Function that gets called to tell the pulser */
/* driver to keep all pulses, even unused ones. */
/*----------------------------------------------*/

bool dg2020_keep_all( void )
{
	dg2020.keep_all = SET;
	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
