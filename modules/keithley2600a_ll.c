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


static void clear_errors( void );
static void comm_failure( void );


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
                     false, false, OPEN_TIMEOUT ) == FAILURE )
        return FAIL;

    k26->is_open = true;

    /* Set a timeout for reads and writes, clear the device and lock
       out the keyboard. */

    if (    vxi11_set_timeout( READ, READ_TIMEOUT ) == FAILURE
         || vxi11_set_timeout( WRITE, WRITE_TIMEOUT ) == FAILURE
         || vxi11_device_clear( ) == FAILURE
         || vxi11_lock_out( true ) == FAILURE )
    {
        vxi11_close( );
        return FAIL;
    }

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

    vxi11_lock_out( false );

    /* Close connection to the device */

    if ( vxi11_close( ) == FAILURE )
        return FAIL;

    k26->is_open = false;
    return OK;
}


/*--------------------------------------------------------------*
 * Sends a command to the device
 *--------------------------------------------------------------*/

bool
keithley2600a_cmd( const char * cmd )
{
	size_t len = strlen( cmd );

    
	if ( vxi11_write( cmd, &len, false ) != SUCCESS )
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

bool
keithley2600a_talk( const char * cmd,
                    char       * reply,
                    size_t       length )
{
    fsc2_assert( length > 1 );

    keithley2600a_cmd( cmd );

    length--;
	if ( vxi11_read( reply, &length, false ) != SUCCESS || length < 1 )
		comm_failure( );

    reply[ length ] = '\0';
	return OK;
}


/*---------------------------------------------------------------*
 * Reads in the complete state of the device
 *---------------------------------------------------------------*/

void
keithley2600a_get_state( void )
{
    unsigned int ch;

    clear_errors( );

    for ( ch = 0; ch < NUM_CHANNELS; ch++ )
    {
        keithley2600a_get_sense( ch );
        
        keithley2600a_get_source_output( ch );
        keithley2600a_get_source_highc( ch );
        keithley2600a_get_source_offmode( ch );
        keithley2600a_get_source_func( ch );
        keithley2600a_get_source_levelv( ch );
        keithley2600a_get_source_leveli( ch );
        keithley2600a_get_source_rangev( ch );
        keithley2600a_get_source_rangei( ch );
        keithley2600a_get_source_autorangev( ch );
        keithley2600a_get_source_autorangei( ch );
        keithley2600a_get_source_lowrangev( ch );
        keithley2600a_get_source_lowrangei( ch );
        keithley2600a_get_source_delay( ch );
        keithley2600a_get_source_offlimiti( ch );
//        keithley2600a_get_source_settling( ch );
        keithley2600a_get_source_sink( ch );

        keithley2600a_get_measure_autorangev( ch );
        keithley2600a_get_measure_autorangei( ch );
        keithley2600a_get_measure_autozero( ch );
    }
}


/*---------------------------------------------------------------*
 * Resets the device
 *---------------------------------------------------------------*/

void
keithley2600a_reset( void )
{
    keithley2600a_cmd( "reset()" );
    keithley2600a_get_state( );
}


/*---------------------------------------------------------------*
 * Resets the device
 *---------------------------------------------------------------*/

static
void
clear_errors( void )
{
    keithley2600a_cmd( "errorqueue.clear()" );
}


/*--------------------------------------------------------------*
 * Converts a line as received from the device to a boolean value
 *--------------------------------------------------------------*/

bool
keithley2600a_line_to_bool( const char * line )
{
    bool res = *line == '1';

    if ( ( *line != '0' && *line != '1' ) || *++line != '.' )
        keithley2600a_bad_data( );

    while ( *++line == '0' )
        /* empty */ ;
    if ( strcmp( line, "e+00\n" ) )
        keithley2600a_bad_data( );

    return res;
}


/*--------------------------------------------------------------*
 * Converts a line as received from the device to an int
 *--------------------------------------------------------------*/

int
keithley2600a_line_to_int( const char * line )
{
    double dres;
    int res;
    char *ep;

    if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
        keithley2600a_bad_data( );

    dres = strtod( line, &ep );
    if ( *ep != '\n' || *++ep || dres > INT_MAX || dres < INT_MIN )
        keithley2600a_bad_data( );

    res = lrnd( dres );
    if ( dres - res != 0 )
        keithley2600a_bad_data( );

    return res;
}


/*--------------------------------------------------------------*
 * Converts a line as received from the device to a double
 *--------------------------------------------------------------*/


double
keithley2600a_line_to_double( const char * line )
{
    double res;
    char *ep;

    if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
        keithley2600a_bad_data( );

    errno = 0;
    res = strtod( line, &ep );
    if (    *ep != '\n'
         || *++ep
         || ( ( res == HUGE_VAL || res == - HUGE_VAL ) && errno == ERANGE ) )
        keithley2600a_bad_data( );

    return res;
}


/*--------------------------------------------------------------*
 * Checks for errors reported by the device and if there are any
 * fetches the corresponding messages and throws an exception after
 * printing out the error messages.
 *--------------------------------------------------------------*/

void
keithley2600a_show_errors( void )
{
    char buf[ 200 ];
    int error_count;
    char * mess = NULL;

    keithley2600a_talk( "print(errorqueue.count)", buf, sizeof buf );
    if ( ( error_count = keithley2600a_line_to_int( buf ) ) < 0 )
        keithley2600a_bad_data( );
    else if ( error_count == 0 )
        return;

    mess = strdup( "Device reports the following errors:\n" );

    TRY
    {
        while ( error_count-- > 0 )
        {
            keithley2600a_talk( "code,emess,esev,enode=errorqueue.next()\n"
                                "print(emess)", buf, sizeof buf );
            mess = T_realloc( mess, strlen( mess ) + strlen( buf ) + 1 );
            strcat( mess, buf );
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( mess );
        RETHROW;
    }

    print( FATAL, mess );
    T_free( mess );
    THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 * Called whenever the device sends unexpected data
 *--------------------------------------------------------------*/

void
keithley2600a_bad_data( void )
{
    print( FATAL, "Devive sends unexpected data.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 * Called whenever communication fails
 *--------------------------------------------------------------*/

static
void
comm_failure( void )
{
    print( FATAL, "Communication with device failed.\n" );
    k26->comm_failed = true;
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
