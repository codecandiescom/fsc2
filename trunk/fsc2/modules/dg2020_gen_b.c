/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


static int Cur_PHS = -1;


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

		eprint( FATAL, SET, "%s: Invalid time base of %s, valid range is "
				"%s to %s.\n", pulser_struct.name,
				dg2020_ptime( timebase ), min, max );
		T_free( min );
		T_free( max );
		THROW( EXCEPTION )
	}

	dg2020.is_timebase = SET;
	dg2020.timebase = timebase;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_assign_function( int function, long pod )
{
	FUNCTION *f = &dg2020.function[ function ];
	POD *p = &dg2020.pod[ pod ];
	

	/* Check that pod number is in valid range */

	if ( pod < 0 || pod > MAX_PODS )
	{
		eprint( FATAL, SET, "%s: Invalid pod number: %ld, valid pod number "
				"are %d-%d.\n", pulser_struct.name, pod, 0, MAX_PODS - 1 );
		THROW( EXCEPTION )
	}

	/* Check pod isn't already in use */

	if ( p->function != NULL )
	{
		eprint( FATAL, SET, "%s: Pod number %ld has already been assigned "
				"to function `%s'.\n", pulser_struct.name, pod,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION )
	}

	/* In this driver we don't have any use for phase functions */

	if ( f->self == PULSER_CHANNEL_PHASE_1 || 
		 f->self == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, SET, "%s: Phase functions can't be used with this "
				"driver.\n", pulser_struct.name );
		THROW( EXCEPTION )
	}

	/* Check that function doesn't get assigned too many pods */

	if ( f->num_pods >= MAX_PODS_PER_FUNC )
	{
		eprint( FATAL, SET, "%s: Function %s has already been assigned the "
				"maximum number of %d pods.\n", pulser_struct.name,
				Function_Names[ f->self ], ( int ) MAX_PODS_PER_FUNC );
		THROW( EXCEPTION )
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
	FUNCTION *f = &dg2020.function[ function ];
	CHANNEL *c = &dg2020.channel[ channel ];


	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		eprint( FATAL, SET, "%s: Invalid channel number: %ld, valid range is "
				"0-%d.\n",
				pulser_struct.name, channel, ( int ) MAX_CHANNELS - 1 );
		THROW( EXCEPTION )
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			eprint( SEVERE, SET, "%s: Channel %ld is assigned twice to "
					"function `%s'.\n", pulser_struct.name, channel,
					Function_Names[ c->function->self ] );
			return FAIL;
		}

		eprint( FATAL, SET, "%s: Channel %ld is already used for function "
				"`%s'.\n", pulser_struct.name, channel,
				Function_Names[ c->function->self ] );
		THROW( EXCEPTION )
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
	FUNCTION *f;


	if ( dg2020.function[ function ].is_delay )
	{
		eprint( FATAL, SET, "%s: Delay for function `%s' has already been "
				"set.\n", pulser_struct.name, Function_Names[ function ] );
		THROW( EXCEPTION )
	}

	/* Check that the delay value is reasonable */

	if ( Delay < 0 )
	{
		if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode ) ||
			 dg2020.is_trig_in_slope || dg2020.is_trig_in_level ||
			 dg2020.is_trig_in_impedance )
		{
			eprint( FATAL, SET, "Negative delays are invalid in EXTERNAL "
					"trigger mode.\n" );
			THROW( EXCEPTION )
		}

		if ( Delay < dg2020.neg_delay )
		{
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			{
				f = &dg2020.function[ i ];
				if ( ! f->is_used || i == function )
					continue;
				f->delay += dg2020.neg_delay - Delay;
			}
			dg2020.neg_delay = dg2020.function[ function ].delay = - Delay;
		}
		else
			dg2020.function[ function ].delay = dg2020.neg_delay - Delay;
	}
	else
		dg2020.function[ function ].delay = Delay - dg2020.neg_delay;
		
	dg2020.function[ function ].is_used = SET;
	dg2020.function[ function ].is_delay = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_function_high_level( int function, double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lrnd( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_HIGH_VOLTAGE || voltage > MAX_POD_HIGH_VOLTAGE )
	{
		eprint( FATAL, SET, "%s: Invalid high level of %g V for function "
				"`%s', valid range is %g V to %g V.\n",
				pulser_struct.name, voltage, Function_Names[ function ],
				MIN_POD_HIGH_VOLTAGE, MAX_POD_HIGH_VOLTAGE );
		THROW( EXCEPTION )
	}
				
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
	voltage = VOLTAGE_RESOLUTION * lrnd( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_LOW_VOLTAGE || voltage > MAX_POD_LOW_VOLTAGE )
	{
		eprint( FATAL, SET, "%s: Invalid low level of %g V for function "
				"`%s', valid range is %g V to %g V.\n",
				pulser_struct.name, voltage, Function_Names[ function ],
				MIN_POD_LOW_VOLTAGE, MAX_POD_LOW_VOLTAGE );
		THROW( EXCEPTION )
	}
				
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
		eprint( FATAL, SET, "%s: Trigger mode has already been set to "
				"%s.\n", pulser_struct.name,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION )
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			eprint( FATAL, SET, "%s: INTERNAL trigger mode and setting a "
					"trigger slope isn't possible.\n", pulser_struct.name );
			THROW( EXCEPTION )
		}

		if ( dg2020.is_trig_in_level )
		{
			eprint( FATAL, SET, "%s: INTERNAL trigger mode and setting a "
					"trigger level isn't possible.\n", pulser_struct.name );
			THROW( EXCEPTION )
		}

		if ( dg2020.is_trig_in_impedance )
		{
			eprint( FATAL, SET, "%s: INTERNAL trigger mode and setting a "
					"trigger impedance is isn't possible.\n",
					pulser_struct.name );
			THROW( EXCEPTION )
		}
	}
	else
	{
		if ( dg2020.is_repeat_time )
		{
			eprint( FATAL, SET, "%s: EXTERNAL trigger mode and setting a "
					"repeat time or frequency isn't possible.\n",
					pulser_struct.name );
			THROW( EXCEPTION )
		}

		if ( dg2020.is_neg_delay )
		{
			eprint( FATAL, SET, "%s: EXTERNAL trigger mode and using "
					"negative delays for functions isn't possible.\n",
					pulser_struct.name );
			THROW( EXCEPTION )
		}
	}

	dg2020.trig_in_mode = mode;
	dg2020.is_trig_in_mode = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_trig_in_level( double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lrnd( voltage / VOLTAGE_RESOLUTION );

	if ( dg2020.is_trig_in_level && dg2020.trig_in_level != voltage )
	{
		eprint( FATAL, SET, "%s: A different level of %g V for the trigger "
				"has already been set.\n", pulser_struct.name,
				dg2020.trig_in_level );
		THROW( EXCEPTION )
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time )
	{
		eprint( SEVERE, SET, "%s: Setting a trigger level is useless in "
				"INTERNAL trigger mode.\n", pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, SET, "%s: Setting a trigger level (thus implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.\n", pulser_struct.name );
		THROW( EXCEPTION )
	}

	if ( voltage > MAX_TRIG_IN_LEVEL || voltage < MIN_TRIG_IN_LEVEL )
	{
		eprint( FATAL, SET, "%s: Invalid level for trigger of %g V, valid "
				"range is %g V to %g V.\n", pulser_struct.name,
				MIN_TRIG_IN_LEVEL, MAX_TRIG_IN_LEVEL );
		THROW( EXCEPTION )
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
		eprint( FATAL, SET, "%s: A different trigger slope (%s) has "
				"already been set.\n", pulser_struct.name,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION )
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time)
	{
		eprint( SEVERE, SET, "%s: Setting a trigger slope is useless in "
				"INTERNAL trigger mode.\n", pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, SET, "s: Setting a trigger slope (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.\n", pulser_struct.name );
		THROW( EXCEPTION )
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
		eprint( FATAL, SET, "%s: A trigger impedance (%s) has already been "
				"set.\n", pulser_struct.name,
				state == LOW ? "LOW = 50 Ohm" : "HIGH = 1 kOhm" );
		THROW( EXCEPTION )
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time )
	{
		eprint( SEVERE, SET, "%s: Setting a trigger impedance is useless "
				"in INTERNAL trigger mode.\n", pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, SET, "%s: Setting a trigger impedance (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.\n", pulser_struct.name );
		THROW( EXCEPTION )
	}

	dg2020.trig_in_impedance = state;
	dg2020.is_trig_in_impedance = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_repeat_time( double time )
{
	if ( dg2020.is_repeat_time &&
		 dg2020.repeat_time != dg2020_double2ticks( time ) )
	{
		eprint( FATAL, SET, "%s: A different repeat time/frequency of %s/"
				"%g Hz has already been set.\n", pulser_struct.name,
				dg2020_pticks( dg2020.repeat_time ),
				1.0 / dg2020_ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION )
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, SET, "%s: Setting a repeat time/frequency and "
				"trigger mode to EXTERNAL isn't possible.\n",
				pulser_struct.name );
		THROW( EXCEPTION )
	}

	if ( dg2020.is_trig_in_slope || dg2020.is_trig_in_level )
	{
		eprint( FATAL, SET, "%s: Setting a repeat time/frequency and a "
				"trigger slope or level isn't possible.\n",
				pulser_struct.name );
		THROW( EXCEPTION )
	}

	if ( time <= 0 )
	{
		eprint( FATAL, SET, "%s: Invalid zero or negative repeat time: "
				"%s.\n", pulser_struct.name, dg2020_ptime( time ) );
		THROW( EXCEPTION )
	}


	dg2020.repeat_time = dg2020_double2ticks( time );
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
		eprint( FATAL, SET, "%s: A differrent minimum pattern length of %s "
				"has already been set.\n", pulser_struct.name,
				dg2020_pticks( dg2020.max_seq_len ) );
		THROW( EXCEPTION )
	}

	/* Check that the value is reasonable */

	if ( seq_len <= 0 )
	{
		eprint( FATAL, SET, "%s: Zero or negative minimum pattern "
				"length.\n", pulser_struct.name );
		THROW( EXCEPTION )
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
		eprint( FATAL, SET, "%s: PHASE function can't be used with this "
				"driver.\n", pulser_struct.name );
		THROW( EXCEPTION )
	}

	/* Check if a function has already been assigned to the phase setup */

	if ( dg2020_phs[ phs ].function != NULL )
	{
		eprint( FATAL, SET, "%s: PHASE_%1d_SETUP has already been "
				"assoiated with function %s.\n", pulser_struct.name,
				phs, Function_Names[ dg2020_phs[ phs ].function->self ] );
		THROW( EXCEPTION )
	}

	f = &dg2020.function[ function ];

	/* Check if a phase setup has been already assigned to the function */

	if ( f->phase_setup != NULL )
	{
		eprint( SEVERE, SET, "%s: Phase setup for function %s has already "
				"been done.\n", Lc, pulser_struct.name,
				Function_Names[ f->self ] );
		return FAIL;
	}

	dg2020_phs[ phs ].function = f;
	f->phase_setup = &dg2020_phs[ phs ];

	return OK;
}


