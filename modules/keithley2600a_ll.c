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


extern Max_Limit * keithley2600s_max_limitv;
extern Max_Limit * keithley2600s_max_limiti;

static size_t keithley2600a_command( const char * cmd );
static size_t keithley2600a_talk( const char * cmd,
								  char       * reply,
								  size_t       max_len,
								  bool         is_exact_len );
static void keithley2600a_failure( void );


static const char *smu[ ] = { "smua", "smub" };


Keithley2600A_T keithley2600a;


/*---------------------------------------------------------------*
 * Returns the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithely2600a_get_sense( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.sense)", smu[ ch ] );
	keithley2600a_talk( buf, buf, 2, SET );
    if (    (    buf[ 0 ] - '0' != SENSE_LOCAL
			  && buf[ 0 ] - '0' != SENSE_REMOTE
			  && buf[ 0 ] - '0' != SENSE_CALA )
		 || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.sense[ cha ] = buf[ 0 ] - '0';
}


/*---------------------------------------------------------------*
 * Sets the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithely2600a_set_sense( unsigned int ch,
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
	keithley2600a_command( buf );

    return keithley2600a.sense[ ch ] = sense;
}


/*---------------------------------------------------------------*
 * Returns the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithely2600a_get_source_offmode( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.offmode)", smu[ ch ] );
	keithley2600a_talk( buf, buf, 2, SET );
    if (    ( buf[ 0 ] - '0' < OUTPUT_NORMAL || buf[ 0 ] - '0' > OUTPUT_ZERO )
		 || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.source.offmode[ cha ] = buf[ 0 ] - '0';
}


/*---------------------------------------------------------------*
 * Sets the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithely2600a_set_source_offmode( unsigned int ch,
								  int          souarce_offmode )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.offmode=%d", smu[ ch ], source_offmode );
	keithley2600a_command( buf );

    return keithley2600a.source.output[ ch ] = source_output;
}


/*---------------------------------------------------------------*
 * Returns if output of the channel is switched on
 *---------------------------------------------------------------*/

bool
keithely2600a_get_source_output( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.output)", smu[ ch ] );
	keithley2600a_talk( buf, buf, 2, SET );
    if ( ( buf[ 0 ] != '0' && buf[ 0 ] != '1' ) || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.source.output[ cha ] = buf[ 0 ] == '1';
}


/*---------------------------------------------------------------*
 * Switches output of the channel on or off
 *---------------------------------------------------------------*/

bool
keithely2600a_set_source_output( unsigned int ch,
                                 bool         source_output )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.output=%d", smu[ ch ], source_output );
	keithley2600a_command( buf );

	/* ZZZZZZZZZZZ  Get any comliance levels changes when switching off? */

    return keithley2600a.source.output[ ch ] = source_output;
}


/*---------------------------------------------------------------*
 * Returns if channel is in high-capacity mode
 *---------------------------------------------------------------*/

bool
keithely2600a_get_source_highc( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.highc)", smu[ ch ] );
	keithley2600a_talk( buf, buf, 2, SET );
    if ( ( buf[ 0 ] != '0' && buf[ 0 ] != '1' ) || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.source.highc[ ch ] = buf[ 0 ] - '0';
}


/*---------------------------------------------------------------*
 * Switches high-capacity mode of the channel on or off (switching
 * it on also modifies a number of other settings!)
 *---------------------------------------------------------------*/

int
keithely2600a_set_source_highc( unsigned int ch,
                                bool         source_highc )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.highc=%d", smu[ ch ], source_highc );
	keithley2600a_command( buf );

    /* Switching to high-capacity mode also changes a few other settings */

    if ( source_highc )
    {
        if ( keithley2600a.source.limiti[ ch ] < 1.e-6 )
            keithley2600a.source.limiti[ ch ] = 1.e-6;

        if ( keithley2600a.source.rangei[ ch ] < 1.e-6 )
            keithley2600a.source.rangei[ ch ] = 1.e-6;

        if ( keithley2600a.source.lowrangei[ ch ] < 1.e-6 )
            keithley2600a.source.lowrangei[ ch ] = 1.e-6;

        if ( keithley2600a.measure.lowrangei[ ch ] < 1.e-6 )
            keithley2600a.measure.lowrangei[ ch ] = 1.e-6;
    }

    return keithley2600a.source.highc[ ch ] = source_highc;
}


/*---------------------------------------------------------------*
 * Returns if the channel is set up as voltage of current source
 *---------------------------------------------------------------*/

int
keithely2600a_get_source_func( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.func)", smu[ ch ] );
	keithley2600a_talk( buf, buf, 2, SET );
    if ( ( buf[ 0 ] != '0' && buf[ 0 ] != '1' ) || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.source.func[ ch ] = buf[ 0 ] - '0';
}


/*---------------------------------------------------------------*
 * Switches the channel between voltage of current source
 *---------------------------------------------------------------*/

int
keithely2600a_set_source_func( unsigned int ch,
                               int          source_func )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    source_func == OUTPUT_DCAMPS
                 || source_func == OUTPUT_DCVOLTS );

    sprintf( buf, "%s.source.fun=%d", smu[ ch ], source_func );
	keithley2600a_command( buf );
    return keithley2600a.source.func[ ch ] = source_func;
}


/*---------------------------------------------------------------*
 * Returns if autorange for voltage measurements with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithely2600a_get_measure_autorangev( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autorangev)", smu[ ch ] );
	keithley2600a_command( buf );
    if ( ( buf[ 0 ] != '0' && buf[ 0 ] != '1' ) || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.measure.autorangev[ ch ] = buf[ 0 ] == '1';
}


/*---------------------------------------------------------------*
 * Switches autorange for voltage measurements with the channel
 * on or off
 *---------------------------------------------------------------*/

