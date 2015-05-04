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


static bool line_to_bool( const char * line );
static int line_to_int( const char * line );
static double line_to_double( const char * line );
static bool command( const char * cmd );
static bool talk( const char * cmd,
                  char       * reply,
                  size_t       length );
static void comm_failure( void );


static const char *smu[ ] = { "smua", "smub" };



/*---------------------------------------------------------------*
 * Returns the sense mode of the channel
 *---------------------------------------------------------------*/

void
keithley2600a_get_state( void )
{
    unsigned int ch;

    for ( ch = 0; ch < NUM_CHANNELS; ch++ )
    {
        keithley2600a_get_sense( ch );
        
        keithley2600a_get_source_offmode( ch );
        keithley2600a_get_source_output( ch );
        keithley2600a_get_source_highc( ch );
        keithley2600a_get_source_func( ch );
        keithley2600a_get_measure_autorangev( ch );
        keithley2600a_get_measure_autorangei( ch );
        keithley2600a_get_measure_autozero( ch );
        keithley2600a_get_source_levelv( ch );
        keithley2600a_get_source_leveli( ch );
    }
}


/*---------------------------------------------------------------*
 * Returns the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_sense( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.sense)", smu[ ch ] );
	talk( buf, buf, sizeof buf );

    k26->sense[ ch ] = line_to_bool( buf );
    if (    k26->sense[ ch ] != SENSE_LOCAL
		 || k26->sense[ ch ] != SENSE_REMOTE
		 || k26->sense[ ch ] != SENSE_CALA )
        comm_failure( );

    return k26->sense[ ch ];
}


/*---------------------------------------------------------------*
 * Sets the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_set_sense( unsigned int ch,
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
	command( buf );

    return k26->sense[ ch ] = sense;
}


/*---------------------------------------------------------------*
 * Returns the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_source_offmode( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.offmode)", smu[ ch ] );
	talk( buf, buf, sizeof buf );

    return k26->source.offmode[ ch ] = line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Sets the output-off mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_set_source_offmode( unsigned int ch,
								  int          source_offmode )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.offmode=%d", smu[ ch ], source_offmode );
	command( buf );

    return k26->source.offmode[ ch ] = source_offmode;
}


/*---------------------------------------------------------------*
 * Returns if output of the channel is switched on
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_output( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.output)", smu[ ch ] );
	talk( buf, buf, sizeof buf );

    return k26->source.output[ ch ] = line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches output of the channel on or off
 *---------------------------------------------------------------*/

bool
keithley2600a_set_source_output( unsigned int ch,
                                 bool         source_output )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.output=%d", smu[ ch ], ( int ) source_output );
	command( buf );

	/* ZZZZZZZZZZZ  Get any comliance levels changes when switching off? */

    return k26->source.output[ ch ] = source_output;
}


/*---------------------------------------------------------------*
 * Returns if channel is in high-capacity mode
 *---------------------------------------------------------------*/

bool
keithley2600a_get_source_highc( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.highc)", smu[ ch ] );
	talk( buf, buf, sizeof buf );

    return k26->source.highc[ ch ] = line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches high-capacity mode of the channel on or off (switching
 * it on also modifies a number of other settings!)
 *---------------------------------------------------------------*/

int
keithley2600a_set_source_highc( unsigned int ch,
                                bool         source_highc )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "%s.source.highc=%d", smu[ ch ], ( int ) source_highc );
	command( buf );

    /* Switching to high-capacity mode also changes a few other settings */

    if ( source_highc )
    {
//        keithley2600a_get_source_limiti( ch );
        if ( k26->source.limiti[ ch ] < 1.0e-6 )
            k26->source.limiti[ ch ] = 1.0e-6;

        if ( k26->source.rangei[ ch ] < 1.e-6 )
            k26->source.rangei[ ch ] = 1.e-6;

        if ( k26->source.lowrangei[ ch ] < 1.e-6 )
            k26->source.lowrangei[ ch ] = 1.e-6;

        if ( k26->measure.lowrangei[ ch ] < 1.e-6 )
            k26->measure.lowrangei[ ch ] = 1.e-6;
    }

    return k26->source.highc[ ch ] = source_highc;
}


