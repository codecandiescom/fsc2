/*
  $Id$
*/



#include "dg2020_f.h"



/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_store_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		char *min = get_string_copy( dg2020_ptime( ( double ) MIN_TIMEBASE ) );
		char *max = get_string_copy( dg2020_ptime( ( double ) MAX_TIMEBASE ) );

		eprint( FATAL, "%s:%ld: %s: Invalid time base of %s, valid range is "
				"%s to %s.", Fname, Lc, pulser_struct.name,
				dg2020_ptime( timebase ),
				min, max );
		T_free( min );
		T_free( max );
		THROW( EXCEPTION );
	}

	dg2020.is_timebase = SET;
	dg2020.timebase = timebase;

	dg2020.function[ PULSER_CHANNEL_PHASE_1 ].psd =
		( Ticks ) ceil( DEFAULT_PHASE_SWITCH_DELAY / timebase );
	dg2020.function[ PULSER_CHANNEL_PHASE_2 ].psd =
		( Ticks ) ceil( DEFAULT_PHASE_SWITCH_DELAY / timebase );

	if ( GRACE_PERIOD != 0 )
		dg2020.grace_period = Ticks_max( ( Ticks ) ceil( GRACE_PERIOD /
														 dg2020.timebase ),
										 1 );
	else
		dg2020.grace_period = 0;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_assign_function( int function, long pod )
{
	FUNCTION *f = &dg2020.function[ function ];
	POD *p = &dg2020.pod[ pod ];
	

	if ( pod < 0 || pod > MAX_PODS )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid pod number: %ld, valid pod number "
				"are %d-%d.", Fname, Lc, pulser_struct.name, pod, 0,
				MAX_PODS - 1 );
		THROW( EXCEPTION );
	}

	if ( p->function != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Pod number %ld has already been assigned "
				"to function `%s'.", Fname, Lc, pulser_struct.name, pod,
				Function_Names[ p->function->self ] );
		THROW( EXCEPTION );
	}

	f->is_used = SET;

	/* Except for the phase functions only one pod can be assigned */

	if ( f->pod != NULL )
	{
		if ( f->self != PULSER_CHANNEL_PHASE_1 && 
			 f->self != PULSER_CHANNEL_PHASE_2 )
		{
			eprint( FATAL, "%s:%ld: %s: A pod has already been assigned to "
					"function `%s'.", Fname, Lc, pulser_struct.name,
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		if ( f->pod2 != NULL )
		{
			eprint( FATAL, "%s:%ld: %s: There have already been two pods "
					"assigned to function `%s'.", Fname, Lc,
					pulser_struct.name, Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		f->pod2 = p;
	}
	else
		f->pod = p;
	
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
		eprint( FATAL, "%s:%ld: %s: Invalid channel number: %ld, valid range "
				"is 0-%d.", Fname, Lc, pulser_struct.name, channel,
				MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			eprint( SEVERE, "%s:%ld: %s: Channel %ld is assigned twice to "
					"function `%s'.", Fname, Lc, pulser_struct.name, channel,
					Function_Names[ c->function->self ] );
			return FAIL;
		}

		eprint( FATAL, "%s:%ld: %s: Channel %ld is already used for function "
				"`%s'.", Fname, Lc, pulser_struct.name, channel,
				Function_Names[ c->function->self ] );
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
	dg2020.function[ function ].is_used = SET;
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
		eprint( FATAL, "%s:%ld: %s: Delay for function `%s' has already been "
				"set.", Fname, Lc, pulser_struct.name,
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
			eprint( FATAL, "%s:%ld: Negative delays are invalid in EXTERNAL "
					"trigger mode.", Fname, Lc );
			THROW( EXCEPTION );
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

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_function_high_level( int function, double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_HIGH_VOLTAGE || voltage > MAX_POD_HIGH_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid high level of %g V for function "
				"`%s', valid range is %g V to %g V.", Fname, Lc,
				pulser_struct.name, voltage, Function_Names[ function ],
				MIN_POD_HIGH_VOLTAGE, MAX_POD_HIGH_VOLTAGE );
		THROW( EXCEPTION );
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
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_LOW_VOLTAGE || voltage > MAX_POD_LOW_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid low level of %g V for function "
				"`%s', valid range is %g V to %g V.", Fname, Lc,
				pulser_struct.name, voltage, Function_Names[ function ],
				MIN_POD_LOW_VOLTAGE, MAX_POD_LOW_VOLTAGE );
		THROW( EXCEPTION );
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
	assert( mode == INTERNAL || mode == EXTERNAL );

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode != mode )
	{
		eprint( FATAL, "%s:%ld: %s: Trigger mode has already been set to %s.",
				Fname, Lc, pulser_struct.name,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger slope is incompatible.", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger level is incompatible.", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_impedance )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger impedance is incompatible.", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( dg2020.is_repeat_time )
		{
			eprint( FATAL, "%s:%ld: %s: EXTERNAL trigger mode and setting a "
					"repeat time or frequency is incompatible.", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_neg_delay )
		{
			eprint( FATAL, "%s:%ld: %s: EXTERNAL trigger mode and using "
					"negative delays for functions is incompatible.",
					Fname, Lc, pulser_struct.name );
			THROW( EXCEPTION );
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
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( dg2020.is_trig_in_level && dg2020.trig_in_level != voltage )
	{
		eprint( FATAL, "%s:%ld: %s: A different level of %g V for the trigger "
				"has already been set.", Fname, Lc, pulser_struct.name,
				dg2020.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time )
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger level is useless in "
				"INTERNAL trigger mode.", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger level (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( voltage > MAX_TRIG_IN_LEVEL || voltage < MIN_TRIG_IN_LEVEL )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid level for trigger of %g V, valid "
				"range is %g V to %g V.", Fname, Lc, pulser_struct.name,
				MIN_TRIG_IN_LEVEL, MAX_TRIG_IN_LEVEL );
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
	assert( slope == POSITIVE || slope == NEGATIVE );

	if ( dg2020.is_trig_in_slope && dg2020.trig_in_slope != slope )
	{
		eprint( FATAL, "%s:%ld: %s: A different trigger slope (%s) has "
				"already been set.", Fname, Lc, pulser_struct.name,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time)
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger slope is useless in "
				"INTERNAL trigger mode.", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger slope (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_slope = slope;
	dg2020.is_trig_in_slope = SET;

	return OK;
}


bool dg2020_set_trig_in_impedance( int state )
{
	assert( state == LOW || state == HIGH );

	if ( dg2020.is_trig_in_impedance && dg2020.trig_in_impedance != state )
	{
		eprint( FATAL, "%s:%ld: %s: A trigger impedance (%s) has already been "
				"set.", Fname, Lc, pulser_struct.name,
				state == LOW ? "LOW = 50 Ohm" : "HIGH = 1 kOhm" );
		THROW( EXCEPTION );
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time )
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger impedance is useless "
				"in INTERNAL trigger mode.", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger impedance (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
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
		eprint( FATAL, "%s:%ld: %s: A different repeat time/frequency of %s /"
				"%g Hz has already been set.", Fname, Lc, pulser_struct.name,
				dg2020_pticks( dg2020.repeat_time ),
				1.0 / dg2020_ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a repeat time/frequency and "
				"trigger mode to EXTERNAL is incompatible.", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_slope || dg2020.is_trig_in_level )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a repeat time/frequency and a "
				"trigger slope or level is incompatible.", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( time <= 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid repeat time %s.",
				Fname, Lc, pulser_struct.name, dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}


	dg2020.repeat_time = dg2020_double2ticks( time );
	dg2020.is_repeat_time = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_phase_reference( int phase, int function )
{
	FUNCTION *p, *f;

	p = &dg2020.function[ phase ];
	f = &dg2020.function[ function ];

	if ( p->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Phase function `%s' has already been "
				"associated with function `%s'.", Fname, Lc,
				pulser_struct.name, Function_Names[ p->self ],
				Function_Names[ p->phase_func->self ] );
		THROW( EXCEPTION );
	}

	if ( f->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Function `%s' has already been "
				"associated with phase function `%s'.", Fname, Lc,
				pulser_struct.name, Function_Names[ f->self ],
				Function_Names[ f->phase_func->self ] );
		THROW( EXCEPTION );
	}

	p->phase_func = f;
	f->phase_func = p;

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool dg2020_setup_phase( int func, PHS phs )
{
	assert( func == PULSER_CHANNEL_PHASE_1 || func == PULSER_CHANNEL_PHASE_2 );

	if ( dg2020.function[ func ].is_phs )
	{
		eprint( WARN, "%s:%ld: %s: Phase setup for function `%s' has "
				"already been done.", Fname, Lc, pulser_struct.name,
				Function_Names[ func ] );
		return FAIL;
	}

	memcpy( &dg2020.function[ func].phs, &phs, sizeof( PHS ) );
	dg2020.function[ func ].is_phs = SET;

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool dg2020_set_phase_switch_delay( int func, double time )
{
	assert( func == PULSER_CHANNEL_PHASE_1 || func == PULSER_CHANNEL_PHASE_2 );

	if ( time < 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Unreasonable (negative) value for "
				"phase switch delay: %s.", Fname, Lc, pulser_struct.name,
				dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.function[ func ].is_psd )
	{
		eprint( FATAL, "%s:%ld: %s: Phase switch delay for phase function "
				"`%s' has already been set.", Fname, Lc, pulser_struct.name,
				Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: %s: Can't set phase switch delay because no "
				"pulser time base has been set.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	dg2020.function[ func ].is_psd = SET;
	dg2020.function[ func ].psd = ( Ticks ) ceil( time / dg2020.timebase );

	return OK;
}


bool dg2020_set_grace_period( double time )
{
	if ( time < 0 )

	{
		eprint( FATAL, "%s:%ld: %s: Unreasonable value for grace period: %s.",
				Fname, Lc, pulser_struct.name, dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_grace_period )
	{
		eprint( FATAL, "%s:%ld: %s: Grace period has already been set.",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: %s: Can't set grace period because no pulser "
				"time base has been set.",Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	dg2020.is_grace_period = SET;
	dg2020.grace_period = ( Ticks ) ceil( time / dg2020.timebase );

	return OK;
}
