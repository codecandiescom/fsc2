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
 * Returns the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_source_offmode( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.offmode)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

    int source_offmode = keithley2600a_line_to_int( buf );
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
    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    source_offmode == OUTPUT_NORMAL
                 || source_offmode == OUTPUT_HIGH_Z
                 || source_offmode == OUTPUT_ZERO );

    char buf[ 50 ];
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
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.output)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

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
    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    k26->source[ ch ].output == source_output
                 || keithley2600a_test_toggle_source_output( ch ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.output=%d", smu[ ch ], ( int ) source_output );
	keithley2600a_cmd( buf );

    /* Despite all our testing an attempt to switch output on may fail if the
       settings still aren't acceptable. So we'd always check if the output
       was in fact switched on. */

    if ( ! keithley2600a_get_source_output( ch ) != source_output )
        keithley2600a_show_errors( );

    return k26->source[ ch ].output = source_output;
}


/*---------------------------------------------------------------*
 * Returns if channel is in high-capacity mode
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_highc( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.highc)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

    return k26->source[ ch ].highc = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches high-capacity mode of the channel on or off (switching
 * it on also modifies a number of other settings!)
 *---------------------------------------------------------------*/

bool
keithley2600a_set_source_highc( unsigned int ch,
                                bool         source_highc )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
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
 * Returns if the channel is set up as voltage or current source
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_func( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.func)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

    return k26->source[ ch ].func = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches the channel between voltage of current source
 *---------------------------------------------------------------*/

bool
keithley2600a_set_source_func( unsigned int ch,
                               bool         source_func )
{
    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    source_func == k26->source[ ch ].func
                 || keithley2600a_test_toggle_source_func( ch ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.fun=%d", smu[ ch ], source_func );
	keithley2600a_cmd( buf );

    /* Switching the sourcing mode may have an effect on the reported measure
       range of the function we're switching from */

    if ( source_func != k26->source[ ch ].func )
    {
        if ( source_func == OUTPUT_DCAMPS )
            keithley2600a_get_measure_rangev( ch );
        else
            keithley2600a_get_measure_rangei( ch );
    }

    return k26->source[ ch ].func = source_func;
}


/*---------------------------------------------------------------*
 * Returns if autorange for source voltage with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_autorangev( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.autorangev)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

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
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "%s.source.autorangev=%d", smu[ ch ], ( int ) autorange );
	keithley2600a_cmd( buf );

    /* Switching autorange on can result in a change of the range setting */

    if ( autorange )
        keithley2600a_get_source_rangev( ch );

    return k26->source[ ch ].autorangev = autorange;
}


/*---------------------------------------------------------------*
 * Returns if autorange for source current with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_autorangei( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.autorangei)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

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
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "%s.source.autorangei=%d", smu[ ch ], ( int ) autorange );
	keithley2600a_cmd( buf );

    /* Switching autorange on can result in a change of the range setting */

    if ( autorange )
        keithley2600a_get_source_rangei( ch );

    return k26->source[ ch ].autorangei = autorange;
}