/*---------------------------------------------------------------*
 * Returns if the channel is set up as voltage of current source
 *---------------------------------------------------------------*/

int
keithley2600a_get_source_func( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "print(%s.source.func)", smu[ ch ] );
	talk( buf, buf, sizeof buf );

    return k26->source.func[ ch ] = line_to_bool( buf );
}


/*---------------------------------------------------------------*
 * Switches the channel between voltage of current source
 *---------------------------------------------------------------*/

int
keithley2600a_set_source_func( unsigned int ch,
                               int          source_func )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );
    fsc2_assert(    source_func == OUTPUT_DCAMPS
                 || source_func == OUTPUT_DCVOLTS );

    sprintf( buf, "%s.source.fun=%d", smu[ ch ], source_func );
	command( buf );

    return k26->source.func[ ch ] = source_func;
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
	talk( buf, buf, sizeof buf );

    return k26->measure.autorangev[ ch ] = line_to_bool( buf );
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
	command( buf );

    return k26->measure.autorangev[ ch ] = autorange;
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
	talk( buf, buf, sizeof buf );

    return k26->measure.autorangei[ ch ] = line_to_bool( buf );
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
	command( buf );

    return k26->measure.autorangei[ ch ] = autorange;
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
	talk( buf, buf, sizeof buf );

    autozero = line_to_int( buf );
    if ( autozero < AUTOZERO_OFF || autozero > AUTOZERO_AUTO )
        comm_failure( );

    return k26->measure.autozero[ ch ] = autozero;
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
	command( buf );

    return k26->measure.autozero[ ch ] = autozero;
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
	talk( buf, buf, sizeof buf );
    if ( strcmp( buf, "false\n" ) && strcmp( buf, "true\n" ) )
        comm_failure( );

    return buf[ 0 ] == 't';
}


/*---------------------------------------------------------------*
 * Returns the source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_levelv( unsigned int ch )
{
	char buf[ 50 ];
	double volts;

	fsc2_assert( ch < NUM_CHANNELS );

	sprintf( buf, "print(%s.source.levelv)", smu[ ch ] );
	talk( buf, buf, sizeof buf );

	volts = line_to_double( buf );
	if ( fabs( volts ) > 1.001 * MAX_SOURCE_LEVELV )
		comm_failure( );

	return k26->source.levelv[ ch ] = volts;
}


/*---------------------------------------------------------------*
 * Sets a source voltage for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_levelv( unsigned int ch,
								 double       volts )
{
	char buf[ 50 ];

	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( volts <= MAX_SOURCE_LEVELV && - MAX_SOURCE_LEVELV );

	sprintf( buf, "%s.source.levelv=%.5g\n", smu[ ch ], volts );
	command( buf );

	return keithley2600a_get_source_levelv( ch );
}


/*---------------------------------------------------------------*
 * Returns the source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_get_source_leveli( unsigned int ch )
{
	char buf[ 50 ];
	double amps;

	fsc2_assert( ch < NUM_CHANNELS );

	sprintf( buf, "print(%s.source.leveli)", smu[ ch ] );
	talk( buf, buf, sizeof sizeof buf );

	amps = line_to_double( buf );
	if ( fabs( amps ) > 1.001 * MAX_SOURCE_LEVELI )
		comm_failure( );

	return k26->source.leveli[ ch ] = amps;
}


/*---------------------------------------------------------------*
 * Sets a source current for the channel
 *---------------------------------------------------------------*/

double
keithley2600a_set_source_leveli( unsigned int ch,
								 double       amps )
{
	char buf[ 50 ];

	fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert( amps <= MAX_SOURCE_LEVELI && amps <= - MAX_SOURCE_LEVELI );

	sprintf( buf, "%s.source.leveli=%.5g\n", smu[ ch ], amps );
	command( buf );

	return keithley2600a_get_source_leveli( ch );
}


/*--------------------------------------------------------------*
 * Converts a line as received from the device to a boolean value
 *--------------------------------------------------------------*/

