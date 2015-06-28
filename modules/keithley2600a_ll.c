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

#if defined BINARY_TRANSFER
static double to_double_simple( const unsigned char * data );
static double to_double_hard(   const unsigned char * data );
static double ( * to_double )(  const unsigned char * data );
#endif

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

size_t
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
	return length;
}


/*---------------------------------------------------------------*
 * Reads in the complete state of the device
 *---------------------------------------------------------------*/

void
keithley2600a_get_state( void )
{
    /* Clear out the error queue */

    clear_errors( );

    /* When using ASCII data transfer from the Keithley2600A to the
       computer make sure all data will be sent in ASCII and with a
       precision of 6 digits (this may be a bit over the top but for
       some measurement ranges the data sheet claims an accuracy of
       better than 10^-5). When using binary transfer pick a function
       for extracting the data and set up endian-ness as needed. */

#if ! defined BINARY_TRANSFER
    keithley2600a_cmd( "format.data=format.ASCII" );
    keithley2600a_cmd( "format.asciiprecision=6" );
#else
    keithley2600a_cmd( "format.data=format.REAL32" );

    if ( sizeof( float ) == 4 )
    {
        int tst = 0;

        to_double = to_double_simple;

        * ( unsigned char * ) &tst = 1;
        if ( tst == 1 )
            keithley2600a_cmd( "format.byteorder = format.LITTLEENDIAN" );
        else
            keithley2600a_cmd( "format.byteorder=format.BIGENDIAN" );
    }
    else
    {
        to_double = to_double_hard;
        keithley2600a_cmd( "format.byteorder=format.LITTLEENDIAN" );
    }
#endif

    keithley2600a_get_line_frequency( );

    const char * model = keithley2600a_get_model( );

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

    for ( unsigned int ch = 0; ch < NUM_CHANNELS; ch++ )
    {
        keithley2600a_get_sense( ch );
        
        keithley2600a_get_source_output( ch );
        keithley2600a_get_source_highc( ch );
        keithley2600a_get_source_offmode( ch );
        keithley2600a_get_source_func( ch );
        keithley2600a_get_source_delay( ch );

        keithley2600a_get_source_autorangev( ch );
        keithley2600a_get_source_autorangei( ch );

        keithley2600a_get_source_limitv( ch );
        keithley2600a_get_source_limiti( ch );

        keithley2600a_get_source_rangev( ch );
        keithley2600a_get_source_rangei( ch );

        keithley2600a_get_source_lowrangev( ch );
        keithley2600a_get_source_lowrangei( ch );

        keithley2600a_get_source_levelv( ch );
        keithley2600a_get_source_leveli( ch );

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

    TALK( "print(localnode.model)", buf, sizeof buf, false, 6 );
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
    fsc2_assert( ch < NUM_CHANNELS );

    char buf[ 50 ];

    sprintf( buf, "printnumber(%s.sense)", smu[ ch ] );
    TALK( buf, buf, sizeof buf, false, 7 );

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
    fsc2_assert( ch < NUM_CHANNELS );
	fsc2_assert(    sense == SENSE_LOCAL
				 || sense == SENSE_REMOTE
				 || sense == SENSE_CALA );

	/* Calibration mode can only be switched to when output is off */

	fsc2_assert(    sense != SENSE_CALA
				 || keithley2600a_get_source_output( ch ) == OUTPUT_OFF );

    char buf[ 50 ];
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

    /* Determining the line frequency can take a lot of time, so raise
       the read timeout considerably (to 5 s to be on the safe side) */

    if ( vxi11_set_timeout( VXI11_READ, 5000000 ) != SUCCESS )
        keithley2600a_comm_failure( );

	TALK( buf, buf, sizeof buf, false, 7 );

    k26->linefreq = keithley2600a_line_to_double( buf );

    if ( vxi11_set_timeout( VXI11_READ, READ_TIMEOUT ) != SUCCESS )
        keithley2600a_comm_failure( );

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

#if ! defined BINARY_TRANSFER
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
#else
bool
keithley2600a_line_to_bool( const char * line )
{
    if ( *line++ != '#' || *line++ != '0' || line[ 4 ] != '\n' )
        keithley2600a_bad_data( );

    return to_double( ( const unsigned char *  ) line ) ? true : false;
}
#endif


/*--------------------------------------------------------------*
 * Converts a line as received from the device to an int
 *--------------------------------------------------------------*/

#if ! defined BINARY_TRANSFER
int
keithley2600a_line_to_int( const char * line )
{
    if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
        keithley2600a_bad_data( );

    char *ep;
    double dres = strtod( line, &ep );

    if ( *ep != '\n' || *++ep || dres > INT_MAX || dres < INT_MIN )
        keithley2600a_bad_data( );

    long int res = lrnd( dres );
    if ( res > INT_MAX || res < INT_MIN || dres - res != 0 )
        keithley2600a_bad_data( );

    return res;
}
#else
int
keithley2600a_line_to_int( const char * line )
{
    if ( *line++ != '#' || *line++ != '0' || line[ 4 ] != '\n' )
        keithley2600a_bad_data( );

    return irnd( to_double( ( const unsigned char *  ) line ) );
}
#endif


/*--------------------------------------------------------------*
 * Converts a line as received from the device to a double
 *--------------------------------------------------------------*/


#if ! defined BINARY_TRANSFER
double
keithley2600a_line_to_double( const char * line )
{
    if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
        keithley2600a_bad_data( );

    errno = 0;

    char *ep;
    double res = strtod( line, &ep );

    if (    *ep != '\n'
         || *++ep
         || ( ( res == HUGE_VAL || res == - HUGE_VAL ) && errno == ERANGE ) )
        keithley2600a_bad_data( );

    return res;
}
#else
double
keithley2600a_line_to_double( const char * line )
{
    if ( *line++ != '#' || *line++ != '0' || line[ 4 ] != '\n' )
        keithley2600a_bad_data( );

    return to_double( ( const unsigned char *  ) line );
}
#endif


/*--------------------------------------------------------------*
 * Converts a line as received from the device into an
 * array of doubles of the requested length
 *--------------------------------------------------------------*/


#if ! defined BINARY_TRANSFER
double *
keithley2600a_line_to_doubles( const char * line,
                               double     * buf,
                               int          cnt )
{
    fsc2_assert( cnt > 0 && buf );

    /* Keep reading until end of line is reached or as many values
       have been read as requested */

    int i = 0;

    while ( 1 )
    {
        if ( ! isdigit( ( int ) *line ) && *line != '-' && *line != '+' )
            keithley2600a_bad_data( );

        errno = 0;

        char *ep;
        double res = strtod( line, &ep );

        if (    (    *ep && *ep != '\n'
                  && *ep != ' '
                  && *ep != ','
                  && *ep != '\t' )
             || (    ( res == HUGE_VAL || res == - HUGE_VAL )
                  && errno == ERANGE ) )
            keithley2600a_bad_data( );

        buf[ i++ ] = res;

        if ( i == cnt )
            return buf;

        line = ++ep;
        while ( *line == ' ' || *line == '\t' )
            line++;
    }
}
#else
double *
keithley2600a_line_to_doubles( const char * line,
                               double     * buf,
                               int          cnt )
{
    fsc2_assert( cnt > 0 && buf );

    if ( *line++ != '#' || *line++ != '0' || line[ 4 * cnt ] != '\n' )
        keithley2600a_bad_data( );

    for ( int i = 0; i < cnt; line += 4, i++ )
        buf[ i ] = to_double( ( const unsigned char * ) line );

    return buf;
}
#endif


/*--------------------------------------------------------------*
 * Functions for converting 32 bit floating point numbers in
 * IEEE 754 format received from the device. The first one is
 * for the usual case that the sizeof(float) on the machine
 * is 4 (endian-ness has already been taken care of). The
 * additonal conversion from ASCII and back looks stupid
 * but guarantees that numbers are rounded to 6 digits, which
 * makes a lot of comparisons with limits easier. The second
 * function is for the rare cases that the machine doesn't
 * use IEEE 754 format or is strange in some other respect.
 *
 * These functions are only needed when the module is compiled
 * for binary transfers from the Keithley2600A to the computer.
 *--------------------------------------------------------------*/

#if defined BINARY_TRANSFER
static
double
to_double_simple( const unsigned char * data )
{
    float x;
    char buf[ 20 ];

    memcpy( &x, data, 4 );
    sprintf( buf, "%.6g", x );
    return strtod( buf, NULL );
}


static
double
to_double_hard( const unsigned char * data )
{
    float x;
    double mant;
    double sign = ( data[ 3 ] & 0x80 ) ? -1.0 : 1.0;
    unsigned char expo = ( ( ( data[ 3 ] & 0x7F ) << 1 ) | ( data[ 2 ] >> 7 ) );

    if ( expo == 0xFF )
        keithley2600a_bad_data( );

    if ( expo )
    {
        mant = ( ( data[ 2 ] | 0x80 ) << 16 ) | ( data[ 1 ] << 8 ) | data[ 0 ];
        x = sign * ( mant / 0x800000 ) * pow( 2.0, expo - 127 );
    }
    else
    {
        mant = ( data[ 2 ] << 16 ) | ( data[ 1 ] << 8 ) | data[ 0 ];
        x = sign * ( mant / 0x800000 ) * pow( 2.0, -126 );
    }

    char buf[ 20 ];
    sprintf( buf, "%.6g", x );

    return strtod( buf, NULL );
}
#endif


/*--------------------------------------------------------------*
 * Checks for errors reported by the device and if there are any
 * fetches the corresponding messages and throws an exception after
 * printing out the error messages.
 *--------------------------------------------------------------*/

void
keithley2600a_show_errors( void )
{
    char buf[ 200 ];
    TALK( "printnumber(errorqueue.count)", buf, sizeof buf, false, 7 );

    int error_count;
    if ( ( error_count = keithley2600a_line_to_int( buf ) ) < 0 )
        keithley2600a_bad_data( );
    else if ( error_count == 0 )
        return;

    char * mess = T_strdup( "Device reports the following errors:\n" );

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
