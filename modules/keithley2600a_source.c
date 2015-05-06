/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "keithley2600a.h"


static const char *smu[ ] = { "smua", "smub" };


/*---------------------------------------------------------------*
 * Returns the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_sense( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.sense)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    k26->sense[ ch ] = keithley2600a_line_to_bool( buf );
    if (    k26->sense[ ch ] != SENSE_LOCAL
		 || k26->sense[ ch ] != SENSE_REMOTE
		 || k26->sense[ ch ] != SENSE_CALA )
        keithley2600a_bad_data( );

    return k26->sense[ ch ];
}


/*---------------------------------------------------------------*
 * Sets the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_set_sense( unsigned int ch,
						 int          sense )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert(    sense == SENSE_LOCAL
				 || sense == SENSE_REMOTE
				 || sense == SENSE_CALA );

	/* Calibration mode can only be switched to when output is off */

	fsc2_assert(    sense != SENSE_CALA
				 || keithley2600a_get_source_output( ch ) == OUTPUT_OFF );

    sprintf( buf, "%s.sense=%d", smu[ ch ], sense );
	keithley2600a_cmd( buf );

    return k26->sense[ ch ] = sense;
}


/*---------------------------------------------------------------*
 * Returns the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_source_offmode( unsigned int ch )
{
    char buf[ 50 ];
    int source_offmode;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.offmode)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    source_offmode = keithley2600a_line_to_int( buf );
    if (    source_offmode != OUTPUT_NORMAL
         && source_offmode != OUTPUT_HIGH_Z
         && source_offmode != OUTPUT_ZERO )
        keithley2600a_bad_data( );

    return k26->source[ ch ].offmode = source_offmode;
}


/*---------------------------------------------------------------*
 * Sets the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_set_source_offmode( unsigned int ch,
								  int          source_offmode )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    source_offmode == OUTPUT_NORMAL
                 || source_offmode == OUTPUT_HIGH_Z
                 || source_offmode == OUTPUT_ZERO );

    sprintf( buf, "%s.source.offmode=%d", smu[ ch ], source_offmode );
	keithley2600a_cmd( buf );

    return k26->source[ ch ].offmode = source_offmode;
}


/*---------------------------------------------------------------*
 * Returns if output of the channel is switched on
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_output( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.output)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->source[ ch ].output = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches output of the channel on or off. Note that the
 * device won't switch the output on if there are any in-
 * consistencies in the set-up for the channel.
 *---------------------------------------------------------------*/

bool
keithley2600a_set_source_output( unsigned int ch,
                                 bool         source_output )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

#if ! defined NDEBUG
    /* Check all settings before witching the channel on - note that the tests
       work only properly when the 'source.output' member of the structure for
       the state of the channel is set. */

    if ( source_output )
    {
        k26->source[ ch ].output = OUTPUT_ON;
        if (    ! keithley2600a_check_source_levelv( ch,
												   k26->source[ ch ].levelv )
				|| ! keithley2600a_check_source_leveli( ch,
												   k26->source[ ch ].leveli )
				|| ! keithley2600a_check_source_rangev( ch,
                                                   k26->source[ ch ].rangev )
				|| ! keithley2600a_check_source_rangei( ch,
                                                   k26->source[ ch ].rangei )
				|| ! keithley2600a_check_source_limitv( ch,
                                                   k26->source[ ch ].limitv )
				|| ! keithley2600a_check_source_limiti( ch,
                                                   k26->source[ ch ].limiti ) )
        {
            k26->source[ ch ].output = OUTPUT_OFF;
            print( FATAL, "Can't switch output on with the current "
                   "settings.\n" );
            THROW( EXCEPTION );
        }

        k26->source[ ch ].output = OUTPUT_OFF;
    }
#endif

    sprintf( buf, "%s.source.output=%d", smu[ ch ], ( int ) source_output );
	keithley2600a_cmd( buf );

    if ( ! source_output )
        return k26->source[ ch ].output = 0;

    /* Despite all our testing an attempt to switch output on may fail if the
       settings still aren't acceptable. So we'd always check if the output
       was in fact switched on. */

    if ( ! keithley2600a_get_source_output( ch ) )
        keithley2600a_show_errors( );

    return k26->source[ ch ].output = source_output;
}


/*---------------------------------------------------------------*
 * Returns if channel is in high-capacity mode
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_highc( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.highc)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->source[ ch ].highc = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches high-capacity mode of the channel on or off (switching
 * it on also modifies a number of other settings!)
 *---------------------------------------------------------------*/

