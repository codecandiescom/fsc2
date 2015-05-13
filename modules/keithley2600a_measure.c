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
 * Returns the setting of the measurement voltage range of the
 * channel (but the value may change if autoranging is on and
 * is overruled by the source voltage range while source function
 * is voltage)
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_rangev( unsigned int ch )
{
    double range;
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rangev)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    range = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_measure_rangev( ch, range ) )
        keithley2600a_bad_data( );

    return range;
}


/*---------------------------------------------------------------*
 * Sets a range for the measurement voltage of the channel, thereby
 * disabling autoranging. Note that this value is "overruled" by
 * the source voltage range if we're in voltage sourcing mode.
 * The function returns the "active" value.
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rangev( unsigned int ch,
                                  double       range )
{
    char buf[ 50 ];

    range = fabs( range );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( range <= keithley2600a_max_measure_rangev( ch ) );

    sprintf( buf, "%s.measure.rangev=%.5g", smu[ ch ], range );
    keithley2600a_cmd( buf );

    k26->measure[ ch ].autorangev = false;

    /* If we're in voltage sourcing mode what the device sets (and returns
       on request) is always the source voltage range, but it remembers
       what we've set as the range for the moment were we switch to
       current sourcing mode */

    if ( k26->source[ ch ].func == OUTPUT_DCVOLTS )
        return k26->measure[ ch ].rangev = range;

    return keithley2600a_get_measure_rangev( ch );
}


/*---------------------------------------------------------------*
 * Returns the "active" setting of the source current range of
 * the channel. This value may not be what has been set for the
 * current range if we're in current sourcing mode, were the source
 * current range setting overrules it.
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_rangei( unsigned int ch )
{
    double range;
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rangei)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    range = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_measure_rangei( ch, range ) )
        keithley2600a_bad_data( );

    return range;
}


/*---------------------------------------------------------------*
 * Sets a range for the measure current of the channel (thereby
 * disabling autoranging). Note that if we're in current sourcing
 * mode this value has no effect and is overruled by the source
 * current range. The function returns the actual value.
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rangei( unsigned int ch,
                                  double       range )
{
    char buf[ 50 ];

    range = fabs( range );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( range <= keithley2600a_max_measure_rangei( ch ) );

    sprintf( buf, "%s.measure.rangei=%.5g", smu[ ch ], range );
    keithley2600a_cmd( buf );

    k26->measure[ ch ].autorangei = false;

    /* If we're in current sourcing mode what the device sets (and returns
       on request) is always the source current range, but it remembers
       what we've set as the range for the moment were we switch to
       voltage sourcing mode */

    if ( k26->source[ ch ].func == OUTPUT_DCAMPS )
        return k26->measure[ ch ].rangei = range;

    return keithley2600a_get_measure_rangei( ch );
}


/*---------------------------------------------------------------*
 * Returns if autorange for voltage measurements with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_autorangev( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autorangev)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->measure[ ch ].autorangev = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches autorange for voltage measurements with the channel
 * on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_set_measure_autorangev( unsigned int ch,
                                      bool         autorange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.autorangev=%d", smu[ ch ], ( int ) autorange );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].autorangev = autorange;
}


/*---------------------------------------------------------------*
 * Returns if autorange for current measurements with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_autorangei( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autorangei)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    return k26->measure[ ch ].autorangei = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches autorange for current measurements with the channel
 * on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_set_measure_autorangei( unsigned int ch,
                                      bool         autorange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.autorangei=%d", smu[ ch ], ( int ) autorange );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].autorangei = autorange;
}


/*---------------------------------------------------------------*
 * Returns the lowest value the measurment voltage range can be
 * set to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_lowrangev( unsigned int ch )
{
    char buf[ 50 ];
    double lowrange;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.lowrangev)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_measure_lowrangev( ch, lowrange ) )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].lowrangev = lowrange;
}


/*---------------------------------------------------------------*
 * Sets the lowest value the measurement voltage range can be set
 * to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_lowrangev( unsigned int ch,
                                     double       lowrange )
{
    char buf[ 50 ];

    lowrange = fabs( lowrange );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( lowrange <= keithley2600a_max_measure_rangev( ch ) );

    sprintf( buf, "%s.measure.lowrangev=%.5g", smu[ ch ], lowrange );
    keithley2600a_cmd( buf );

    /* If the device is measurement autoranging the changed lower limit
       may have resulted in a change of the range */

    if ( k26->measure[ ch ].autorangev )
        keithley2600a_get_measure_rangev( ch );

    return keithley2600a_get_measure_lowrangev( ch );
}


/*---------------------------------------------------------------*
 * Returns the lowest value the measurement current range can be
 * set to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_lowrangei( unsigned int ch )
{
    char buf[ 50 ];
    double lowrange;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.lowrangei)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_measure_lowrangei( ch, lowrange ) )
        keithley2600a_bad_data( );

    return keithley2600a_get_measure_lowrangei( ch );
}


/*---------------------------------------------------------------*
 * Sets the lowest value the measurement current range can be set
 * to while autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_lowrangei( unsigned int ch,
                                     double       lowrange )
{
    char buf[ 50 ];

    lowrange = fabs( lowrange );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( lowrange <= keithley2600a_max_measure_rangei( ch ) );

    sprintf( buf, "%s.measure.lowrangei=%.5g", smu[ ch ], lowrange );
    keithley2600a_cmd( buf );

    /* If the device is measurement autoranging the changed lower limit
       may have resulted in a change of the range */

    if ( k26->measure[ ch ].autorangei )
        keithley2600a_get_measure_rangei( ch );

    return keithley2600a_get_measure_lowrangei( ch );
}


