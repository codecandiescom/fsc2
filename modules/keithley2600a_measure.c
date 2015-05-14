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
#include "vxi11_user.h"


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
    keithley2600a_talk( buf, buf, sizeof buf, false );

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
    keithley2600a_talk( buf, buf, sizeof buf, false );

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
 * Returns if autoranging for voltage measurements with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_autorangev( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autorangev)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf, false );

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
 * Returns if autoranging for current measurements with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_autorangei( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autorangei)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf, false );

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
 * set to by autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_lowrangev( unsigned int ch )
{
    char buf[ 50 ];
    double lowrange;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.lowrangev)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_measure_lowrangev( ch, lowrange ) )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].lowrangev = lowrange;
}


/*---------------------------------------------------------------*
 * Sets the lowest value the measurement voltage range can be set
 * to by autoranging
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
 * set to by autoranging
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_lowrangei( unsigned int ch )
{
    char buf[ 50 ];
    double lowrange;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.lowrangei)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    lowrange = keithley2600a_line_to_double( buf );
    if ( ! keithley2600a_check_measure_lowrangei( ch, lowrange ) )
        keithley2600a_bad_data( );

    return keithley2600a_get_measure_lowrangei( ch );
}


/*---------------------------------------------------------------*
 * Sets the lowest value the measurement current range can be set
 * to by autoranging
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
 * Returns if autozero for measurements with the channel is off (0),
 * once (1) or auto (2)
 *---------------------------------------------------------------*/

int
keithley2600a_get_measure_autozero( unsigned int ch )
{
    char buf[ 50 ];
    int autozero;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autozero)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf, false );

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
 * Does a measurement of voltage, current, power or resistance
 *---------------------------------------------------------------*/

