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
#if ! defined KEITHLEY2600A_MEASURE_H_
#define KEITHLEY2600A_MEASURE_H_


double keithley2600a_get_measure_rangev( unsigned int ch );
double keithley2600a_set_measure_rangev( unsigned int ch,
                                         double       range );

double keithley2600a_get_measure_rangei( unsigned int ch );
double keithley2600a_set_measure_rangei( unsigned int ch,
                                         double       range );

bool keithley2600a_get_measure_autorangev( unsigned int ch );
bool keithley2600a_set_measure_autorangev( unsigned int ch,
                                           bool         autorange );

bool keithley2600a_get_measure_autorangei( unsigned int ch );
bool keithley2600a_set_measure_autorangei( unsigned int ch,
                                           bool         autorange );

double keithley2600a_get_measure_lowrangev( unsigned int ch );
double keithley2600a_set_measure_lowrangev( unsigned int ch,
                                            double       lowrange );
double keithley2600a_get_measure_lowrangei( unsigned int ch );
double keithley2600a_set_measure_lowrangei( unsigned int ch,
                                            double       lowrange );

int keithley2600a_get_measure_autozero( unsigned int ch );
int keithley2600a_set_measure_autozero( unsigned int ch,
                                        int          autozero );

double keithley2600a_measure( unsigned int ch,
							  int          what );
double const * keithley2600a_measure_iv( unsigned int ch );

double keithley2600a_get_measure_time( unsigned int ch );
double keithley2600a_set_measure_time( unsigned int ch,
                                       double       t );

int keithley2600a_get_measure_count( unsigned int ch );
int keithley2600a_set_measure_count( unsigned int ch,
                                     int          count );

double keithley2600a_get_measure_rel_levelv( unsigned int ch );
double keithley2600a_set_measure_rel_levelv( unsigned int ch,
                                             double       offset );

double keithley2600a_get_measure_rel_leveli( unsigned int ch );
double keithley2600a_set_measure_rel_leveli( unsigned int ch,
                                             double       offset );

bool keithley2600a_get_measure_rel_levelv_enabled( unsigned int ch );
bool keithley2600a_set_measure_rel_levelv_enabled( unsigned int ch,
                                                   bool         on_off );

bool keithley2600a_get_measure_rel_leveli_enabled( unsigned int ch );
bool keithley2600a_set_measure_rel_leveli_enabled( unsigned int ch,
                                                   bool         on_off );

double keithley2600a_get_measure_delay( unsigned int ch );
double keithley2600a_set_measure_delay( unsigned int ch,
                                        double       delay );

int keithley2600a_get_measure_filter_type( unsigned int ch );
int keithley2600a_set_measure_filter_type( unsigned int ch,
                                           int          type );

int keithley2600a_get_measure_filter_count( unsigned int ch );
int keithley2600a_set_measure_filter_count( unsigned int ch,
                                            int          count );

bool keithley2600a_get_measure_filter_enabled( unsigned int ch );
bool keithley2600a_set_measure_filter_enabled( unsigned int ch,
                                               bool         on_off );

double * keithley2600a_sweep_and_measure( unsigned int ch,
                                         int           sweep_what,
                                          int          measure_what,
                                          double       start,
                                          double       end,
                                          int          num_points );
double * keithley2600a_list_sweep_and_measure( unsigned int  ch,
                                               int           sweep_what,
                                               int           measure_what,
                                               const Var_T * list );

bool keithley2600a_contact_check( unsigned int ch );
const double * keithley2600a_contact_resistance( unsigned int ch );

double keithley2600a_get_contact_threshold( unsigned int ch );
double keithley2600a_set_contact_threshold( unsigned int ch,
                                            double       threshold );

int keithley2600a_get_contact_speed( unsigned int ch );
int keithley2600a_set_contact_speed( unsigned int ch,
                                     int          speed );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
