/*
  $Id$
*/



#include "dg2020_f.h"


static int Cur_PHS = -1;


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool dg2020_store_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		char *min = get_string_copy( dg2020_ptime( ( double ) MIN_TIMEBASE ) );
		char *max = get_string_copy( dg2020_ptime( ( double ) MAX_TIMEBASE ) );

		eprint( FATAL, "%s:%ld: %s: Invalid time base of %s, valid range is "
				"%s to %s.\n", Fname, Lc, pulser_struct.name,
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
				"are %d-%d.\n", Fname, Lc, pulser_struct.name, pod, 0,
				MAX_PODS - 1 );
		THROW( EXCEPTION );
	}

	if ( p->function != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Pod number %ld has already been assigned "
				"to function `%s'.\n", Fname, Lc, pulser_struct.name, pod,
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
					"function `%s'.\n", Fname, Lc, pulser_struct.name,
					Function_Names[ f->self ] );
			THROW( EXCEPTION );
		}

		if ( f->pod2 != NULL )
		{
			eprint( FATAL, "%s:%ld: %s: There have already been two pods "
					"assigned to function `%s'.\n", Fname, Lc,
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
				"is 0-%d.\n", Fname, Lc, pulser_struct.name, channel,
				MAX_CHANNELS - 1 );
		THROW( EXCEPTION );
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			eprint( SEVERE, "%s:%ld: %s: Channel %ld is assigned twice to "
					"function `%s'.\n", Fname, Lc, pulser_struct.name, channel,
					Function_Names[ c->function->self ] );
			return FAIL;
		}

		eprint( FATAL, "%s:%ld: %s: Channel %ld is already used for function "
				"`%s'.\n", Fname, Lc, pulser_struct.name, channel,
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
				"set.\n", Fname, Lc, pulser_struct.name,
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
					"trigger mode.\n", Fname, Lc );
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
				"`%s', valid range is %g V to %g V.\n", Fname, Lc,
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
				"`%s', valid range is %g V to %g V.\n", Fname, Lc,
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
		eprint( FATAL, "%s:%ld: %s: Trigger mode has already been set to "
				"%s.\n", Fname, Lc, pulser_struct.name,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( dg2020.is_trig_in_slope )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger slope isn't possible.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger level isn't possible.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_trig_in_impedance )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger impedance isn't possible.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( dg2020.is_repeat_time )
		{
			eprint( FATAL, "%s:%ld: %s: EXTERNAL trigger mode and setting a "
					"repeat time or frequency isn't possible.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( dg2020.is_neg_delay )
		{
			eprint( FATAL, "%s:%ld: %s: EXTERNAL trigger mode and using "
					"negative delays for functions isn't possible.\n",
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
				"has already been set.\n", Fname, Lc, pulser_struct.name,
				dg2020.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time )
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger level is useless in "
				"INTERNAL trigger mode.\n", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger level (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions isn't possible.\n", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( voltage > MAX_TRIG_IN_LEVEL || voltage < MIN_TRIG_IN_LEVEL )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid level for trigger of %g V, valid "
				"range is %g V to %g V.\n", Fname, Lc, pulser_struct.name,
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
				"already been set.\n", Fname, Lc, pulser_struct.name,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time)
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger slope is useless in "
				"INTERNAL trigger mode.\n", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger slope (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions isn't possible.\n", Fname, Lc,
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
				"set.\n", Fname, Lc, pulser_struct.name,
				state == LOW ? "LOW = 50 Ohm" : "HIGH = 1 kOhm" );
		THROW( EXCEPTION );
	}

	if ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
		 dg2020.is_repeat_time )
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger impedance is useless "
				"in INTERNAL trigger mode.\n", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( dg2020.is_neg_delay &&
		 ! ( ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == INTERNAL ) ||
			 dg2020.is_repeat_time ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger impedance (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions isn't possible.\n", Fname, Lc,
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
		eprint( FATAL, "%s:%ld: %s: A different repeat time/frequency of %s/"
				"%g Hz has already been set.\n", Fname, Lc, pulser_struct.name,
				dg2020_pticks( dg2020.repeat_time ),
				1.0 / dg2020_ticks2double( dg2020.repeat_time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_mode && dg2020.trig_in_mode == EXTERNAL )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a repeat time/frequency and "
				"trigger mode to EXTERNAL isn't possible.\n", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_trig_in_slope || dg2020.is_trig_in_level )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a repeat time/frequency and a "
				"trigger slope or level isn't possible.\n", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( time <= 0 )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid repeat time %s.\n",
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


	/* First a sanity check... */

	assert ( Cur_PHS != - 1 ? ( Cur_PHS == phase ) : 1 );

	/* The phase function can't be phase cycled... */

	if ( function == PULSER_CHANNEL_PHASE_1 ||
		 function == PULSER_CHANNEL_PHASE_2 )
	{
		eprint( FATAL, "%s:%ld: %s: A PHASE function can't be phase cycled.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	f = &dg2020.function[ function ];

	p = &dg2020.function[ phase == 0 ? PULSER_CHANNEL_PHASE_1 :
						               PULSER_CHANNEL_PHASE_2 ];

	if ( p->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Phase function `%s' has already been "
				"associated with function `%s'.\n", Fname, Lc,
				pulser_struct.name, Function_Names[ p->self ],
				Function_Names[ p->phase_func->self ] );
		THROW( EXCEPTION );
	}

	if ( f->phase_func != NULL )
	{
		eprint( FATAL, "%s:%ld: %s: Function `%s' has already been "
				"associated with phase function `%s'.\n", Fname, Lc,
				pulser_struct.name, Function_Names[ f->self ],
				Function_Names[ f->phase_func->self ] );
		THROW( EXCEPTION );
	}

	p->phase_func = f;
	f->phase_func = p;

	return OK;
}


/*-----------------------------------------------------------------------*/
/* This function is called for each of the definitions of how a phase is */
/* realized by the combination of pod channel outputs. There are 2 pod   */
/* channels and each of the 4 phase types has to be realized by a        */
/* different combination (i.e. both off, both on or one off and the      */
/* other on).                                                            */
/* 'function' is the phase function the data are to be used for (i.e. 0  */
/*   means PHASE_1, 1 means PHASE_2)                                     */
/* 'type' means the type of phase, see global.h (PHASE_PLUS/MINUX_X/Y)   */
/* 'pod' tells if the value is for the first or the second pod channel   */
/*   (0: first pod channel, 1: second pod channel, -1: pick the one not  */
/*    set yet)                                                           */
/* 'val' means high or low to be set on the pod channel to set the       */
/*    requested phase(0: low, non-zero: high)                            */
/* 'protocol': there are different ways to realize a phase, currently    */
/*    there's the Frankfurt and the Berlin method. This driver can only  */
/*    accept settings the Frankfurt type method. Sometimes it is not     */
/*    possible to tell from the input which of these methods is used, in */
/*    this case also accept the data.                                    */
/*-----------------------------------------------------------------------*/

bool dg2020_phase_setup_prep( int func, int type, int pod, long val,
							  long protocol )
{
	/* First a sanity check... */

	assert ( Cur_PHS != - 1 ? ( Cur_PHS == func ) : 1 );

	/* This driver only accepts the Frankfurt method of declaring a
	   phase setup - unrecognized method is also ok */ 

	if ( protocol != PHASE_FFM_PROT && protocol != PHASE_UNKNOWN_PROT )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid syntax for this driver.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	/* Not all phase types are valid here */

	if ( type != PHASE_PLUS_X && type != PHASE_MINUS_X &&
		 type != PHASE_PLUS_Y && type != PHASE_MINUS_Y )
	{
		eprint( FATAL, "%s:%ld: %s: Phase of type %s can't be used with this "
				"driver.\n", Fname, Lc, pulser_struct.name,
				Phase_Types[ type ] );
		THROW( EXCEPTION );
	}

	Cur_PHS = func;

	if ( pod == -1 )
	{
		if ( phs[ func ].is_var[ type ][ 0 ] )
			pod = 1;
		else
			pod = 0;

		if ( phs[ func ].is_var[ type ][ pod ] )
		{
			eprint( FATAL, "%s:%ld: Both output states for phase %s of "
					"function `%s' already have been defined.\n", Fname,
					Lc, Phase_Types[ type ], func == 0 ?
					Function_Names[ PULSER_CHANNEL_PHASE_1 ] :
					Function_Names[ PULSER_CHANNEL_PHASE_2 ] );
			THROW( EXCEPTION );
		}
	}

	if ( phs[ func ].is_var[ type ][ pod ] )
	{
		eprint( FATAL, "%s:%ld: %s: Output state of %d. pod for phase %s "
				"of function `%s' already has been defined.\n", Fname, Lc,
				pulser_struct.name, pod + 1, Phase_Types[ type ],
				func == 0 ? Function_Names[ PULSER_CHANNEL_PHASE_1 ] :
				Function_Names[ PULSER_CHANNEL_PHASE_2 ] );
		THROW( EXCEPTION );
	}

	phs[ func ].var[ type ][ pod ] = val != 0 ? 1 : 0;
	phs[ func ].is_var[ type ][ pod ] = SET;

	return OK;
}


/*-----------------------------------------------------------------------*/
/* Now that we got all information there is about the phase setup do all */
/* possible checks and finally store the state.                          */
/*-----------------------------------------------------------------------*/

bool dg2020_phase_setup( int func )
{
	int i, j;
	bool cons[ 4 ] = { UNSET, UNSET, UNSET, UNSET };
	bool ret;


	assert( Cur_PHS != -1 && Cur_PHS == func );

	/* Check that for all phase types data are set */

	for ( i = 0; i < 4; i++ )
	{
		for ( j = 0; j < 2; j++ )
			if ( ! phs[ func ].is_var[ i ][ j ] )
			{
				if ( func == 2 )
					eprint( FATAL, "%s:%ld: %s: Incomplete data for phase "
							"setup of phase functions.\n", Fname, Lc,
							pulser_struct.name );
				else
					eprint( FATAL, "%s:%ld: %s: Incomplete data for phase "
							"setup of function `%s'.\n", Fname, Lc,
							pulser_struct.name, func == 0 ?
							Function_Names[ PULSER_CHANNEL_PHASE_1 ] :
							Function_Names[ PULSER_CHANNEL_PHASE_2 ] );
				THROW( EXCEPTION );
			}

		cons[ phs[ func ].var[ i ][ 0 ] + 2 * phs[ func ].var[ i ][ 1 ] ]
			= SET;
	}

	/* Check that the data are consistent, i.e. different phase types haven't
	   been assigned the same data */

	for ( i = 0; i < 4; i++ )
		if ( ! cons[ i ] )
		{
			eprint( FATAL, "%s:%ld: %s: Inconsistent data for phase setup "
					"of function `%s'.\n", Fname, Lc, pulser_struct.name,
					func == 0 ? Function_Names[ PULSER_CHANNEL_PHASE_1 ] :
					Function_Names[ PULSER_CHANNEL_PHASE_2 ] );
			THROW( EXCEPTION );
		}

	if ( func == 0 )
		ret = dg2020_phase_setup_finalize( PULSER_CHANNEL_PHASE_1,
											phs[ func ] );
	else
		ret = dg2020_phase_setup_finalize( PULSER_CHANNEL_PHASE_2,
											phs[ func ] );

	Cur_PHS = -1;

	return ret;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool dg2020_phase_setup_finalize( int func, PHS phs )
{
	assert( func == PULSER_CHANNEL_PHASE_1 || func == PULSER_CHANNEL_PHASE_2 );

	if ( dg2020.function[ func ].is_phs )
	{
		eprint( WARN, "%s:%ld: %s: Phase setup for function `%s' has "
				"already been done.\n", Fname, Lc, pulser_struct.name,
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
				"phase switch delay: %s.\n", Fname, Lc, pulser_struct.name,
				dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.function[ func ].is_psd )
	{
		eprint( FATAL, "%s:%ld: %s: Phase switch delay for phase function "
				"`%s' has already been set.\n", Fname, Lc, pulser_struct.name,
				Function_Names[ func ] );
		THROW( EXCEPTION );
	}

	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: %s: Can't set phase switch delay because no "
				"pulser time base has been set.\n",
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
		eprint( FATAL, "%s:%ld: %s: Unreasonable value for grace period: "
				"%s.\n", Fname, Lc, pulser_struct.name, dg2020_ptime( time ) );
		THROW( EXCEPTION );
	}

	if ( dg2020.is_grace_period )
	{
		eprint( FATAL, "%s:%ld: %s: Grace period has already been set.\n",
				Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	if ( ! dg2020.is_timebase )
	{
		eprint( FATAL, "%s:%ld: %s: Can't set grace period because no pulser "
				"time base has been set.\n",Fname, Lc, pulser_struct.name );
		THROW( EXCEPTION );
	}

	dg2020.is_grace_period = SET;
	dg2020.grace_period = ( Ticks ) ceil( time / dg2020.timebase );

	return OK;
}
