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


static const char *smu[ ] = { "smua", "smub" };


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

    /* Clean up: remove functions we may have created */

    keithley2600a_cmd( "fsc2_list = nil fsc2_lin = nil" );

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
		keithley2600a_comm_failure( );

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
                    size_t       length,
                    bool         allow_abort )
{
    fsc2_assert( length > 1 );

    keithley2600a_cmd( cmd );

    length--;
    if ( vxi11_read( reply, &length, allow_abort ) != SUCCESS || length < 1 )
        keithley2600a_comm_failure( );

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
    const char * model;

    clear_errors( );

    /* Make sure all data will be sent in ASCII and with a prescision of 6 */

    keithley2600a_cmd( "format.data = format.ASCII" );
    keithley2600a_cmd( "format.asciiprecision = 6" );

    keithley2600a_get_line_frequency( );

    model = keithley2600a_get_model( );

#if defined _2601A
    if ( strcmp( model, "2601A\n" ) )
        print( WARN, "Module was compiled for a 2601A but device reports it's "
               "a %s.\n", model );
#elif defined _2602A
    if ( strcmp( model, "2602A\n" ) )
        print( WARN, "Module was compiled for a 2602A but device reports it's "
               "a %s.\n", model );
#elif defined _2611A
    if ( strcmp( model, "2611A\n" ) )
        print( WARN, "Module was compiled for a 2611A but device reports it's "
               "a %s.\n", model );
#elif defined _2612A
    if ( strcmp( model, "2612A\n" ) )
        print( WARN, "Module was compiled for a 2612A but device reports it's "
               "a %s.\n", model );
#elif defined _2635A
    if ( strcmp( model, "2612A\n" ) )
        print( WARN, "Module was compiled for a 2635A but device reports it's "
               "a %s.\n", model );
#elif defined _2636A
    if ( strcmp( model, "2636A\n" ) )
        print( WARN, "Module was compiled for a 2636A but device reports it's "
               "a %s.\n", model );
#endif

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

        keithley2600a_get_source_settling( ch );

        keithley2600a_get_source_sink( ch );

        keithley2600a_get_measure_autorangev( ch );
        keithley2600a_get_measure_autorangei( ch );

        keithley2600a_get_measure_autozero( ch );

        /* Sefault to not doing single measurements */

        if ( keithley2600a_get_measure_count( ch ) != 1 )
            keithley2600a_set_measure_count( ch, 1 );

        keithley2600a_get_measure_time( ch );
        keithley2600a_get_measure_delay( ch );

        keithley2600a_get_measure_rel_levelv( ch );
        keithley2600a_get_measure_rel_levelv_enabled( ch );

        keithley2600a_get_measure_rel_leveli( ch );
        keithley2600a_get_measure_rel_leveli_enabled( ch );

        keithley2600a_get_measure_filter_type( ch );
        keithley2600a_get_measure_filter_count( ch );
        keithley2600a_get_measure_filter_enabled( ch );

        keithley2600a_get_contact_threshold( ch );
        keithley2600a_get_contact_speed( ch );
    }

    k26->lin_sweeps_prepared  = false;
    k26->list_sweeps_prepared = false;
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
 * Returns the model
 *---------------------------------------------------------------*/

const char *
keithley2600a_get_model( void )
{
    static char buf[ 50 ];

    keithley2600a_talk( "print(localnode.model)", buf, sizeof buf,
                        false );
    return buf;
}


/*---------------------------------------------------------------*
 * Resets the error queue
 *---------------------------------------------------------------*/

static
void
clear_errors( void )
{
    keithley2600a_cmd( "errorqueue.clear()" );
}


/*---------------------------------------------------------------*
 * Returns the sense mode of the channel
 *---------------------------------------------------------------*/