int
keithley2600a_set_source_highc( unsigned int ch,
                                bool         source_highc )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.highc=%d", smu[ ch ], ( int ) source_highc );
	keithley2600a_cmd( buf );

    /* Switching to high-capacity mode may also changes a few other settings */
    /* ZZZZZZZZ We should test if these new settings don't conflict with
       some others! */

    if ( source_highc )
    {
        double v = k26->source[ ch ].limiti;
        if ( v != keithley2600a_get_source_limiti( ch ) )
            print( WARN, "Current limit has been adjusted for channel %u to "
                   "%g A.\n", ch + 1, k26->source[ ch ].limiti );

        v = k26->source[ ch ].rangei;
        if ( v != keithley2600a_get_source_rangei( ch ) )
            print( WARN, "Current range has been adjusted for channel %u to "
                   "%g A.\n", ch + 1, k26->source[ ch ].rangei );

        v = k26->source[ ch ].lowrangei;
        if ( v != keithley2600a_get_source_lowrangei( ch ) )
            print( WARN, "Lowest source current range used for autoranging "
                   "has been adjusted for channel %u to %g A.\n", ch + 1,
                   k26->source[ ch ].lowrangei );

        v = k26->measure[ ch ].lowrangei;
        if ( v != keithley2600a_get_measure_lowrangei( ch ) )
            print( WARN, "Lowest measurement current range used for "
                   "autoranging has been adjusted for channel %u to %g A.\n",
                   ch + 1, k26->measure[ ch ].lowrangei );
    }

    return k26->source[ ch ].highc = source_highc;
}


/*---------------------------------------------------------------*
 * Returns if the channel is set up as voltage of current source
 *---------------------------------------------------------------*/

int
keithley2600a_get_source_func( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.func)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->source[ ch ].func = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches the channel between voltage of current source
 *---------------------------------------------------------------*/

int
keithley2600a_set_source_func( unsigned int ch,
                               int          source_func )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    source_func == OUTPUT_DCAMPS
                 || source_func == OUTPUT_DCVOLTS );

    sprintf( buf, "%s.source.fun=%d", smu[ ch ], source_func );
	keithley2600a_cmd( buf );

    return k26->source[ ch ].func = source_func;
}