/*---------------------------------------------------------------------*/
/* This funcion gets called for setting a phase type - pod association */
/* in a PHASE_SETUP commmand.                                          */
/*---------------------------------------------------------------------*/

bool dg2020_phase_setup_prep( int phs, int type, int pod, long val,
							  long protocol )
{
	pod = pod;                        /* keep the compiler happy... */
	fsc2_assert ( Cur_PHS != - 1 ? ( Cur_PHS == phs ) : 1 );

	Cur_PHS = phs;

	/* Check that we don't get stuff only to be used with the Frankfurt
	   pulser driver */

	if ( protocol != PHASE_UNKNOWN_PROT && protocol != PHASE_BLN_PROT )
	{
		eprint( FATAL, SET, "%s: Invalid syntax for this driver.\n",
				pulser_struct.name );
		THROW( EXCEPTION )
	}

	/* Make sure the phase type is supported */

	if  ( type < PHASE_PLUS_X || type > PHASE_CW )
	{
		eprint( FATAL, SET, "%s: Phase type `%s' not supported by this "
				"driver.\n", pulser_struct.name,
				Phase_Types[ type ] );
		THROW( EXCEPTION )
	}

	type -= PHASE_PLUS_X;

	/* Complain if a pod has already been assigned for this phase type */

	if ( dg2020_phs[ phs ].is_set[ type ] )
	{
		fsc2_assert( dg2020_phs[ phs ].pod[ type ] != NULL );

		eprint( SEVERE, SET, "%s: Pod for controlling phase type `%s' has "
				"already been set to %d.\n", pulser_struct.name,
				Phase_Types[ type ], dg2020_phs[ phs ].pod[ type ]->self );
		return FAIL;
	}

	/* Check that pod number is in valid range */

	if ( val < 0 || val >= MAX_PODS )
	{
		eprint( FATAL, SET, "%s: Invalid pod number %d.\n",
				pulser_struct.name, val );
		THROW( EXCEPTION )
	}

	dg2020_phs[ phs ].is_set[ type ] = SET;
	dg2020_phs[ phs ].pod[ type ] = &dg2020.pod[ val ];

	return OK;
}


/*-------------------------------------------------*/
/* Function just does a primary sanity check after */
/* a PHASE_SETUP command has been parsed.          */
/*-------------------------------------------------*/

bool dg2020_phase_setup( int phs )
{
	bool is_set = UNSET;
	int i;


	fsc2_assert( Cur_PHS != -1 && Cur_PHS == phs );

	if ( dg2020_phs[ phs ].function == NULL )
	{
		eprint( SEVERE, SET, "%s: No function has been associated with "
				"PHASE_%1d_SETUP.\n", pulser_struct.name, phs );
		return FAIL;
	}

	for ( i = 0; i <= PHASE_CW - PHASE_PLUS_X; i++ )
		is_set |= dg2020_phs[ phs ].is_set[ i ];		

	if ( ! is_set )
	{
		eprint( SEVERE, SET, "%s: No pods have been assigned to phase "
				"in PHASE_%1d_SETUP.\n", pulser_struct.name, phs );
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