int
keithley2600a_get_sense( unsigned int ch )
{
    char buf[ 50 ];

    fsc2_assert( ch < NUM_CHANNELS );

    sprintf( buf, "printnumber(%s.sense)", smu[ ch ] );
	keithley2600a_talk( buf, buf, sizeof buf, false );

    k26->sense[ ch ] = keithley2600a_line_to_int( buf );
    if (    k26->sense[ ch ] != SENSE_LOCAL
		 && k26->sense[ ch ] != SENSE_REMOTE
		 && k26->sense[ ch ] != SENSE_CALA )
        keithley2600a_bad_data( );

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
	keithley2600a_cmd( buf );

    return k26->sense[ ch ] = sense;
}


/*--------------------------------------------------------------*
 * Requests the frequency of the power line
 *--------------------------------------------------------------*/

double
keithley2600a_get_line_frequency( void )
{
    char buf[ 50 ] = "printnumber(localnode.linefreq)";

	keithley2600a_talk( buf, buf, sizeof buf, false );

    k26->linefreq = keithley2600a_line_to_double( buf );

    if ( k26->linefreq != 50 && k26->linefreq != 60 )
        keithley2600a_bad_data( );

    return k26->linefreq;
}


/*--------------------------------------------------------------*
 * Prepares and sends LUA functions to the device for doing linear
 * sweeps. Note that functions need to be relatively short (it's
 * not documented how long they may be but it looks like 1024
 * bytes is the limit) since otherwise the input buffer of the
 * device over-runs. No line-feeds are allowed at the ends of the
 * lines of the funtions!
 *--------------------------------------------------------------*/

void
keithley2600a_prep_lin_sweeps( void )
{
    /* Create an empty LUA table, we're going to put all functions needed
       for linear sweeps into it */

    const char * cmd = "fsc2_lin = { }";
    keithley2600a_cmd( cmd );

    /* Add LUA function for doing the linear sweep - it first initializes
       the source settings, the allocates memory for the results, sets up
       the trigger system amd runs the sweep, then prints out the results
       and resets the source settings */

    cmd =
"fsc2_lin.sweep_and_measure = function (ch, sweep, meas, startl, endl, cnt)"
"  local f, ar, r"
"  if sweep == 'v' then f, ar, r = fsc2_lin.prep_sweepv(ch, startl, endl, cnt)"
"  else                 f, ar, r = fsc2_lin.prep_sweepi(ch, startl, endl, cnt)"
"  end"
"  local mbuf1 = ch.makebuffer(cnt)"
"  local mbuf2 = meas == 'iv' and ch.makebuffer(cnt) or nil"
"  fsc2_lin.run_sweep(ch, meas, cnt, mbuf1, mbuf2)"
"  if meas ~= 'iv' then printbuffer(1, cnt, mbuf1)"
"  else                 printbuffer(1, cnt, mbuf1, mbuf2)"
"  end"
"  ch.source.output = ch.DISABLE "
"  ch.source.func = f"
"  if sweep == 'v' then"
"    ch.source.rangev = r"
"    ch.source.autorangev = ar"
"  else"
"    ch.source.rangei = r"
"    ch.source.autorangei = ar"
"  end "
"end";
    keithley2600a_cmd( cmd );

    /* LUA Function for preparing a linear voltage sweep */

    cmd = 
"fsc2_lin.prep_sweepv = function (ch, startl, endl, cnt)"
"  ch.source.output = ch.DISABLE"
"  local f, ar, r = ch.source.func, ch.source.autorangev, ch.source.rangev"
"  ch.source.rangev = math.max(math.abs(startl), math.abs(endl))"
"  ch.trigger.source.limiti = ch.source.limiti"
"  ch.source.func = ch.OUTPUT_DCVOLTS"
"  ch.trigger.source.linearv(startl, endl, cnt)"
"  return f, ar, r "
"end";
    keithley2600a_cmd( cmd );

    /* LUA Function for preparing a linear current sweep */

    cmd =
"fsc2_lin.prep_sweepi = function (ch, startl, endl, cnt)"
"  ch.source.output = ch.DISABLE"
"  local f, ar, r = ch.source.func, ch.source.autorangei, ch.source.rangei"
"  ch.source.rangei = math.max(math.abs(startl), math.abs(endl))"
"  ch.trigger.source.limitv = ch.source.limitv"
"  ch.source.func = ch.OUTPUT_DCAMPS"
"  ch.trigger.source.lineari(startl, endl, cnt)"
"  return f, ar, r "
"end";
    keithley2600a_cmd( cmd );

    /* LUA function for setting up the trigger system and starting the
       linear sweep while measuring (we can already start reading values
       while the sweep is running) */

    cmd =
"fsc2_lin.run_sweep = function (ch, meas, cnt, mbuf1, mbuf2)"
"  ch.trigger.source.action = ch.ENABLE"
"  if     meas == 'v' then ch.trigger.measure.v(mbuf1)"
"  elseif meas == 'i' then ch.trigger.measure.i(mbuf1)"
"  elseif meas == 'p' then ch.trigger.measure.p(mbuf1)"
"  elseif meas == 'r' then ch.trigger.measure.r(mbuf1)"
"  else                    ch.trigger.measure.iv(mbuf2, mbuf1)"
"  end"
"  ch.trigger.measure.action = ch.ENABLE"
"  ch.trigger.count = cnt"
"  ch.trigger.arm.count = 1"
"  ch.trigger.endsweep.action = ch.SOURCE_IDLE"
"  ch.source.output = ch.ENABLE"
"  ch.trigger.initiate()"
"end";
    keithley2600a_cmd( cmd );

    k26->lin_sweeps_prepared = true;
}


