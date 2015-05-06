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
 * Returns the low range voltage setting
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
 * Sets the low range voltage setting
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_lowrangev( unsigned int ch,
									 double       lowrange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_measure_lowrangev( ch, lowrange ) );

    sprintf( buf, "%s.measure.lowrangev=%.5g", smu[ ch ], lowrange );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].lowrangev = lowrange;
}


/*---------------------------------------------------------------*
 * Returns the low range current setting
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

    return k26->measure[ ch ].lowrangei = lowrange;
}


/*---------------------------------------------------------------*
 * Sets the low range current setting
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_lowrangei( unsigned int ch,
									 double       lowrange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_measure_lowrangei( ch, lowrange ) );

    sprintf( buf, "%s.measure.lowrangei=%.5g", smu[ ch ], lowrange );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].lowrangei = lowrange;
}


/*---------------------------------------------------------------*
 * Returns the currently set voltage measurement range
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_rangev( unsigned int ch )
{
    char buf[ 50 ];
	double range;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rangev)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

	range = keithley2600a_line_to_double( buf );
	if ( ! keithley2600a_check_measure_rangev( ch, range ) )
		keithley2600a_bad_data( );

    return k26->measure[ ch ].rangev = range;
}


/*---------------------------------------------------------------*
 * Sets the voltage measurement range
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rangev( unsigned int ch,
								  double       range )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_measure_rangev( ch, range ) );

    sprintf( buf, "%s.measure.rangev=%.5g", smu[ ch ], range );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].rangev = range;
}


/*---------------------------------------------------------------*
 * Returns the currently set current measurement range
 *---------------------------------------------------------------*/

double
keithley2600a_get_measure_rangei( unsigned int ch )
{
    char buf[ 50 ];
	double range;

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.rangei)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf );

	range = keithley2600a_line_to_double( buf );
	if ( ! keithley2600a_check_measure_rangei( ch, range ) )
		keithley2600a_bad_data( );

    return k26->measure[ ch ].rangei = range;
}


/*---------------------------------------------------------------*
 * Sets the current measurement range
 *---------------------------------------------------------------*/

double
keithley2600a_set_measure_rangei( unsigned int ch,
								  double       range )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( keithley2600a_check_measure_rangei( ch, range ) );

    sprintf( buf, "%s.measure.rangei=%.5g", smu[ ch ], range );
	keithley2600a_cmd( buf );

    return k26->measure[ ch ].rangei = range;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