/*---------------------------------------------------------------*
 * Returns if autozero for measurements with the channel
 * is off (0), once (1) or auto (2)
 *---------------------------------------------------------------*/

int
keithley2600a_get_measure_autozero( unsigned int ch )
{
    char buf[ 50 ];
    int autozero;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autozero)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

    autozero = keithley2600a_line_to_int( buf );
    if ( autozero < AUTOZERO_OFF || autozero > AUTOZERO_AUTO )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].autozero = autozero;
}


/*---------------------------------------------------------------*
 * Switches autozero for measurements with the channel to off (0),
 * once (1) or auto (2)
 *---------------------------------------------------------------*/

int
keithley2600a_set_measure_autozero( unsigned int ch,
                                    int          autozero )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( autozero >= AUTOZERO_OFF && autozero <= AUTOZERO_AUTO );

    sprintf( buf, "%s.measure.autozero=%d", smu[ ch ], autozero );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].autozero = autozero;
}


/*---------------------------------------------------------------*
 * Makes a simple measurement of voltage, current, power or resistance
 *---------------------------------------------------------------*/

double
keithley2600a_measure( unsigned int ch,
                       int          what )
{
    static char method[ ] = { 'v', 'i', 'p', 'r' };
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(  what >= VOLTAGE && what <= RESISTANCE );

    if ( k26->measure[ ch ].count > 1 )
        keithley2600a_set_measure_count( ch, 1 );

    sprintf( buf, "print(%s.measure.%c())", smu[ ch ], method[ what ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    k26->measure[ ch ].count = 1;
    return keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Makes a simple measurement of voltage, current, power or resistance
 *---------------------------------------------------------------*/

double const *
keithley2600a_measure_iv( unsigned int ch )
{
    static double iv[ 2 ];
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    if ( k26->measure[ ch ].count > 1 )
        keithley2600a_set_measure_count( ch, 1 );

    sprintf( buf, "print(%s.measure.iv())", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    k26->measure[ ch ].count = 1;
    return keithley2600a_line_to_doubles( buf, iv, 2 );
}


/*---------------------------------------------------------------*
 * Returns the time used for measuring a data point (AD converter
 * integration time, ramge depends on the line frequency)
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_time( unsigned int ch )
{
    char buf[ 50 ];
    double t;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.nplc)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    t = keithley2600a_line_to_double( buf ) / k26->linefreq;
    if ( ! keithley2600a_check_measure_time( t ) )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].time = t;
}



/*---------------------------------------------------------------*
 * Sets the time used for measuring a data point (AD converter
 * integration time, ramge depends on the line frequency)
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_time( unsigned int ch,
                                double       t )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( keithley2600a_check_measure_time( t ) );

    sprintf( buf, "%s.measure.nplc=%.3f", smu[ ch ], t * k26->linefreq );
    keithley2600a_cmd( buf );

    return keithley2600a_get_measure_time( ch );
}


/*---------------------------------------------------------------*
 * Returns the number of measurements to be done
 *---------------------------------------------------------------*/

int
keithley2600a_get_measure_count( unsigned int ch )
{
    char buf[ 50 ];
    int count;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.count)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    count = keithley2600a_line_to_int( buf );
    if ( count <= 0 )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].count = count;
}


/*---------------------------------------------------------------*
 * Sets the number of measurements to be done
 *---------------------------------------------------------------*/

int
keithley2600a_set_measure_count( unsigned int ch,
                                 int          count )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( count > 0 );

    sprintf( buf, "%s.measure.count=%d", smu[ ch ], count );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].count = count;
}


/*---------------------------------------------------------------*
 * Returns the measurement voltage offset
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_rel_levelv( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rel.levelv)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    return k26->measure[ ch ].relv.level = keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Sets the measurement voltage offset
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rel_levelv( unsigned int ch
                                      double       offset )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.rel.levelv=%.5g)", smu[ ch ], offset );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].relv.level = offset;
}


/*---------------------------------------------------------------*
 * Returns the measurement current offset
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_rel_leveli( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rel.leveli)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    return k26->measure[ ch ].reli.level = keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Sets the measurement current offset
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rel_leveli( unsigned int ch
                                      double       offset )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.rel.leveli=%.5g)", smu[ ch ], offset );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].reli.level = offset;
}


/*---------------------------------------------------------------*
 * Returns if the measurement voltage offset is enabled
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_rel_levelv_enabled( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rel.enablev)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    return k26->measure[ ch ].relv.enabled = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Sets if the measurement voltage offset is enabled
 *---------------------------------------------------------------*/

bool
keithley2600a_set_measure_rel_levelv_enabled( unsigned int ch,
                                              bool         on_off )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.rel.enablev=%d", smu[ ch ], on_off );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].relv.enabled = on_off;
}


/*---------------------------------------------------------------*
 * Returns if the measurement current offset is enabled
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_rel_leveli_enabled( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rel.enablei)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf );

    return k26->measure[ ch ].reli.enabled = keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Sets if the measurement current offset is enabled
 *---------------------------------------------------------------*/

bool
keithley2600a_set_measure_rel_leveli_enabled( unsigned int ch,
                                              bool         on_off )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.rel.enablei=%d", smu[ ch ], on_off );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].reli.enabled = on_off;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