/*--------------------------------------------------------------*
 * Prepares and sends LUA functions to the device for doing list
 * sweeps. Note that functions need to be relatively short (it's
 * not documented how long they may be but it looks like 1024
 * bytes is the limit) since otherwise the input buffer of the
 * device over-runs. No line-feeds are allowed at the ends of the
 * lines of the funtions!
 *--------------------------------------------------------------*/

void
keithley2600a_prep_list_sweeps( void )
{
    /* Create an empty LUA table, we're going to put all functions needed
       for list sweeps into it */

    const char * cmd = "fsc2_list = { }";
    keithley2600a_cmd( cmd );

    /* Add LUA function for doing the list sweep - it first initializes
       the source settings, the allocates memory for the results, sets up
       the trigger system amd runs the sweep, then prints out the results
       and resets the source settings */

    cmd =
"fsc2_list.sweep_and_measure = function (ch, sweep, meas, maxl)"
"  local cnt = table.getn(fsc2_list.list)"
"  local f, ar, r"
"  if sweep == 'v' then f, ar, r = fsc2_list.prep_sweepv(ch, maxl)"
"  else                 f, ar, r = fsc2_list.prep_sweepi(ch, maxl)"
"  end"
"  fsc2_list.list = nil"
"  local mbuf1 = ch.makebuffer(cnt)"
"  local mbuf2 = meas == 'iv' and ch.makebuffer(cnt) or nil"
"  fsc2_list.run_sweep(ch, meas, cnt, mbuf1, mbuf2)"
"  if meas ~= 'iv' then printbuffer(1, cnt, mbuf1)"
"  else                 printbuffer(1, cnt, mbuf1, mbuf2)"
"  end"
"  ch.source.output = ch.DISABLE "
"  ch.source.func = f"
"  if sweep == 'v' then"
"    ch.source.rangev = r"
"    ch.source.autorangev = ar"
"  else"
"    ch.source.rangei = r"
"    ch.source.autorangei = ar"
"  end "
"end";
    keithley2600a_cmd( cmd );

    /* LUA Function for preparing a list voltage sweep */

    cmd = 
"fsc2_list.prep_sweepv = function (ch, maxl)"
"  ch.source.output = ch.DISABLE"
"  local f, ar, r = ch.source.func, ch.source.autorangev, ch.source.rangev"
"  ch.source.rangev = maxl"
"  ch.trigger.source.limiti = ch.source.limiti"
"  ch.source.func = ch.OUTPUT_DCVOLTS"
"  ch.trigger.source.listv(fsc2_list.list)"
"  return f, ar, r "
"end";
    keithley2600a_cmd( cmd );

    /* LUA Function for preparing a list current sweep */

    cmd =
"fsc2_list.prep_sweepi = function (ch, maxl)"
"  ch.source.output = ch.DISABLE"
"  local f, ar, r = ch.source.func, ch.source.autorangei, ch.source.rangei"
"  ch.source.rangei = maxl"
"  ch.trigger.source.limitv = ch.source.limitv"
"  ch.source.func = ch.OUTPUT_DCAMPS"
"  ch.trigger.source.listi(fsc2_list.list)"
"  return f, ar, r "
"end";
    keithley2600a_cmd( cmd );

    /* LUA function for setting up the trigger system and starting the
       list sweeping while measuring (we can start reading data while
       the sweep is running) */

    cmd =
"fsc2_list.run_sweep = function (ch, meas, cnt, mbuf1, mbuf2)"
"  ch.trigger.source.action = ch.ENABLE"
"  if     meas == 'v' then ch.trigger.measure.v(mbuf1)"
"  elseif meas == 'i' then ch.trigger.measure.i(mbuf1)"
"  elseif meas == 'p' then ch.trigger.measure.p(mbuf1)"
"  elseif meas == 'r' then ch.trigger.measure.r(mbuf1)"
"  else                    ch.trigger.measure.iv(mbuf2, mbuf1)"
"  end"
"  ch.trigger.measure.action = ch.ENABLE"
"  ch.trigger.count = cnt"
"  ch.trigger.arm.count = 1"
"  ch.trigger.endsweep.action = ch.SOURCE_IDLE"
"  ch.source.output = ch.ENABLE"
"  ch.trigger.initiate()"
"end";
    keithley2600a_cmd( cmd );

    /* LUA function for appending array 'fsc2_list.l' to array
       'fsc2_list.listx' */

    cmd =
"fsc2_list.merge = function ()"
"  local sa = table.getn(fsc2_list.list)"
"  local sb = table.getn(fsc2_list.l)"
"  for i = 1, sb do"
"    fsc2_list.list[i + sa ] = fsc2_list.l[ i ]"
"  end "
"end";
    keithley2600a_cmd( cmd );

    k26->list_sweeps_prepared = true;
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
 * Converts a line as received from the device into an
 * array of doubles of the requested length
 *--------------------------------------------------------------*/


double *
keithley2600a_line_to_doubles( const char * line,
                               double     * buf,
                               int          cnt )
{
    int i = 0;

    fsc2_assert( cnt > 0 && buf );

    /* Keep reading until end of line is reached or as many values
       have been read as requested */

    while ( 1 )
    {
        double res;
        char *ep;

        if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
            keithley2600a_bad_data( );

        errno = 0;
        res = strtod( line, &ep );
        if (    ( *ep && *ep != '\n' && *ep != ' ' && *ep != ',' )
             || (    ( res == HUGE_VAL || res == - HUGE_VAL )
                  && errno == ERANGE ) )
            keithley2600a_bad_data( );

        buf[ i++ ] = res;

        if ( i == cnt )
            return buf;

        line = ++ep;
        while ( *line == ' ' || *line == ',')
            line++;
    }
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

    keithley2600a_talk( "printnumber(errorqueue.count)", buf, sizeof buf,
                        false );
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
                                "print(emess)", buf, sizeof buf, false );
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
    print( FATAL, "Devive sent unexpected data.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 * Called whenever communication fails
 *--------------------------------------------------------------*/

void
keithley2600a_comm_failure( void )
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