double
keithley2600a_measure( unsigned int ch,
                       int          what )
{
    static char method[ ] = { 'v', 'i', 'p', 'r' };
    char buf[ 50 ];
    long timeout;
    int cnt = 1;

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(  what >= VOLTAGE && what <= RESISTANCE );

    /* Doing a measurement can take quite some time (if the time for
       measurements is set to the maximum and average filtering with
       the maximum number of points is on, this alone will take 50 s,
       and then there's also the pre-measurement delay), so raise the
       read timeout accordingly with a bit (20%) left to spare */

    if (    k26->measure[ ch ].filter.enabled
         && k26->measure[ ch ].filter.type == FILTER_REPEAT_AVG )
        cnt = k26->measure[ ch ].filter.count;
    timeout =   lrnd( 1.2e6 * (   k26->source[ ch ].delay
                                + k26->measure[ ch ].delay
                                + cnt * k26->measure[ ch ].time ) )
              + READ_TIMEOUT;
    
    if ( vxi11_set_timeout( VXI11_READ, timeout ) != SUCCESS )
        keithley2600a_comm_failure( );

    sprintf( buf, "print(%s.measure.%c())", smu[ ch ], method[ what ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    /* Reset the read timeout again */

    if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
        keithley2600a_comm_failure( );

    return keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Does a simultaneous measurement of voltage and current
 *---------------------------------------------------------------*/

double const *
keithley2600a_measure_iv( unsigned int ch )
{
    static double iv[ 2 ];
    char buf[ 50 ];
    long timeout;
    int cnt = 1;

    fsc2_assert( ch < NUM_CHANNELS );

    /* Make sure the read timeout is large enough */

    if (    k26->measure[ ch ].filter.enabled
         && k26->measure[ ch ].filter.type == FILTER_REPEAT_AVG )
        cnt = k26->measure[ ch ].filter.count;
    timeout =   lrnd( 1.0e6 * (   k26->source[ ch ].delay
                                + k26->measure[ ch ].delay
                                + cnt * k26->measure[ ch ].time ) )
              + READ_TIMEOUT;
    
    if ( vxi11_set_timeout( VXI11_READ, timeout ) != SUCCESS )
        keithley2600a_comm_failure( );

    sprintf( buf, "print(%s.measure.iv())", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    /* Reset the read timeout again */

    if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
        keithley2600a_comm_failure( );

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
    keithley2600a_talk( buf, buf, sizeof buf, false );

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

    sprintf( buf, "%s.measure.nplc=%.5g", smu[ ch ], t * k26->linefreq );
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
    keithley2600a_talk( buf, buf, sizeof buf, false );

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
    keithley2600a_talk( buf, buf, sizeof buf, false );

    return k26->measure[ ch ].relv.level = keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Sets the measurement voltage offset
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rel_levelv( unsigned int ch,
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
    keithley2600a_talk( buf, buf, sizeof buf, false );

    return k26->measure[ ch ].reli.level = keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Sets the measurement current offset
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rel_leveli( unsigned int ch,
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
    keithley2600a_talk( buf, buf, sizeof buf, false );

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
    keithley2600a_talk( buf, buf, sizeof buf, false );

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


/*---------------------------------------------------------------*
 * Returns the measure delay for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_delay( unsigned int ch )
{
   char buf[ 50 ];
   double delay;

   fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.delay)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf, false );
 
    delay = keithley2600a_line_to_double( buf );
    if ( delay < 0 && delay != DELAY_AUTO )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].delay = delay;
}


/*---------------------------------------------------------------*
 * Sets the measure delay for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_delay( unsigned int ch,
                                 double       delay )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( delay >= 0 || delay == DELAY_AUTO );

    printf( buf, "%s.measure.delay=%.5g", smu[ ch ], delay );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].delay = delay;
}


/*---------------------------------------------------------------*
 * Returns the measure filter type for the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_measure_filter_type( unsigned int ch )
{
    char buf[ 50 ];
    int type;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.filter.type)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    type = keithley2600a_line_to_int( buf );
    if (    type != FILTER_MOVING_AVG
         && type != FILTER_REPEAT_AVG
         && type != FILTER_MEDIAN )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].filter.type = type;
}


/*---------------------------------------------------------------*
 * Sets the measure filter type for the channel
 *---------------------------------------------------------------*/

int
keithley2600a_set_measure_filter_type( unsigned int ch,
                                       int          type )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    type == FILTER_MOVING_AVG
                 || type == FILTER_REPEAT_AVG
                 || type == FILTER_MEDIAN );

    sprintf( buf, "%s.measure.filter.type=%d", smu[ ch ], type );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].filter.type = type;
}


/*---------------------------------------------------------------*
 * Returns the measure filter count for the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_measure_filter_count( unsigned int ch )
{
    char buf[ 50 ];
    int count;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.filter.count)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    count = keithley2600a_line_to_int( buf );
    if ( count < 1 || count > MAX_FILTER_COUNT )
        keithley2600a_bad_data( );

    return k26->measure[ ch ].filter.count = count;
}


/*---------------------------------------------------------------*
 * Sets the measure filter count for the channel
 *---------------------------------------------------------------*/

int
keithley2600a_set_measure_filter_count( unsigned int ch,
                                        int          count )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( count >= 1 && count <= MAX_FILTER_COUNT );

    sprintf( buf, "%s.measure.filter.count=%d", smu[ ch ], count );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].filter.count = count;
}


/*---------------------------------------------------------------*
 * Returns if the measure filter for the channel is enabled
 *---------------------------------------------------------------*/

bool
keithley2600a_get_measure_filter_enabled( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.filter.enable)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    return k26->measure[ ch ].filter.enabled =
                                            keithley2600a_line_to_bool( buf );;
}


/*---------------------------------------------------------------*
 * Enables or disabled the measure filter for the channel
 *---------------------------------------------------------------*/

bool
keithley2600a_set_measure_filter_enabled( unsigned int ch,
                                          bool         on_off )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.filter.enable=%d", smu[ ch ], on_off ? 1 : 0 );
    keithley2600a_cmd( buf );

    return k26->measure[ ch ].filter.enabled = on_off;
}


/*---------------------------------------------------------------*
 * Function for doing a voltage or current sweep and measuring
 * either voltage, current, power or resistance at each sweep step
 * position.
 *
 * ch            channel to be used
 * sweep_what    what to sweep (voltage or current)
 * measure_what  what to measure (voltage, current, power or resistance)
 * start         start voltage or current of sweep
 * end           end voltage or current of sweep
 * num_points    number of sweep step positions
 *
 * Returns a newly allocated array with as many doubles as the
 * number of points in the sweep.
 *
 * Function switches output of and may change the source function.
 *---------------------------------------------------------------*/