/*---------------------------------------------------------------*
 * Returns if autorange for source voltage with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_autorangev( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.autorangev)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->source[ ch ].autorangev = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches autorange for source voltage with the channel
 * on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_set_source_autorangev( unsigned int ch,
                                     bool         autorange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.autorangev=%d", smu[ ch ], ( int ) autorange );
	keithley2600a_cmd( buf );

    return k26->source[ ch ].autorangev = autorange;
}


/*---------------------------------------------------------------*
 * Returns if autorange for source current with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_autorangei( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.autorangei)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->source[ ch ].autorangei = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches autorange for source current with the channel
 * on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_set_source_autorangei( unsigned int ch,
                                     bool         autorange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.autorangei=%d", smu[ ch ], ( int ) autorange );
	keithley2600a_cmd( buf );

    return k26->source[ ch ].autorangei = autorange;
}


/*---------------------------------------------------------------*
 * Sets a range for the source voltage of the channel (thereby
 * disabling autoranging)
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_rangev( unsigned int ch )
{
    double range;
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.rangev)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    range = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_rangev( ch, range ) )
        keithley2600a_bad_data( );

    k26->source[ ch ].autorangev = UNSET;
    return k26->source[ ch ].rangev = range;
}


/*---------------------------------------------------------------*
 * Returns the setting of the source voltage range of the channel
 * (but the value has no effect when autoranging is on)
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_rangev( double       range,
                                 unsigned int ch )
{
    char buf[ 50 ];

    range = keithley2600a_best_source_rangev( range );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( keithley2600a_check_source_rangev( ch, range ) );

    sprintf( buf, "%s.source.rangev=%.5g", smu[ ch ], range );

    return k26->source[ ch ].rangev = range;
}


/*---------------------------------------------------------------*
 * Sets a range for the source current of the channel (thereby
 * disabling autoranging)
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_rangei( unsigned int ch )
{
    double range;
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.rangei)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    range = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_rangei( ch, range ) )
        keithley2600a_bad_data( );

    k26->source[ ch ].autorangei = UNSET;
    return k26->source[ ch ].rangei = range;
}


/*---------------------------------------------------------------*
 * Returns the setting of the source voltage range of the channel
 * (but the value has no effect when autoranging is on)
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_rangei( unsigned int ch,
								 double       range )
{
    char buf[ 50 ];

    range = keithley2600a_best_source_rangei( range );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( keithley2600a_check_source_rangei( ch, range ) );

    sprintf( buf, "%s.source.rangei=%.5g", smu[ ch ], range );

    return k26->source[ ch ].rangei = range;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_lowrangev( unsigned int ch )
{
    char buf[ 50 ];
    double lowrange;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.lowrangev)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_lowrangev( ch, lowrange ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].lowrangev = lowrange;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_lowrangev( unsigned int ch,
									double       lowrange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( keithley2600a_check_source_lowrangev( ch, lowrange ) );

    sprintf( buf, "%s.source.lowrangev=%.5g", smu[ ch ], lowrange );
    keithley2600a_cmd( buf );

    return k26->source[ ch ].lowrangev = lowrange;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_lowrangei( unsigned int ch )
{
    char buf[ 50 ];
    double lowrange;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.lowrangei)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_lowrangei( ch, lowrange ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].lowrangei = lowrange;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_lowrangei( unsigned int ch,
                                    double       lowrange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( keithley2600a_check_source_lowrangei( ch, lowrange ) );

    sprintf( buf, "%s.source.lowrangei=%.5g", smu[ ch ], lowrange );
    keithley2600a_cmd( buf );

    return k26->source[ ch ].lowrangei = lowrange;
}


/*---------------------------------------------------------------*
 * Returns the source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_levelv( unsigned int ch )
{
	char buf[ 50 ];
	double volts;

	fsc2_assert( ch < NUM_CHANNELS );

	sprintf( buf, "print(%s.source.levelv)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

	volts = keithley2600a_line_to_double( buf );
	if ( ! keithley2600a_check_source_levelv( ch, volts ) )
		keithley2600a_bad_data( );

	return k26->source[ ch ].levelv = volts;
}


/*---------------------------------------------------------------*
 * Sets a source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_levelv( unsigned int ch,
								 double       volts )
{
	char buf[ 50 ];

	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_levelv( ch, volts ) );

	sprintf( buf, "%s.source.levelv=%.5g", smu[ ch ], volts );
	keithley2600a_cmd( buf );

	return keithley2600a_get_source_levelv( ch );
}


/*---------------------------------------------------------------*
 * Returns the source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_leveli( unsigned int ch )
{
	char buf[ 50 ];
	double amps;

	fsc2_assert( ch < NUM_CHANNELS );

	sprintf( buf, "print(%s.source.leveli)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof sizeof buf );

	amps = keithley2600a_line_to_double( buf );
	if ( ! keithley2600a_check_source_leveli( ch, amps ) )
		keithley2600a_bad_data( );

	return k26->source[ ch ].leveli = amps;
}


/*---------------------------------------------------------------*
 * Sets the source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_leveli( unsigned int ch,
								 double       amps )
{
	char buf[ 50 ];

	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_leveli( ch, amps ) );

	sprintf( buf, "%s.source.leveli=%.5g", smu[ ch ], amps );
	keithley2600a_cmd( buf );

	return keithley2600a_get_source_leveli( ch );
}


/*---------------------------------------------------------------*
 * Returns the source voltage (compliance) lmit for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_limitv( unsigned int ch )
{
	char buf[ 50 ];
	double volts;

	fsc2_assert( ch < NUM_CHANNELS );

	sprintf( buf, "print(%s.source.limitv)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

	volts = keithley2600a_line_to_double( buf );
	if ( keithley2600a_check_source_limitv( ch, volts ) )
		keithley2600a_bad_data( );

	return k26->source[ ch ].limitv = volts;
}


/*---------------------------------------------------------------*
 * Sets a source voltage (compliance) limit for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_limitv( unsigned int ch,
								 double       volts )
{
	char buf[ 50 ];

	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_limitv( ch, volts ) );

	sprintf( buf, "%s.source.limitv=%.5g", smu[ ch ], volts );
	keithley2600a_cmd( buf );

	return keithley2600a_get_source_limitv( ch );
}


/*---------------------------------------------------------------*
 * Returns the source current (compliance) limit for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_limiti( unsigned int ch )
{
	char buf[ 50 ];
	double amps;

	fsc2_assert( ch < NUM_CHANNELS );

	sprintf( buf, "print(%s.source.limiti)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof sizeof buf );

	amps = keithley2600a_line_to_double( buf );
    if ( keithley2600a_check_source_limiti( ch, amps ) )
        keithley2600a_bad_data( );

	return k26->source[ ch ].limiti = amps;
}


/*---------------------------------------------------------------*
 * Sets a source current (compliance) limit for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_limiti( unsigned int ch,
								 double       amps )
{
	char buf[ 50 ];

	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_limiti( ch, amps ) );

	sprintf( buf, "%s.source.limiti=%.5g", smu[ ch ], amps );
	keithley2600a_cmd( buf );

	return keithley2600a_get_source_limiti( ch );
}


/*---------------------------------------------------------------*
 * Returns if output is in compliance, i.e. source voltage or
 * current is clamped in order not to exceed the set limits.
 *---------------------------------------------------------------*/

bool
keithley2600a_get_compliance( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.compliance)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );
    if ( strcmp( buf, "false\n" ) && strcmp( buf, "true\n" ) )
        keithley2600a_bad_data( );

    return buf[ 0 ] == 't';
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
