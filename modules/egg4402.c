/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#include "fsc2_module.h"
#include "gpib.h"


/* Include configuration information for the device */

#include "egg4402.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define EGG4402_TEST_CURVE_LENGTH  4096L

/* Declaration of exported functions */

int egg4402_init_hook(       void );
int egg4402_exp_hook(        void );
int egg4402_end_of_exp_hook( void );

Var_T * boxcar_name(              Var_T * /* v */ );
Var_T * boxcar_curve_length(      Var_T * /* v */ );
Var_T * boxcar_get_curve(         Var_T * /* v */ );
Var_T * boxcar_start_acquisition( Var_T * /* v */ );
Var_T * boxcar_stop_acquisition(  Var_T * /* v */ );
Var_T * boxcar_single_shot(       Var_T * /* v */ );
Var_T * boxcar_command(           Var_T * /* v */ );

/* Locally used functions */

static bool egg4402_init( const char * name );

static void egg4402_failure( void );

static void egg4402_query( char * buffer,
                           long * length,
                           bool   wait_for_stop );

static bool egg4402_command( const char * cmd );


static struct {
    int device;
    bool fatal_error;  /* set on communication errors to avoid long delays on
                          exit due to communication with non-reacting device */
} egg4402;


/*------------------------------------*
 * Init hook function for the module.
 *------------------------------------*/

int
egg4402_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed */

    Need_GPIB = SET;

    /* Reset several variables in the structure describing the device */

    egg4402.device = -1;

    return 1;
}


/*--------------------------------------------------*
 * Start of experiment hook function for the module
 *--------------------------------------------------*/

