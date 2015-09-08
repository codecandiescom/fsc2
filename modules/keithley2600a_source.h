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
#if ! defined KEITHLEY2600A_SOURCE_H_
#define KEITHLEY2600A_SOURCE_H_


int keithley2600a_get_source_offmode( unsigned int ch );
int keithley2600a_set_source_offmode( unsigned int ch,
                                      int          source_offmode );

bool keithley2600a_get_source_output( unsigned int ch );
bool keithley2600a_set_source_output( unsigned int ch,
                                      bool         source_output );

bool keithley2600a_get_source_highc( unsigned int ch );
bool keithley2600a_set_source_highc( unsigned int ch,
                                     bool         source_highc );

bool keithley2600a_get_source_func( unsigned int ch );
bool keithley2600a_set_source_func( unsigned int ch,
                                    bool         source_func );

bool keithley2600a_get_source_autorangev( unsigned int ch );
bool keithley2600a_set_source_autorangev( unsigned int ch,
                                          bool         autorange );

bool keithley2600a_get_source_autorangei( unsigned int ch );
bool keithley2600a_set_source_autorangei( unsigned int ch,
                                          bool         autorange );

double keithley2600a_get_source_rangev( unsigned int ch );
double keithley2600a_set_source_rangev( unsigned int ch,
                                        double       range );

double keithley2600a_get_source_rangei( unsigned int ch );
double keithley2600a_set_source_rangei( unsigned int ch,
                                        double       range );


double keithley2600a_get_source_lowrangev( unsigned int ch );
double keithley2600a_set_source_lowrangev( unsigned int ch,
                                           double       lowrange );

double keithley2600a_get_source_lowrangei( unsigned int ch );
double keithley2600a_set_source_lowrangei( unsigned int ch,
                                           double       lowrange );

double keithley2600a_get_source_levelv( unsigned int ch );
double keithley2600a_set_source_levelv( unsigned int ch,
                                        double       volts );

double keithley2600a_get_source_leveli( unsigned int ch );
double keithley2600a_set_source_leveli( unsigned int ch,
                                        double       amps );

double keithley2600a_get_source_limitv( unsigned int ch );
double keithley2600a_set_source_limitv( unsigned int ch,
                                        double       volts );

double keithley2600a_get_source_limiti( unsigned int ch );
double keithley2600a_set_source_limiti( unsigned int ch,
                                        double       amps );

bool keithley2600a_get_compliance( unsigned int ch );

double keithley2600a_get_source_delay( unsigned int ch );
double keithley2600a_set_source_delay( unsigned int ch,
                                       double       delay );

double keithley2600a_get_source_offlimiti( unsigned int ch );
double keithley2600a_set_source_offlimiti( unsigned int ch,
                                           double       limit );

double keithley2600a_get_source_sink( unsigned int ch );
double keithley2600a_set_source_sink( unsigned int ch,
                                      double       sink );

double keithley2600a_get_source_settling( unsigned int ch );
double keithley2600a_set_source_settling( unsigned int ch,
                                          int          settle );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
