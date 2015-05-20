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

static double prepare_sweep_list( const Var_T * v );


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

    sprintf( buf, "%s.measure.rangev=%.6g", smu[ ch ], range );
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

    sprintf( buf, "%s.measure.rangei=%.6g", smu[ ch ], range );
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

    sprintf( buf, "%s.measure.lowrangev=%.6g", smu[ ch ], lowrange );
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

    sprintf( buf, "%s.measure.lowrangei=%.6g", smu[ ch ], lowrange );
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
    timeout =   lrnd( 1.2e6 * (   ( k26->source[ ch ].delay > 0 ?
                                    k26->source[ ch ].delay : 0 )
                                  + ( k26->measure[ ch ].delay > 0 ?
                                      k26->measure[ ch ].delay : 0 )
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
    timeout =   lrnd( 1.0e6 * (   ( k26->source[ ch ].delay > 0 ?
                                    k26->source[ ch ].delay : 0 )
                                  + ( k26->measure[ ch ].delay > 0 ?
                                      k26->measure[ ch ].delay : 0 )
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

    sprintf( buf, "%s.measure.nplc=%.6g", smu[ ch ], t * k26->linefreq );
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

    sprintf( buf, "%s.measure.rel.levelv=%.6g)", smu[ ch ], offset );
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

    sprintf( buf, "%s.measure.rel.leveli=%.6g)", smu[ ch ], offset );
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

    printf( buf, "%s.measure.delay=%.6g", smu[ ch ], delay );
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
 * either voltage, current, power, resistance or voltage and
 * current at each sweep step position.
 *
 * ch            channel to be used
 * sweep_what    what to sweep (voltage or current)
 * measure_what  what to measure (voltage, current, power, resistance
 *               or both voltage and current)
 * start         start voltage or current of sweep
 * end           end voltage or current of sweep
 * num_points    number of sweep step positions
 *
 * Returns a newly allocated array with as many doubles as the
 * number of points in the sweep (or twice that many for simul-
 * taneous meaurements of voltages and currents).
 *
 * Function switches output off at end.
 *---------------------------------------------------------------*/

double *
keithley2600a_sweep_and_measure( unsigned int ch,
                                 int          sweep_what,
                                 int          measure_what,
                                 double       start,
                                 double       end,
                                 int          num_points )
{
    static const char * method[ ] = { "v", "i", "p", "r", "iv" };
    char * cmd = NULL;
    char * buf = NULL;
    double * data = NULL;
    long timeout;
    int cnt = 1;
    int num_data_points =   ( measure_what == VOLTAGE_AND_CURRENT ? 2 : 1 )
                          * num_points;

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
                 || measure_what == RESISTANCE
                 || measure_what == VOLTAGE_AND_CURRENT);
    fsc2_assert( num_points >= 2 );

    /* If it hasn't been done yet send a few LUA functions to the device
       needed for doing linear sweeps */

    if ( ! k26->lin_sweeps_prepared )
        keithley2600a_prep_lin_sweeps( );

    /* Doing a sweep can take quite some time and we got to wait for
       it to finish, so raise the read timeout accordingly with a
       bit (20%) left to spare */

    if (    k26->measure[ ch ].filter.enabled
         && k26->measure[ ch ].filter.type == FILTER_REPEAT_AVG )
        cnt = k26->measure[ ch ].filter.count;
    timeout =   lrnd(   1.2e6
                      * num_points
                      * (   ( k26->source[ ch ].delay > 0 ?
                              k26->source[ ch ].delay : 0 )
                          + ( k26->measure[ ch ].delay > 0 ?
                              k26->measure[ ch ].delay : 0 )
                          + cnt * k26->measure[ ch ].time ) )
              + READ_TIMEOUT;
    
    if ( vxi11_set_timeout( VXI11_READ, timeout ) != SUCCESS )
        keithley2600a_comm_failure( );

    /* Create the LUA command to run the linear sweep, uing functions
       previously sent to the device */

    cmd = get_string( "fsc2_lin.sweep_and_measure(%s, '%s', '%s', "
                      "%.6g, %.6g, %d)",
                      smu[ ch ], method[ sweep_what ], method[ measure_what ],
                      start, end, num_points );

    TRY
    {
        buf = T_malloc( 20 * num_data_points );
        keithley2600a_talk( cmd, buf, 20 * num_data_points, true );
        cmd = T_free( cmd );

        if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
            keithley2600a_comm_failure( );

        data = T_malloc( num_data_points * sizeof *data );
        keithley2600a_line_to_doubles( buf, data, num_data_points );
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
 * Function for doing a voltage or current list sweep and measuring
 * either voltage, current, power, resistance or voltage and current
 * at each sweep step position.
 *
 * ch            channel to be used
 * sweep_what    what to sweep (voltage or current)
 * measure_what  what to measure (voltage, current, power, resistance
 *               or both voltage and current)
 * list          EDL variable with a list of the values to be used
 *               in the sweep
 *
 * Returns a newly allocated array with as many doubles as the
 * number of points in the sweep list (or twice that many for
 * simultaneous meaurements of voltages and currents).
 *
 * Function switches output off at end.
 *---------------------------------------------------------------*/

double *
keithley2600a_list_sweep_and_measure( unsigned int  ch,
                                      int           sweep_what,
                                      int           measure_what,
                                      const Var_T * v )
{
    static const char * method[ ] = { "v", "i", "p", "r", "iv" };
    char * cmd = NULL;
    char * buf = NULL;
    double * data = NULL;
    long timeout;
    int cnt = 1;
    double max_val;

    /* If it hasn't been done yet send a few LUA functions to the device
       needed for doing linear sweeps */

    if ( ! k26->list_sweeps_prepared )
        keithley2600a_prep_list_sweeps( );

    max_value = prepare_sweep_list( v );

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    ( sweep_what == VOLTAGE && max_val <= MAX_SOURCE_LEVELV )
                 || ( sweep_what == CURRENT && max_val <= MAX_SOURCE_LEVELI ) );
 
    /* Doing a sweep can take quite some time and we got to wait for
       it to finish, so raise the read timeout accordingly with a
       bit (20%) left to spare */

    if (    k26->measure[ ch ].filter.enabled
         && k26->measure[ ch ].filter.type == FILTER_REPEAT_AVG )
        cnt = k26->measure[ ch ].filter.count;
    timeout =   lrnd(   1.2e6
                      * v->len
                      * (   ( k26->source[ ch ].delay > 0 ?
                              k26->source[ ch ].delay : 0 )
                          + ( k26->measure[ ch ].delay > 0 ?
                              k26->measure[ ch ].delay : 0 )
                          + cnt * k26->measure[ ch ].time ) )
              + READ_TIMEOUT;
    
    if ( vxi11_set_timeout( VXI11_READ, timeout ) != SUCCESS )
        keithley2600a_comm_failure( );

    TRY
    {
        cmd = get_string( "fsc2_list.sweep_and_measure(%s, '%s', '%s', %.6g)",
                          smu[ ch ], method[ sweep_what ],
                          method[ measure_what ], max_val );

        double num_data_points = v->len
                             * ( measure_what == VOLTAGE_AND_CURRENT ? 2 : 1 );
        buf = T_malloc( 20 * num_data_points );
        keithley2600a_talk( cmd, buf, 20 * num_data_points, true );
        cmd = T_free( cmd );

        if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
            keithley2600a_comm_failure( );

        data = T_malloc( num_data_points * sizeof *data );
        keithley2600a_line_to_doubles( buf, data, num_data_points );
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
 * Helper function for assembling an ASCII comma-separated list of
 * all values from a 1-dimensional integer or floating point EDL
 * arrray - the returned char array has to be free()ed by the
 * caller
 *---------------------------------------------------------------*/

static
double
prepare_sweep_list( const Var_T * v )
{
    ssize_t i;
    char buf[ 50 ];
    double max_val = 0;

    fsc2_assert( v->type == FLOAT_ARR || v->type == INT_ARR );
    fsc2_assert( v->dim == 1 && v->len >= 2 && v->len <= MAX_SWEEP_POINTS );

    keithley2600a_cmd( "fsc2_list.list = {}" );

    /* The obvious thing to do would be creating a string and pass it to
       the device, but there's only a very small input buffer and too
       many list elements easily could over-run it. Thuw we do it the
       hard way and vreate a table for the sweep points and append
       each of them successively to it. Not nice but better than getting
       into troubles that can be very ahr to explain to the users... */

    for ( i = 0; i < v->len; i++ )
    {
        if ( v->type == INT_VAR )
        {
            sprintf( buf, "table.insert(fsc2_list.list, %.6g)",
                     ( double ) v->val.lpnt[ i ] );
            max_val = d_max( max_val, fabs( v->val.lpnt[ i ] ) );
        }
        else
        {
            sprintf( buf, "table.insert(fsc2_list.list, %.6g)",
                     v->val.dpnt[ i ] );
            max_val = d_max( max_val, fabs( v->val.dpnt[ i ] ) );
        }

        keithley2600a_cmd( buf );
    }

    return max_val;
}


/*---------------------------------------------------------------*
 * Does a contact check measurement - note that this not possible if
 * output is off and on HIGH_Z output mode or the source current
 * is limited to less than 1 mA.
 *---------------------------------------------------------------*/

bool
keithley2600a_contact_check( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    k26->source[ ch ].output
                 || (    k26->source[ ch ].offmode != OUTPUT_HIGH_Z
                      && k26->source[ ch ].offlimiti >=
                                                 MIN_CONTACT_CURRENT_LIMIT ) );

    sprintf( buf, "print(%s.contact.check())", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    return keithley2600a_line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Measures and returns the low and high resistance measured
 * in a contact measurement
 *---------------------------------------------------------------*/

const double *
keithley2600a_contact_resistance( unsigned int ch )
{
    char buf[ 50 ];
    double r[ 2 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    k26->source[ ch ].output
                 || (    k26->source[ ch ].offmode != OUTPUT_HIGH_Z
                      && k26->source[ ch ].offlimiti >=
                                                 MIN_CONTACT_CURRENT_LIMIT ) );

    sprintf( buf, "print(%s.contact.r())", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    return keithley2600a_line_to_doubles( buf, r, 2 );
}


/*---------------------------------------------------------------*
 * Returns the contact threshold resistance
 *---------------------------------------------------------------*/

double
keithley2600a_get_contact_threshold( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.contact.threshold)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    return k26->contact[ ch ].threshold = keithley2600a_line_to_double( buf );
}


/*---------------------------------------------------------------*
 * Sets the contact threshold resistance
 *---------------------------------------------------------------*/

double
keithley2600a_set_contact_threshold( unsigned int ch,
                                     double       threshold )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( threshold >= 0 && threshold < 1e37 );

    sprintf( buf, "%s.contact.threshold=%.6g", smu[ ch ], threshold );
    keithley2600a_cmd( buf );

    return k26->contact[ ch ].threshold = threshold;
}


/*---------------------------------------------------------------*
 * Returns the contact measurement speed
 *---------------------------------------------------------------*/

int
keithley2600a_get_contact_speed( unsigned int ch )
{
    char buf[ 50 ];
    int speed;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.contact.speed)", smu[ ch ] );
    keithley2600a_talk( buf, buf, sizeof buf, false );

    speed = keithley2600a_line_to_int( buf );
    if ( speed < CONTACT_FAST || speed > CONTACT_SLOW )
        keithley2600a_bad_data( );

    return k26->contact[ ch ].speed = speed;
}


/*---------------------------------------------------------------*
 * Sets the contact measurement speed
 *---------------------------------------------------------------*/

int
keithley2600a_set_contact_speed( unsigned int ch,
                                 int          speed )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( speed >= CONTACT_FAST && speed <= CONTACT_SLOW );

    sprintf( buf, "%s.contact.speed=%d", smu[ ch ], speed );
    keithley2600a_cmd( buf );

    return k26->contact[ ch ].speed = speed;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