static
bool
line_to_bool( const char * line )
{
    bool res = *line == '1';

    if ( ( *line != '0' && *line != '1' ) || *++line != '.' )
        comm_failure( );

    while ( *++line == '0' )
        /* empty */ ;
    if ( strcmp( line, "e+00\n" ) )
        comm_failure( );

    return res;
}


/*--------------------------------------------------------------*
 * Converts a line as received from the device to an int
 *--------------------------------------------------------------*/

static
int
line_to_int( const char * line )
{
    double dres;
    int res;
    char *ep;

    if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
        comm_failure( );

    dres = strtod( line, &ep );
    if ( *ep != '\n' || *++ep || dres > INT_MAX || dres < INT_MIN )
        comm_failure( );

    res = lrnd( dres );
    if ( dres - res != 0 )
        comm_failure( );

    return res;
}


/*--------------------------------------------------------------*
 * Converts a line as received from the device to a double
 *--------------------------------------------------------------*/

static
double
line_to_double( const char * line )
{
    double res;
    char *ep;

    if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
        comm_failure( );

    errno = 0;
    res = strtod( line, &ep );
    if (    *ep != '\n'
         || *++ep
         || ( ( res == HUGE_VAL || res == - HUGE_VAL ) && errno == ERANGE ) )
        comm_failure( );

    return res;
}


/*--------------------------------------------------------------*
 * Sends a command to the device
 *--------------------------------------------------------------*/

static
bool
command( const char * cmd )
{
	size_t len = strlen( cmd );

    
	if ( vxi11_write( cmd, &len, UNSET ) != SUCCESS )
		comm_failure( );

	return OK;
}


/*--------------------------------------------------------------*
 * Sends a command to the device and returns its reply. The
 * value of length' must not be larger than the reply buffer.
 * Note that only one less character will be read since the
 * last character is reserved for the training '\0' which is
 * appended automatically.
 *--------------------------------------------------------------*/

static
bool
talk( const char * cmd,
      char       * reply,
      size_t       length )
{
    fsc2_assert( length > 1 );

	size_t len = strlen( cmd );

	if ( vxi11_write( cmd, &len, UNSET ) != SUCCESS )
		comm_failure( );

    length--;
	if ( vxi11_read( reply, &length, UNSET ) != SUCCESS || length < 1 )
		comm_failure( );

    reply[ length ] = '\0';
	return OK;
}


/*--------------------------------------------------------------*
 * Opens connection to the device and sets iy up for further work
 *--------------------------------------------------------------*/

bool
keithley2600a_open( void )
{
    /* Never try to open the device more than once */

    if ( k26->is_open )
        return SUCCESS;

    /* Try to open an connection to the device */

	if ( vxi11_open( DEVICE_NAME, NETWORK_ADDRESS, VXI11_NAME,
                     UNSET, OPEN_TIMEOUT ) == FAILURE )
        return FAIL;

    k26->is_open = SET;
    fprintf( stderr, "Open succeeded.\n" );

    /* Set a timeout for reads and writes, clear the device and lock
       out the keyboard. */

    if (    vxi11_set_timeout( READ, READ_TIMEOUT ) == FAILURE
         || vxi11_set_timeout( WRITE, WRITE_TIMEOUT ) == FAILURE
         || vxi11_device_clear( ) == FAILURE
         || vxi11_lock_out( SET ) == FAILURE )
    {
        vxi11_close( );
        return FAIL;
    }

    fprintf( stderr, "Set-up succeeded.\n" );

    return OK;
}


/*--------------------------------------------------------------*
 * Closes connection to the device
 *--------------------------------------------------------------*/

bool
keithley2600a_close( void )
{
    if ( ! k26->is_open )
        return OK;

    /* Unlock the keyboard */

    vxi11_lock_out( UNSET );

    /* Close connection to the device */

    if ( vxi11_close( ) == FAILURE )
        return FAIL;

    fprintf( stderr, "Close succeeded.\n" );

    k26->is_open = UNSET;
    return OK;
}


/*--------------------------------------------------------------*
 * Called whenever something hishy happens during communication
 * (including the device sending unexpected data)
 *--------------------------------------------------------------*/

static
void
comm_failure( void )
{
    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