bool
keithely2600a_set_measure_autorangev( unsigned int ch,
                                      bool         autorange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.autorangev=%s", smu[ ch ], autorange );
	keithley2600a_command( buf );
    return keithley2600a.measure.autorangev[ ch ] = autorange;
}


/*---------------------------------------------------------------*
 * Returns if autorange for current measurements with the channel
 * is on or off
 *---------------------------------------------------------------*/

bool
keithely2600a_get_measure_autorangei( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autorangei)", smu[ ch ] );
	keithley2600a_command( buf );
    if ( ( buf[ 0 ] != '0' && buf[ 1 ] != '1' ) || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.measure.autorangei[ ch ] = buf[ 0 ] == '1';
}


/*---------------------------------------------------------------*
 * Switches autorange for current measurements with the channel
 * on or off
 *---------------------------------------------------------------*/

bool
keithely2600a_set_measure_autorangei( unsigned int ch,
                                      bool         autorange )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.measure.autorangei=%d", smu[ ch ], autorange );
	keithley2600a_command( buf );
    return keithley2600a.measure.autorangei[ ch ] = autorange;
}


/*---------------------------------------------------------------*
 * Returns if autozero for measurements with the channel
 * is off (0), once (1) or auto (2)
 *---------------------------------------------------------------*/

int
keithely2600a_get_measure_autozero( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.measure.autozero)", smu[ ch ] );
	keithley2600a_command( buf );
    if (    ( buf[ 0 ] - '0' < AUTOZRO_OFF && buf[ 1 ] - '0' > AUTOZERO_AUTO ) )
         || buf[ 1 ] != '\n' )
        keithley2600a_failure( );

    return keithley2600a.measure.autozero[ ch ] = buf[ 0 ] == '1';
}


/*---------------------------------------------------------------*
 * Switches autozero for measurements with the channel to off (0),
 * once (1) or auto (2)
 *---------------------------------------------------------------*/

int
keithely2600a_set_measure_autozero( unsigned int ch,
                                    int          autozero )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert( autozero >= AUTOZERO_OFF && autozero <= AUTOZERO_AUTO )

    sprintf( buf, smu[ ch ] "measure.autozero=%d", autozero );
	keithley2600a_command( buf );
    return keithley2600a.measure.autozero[ ch ] = autozeroa;
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
	keithley2600a_talk( buf, buf, sizeof buf - 1, UNSET );
	buf[ strlen( buf ) ] = '\0';
    if ( strcmp( buf, "false\n" ) && strcmp( buf, "true\n" ) )
        keithley2600a_failure( );

    return buf[ 0 ] == 't';
}


/*---------------------------------------------------------------*
 * Sets a source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_levelv( unsigned int ch,
								 double       volts )
{
	char buf[ 50 ];

	fsc2_assert( ch < MAX_CHANNELS );
	fsc2_assert( volts <= MAX_SOURCE_LEVELV && - MAX_SOURCE_LEVELV );

	sprintf( buf, "%s.source.levelv=%.5g\n", smu[ ch ], volts );
	keithley2600a_command( buf );

	return keithley2600a_get_source_levelv( ch );
}


/*---------------------------------------------------------------*
 * Returns the source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_levelv( unsigned int ch )
{
	char buf[ 50 ];
	char *ep;
	double volts;

	fsc2_assert( ch < MAX_CHANNELS );

	sprintf( buf, "print(%s.source.levelv)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf - 1, UNSET );

	buf[ strlen( buf ) ] = '\0';
	volts = strtod( buf, &ep );
	if ( *ep != '\n' || fabs( volts ) > 1.001 * MAX_SOURCE_LEVELV )
		keitley2600a_failure( );

	return keithley2600a.sourve.levelv[ c ] = volts;
}


/*---------------------------------------------------------------*
 * Sets a source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_leveli( unsigned int ch,
								 double       amps )
{
	char buf[ 50 ];

	fsc2_assert( ch < MAX_CHANNELS );
	fsc2_assert( volts <= MAX_SOURCE_LEVELI && - MAX_SOURCE_LEVELI );

	sprintf( buf, "%s.source.leveli=%.5g\n", smu[ ch ], amps );
	keithley2600a_command( buf );

	return keithley2600a_get_source_leveli( ch );
}


/*---------------------------------------------------------------*
 * Returns the source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_leveli( unsigned int ch )
{
	char buf[ 50 ];
	char *ep;
	double amps;

	fsc2_assert( ch < MAX_CHANNELS );

	sprintf( buf, "print(%s.source.leveli)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf - 1, UNSET );

	buf[ strlen( buf ) ] = '\0';
	amps = strtod( buf, &ep );
	if ( *ep != '\n' || fabs( amps ) > 1.001 * MAX_SOURCE_LEVELI )
		keitley2600a_failure( );

	return keithley2600a.sourve.leveli[ c ] = v;
}


/*---------------------------------------------------------------*
 * Sends a command to the device, not expecting a reply
 *---------------------------------------------------------------*/

static
size_t
keithley2600a_command( const char * cmd )
{
	return strlen( cmd );
}


/*---------------------------------------------------------------*
 * Sends a command to the device, expecting a reply with, if
 * 'is_exact_len' set, 'max_len' characters, otherwise up to
 * 'max_len' characters.
 *---------------------------------------------------------------*/

static
size_t
keithley2600a_talk( const char * cmd,
					char       * reply,
					size_t       max_len,
					bool         is_exact_len )
{
	return max_len;
}


static
void
keithley2600a_failure( void )
{
    print( FATAL, "Communication with source-meter failed.\n" );
    THROW( EXCEPTION );
}



/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