double *
keithley2600a_sweep_and_measure( unsigned int ch,
                                 int          sweep_what,
                                 int          measure_what,
                                 double       start,
                                 double       end,
                                 int          num_points )
{
    static char method[ ] = { 'v', 'i', 'p', 'r' };
    char * cmd = NULL;
    char * buf = NULL;
    double * data = NULL;
    long timeout;
    int cnt;

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(   (    sweep_what == VOLTAGE
                     && fabs( start ) <= MAX_SOURCE_LEVELV
                     && fabs( end   ) <= MAX_SOURCE_LEVELV )
                 || (    sweep_what == CURRENT
                      && fabs( start ) <= MAX_SOURCE_LEVELI
                      && fabs( end   ) <= MAX_SOURCE_LEVELI ) );
    fsc2_assert(    measure_what == VOLTAGE
                 || measure_what == CURRENT
                 || measure_what == POWER
                 || measure_what == RESISTANCE );
    fsc2_assert( num_points >= 1 );

    /* Doing a sweep can take quite some time and we got to wait for
       it to finish, so raise the read timeout accordingly with a
       bit (20%) left to spare */

    if (    k26->measure[ ch ].filter.enabled
         && k26->measure[ ch ].filter.type == FILTER_REPEAT_AVG )
        cnt = k26->measure[ ch ].filter.count;
    timeout =   lrnd(   1.2e6
                      * num_points
                      * (   k26->source[ ch ].delay
                          + k26->measure[ ch ].delay
                          + cnt * k26->measure[ ch ].time ) )
              + READ_TIMEOUT;
    
    if ( vxi11_set_timeout( VXI11_READ, timeout ) != SUCCESS )
        keithley2600a_comm_failure( );

    /* Create the LUA commands to be sent to the device to set-up the
       sweep, enable output, start the sweep. wait for it to finish,
       disable output, print out the results and clean up. */

    cmd = get_string( "ch=%s\n"
                      "cnt=%d\n"
                      "ch.source.output=0\n"
                      "old_autorange=ch.source.autorange%c\n"
                      "old_range=ch.source.range%c\n"
                      "ch.source.range%c=%.5g\n"
                      "ch.source.func=%d\n"
                      "mbuf=ch.makebuffer(cnt)\n"
                      "ch.trigger.source.linear%c(%.5g,%.5g,cnt)\n"
                      "ch.trigger.source.action=1\n"
                      "ch.trigger.measure.%c(mbuf)\n"
                      "ch.trigger.measure.action=1\n"
                      "ch.trigger.count=cnt\n"
                      "ch.trigger.arm.count=1\n"
                      "ch.source.output=1\n"
                      "ch.trigger.initiate()\n"
                      "waitcomplete()\n"
                      "ch.source.output=0\n"
                      "ch.printbuffer(1,cnt,mbuf)\n"
                      "ch.source.range%c=old_range\n"
                      "ch.source.autorange%c=old_autorange\n"
                      "old_autorange=nil\n"
                      "old_range=nil\n"
                      "mbuf=nil\n"
                      "cnt=nil\n"
                      "ch=nil\n",
                      smu[ ch ],
                      num_points,
                      method[ sweep_what ],
                      method[ sweep_what ],
                      method[ sweep_what ],
                      d_max( fabs( start ), fabs( end ) ),
                      sweep_what == VOLTAGE ? OUTPUT_DCVOLTS : OUTPUT_DCAMPS,
                      method[ sweep_what ], start, end,
                      method[ measure_what ],
                      method[ sweep_what ],
                      method[ sweep_what ] );

    buf = T_malloc( 20 * cnt );

    TRY
    {
        keithley2600a_talk( cmd, buf, 20 * cnt, true );
        cmd = T_free( cmd );

        if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
            keithley2600a_comm_failure( );

        data = T_malloc( cnt * sizeof *data );
        keithley2600a_line_to_doubles( buf, data, cnt );
        buf = T_free( buf );
        TRY_SUCCESS;
    }
    CATCH( USER_BREAK_EXCEPTION )
    {
        sprintf( cmd, "%s.abort()\n%s.source.output=0", smu[ ch ], smu[ ch ] );
        keithley2600a_cmd( cmd );
        T_free( cmd );
        T_free( buf );
        RETHROW;
    }
    OTHERWISE
    {
        T_free( cmd );
        T_free( buf );
        T_free( data );
        RETHROW;
    }

    k26->source[ ch ].output = false;
    return data;
}


/*---------------------------------------------------------------*
 * Function for doing a voltage or current sweep and measuring
 * voltage and current simultaneously at each sweep step position.
 *
 * ch            channel to be used
 * sweep_what    what to sweep (voltage or current)
 * start         start voltage or current of sweep
 * end           end voltage or current of sweep
 * num_points    number of sweep step positions
 *
 * Returns a newly allocated array with as twice as many doubles as
 * the number of points in the sweep, containing alternating pairs
 * of the measured voltages and currents.
 *
 * Function switches output of and may change the source function.
 *---------------------------------------------------------------*/