int
egg4402_exp_hook( void )
{
    /* Initialize the box-car */

    egg4402.fatal_error = UNSET;

    if ( ! egg4402_init( DEVICE_NAME ) )
    {
        print( FATAL, "Initialization of device failed: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }

    return 1;
}


/*------------------------------------------------*
 * End of experiment hook function for the module
 *------------------------------------------------*/

int
egg4402_end_of_exp_hook( void )
{
    /* Switch boxcar back to local mode */

    if ( egg4402.device >= 0 && ! egg4402.fatal_error )
        gpib_local( egg4402.device );

    egg4402.fatal_error = UNSET;
    egg4402.device = -1;

    return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
boxcar_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

Var_T *
boxcar_curve_length( Var_T * v )
{
    char buffer[ 100 ];
    long length = sizeof buffer;
    long num_points;
    long inter;


    if ( v == NULL )
    {
        if ( FSC2_MODE == TEST )
            return vars_push( INT_VAR, EGG4402_TEST_CURVE_LENGTH );

        egg4402_command( "CL\n" );
        egg4402_query( buffer, &length, UNSET );

        buffer[ length - 1 ] = '\0';
        return vars_push( INT_VAR, T_atol( buffer ) );
    }

    num_points = get_long( v, "number of points" );

    too_many_arguments( v );

    if ( num_points < 32 || num_points > 4096 )
    {
        print( FATAL, "Invalid number of points.\n" );
        THROW( EXCEPTION );
    }

    inter = num_points;
    while ( inter != 1 )
    {
        inter /= 2;
        if ( inter != 1 && inter & 1 )
        {
            print( FATAL, "Invalid number of points (%ld).\n", num_points );
            THROW( EXCEPTION );
        }
    }

    if ( FSC2_MODE == TEST )
        return vars_push( INT_VAR, num_points );

    sprintf( buffer, "CL %ld\n", num_points );
    egg4402_command( buffer );

    return vars_push( INT_VAR, num_points );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

Var_T *
boxcar_get_curve( Var_T * v )
{
    unsigned char *buffer;
    double *ret_buffer;
    char cmd[ 100 ];
    long length;
    long curve_type = -1;
    long curve_number = 0;
    long max_points;
    long first, last;
    long num_points;
    long i;
    unsigned char *cc, *cn;
    Var_T *cl;
    double tmos[ 7 ] = { 1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0 };
    int new_timo;
    double max_time;
    bool size_dynamic = UNSET;


    CLOBBER_PROTECT( curve_type );
    CLOBBER_PROTECT( first );
    CLOBBER_PROTECT( last );

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    /* If the first variable is a number, the second also must be a number.
       The first number must be 0 or 1 for live curve or memory curve, the
       second must be the curve number */

    if ( v->type & ( INT_VAR | FLOAT_VAR ) )
        curve_type = get_long( v, "curve type" );
    else                             /* first argument is a string */
    {
        if (    ! strcmp( v->val.sptr, "LC" )
             || ! strcmp( v->val.sptr, "LIVECURVE" )
             || ! strcmp( v->val.sptr, "LIVE_CURVE" ) )
            curve_type = 0;
        else if (    ! strcmp( v->val.sptr, "MC" )
                  || ! strcmp( v->val.sptr, "MEMORYCURVE" )
                  || ! strcmp( v->val.sptr, "MEMORY_CURVE" ) )
            curve_type = 1;
    }

    if ( curve_type == -1 )
    {
        print( FATAL, "First argument ('%s') isn't a valid curve.\n",
               v->val.sptr );
        THROW( EXCEPTION );
    }

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing curve number.\n" );
        THROW( EXCEPTION );
    }

    curve_number = get_long( v, "curve number" );

    v = vars_pop( v );

    /* Check validity of curve type and number argument */

    if ( curve_type == 0 && ( curve_number < 1 || curve_number > 2 ) )
    {
        print( FATAL, "Invalid live curve number %d.\n", curve_number );
        THROW( EXCEPTION );
    }

    if ( curve_type == 1 && ( curve_number < 1 || curve_number > 3 ) )
    {
        print( FATAL, "Invalid memory curve number %d.\n", curve_number );
        THROW( EXCEPTION );
    }

    cl = boxcar_curve_length( NULL );
    max_points = cl->val.lval;
    vars_pop( cl );

    if ( v == NULL )
    {
        first = 0;
        last = max_points - 1;
        size_dynamic = SET;
    }
    else
    {
        first = get_long( v, "first point" );

        if ( first < 0 || first >= max_points )
        {
            print( FATAL, "Invalid value (%ld) for first point of curve.\n",
                   first + 1 );
            THROW( EXCEPTION );
        }

        if ( ( v = vars_pop( v ) ) == NULL )
        {
            last = max_points - 1;
            size_dynamic = SET;
        }
        else
        {
            last = get_long( v, "last point" ) - 1;

            if ( last < 0 || last >= max_points )
            {
                print( FATAL, "Invalid value (%ld) for last point.\n",
                       last + 1 );
                THROW( EXCEPTION );
            }
        }

        if ( first >= last )
        {
            print( FATAL, "Last point not larger than first point.\n" );
            THROW( EXCEPTION );
        }
    }

    too_many_arguments( v );

    num_points = last - first + 1;

    if ( FSC2_MODE == TEST )
    {
        ret_buffer = T_calloc( num_points, sizeof *ret_buffer );
        cl = vars_push( FLOAT_ARR, ret_buffer, last - first + 1 );
        if ( size_dynamic )
            cl->flags |= IS_DYNAMIC;
        T_free( ret_buffer );
        return cl;
    }

    /* Get a buffer large enough to hold all data */

    length = num_points * 15 + 1;
    buffer = T_malloc( length );

    /* Set the curve to use for the transfer */

    sprintf( cmd, "CRV %ld %ld\n", curve_type, curve_number );
    egg4402_command( cmd );

    /* Tell the boxcar to send points */

    sprintf( cmd, "DC %ld %ld\n", first, num_points );
    egg4402_command( cmd );

    /* Try to read the data - on failure we still have to free the buffer */

    TRY
    {
        /* Set a timeout value that we have at least 20 ms per point - this
           seems to be the upper limit needed. Don't set timout to less than
           1s. */

        max_time = 0.02 * num_points;
        new_timo = 0;
        while ( tmos[ new_timo ] < max_time )
            new_timo++;
        new_timo += 11;
        gpib_timeout( egg4402.device, new_timo );

        egg4402_query( ( char * ) buffer, &length, curve_type ? UNSET : SET );
        TRY_SUCCESS;
    }
    CATCH( USER_BREAK_EXCEPTION )
    {
        T_free( buffer );
        RETHROW( );
    }
    OTHERWISE
    {
        T_free( buffer );
        RETHROW( );
    }

    /* Get a buffer for the data in binary form and convert the ASCII data */

    buffer[ length - 1 ] = '\0';
    ret_buffer = T_malloc( num_points * sizeof *ret_buffer );

    for ( i = 0, cc = buffer; i < num_points; cc = cn, i++ )
    {
        cn = ( unsigned char * ) strchr( ( char * ) cc, '\r' );
        if ( cn == NULL )
        {
            T_free( buffer );
            print( FATAL, "Received unexpected data.\n" );
            THROW( EXCEPTION );
        }
        *cn = '\0';
        cn += 2;                  /* the boxcar uses "\r\n" as separator */

        ret_buffer[ i ] = T_atod( ( char * ) cc );
    }

    T_free( buffer );

    cl = vars_push( FLOAT_ARR, ret_buffer, last - first + 1 );
    T_free( ret_buffer );
    return cl;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
boxcar_start_acquisition( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == EXPERIMENT )
        egg4402_command( "START\n" );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
boxcar_stop_acquisition( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == EXPERIMENT )
        egg4402_command( "STOP\n" );

    return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------*
 * This function starts an acquisition but pauses the acquisition
 * when the first data point has been measured. It then returns
 * of the first point from either one or both live curves. The
 * function expects either 1 or 2 arguments, the livecurve
 * the data point(s) are to be fetched from.
 *----------------------------------------------------------------*/

Var_T *
boxcar_single_shot( Var_T * v )
{
    long channel_1 = 1, channel_2 = -1;
    double data[ 2 ];
    unsigned char stb = 0;
    char cmd[ 100 ];
    char reply[ 16 ];
    long length = sizeof reply;
    char *cn;


    if ( v == NULL )
        print( SEVERE, "Missing channel argument, using channel 1.\n" );
    else
    {
        channel_1 = get_long( v, "first channel number" );
        if ( channel_1 < 1 || channel_1 > 2 )
        {
            print( FATAL, "Invalid curve number %d.\n", channel_1 );
            THROW( EXCEPTION );
        }

        if ( ( v = vars_pop( v ) ) != NULL )
        {
            channel_2 = get_long( v, "second channel number" );
            if ( channel_2 < 1 || channel_2 > 2 )
            {
                print( FATAL, "Invalid curve number %d.\n", channel_2 );
                THROW( EXCEPTION );
            }
        }
    }

    if ( FSC2_MODE == TEST )
    {
        data[ 0 ] = random( ) / ( double ) RAND_MAX;
        if ( channel_2 == -1 )
            return vars_push( FLOAT_VAR, data[ 0 ] );
        data[ 1 ] = random( ) / ( double ) RAND_MAX;
        return vars_push( FLOAT_ARR, data, 2 );
    }

    /* Start the acquisition and already set the transfer type to ASCII floats
       and the curve to the first channel */

    egg4402_command( "START\n" );
    sprintf( cmd, "CRV %d %ld\n", 0, channel_1 );
    egg4402_command( cmd );

    /* Wait for the BUFFER DATA bit to become set, indicating that there
       is at least one point of a LIVECURVE to be read */

    while ( 1 )
    {
        if ( gpib_serial_poll( egg4402.device, &stb ) == FAILURE )
            egg4402_failure( );
        if ( stb & 0x8 )
            break;
        stop_on_user_request( );
        fsc2_usleep( 100000, UNSET );
    }

    /* When at least one point is available pause the acquisition and fetch
       a single point */

    egg4402_command( "P\n" );
    egg4402_command( "DC 0 1\n" );

    if ( gpib_read( egg4402.device, reply, &length ) == FAILURE )
        egg4402_failure( );

    cn = strchr( ( char * ) reply, '\r' );
    if ( cn == NULL )
    {
        print( FATAL, "Received unexpected data.\n" );
        THROW( EXCEPTION );
    }
    *cn = '\0';

    data[ 0 ] = T_atod( reply );

    /* If no data point from the other channel was requested return the
       value of the data point */

    if ( channel_2 == -1 )
        return vars_push( FLOAT_VAR, data[ 0 ] );

    /* Otherwise switch to getting data from the second curve and also
       get one point */

    sprintf( cmd, "CRV %d %ld\n", 0, channel_2 );
    egg4402_command( cmd );
    egg4402_command( "DC 0 1\n" );

    length = sizeof reply;
    if ( gpib_read( egg4402.device, reply, &length ) == FAILURE )
        egg4402_failure( );

    cn = strchr( ( char * ) reply, '\r' );
    if ( cn == NULL )
    {
        print( FATAL, "Received unexpected data.\n" );
        THROW( EXCEPTION );
    }
    *cn = '\0';

    data[ 1 ] = T_atod( reply );

    return vars_push( FLOAT_ARR, data, 2 );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
boxcar_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            egg4402_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW( );
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static bool
egg4402_init( const char * name )
{
    fsc2_assert( egg4402.device < 0 );

    if ( gpib_init_device( name, &egg4402.device ) == FAILURE )
        return FAIL;

    return OK;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
egg4402_failure( void )
{
    print( FATAL, "Communication with boxcar failed.\n" );
    egg4402.fatal_error = SET;
    THROW( EXCEPTION );
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

static void
egg4402_query( char * buffer,
               long * length,
               bool   wait_for_stop )
{
    unsigned char stb = 0;


    /* Before trying to read anything from the device check that the OUTPUT
       READY bits is set. If 'wait_for_stop' is set wait also for the CURVE
       NOT RUNNING bit to become set. */

    while ( 1 )
    {
        if ( gpib_serial_poll( egg4402.device, &stb ) == FAILURE )
            egg4402_failure( );
        if (    ( ! wait_for_stop && stb & 0x80 )
             || ( wait_for_stop && stb & 4 && stb & 0x80 ) )
            break;
        stop_on_user_request( );
        fsc2_usleep( 100000, UNSET );
    }

    if ( gpib_read( egg4402.device, buffer, length ) == FAILURE )
        egg4402_failure( );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
egg4402_command( const char * cmd )
{
    if ( gpib_write( egg4402.device, cmd, strlen( cmd ) ) == FAILURE )
        egg4402_failure( );

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
