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

int keithley2600a_get_measure_autozero( unsigned int ch );
int keithley2600a_set_measure_autozero( unsigned int ch,
                                        int          autozero );

double keithley2600a_get_measure_lowrangev( unsigned int ch );
double keithley2600a_set_measure_lowrangev( unsigned int ch,
                                            double       lowrange );
double keithley2600a_get_measure_lowrangei( unsigned int ch );
double keithley2600a_set_measure_lowrangei( unsigned int ch,
                                            double       lowrange );

double keithley2600a_measure( unsigned int ch,
							  int          what );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
