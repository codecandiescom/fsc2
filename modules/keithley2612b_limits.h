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


#pragma once
#if ! defined KEITHLEY2600A_LIMITS_H_
#define KEITHLEY2600A_LIMITS_H_

#include "keithley2612b.h"


/* Number of of channels */

#if    defined _2601A || defined _2601B \
    || defined _2611A || defined _2611B \
    || defined _2635A || defined _2635B
#define NUM_CHANNELS 1
#else
#define NUM_CHANNELS 2
#endif

/*..................................................................*
 * Maximum source voltages and currents
 *..................................................................*/

#if    defined _2601A || defined _2602A \
    || defined _2601B || defined _2602B || defined _2604B
#define MAX_SOURCE_LEVELV  40.4
#define MAX_SOURCE_LEVELI  3.03
#else
#define MAX_SOURCE_LEVELV  202.0
#define MAX_SOURCE_LEVELI  1.515
#endif


#define keithley2612b_min_measure_lowrangei keithley2612b_min_source_lowrangei


double keithley2612b_best_source_rangev( unsigned int ch,
                                         double       volts );
double keithley2612b_best_source_rangei( unsigned int ch,
                                         double       amps );

double keithley2612b_max_source_levelv( unsigned int ch );
double keithley2612b_max_source_leveli( unsigned int ch );
bool keithley2612b_check_source_levelv( unsigned int ch,
										double       volts );
bool keithley2612b_test_source_levelv( unsigned int ch );
bool keithley2612b_check_source_leveli( unsigned int ch,
										double       amps );
bool keithley2612b_test_source_leveli( unsigned int ch );

double keithley2612b_min_source_rangev( unsigned int ch );
double keithley2612b_max_source_rangev( unsigned int ch );
double keithley2612b_min_source_rangei( unsigned int ch );
double keithley2612b_max_source_rangei( unsigned int ch );
bool keithley2612b_check_source_rangev( unsigned int ch,
                                        double       range );
bool keithley2612b_test_source_rangev( unsigned int ch );
bool keithley2612b_check_source_rangei( unsigned int ch,
										double       range );
bool keithley2612b_test_source_rangei( unsigned int ch );

double keithley2612b_max_source_limitv( unsigned int ch );
double keithley2612b_max_source_limiti( unsigned int ch );
double keithley2612b_min_source_limitv( unsigned int ch );
double keithley2612b_min_source_limiti( unsigned int ch );

bool keithley2612b_check_source_limitv( unsigned int ch,
										double       limit );
bool keithley2612b_test_source_limitv( unsigned int ch );
bool keithley2612b_check_source_limiti( unsigned int ch,
										double       limit );
bool keithley2612b_test_source_limiti( unsigned int ch );

double keithley2612b_min_source_lowrangev( unsigned int ch );
double keithley2612b_min_source_lowrangei( unsigned int ch );

bool keithley2612b_check_source_lowrangev( unsigned int ch,
                                           double       lowrange );
bool keithley2612b_check_source_lowrangei( unsigned int ch,
                                           double       lowrange );

double keithley2612b_min_source_offlimiti( unsigned int ch );
double keithley2612b_max_source_offlimiti( unsigned int ch );
bool keithley2612b_check_source_offlimiti( unsigned int ch,
                                           double       offlimiti );

bool keithley2612b_check_measure_rangev( unsigned int ch,
                                         double       range );
bool keithley2612b_check_measure_rangei( unsigned int ch,
                                         double       range );

bool keithley2612b_test_toggle_source_output( unsigned int ch );
bool keithley2612b_test_toggle_source_func( unsigned int ch );

double keithley2612b_max_measure_rangev( unsigned ch );
double keithley2612b_max_measure_rangei( unsigned ch );

double keithley2612b_best_measure_rangev( unsigned int ch,
                                          double       volts );
double keithley2612b_best_measure_rangei( unsigned int ch,
                                          double       amps );

bool keithley2612b_check_measure_lowrangev( unsigned int ch,
                                            double       lowrange );
bool keithley2612b_check_measure_lowrangei( unsigned int ch,
                                            double       lowrange );

double keithley2612b_min_measure_time( void );
double keithley2612b_max_measure_time( void );
bool keithley2612b_check_measure_time( double t );

#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
