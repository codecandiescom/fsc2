/*
  $Id$
*/

#include "hfs9000.h"


/*------------------------------------------------------------------*/
/* Function is called via the TIMEBASE command to set the timebase  */
/* used with the pulser - got to be called first because all nearly */
/* all other functions depend on the timebase setting !             */
/*------------------------------------------------------------------*/

bool hfs9000_store_timebase( double timebase )
{
	if ( timebase < MIN_TIMEBASE || timebase > MAX_TIMEBASE )
	{
		char *min =
			get_string_copy( hfs9000_ptime( ( double ) MIN_TIMEBASE ) );
		char *max =
			get_string_copy( hfs9000_ptime( ( double ) MAX_TIMEBASE ) );

		eprint( FATAL, "%s:%ld: %s: Invalid time base of %s, valid range is "
				"%s to %s.\n", Fname, Lc, pulser_struct.name,
				hfs9000_ptime( timebase ), min, max );
		T_free( min );
		T_free( max );
		THROW( EXCEPTION );
	}

	hfs9000.is_timebase = SET;
	hfs9000.timebase = timebase;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_assign_channel_to_function( int function, long channel )
{
	FUNCTION *f = &hfs9000.function[ function ];
	CHANNEL *c = &hfs9000.channel[ channel ];


	if ( channel < 0 || channel >= MAX_CHANNELS )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid channel number: %ld, valid range "
				"is 1-%d.\n", Fname, Lc, pulser_struct.name, channel,
				( int ) MAX_CHANNELS );
		THROW( EXCEPTION );
	}

	if ( channel == HFS9000_TRIG_OUT )
	{
		if ( f->is_inverted )
		{
			eprint( FATAL, "%s:%ld: %s: Function `%s' has been set to use "
					"inverted logic. This can't be done with TRIGER_OUT "
					"channel.\n", Fname, Lc, pulser_struct.name,
					Function_Names[ function ] );
			THROW( EXCEPTION );
		}

		if ( f->is_high_level || f->is_low_level )
		{
			eprint( FATAL, "%s:%d: %s: For function `%s' a voltage level has "
					"been set. This can't be done with TRIGER_OUT channel.\n",
					Fname, Lc, pulser_struct.name,
					Function_Names[ function ] );
			THROW( EXCEPTION );
		}
	}

	if ( c->function != NULL )
	{
		if ( c->function->self == function )
		{
			if ( channel == HFS9000_TRIG_OUT )
				eprint( SEVERE, "%s:%ld: %s: TRIGGER_OUT channel is assigned "
						"twice to function `%s'.\n", Fname, Lc,
						pulser_struct.name,
						Function_Names[ c->function->self ] );
			else
				eprint( SEVERE, "%s:%ld: %s: Channel %ld is assigned twice to "
						"function `%s'.\n", Fname, Lc, pulser_struct.name,
						channel, Function_Names[ c->function->self ] );
			return FAIL;
		}

		if ( channel == HFS9000_TRIG_OUT )
			eprint( FATAL, "%s:%ld: %s: TRIGGER_OUT channel is already used "
					"for function `%s'.\n", Fname, Lc, pulser_struct.name,
					Function_Names[ c->function->self ] );
		else
			eprint( FATAL, "%s:%ld: %s: Channel %ld is already used for "
					"function `%s'.\n", Fname, Lc, pulser_struct.name, channel,
					Function_Names[ c->function->self ] );
		THROW( EXCEPTION );
	}

	f->is_used = SET;
	f->channel = c;

	c->function = f;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_invert_function( int function )
{
	if ( hfs9000.function[ function ].channel != NULL &&
		 hfs9000.function[ function ].channel->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, "%s:%ld: %s: Function `%s' is associated with "
				"TRIGGER_OUT channel whose polarity can't be inverted.\n",
				Fname, Lc, pulser_struct.name, Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	hfs9000.function[ function ].is_inverted = SET;
	hfs9000.function[ function ].is_used = SET;
	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_function_delay( int function, double delay )
{
	Ticks Delay = hfs9000_double2ticks( delay );
	int i;
	FUNCTION *f;


	if ( hfs9000.function[ function ].is_delay )
	{
		eprint( FATAL, "%s:%ld: %s: Delay for function `%s' has already been "
				"set.\n", Fname, Lc, pulser_struct.name,
				Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	/* Check that the delay value is reasonable */

	if ( Delay < 0 )
	{
		if ( ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode ) ||
			 hfs9000.is_trig_in_slope || hfs9000.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: Negative delays are invalid in EXTERNAL "
					"trigger mode.\n", Fname, Lc );
			THROW( EXCEPTION );
		}

		if ( Delay < hfs9000.neg_delay )
		{
			for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
			{
				f = &hfs9000.function[ i ];
				if ( ! f->is_used || i == function )
					continue;
				f->delay += hfs9000.neg_delay - Delay;
			}
			hfs9000.neg_delay = hfs9000.function[ function ].delay = - Delay;
		}
		else
			hfs9000.function[ function ].delay = hfs9000.neg_delay - Delay;
	}
	else
		hfs9000.function[ function ].delay = Delay - hfs9000.neg_delay;
		
	hfs9000.function[ function ].is_used = SET;
	hfs9000.function[ function ].is_delay = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_function_high_level( int function, double voltage )
{
	if ( hfs9000.function[ function ].channel != NULL &&
		 hfs9000.function[ function ].channel->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, "%s:%ld: %s: Function `%s' is associated with "
				"TRIGGER_OUT channel that doesn't allow setting of levels.\n",
				Fname, Lc, pulser_struct.name, Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_HIGH_VOLTAGE || voltage > MAX_POD_HIGH_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid high level of %g V for function "
				"`%s', valid range is %g V to %g V.\n", Fname, Lc,
				pulser_struct.name, voltage, Function_Names[ function ],
				MIN_POD_HIGH_VOLTAGE, MAX_POD_HIGH_VOLTAGE );
		THROW( EXCEPTION );
	}
				
	if ( hfs9000.function[ function ].is_low_level )
		hfs9000_check_pod_level_diff( voltage,
									  hfs9000.function[ function ].low_level );

	hfs9000.function[ function ].high_level = voltage;
	hfs9000.function[ function ].is_high_level = SET;
	hfs9000.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_function_low_level( int function, double voltage )
{
	if ( hfs9000.function[ function ].channel != NULL &&
		 hfs9000.function[ function ].channel->self == HFS9000_TRIG_OUT )
	{
		eprint( FATAL, "%s:%ld: %s: Function `%s' is associated with "
				"TRIGGER_OUT channel that doesn't allow setting of levels.\n",
				Fname, Lc, pulser_struct.name, Function_Names[ function ] );
		THROW( EXCEPTION );
	}

	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( voltage < MIN_POD_LOW_VOLTAGE || voltage > MAX_POD_LOW_VOLTAGE )
	{
		eprint( FATAL, "%s:%ld: %s: Invalid low level of %g V for function "
				"`%s', valid range is %g V to %g V.\n", Fname, Lc,
				pulser_struct.name, voltage, Function_Names[ function ],
				MIN_POD_LOW_VOLTAGE, MAX_POD_LOW_VOLTAGE );
		THROW( EXCEPTION );
	}
				
	if ( hfs9000.function[ function ].is_high_level )
		hfs9000_check_pod_level_diff( hfs9000.function[ function ].high_level,
									  voltage );

	hfs9000.function[ function ].low_level = voltage;
	hfs9000.function[ function ].is_low_level = SET;
	hfs9000.function[ function ].is_used = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_trigger_mode( int mode )
{
	assert( mode == INTERNAL || mode == EXTERNAL );


	if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode != mode )
	{
		eprint( FATAL, "%s:%ld: %s: Trigger mode has already been set to "
				"%s.\n", Fname, Lc, pulser_struct.name,
				mode == EXTERNAL ? "INTERNAL" : "EXTERNAL" );
		THROW( EXCEPTION );
	}

	if ( mode == INTERNAL )
	{
		if ( hfs9000.is_trig_in_slope )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger slope isn't possible.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}

		if ( hfs9000.is_trig_in_level )
		{
			eprint( FATAL, "%s:%ld: %s: INTERNAL trigger mode and setting a "
					"trigger level isn't possible.\n", Fname, Lc,
					pulser_struct.name );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( hfs9000.is_neg_delay )
		{
			eprint( FATAL, "%s:%ld: %s: EXTERNAL trigger mode and using "
					"negative delays for functions isn't possible.\n",
					Fname, Lc, pulser_struct.name );
			THROW( EXCEPTION );
		}
	}

	hfs9000.trig_in_mode = mode;
	hfs9000.is_trig_in_mode = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_trig_in_level( double voltage )
{
	voltage = VOLTAGE_RESOLUTION * lround( voltage / VOLTAGE_RESOLUTION );

	if ( hfs9000.is_trig_in_level && hfs9000.trig_in_level != voltage )
	{
		eprint( FATAL, "%s:%ld: %s: A different level of %g V for the trigger "
				"has already been set.\n", Fname, Lc, pulser_struct.name,
				hfs9000.trig_in_level );
		THROW( EXCEPTION );
	}

	if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL )
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger level is useless in "
				"INTERNAL trigger mode.\n", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( hfs9000.is_neg_delay &&
		 ! ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger level (thus implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.\n", Fname, Lc,
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

	hfs9000.trig_in_level = voltage;
	hfs9000.is_trig_in_level = SET;

	return OK;
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

bool hfs9000_set_trig_in_slope( int slope )
{
	assert( slope == POSITIVE || slope == NEGATIVE );

	if ( hfs9000.is_trig_in_slope && hfs9000.trig_in_slope != slope )
	{
		eprint( FATAL, "%s:%ld: %s: A different trigger slope (%s) has "
				"already been set.\n", Fname, Lc, pulser_struct.name,
				slope == POSITIVE ? "NEGATIVE" : "POSITIVE" );
		THROW( EXCEPTION );
	}

	if ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL )
	{
		eprint( SEVERE, "%s:%ld: %s: Setting a trigger slope is useless in "
				"INTERNAL trigger mode.\n", Fname, Lc, pulser_struct.name );
		return FAIL;
	}

	if ( hfs9000.is_neg_delay &&
		 ! ( hfs9000.is_trig_in_mode && hfs9000.trig_in_mode == INTERNAL ) )
	{
		eprint( FATAL, "%s:%ld: %s: Setting a trigger slope (implicitly "
				"selecting EXTERNAL trigger mode) while using negative delays "
				"for functions is incompatible.\n", Fname, Lc,
				pulser_struct.name );
		THROW( EXCEPTION );
	}

	hfs9000.trig_in_slope = slope;
	hfs9000.is_trig_in_slope = SET;

	return OK;
}