double *
keithley2600a_sweep_and_measureiv( unsigned int ch,
                                   int          sweep_what,
                                   double       start,
                                   double       end,
                                   int          num_points )
{
    static char method[ ] = { 'v', 'i', 'p', 'r' };
    char * cmd = NULL;
    char * buf= NULL;
    double * data = NULL;
    long timeout;
    int cnt;

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(   (    sweep_what == VOLTAGE
                     && fabs( start ) <= MAX_SOURCE_LEVELV
                     && fabs( end   ) <= MAX_SOURCE_LEVELV )
                 || (    sweep_what == CURRENT
                      && fabs( start ) <= MAX_SOURCE_LEVELI
                      && fabs( end   ) <= MAX_SOURCE_LEVELI ) );
    fsc2_assert( num_points >= 1 );

    /* Doing a sweep can take quite some time and we got to wait for
       it to finish, so raise the read timeout accordingly with a
       bit (20%) left to spare */

    if (    k26->measure[ ch ].filter.enabled
         && k26->measure[ ch ].filter.type == FILTER_REPEAT_AVG )
        cnt = k26->measure[ ch ].filter.count;
    timeout =   lrnd(   1.2e6
                      * num_points
                      * (   k26->source[ ch ].delay
                          + k26->measure[ ch ].delay
                          + cnt * k26->measure[ ch ].time ) )
              + READ_TIMEOUT;
    
    if ( vxi11_set_timeout( VXI11_READ, timeout ) != SUCCESS )
        keithley2600a_comm_failure( );

    /* Create the LUA commands to be sent to the device to set-up the
       sweep, enable output, start the sweep. wait for it to finish,
       disable output, print out the results and clean up. */

    cmd = get_string( "ch=%s\n"
                      "cnt=%d\n"
                      "ch.source.output=0\n"
                      "old_autorange=ch.source.autorange%c\n"
                      "old_range=ch.source.range%c\n"
                      "ch.source.range%c=%.5g\n"
                      "ch.source.func=%d\n"
                      "vbuf=ch.makebuffer(cnt)\n"
                      "ibuf=ch.makebuffer(cnt)\n"
                      "ch.trigger.source.linear%c(%.5g,%.5g,cnt)\n"
                      "ch.trigger.source.action=1\n"
                      "ch.trigger.measure.iv(ibuf,vbuf)\n"
                      "ch.trigger.measure.action=1\n"
                      "ch.trigger.count=cnt\n"
                      "ch.trigger.arm.count=1\n"
                      "ch.source.output=1\n"
                      "ch.trigger.initiate()\n"
                      "waitcomplete()\n"
                      "ch.source.output=0\n"
                      "ch.printbuffer(1,cnt,vbuf,ibuf)\n"
                      "ch.source.range%c=old_range\n"
                      "ch.source.autorange%c=old_autorange\n"
                      "old_autorange=nil\n"
                      "old_range=nil\n"
                      "ibuf=nil\n"
                      "vbuf=nil\n"
                      "cnt=nil\n"
                      "ch=nil\n",
                      smu[ ch ],
                      num_points,
                      method[ sweep_what ],
                      method[ sweep_what ],
                      method[ sweep_what ],
                      d_max( fabs( start ), fabs( end ) ),
                      sweep_what == VOLTAGE ? OUTPUT_DCVOLTS : OUTPUT_DCAMPS,
                      method[ sweep_what ], start, end,
                      method[ sweep_what ],
                      method[ sweep_what ] );
    buf = T_malloc( 2 * 20 * cnt );

    TRY
    {
        keithley2600a_talk( cmd, buf, 20 * cnt, true );
        cmd = T_free( cmd );

        if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
            keithley2600a_comm_failure( );

        data = T_malloc( 2 * cnt * sizeof *data );
        keithley2600a_line_to_doubles( buf, data, 2 * cnt );
        T_free( buf );
        TRY_SUCCESS;
    }
    CATCH( USER_BREAK_EXCEPTION )
    {
        sprintf( cmd, "%s.abort()\n%s.source.output=0", smu[ ch ], smu[ ch ] );
        keithley2600a_cmd( cmd );
        T_free( cmd );
        T_free( buf );
        RETHROW;
    }
    OTHERWISE
    {
        T_free( cmd );
        T_free( buf );
        T_free( data );
        RETHROW;
    }

    k26->source[ ch ].output = false;
    return data;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
