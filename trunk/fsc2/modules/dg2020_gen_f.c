/*
  $Id$
*/



#include "dg2020.h"



/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_store_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		char *min = get_string_copy( dg2020_ptime( ( double ) MIN_TIMEBASE ) );
		char *max = get_string_copy( dg2020_ptime( ( double ) MAX_TIMEBASE ) );

		eprint( FATAL, "%s:%ld: DG2020: Invalid time base of %s, valid range "
				"is %s to %s.\n", Fname, Lc, dg2020_ptime( timebase ),
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
		eprint( FATAL, "%s:%ld: DG2020: Invalid pod number: %ld, valid pod "
				"number are %d-%d.\n", Fname, Lc, pod, 0, MAX_PODS - 1 );
		THROW( EXCEPTION );
	}

	if ( p->function != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Pod number %ld has already been "
				"assigned to function `%s'.\n", Fname, Lc, pod,
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
			eprint( FATAL, "%s:%ld: DG2020: A pod has already been assigned "
					"to function `%s'.\n", Fname, Lc,
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		if ( f->pod2 != NULL )
		{
			eprint( FATAL, "%s:%ld: DG2020: There have already been two pods "
					"assigned to function `%s'.\n", Fname, Lc,
					Function_Names[ f->self ] );
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
		eprint( FATAL, "%s:%ld: DG2020: Invalid channel number: %ld, valid "
				"range is 0-%d.\n", Fname, Lc, channel, MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			eprint( SEVERE, "%s:%ld: DG2020: Channel %ld is assigned twice "
					"to function `%s'.\n", Fname, Lc, channel,
					Function_Names[ c->function->self ] );
			return FAIL;
		}

		eprint( FATAL, "%s:%ld: DG2020: Channel %ld is already used for "
				"function `%s'.\n", Fname, Lc, channel,
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

bool dg2020_set_delay_function( int function, double delay )
{
	if ( dg2020.function[ function ].is_delay )
	{
		eprint( FATAL, "%s:%ld: DG2020: Delay for function `%s' has already "
				"been set.\n", Fname, Lc, Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* check that the delay value is reasonable */

	if ( delay < 0 )
	{
		eprint( FATAL, "%s:%ld: Invalid negative delay: %s\n", Fname, Lc,
				dg2020_ptime( delay ) );
		THROW( EXCEPTION );
	}

	dg2020.function[ function ].delay = dg2020_double2ticks( delay );
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
		eprint( FATAL, "%s:%ld: DG2020: Invalid high level of %g V for "
				"function `%s', valid range is %g V to %g V.\n", Fname, Lc,
				voltage, Function_Names[ function ], MIN_POD_HIGH_VOLTAGE,
				MAX_POD_HIGH_VOLTAGE );
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
		eprint( FATAL, "%s:%ld: DG2020: Invalid low level of %g V for "
				"function `%s', valid range is %g V to %g V.\n", Fname, Lc,
				voltage, Function_Names[ function ], MIN_POD_LOW_VOLTAGE,
				MAX_POD_LOW_VOLTAGE );
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
		eprint( FATAL, "%s:%ld: DG2020: Trigger mode has already been set to "
				"%s.\n", Fname, Lc,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
					"a trigger slope is incompatible.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
					"a trigger level is incompatible.\n", Fname, Lc );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( dg2020.is_repeat_time )
		{
			eprint( FATAL, "%s:%ld: DG2020: External trigger mode and setting "
					"a repeat time or frequency is incompatible.\n",
					Fname, Lc );
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
		eprint( FATAL, "%s:%ld: DG2020: A different level of %g V for the "
				"trigger has already been set.\n", Fname, Lc,
				dg2020.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
				"a trigger level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time as well as "
				"a trigger level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( voltage > MAX_TRIG_IN_LEVEL || voltage < MIN_TRIG_IN_LEVEL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid level for trigger of %g V, "
				"valid range is %g V to %g V.\n", Fname, Lc, MIN_TRIG_IN_LEVEL,
				MAX_TRIG_IN_LEVEL );
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
		eprint( FATAL, "%s:%ld: DG2020: A different trigger slope (%s) "
				"has already been set.\n", Fname, Lc,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Internal trigger mode and setting "
				"a trigger slope is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_repeat_time )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time as well as "
				"a trigger slope is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	dg2020.trig_in_slope = slope;
	dg2020.is_trig_in_slope = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_set_repeat_time( double time )
{
	if ( dg2020.is_repeat_time &&
		 dg2020.repeat_time != dg2020_double2ticks( time ) )
	{
		eprint( FATAL, "%s:%ld: DG2020: A different repeat time/frequency of "
				"%s / %g Hz has already been set.\n", Fname, Lc,
				dg2020_pticks( dg2020.repeat_time ),
				1.0 / dg2020_ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time/frequency and "
				"trigger mode to internal is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_slope || dg2020.is_trig_in_level )
	{
		eprint( FATAL, "%s:%ld: DG2020: Setting a repeat time/frequency and "
				"a trigger slope or level is incompatible.\n", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( time <= 0 )
	{
		eprint( FATAL, "%s:%ld: DG2020: Invalid repeat time %s.\n",
				Fname, Lc, dg2020_ptime( time ) );
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
		eprint( FATAL, "%s:%ld: DG2020: Phase function `%s' has already been "
				"associated with function `%s'.\n", Fname, Lc,
				Function_Names[ p->self ],
				Function_Names[ p->phase_func->self ] );
		THROW( EXCEPTION );
	}

	if ( f->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: DG2020: Function `%s' has already been "
				"associated with phase function `%s'.\n", Fname, Lc,
				Function_Names[ f->self ],
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
		eprint( WARN, "%s:%ld: DG2020: Phase setup for function `%s' has "
				"already been done.\n", Fname, Lc, Function_Names[ func ] );
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
		eprint( FATAL, "%s:%ld: DG2020: Unreasonable (negative) value for "
				"phase switch delay: %s.\n", Fname, Lc, dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.function[ func ].is_psd )
	{
		eprint( FATAL, "%s:%ld: DG2020: Phase switch delay for phase function "
				"`%s' has already been set.\n", Fname, Lc,
				Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: DG2020: Can't set phase switch delay because "
				"no pulser time base has been set.\n",Fname, Lc );
		THROW( EXCEPTION );
	}

	dg2020.function[ func ].is_psd = SET;
	dg2020.function[ func ].psd = ( Ticks ) ceil( time / dg2020.timebase );

	/* If the delay is set for PHASE_1 and no value has been set for PHASE2
	   yet, preliminary use the value also for PHASE_2 */

	if ( func == PULSER_CHANNEL_PHASE_1 &&
		 ! dg2020.function[ PULSER_CHANNEL_PHASE_2 ].is_psd )
		dg2020.function[ PULSER_CHANNEL_PHASE_2 ].psd =
			( Ticks ) ceil( time / dg2020.timebase );

	return OK;
}