/*---------------------------------------------------------------*
 * Returns the setting of the source voltage range of the channel
 * (but the value may change if autoranging is on)
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_rangev( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.rangev)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    double range = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_rangev( ch, range ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].rangev = range;
}


/*---------------------------------------------------------------*
 * Sets a range for the source voltage of the channel (thereby
 * disabling autoranging)
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_rangev( unsigned int ch,
                                 double       range )
{
    fsc2_assert( ch < NUM_CHANNELS );

    range = fabs( range );

    fsc2_assert( range <= keithley2600a_max_source_rangev( ch ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.rangev=%.6g", smu[ ch ], range );
    keithley2600a_cmd( buf );

    k26->source[ ch ].autorangev = false;
    return keithley2600a_get_source_rangev( ch );
}


/*---------------------------------------------------------------*
 * Returns the setting of the source current range of the channel
 * (but the actual value may bw different if autoranging is on)
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_rangei( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.rangei)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    double range = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_rangei( ch, range ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].rangei = range;
}


/*---------------------------------------------------------------*
 * Sets a range for the source current of the channel (thereby
 * disabling autoranging)
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_rangei( unsigned int ch,
								 double       range )
{
    fsc2_assert( ch < NUM_CHANNELS );

    range = fabs( range );

    fsc2_assert( range <= keithley2600a_max_source_rangev( ch ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.rangei=%.6g", smu[ ch ], range );
    keithley2600a_cmd( buf );

    k26->source[ ch ].autorangei = false;
    return keithley2600a_get_source_rangei( ch );
}


/*---------------------------------------------------------------*
 * Returns the lowest value the source voltage range can be set
 * to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_lowrangev( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.lowrangev)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    double lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_lowrangev( ch, lowrange ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].lowrangev = lowrange;
}


/*---------------------------------------------------------------*
 * Sets the lowest value the source voltage range can be set
 * to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_lowrangev( unsigned int ch,
									double       lowrange )
{
    fsc2_assert( ch < NUM_CHANNELS );

    lowrange = fabs( lowrange );

    fsc2_assert( lowrange = keithley2600a_max_source_rangev( ch ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.lowrangev=%.6g", smu[ ch ], lowrange );
    keithley2600a_cmd( buf );

    /* If the device is source autoranging the changed lower limit may have
       resulted in a change of the range */

    if ( k26->source[ ch ].autorangev )
        keithley2600a_get_source_rangev( ch );

    return keithley2600a_get_source_lowrangev( ch );
}


/*---------------------------------------------------------------*
 * Returns the lowest value the source current range can be set
 * to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_lowrangei( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.lowrangei)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    double lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_lowrangei( ch, lowrange ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].lowrangei = lowrange;
}


/*---------------------------------------------------------------*
 * Sets the lowest value the source current range can be set
 * to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_lowrangei( unsigned int ch,
                                    double       lowrange )
{
    fsc2_assert( ch < NUM_CHANNELS );

    lowrange = fabs( lowrange );

    fsc2_assert( lowrange <= keithley2600a_max_source_rangev( ch ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.lowrangei=%.6g", smu[ ch ], lowrange );
    keithley2600a_cmd( buf );

    /* If the device is source autoranging the changed lower limit may have
       resulted in a change of the range */

    if ( k26->source[ ch ].autorangei )
        keithley2600a_get_source_rangei( ch );

    return keithley2600a_get_source_lowrangei( ch );
}


/*---------------------------------------------------------------*
 * Returns the source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_levelv( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	char buf[ 50 ];
	sprintf( buf, "printnumber(%s.source.levelv)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

	double volts = keithley2600a_line_to_double( buf );
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
	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_levelv( ch, volts ) );

	char buf[ 50 ];
	sprintf( buf, "%s.source.levelv=%.6g", smu[ ch ], volts );
	keithley2600a_cmd( buf );

    /* If autoranging is on setting a new level could nodify the range */

    if ( k26->source[ ch ].autorangev )
        keithley2600a_get_source_rangev( ch );

	return keithley2600a_get_source_levelv( ch );
}


/*---------------------------------------------------------------*
 * Returns the source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_leveli( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	char buf[ 50 ];
	sprintf( buf, "printnumber(%s.source.leveli)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

	double amps = keithley2600a_line_to_double( buf );
	if ( ! keithley2600a_check_source_leveli( ch, amps ) )
		keithley2600a_bad_data( );

    /* If autoranging is on setting a new level could modify the range */

    if ( k26->source[ ch ].autorangei )
        keithley2600a_get_source_rangei( ch );

	return k26->source[ ch ].leveli = amps;
}


/*---------------------------------------------------------------*
 * Sets the source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_leveli( unsigned int ch,
								 double       amps )
{
	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_leveli( ch, amps ) );

	char buf[ 50 ];
	sprintf( buf, "%s.source.leveli=%.6g", smu[ ch ], amps );
	keithley2600a_cmd( buf );

	return keithley2600a_get_source_leveli( ch );
}


/*---------------------------------------------------------------*
 * Returns the source voltage (compliance) lmit for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_limitv( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	char buf[ 50 ];
	sprintf( buf, "printnumber(%s.source.limitv)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

	double volts = keithley2600a_line_to_double( buf );
	if ( ! keithley2600a_check_source_limitv( ch, volts ) )
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
	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_limitv( ch, volts ) );

	char buf[ 50 ];
	sprintf( buf, "%s.source.limitv=%.6g", smu[ ch ], volts );
	keithley2600a_cmd( buf );

	return keithley2600a_get_source_limitv( ch );
}


/*---------------------------------------------------------------*
 * Returns the source current (compliance) limit for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_limiti( unsigned int ch )
{
	fsc2_assert( ch < NUM_CHANNELS );

	char buf[ 50 ];
	sprintf( buf, "printnumber(%s.source.limiti)", smu[ ch ] );
	TALK( buf, buf, sizeof buf, false, 7 );

	double amps = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_limiti( ch, amps ) )
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
	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_source_limiti( ch, amps ) );

	char buf[ 50 ];
	sprintf( buf, "%s.source.limiti=%.6g", smu[ ch ], amps );
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
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "print(%s.source.compliance)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf, false );

    if ( strcmp( buf, "false\n" ) && strcmp( buf, "true\n" ) )
        keithley2600a_bad_data( );

    return buf[ 0 ] == 't';
}


/*---------------------------------------------------------------*
 * Returns the source delay for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_delay( unsigned int ch )
{
   fsc2_assert( ch < NUM_CHANNELS );

   char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.delay)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );
 
    double delay = keithley2600a_line_to_double( buf );
    if ( delay < 0 && delay != DELAY_AUTO )
        keithley2600a_bad_data( );

    return k26->source[ ch ].delay = delay;
}


/*---------------------------------------------------------------*
 * Sets the source delay for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_delay( unsigned int ch,
                                double       delay )
{
    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( delay >= 0 || delay == DELAY_AUTO );

    char buf[ 50 ];
    printf( buf, "%s.source.delay=%.6g", smu[ ch ], delay );
    keithley2600a_cmd( buf );

    return k26->source[ ch ].delay = delay;
}


/*---------------------------------------------------------------*
 * Returns the off current limit
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_offlimiti( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.offlimiti)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    double limit = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_source_offlimiti( ch, limit ) )
        keithley2600a_bad_data( );

    return k26->source[ ch ].offlimiti = limit;
}


/*---------------------------------------------------------------*
 * Sets the off current limit
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_offlimiti( unsigned int ch,
                                    double       limit )
{
    fsc2_assert( ch < NUM_CHANNELS );

    limit = fabs( limit );

    fsc2_assert( keithley2600a_check_source_offlimiti( ch, limit ) );

    char buf[ 50 ];
    sprintf( buf, "%s.source.offlimiti=%.6g", smu[ ch ], limit );
    keithley2600a_cmd( buf );

    /* Better check result, it's not too well documented what the limits
       really are */

    return keithley2600a_get_source_offlimiti( ch );
}


/*---------------------------------------------------------------*
 * Returns the sink mode for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_sink( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.sink)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    return k26->source[ ch ].sink = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Sets the sink mode for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_sink( unsigned int ch,
                               double       sink )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "%s.source.offlimiti=%d)", smu[ ch ], sink ? 1 : 0 );
    keithley2600a_cmd( buf );

    return k26->source[ ch ].sink = sink;
}


/*---------------------------------------------------------------*
 * Returns the settling mode for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_settling( unsigned int ch )
{
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];
    sprintf( buf, "printnumber(%s.source.settling)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

    int settle = keithley2600a_line_to_int( buf );
    if (    settle != SETTLE_SMOOTH
         && settle != SETTLE_FAST_RANGE
         && settle != SETTLE_FAST_POLARITY
         && settle != SETTLE_DIRECT_IRANGE
         && settle != SETTLE_SMOOTH_100NA
         && settle != SETTLE_FAST_ALL )
        keithley2600a_bad_data( );

    return k26->source[ ch ].settling = settle;
}


/*---------------------------------------------------------------*
 * Sets the settling mode for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_settling( unsigned int ch,
                                   int          settle )
{
    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    settle == SETTLE_SMOOTH
                 || settle == SETTLE_FAST_RANGE
                 || settle == SETTLE_FAST_POLARITY
                 || settle == SETTLE_DIRECT_IRANGE
                 || settle == SETTLE_SMOOTH_100NA
                 || settle == SETTLE_FAST_ALL );

    char buf[ 50 ];
    sprintf( buf, "%s.source.settling=%d", smu[ ch ], settle );
    keithley2600a_cmd( buf );

    return k26->source[ ch ].settling = settle;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
