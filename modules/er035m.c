/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
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

#include "er035m.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER035M_TEST_FIELD 2000.0    /* returned as current field in test run */
#define ER035M_TEST_RES   0.001

#define LOW_RES     0               /* 0.1 G */
#define MEDIUM_RES  1               /* 0.01 G */
#define HIGH_RES    2               /* 0.001 G */
#define UNDEF_RES  -1

#define PROBE_ORIENTATION_PLUS       0
#define PROBE_ORIENTATION_MINUS      1
#define PROBE_ORIENTATION_UNDEFINED -1
#define UNDEF_RESOLUTION            -1


/* exported functions and symbols */

int er035m_init_hook(       void );
int er035m_test_hook(       void );
int er035m_exp_hook(        void );
int er035m_end_of_exp_hook( void );

Var_T * gaussmeter_name(               Var_T * v );
Var_T * gaussmeter_field(              Var_T * v );
Var_T * find_field(                    Var_T * v );
Var_T * gaussmeter_resolution(         Var_T * v );
Var_T * gaussmeter_probe_orientation(  Var_T * v );
Var_T * gaussmeter_command(            Var_T * v );
Var_T * gaussmeter_wait(               Var_T * v );
Var_T * gaussmeter_upper_search_limit( Var_T * v );
Var_T * gaussmeter_lower_search_limit( Var_T * v );


/* internally used functions */

static double er035m_get_field( void );

static int er035m_get_resolution( void );

static void er035m_set_resolution( int res_index );

static bool er035m_command( const char * cmd );

static long er035m_get_upper_search_limit( void );

static long er035m_get_lower_search_limit( void );

static void er035m_set_upper_search_limit( long ul );

static void er035m_set_lower_search_limit( long ll );

static bool er035m_talk( const char * cmd,
                         char *       reply,
                         long *       length );

static void er035m_failure( void );


struct NMR {
    int state;
    int device;
    bool is_needed;
    const char *name;
    double field;
    int resolution;
    int probe_orientation;
    int probe_type;
    long upper_search_limit;
    long lower_search_limit;
};


static struct NMR nmr, nmr_stored;
static double res_list[ 3 ] = { 0.1, 0.01, 0.001 };
bool is_gaussmeter = UNSET;         /* tested by magnet power supply driver */


/* The gaussmeter seems to be more cooperative if we wait for some time
   (i.e. 200 ms) after each write operation... */

#define E2_US       100000    /* 100 ms, used in calls of usleep() */
#define ER035M_WAIT 200000    /* this means 200 ms for usleep() */

/* If the field is unstable the gausmeter might never get to the state where
   the field value is valid with the requested resolution even though the lock
   state is achieved. 'ER035M_S_MAX_RETRIES' tells how many times we retry in
   this case. With a value of 100 and the current setting of 'ER035M_S_WAIT'
   of 200 ms it will take at least 20 s before we finally give up.

   Take care: This does not mean that we stop trying to get the field while
   the gaussmeter is still actively searching but only in the case that a
   lock is already achieved but the field value is too unstable! */

#define ER035M_MAX_RETRIES 100


#define PROBE_TYPE_F0 0       /* S-band probe */
#define PROBE_TYPE_F1 1       /* X-band probe */


long lower_search_limits[ 2 ] = {  450, 1450 };
long upper_search_limits[ 2 ] = { 2400, 20000 };


enum {
       ER035M_UNKNOWN = -1,
       ER035M_LOCKED = 0,
       ER035M_SU_ACTIVE,
       ER035M_SD_ACTIVE,
       ER035M_OU_ACTIVE,
       ER035M_OD_ACTIVE,
       ER035M_SEARCH_AT_LIMIT
};



/**************************************************/
/*                                                */
/*                  hook functions                */
/*                                                */
/**************************************************/


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_init_hook( void )
{
    /* Set global flag to tell magnet power supply driver that a
       gaussmeter has been loaded */

    is_gaussmeter = SET;

    if ( exists_device( "bh15" ) )
    {
        print( FATAL, "Driver for BH15 field controller is already loaded - "
               "only one field control gaussmeter can be used.\n" );
        THROW( EXCEPTION );
    }

    if ( exists_device( "er035m_s" ) )
    {
        print( FATAL, "Driver for ER035M gaussmeter (serial port version) is "
               "already loaded - only one field controlling gaussmeter can be "
               "used.\n" );
        THROW( EXCEPTION );
    }

    if ( exists_device( "er035m_sa" ) || exists_device( "er035m_sas" ) )
    {
        print( FATAL, "Driver for ER035M field controlling gaussmeter must "
               "be first gaussmeter in DEVICES list.\n" );
        THROW( EXCEPTION );
    }

    if ( ! exists_device( "aeg_s_band" ) && ! exists_device( "aeg_x_band" ) )
        print( WARN, "Driver for NMR gaussmeter is loaded but no appropriate "
               "magnet power supply driver.\n" );

    Need_GPIB = SET;
    nmr.is_needed = SET;
    nmr.name = DEVICE_NAME;
    nmr.state = ER035M_UNKNOWN;
    nmr.resolution = UNDEF_RES;
    nmr.probe_orientation = PROBE_ORIENTATION_UNDEFINED;
    nmr.device = -1;

    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_test_hook( void )
{
    nmr_stored = nmr;
    nmr.lower_search_limit = lower_search_limits[ PROBE_TYPE_F0 ];
    nmr.upper_search_limit = upper_search_limits[ PROBE_TYPE_F1 ];
    return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int
er035m_exp_hook( void )
{
    char buffer[ 21 ], *bp;
    long length = sizeof buffer;
    int cur_res;
    Var_T *v;


    nmr = nmr_stored;

    if ( ! nmr.is_needed )
        return 1;

    fsc2_assert( nmr.device < 0 );

    if ( gpib_init_device( nmr.name, &nmr.device ) == FAILURE )
    {
        nmr.device = -1;
        print( FATAL, "Initialization of device failed: %s.\n",
               gpib_last_error( ) );
        THROW( EXCEPTION );
    }
    fsc2_usleep( ER035M_WAIT, UNSET );

    /* Send a "Selected Device Clear" - otherwise the gaussmeter does not
       work ... */

    gpib_clear_device( nmr.device );
    fsc2_usleep( ER035M_WAIT, UNSET );

    /* Find out the curent resolution, and if necessary, change it to the
       value requested by the user. */

    cur_res = er035m_get_resolution( );

    if ( nmr.resolution == UNDEF_RES )
        nmr.resolution = cur_res;
    else if ( nmr.resolution != cur_res )
        er035m_set_resolution( nmr.resolution );

    /* Ask gaussmeter to send status byte and test if it does */

    nmr.state = ER035M_UNKNOWN;

 try_again:

    stop_on_user_request( );

    length = sizeof buffer;
    er035m_talk( "PS\r", buffer, &length );

    if ( length < 3 || ! strchr( buffer, '\r' ) )
    {
        print( FATAL, "Undocumented data received.\n" );
        THROW( EXCEPTION );
    }

    /* Now look if the status byte says that device is OK where OK means that
       for the X-band magnet the F1 probe and for the S-band the F0 probe is
       connected, modulation is on and the gaussmeter is either in locked
       state or is actively searching to achieve the lock (if it's just in
       TRANS L-H or H-L state check again) */

    bp = buffer + 2;     /* skip first two chars of status byte */

    do     /* Loop through the remaining chars in status byte */
    {
        switch ( *bp )
        {
            case '0' :      /* Probe F0 is connected -> OK for S-band */
                if ( exists_device( "aeg_s_band" ) )
                {
                    nmr.probe_type = PROBE_TYPE_F0;
                    break;
                }
                print( FATAL, "Wrong field probe (F0) connected.\n" );
                THROW( EXCEPTION );

            case '1' :      /* Probe F1 is connected -> OK for X-band*/
                if ( exists_device( "aeg_x_band" ) )
                {
                    nmr.probe_type = PROBE_TYPE_F1;
                    break;
                }
                print( FATAL, "Wrong field probe (F1) connected.\n" );
                THROW( EXCEPTION );

            case '2' :      /* No probe connected -> error */
                print( FATAL, "No field probe connected.\n" );
                THROW( EXCEPTION );

            case '3' :      /* Error temperature -> error */
                print( FATAL, "Temperature error.\n" );
                THROW( EXCEPTION );

            case '4' :      /* TRANS L-H -> test again */
                fsc2_usleep( 500000, SET );
                goto try_again;

            case '5' :      /* TRANS L-H -> test again */
                fsc2_usleep( 500000, SET );
                goto try_again;

            case '6' :      /* MOD OFF -> error (should never happen */
                print( FATAL, "Modulation is switched off.\n" );
                THROW( EXCEPTION );

            case '7' :      /* MOD POS -> OK */
                nmr.probe_orientation = PROBE_ORIENTATION_PLUS;
                break;

            case '8' :      /* MOD NEG -> OK*/
                nmr.probe_orientation = PROBE_ORIENTATION_MINUS;
                break;

            case '9' :      /* System in lock -> very good... */
                nmr.state = ER035M_LOCKED;
                break;

            case 'A' :      /* FIELD ? -> error */
                print( FATAL, "Gaussmeter can't find the field "
                       "(\"FIELD?\").\n" );
                THROW( EXCEPTION );

            case 'B' :      /* SU active -> OK */
                nmr.state = ER035M_SU_ACTIVE;
                break;

            case 'C' :      /* SD active -> OK */
                nmr.state = ER035M_SD_ACTIVE;
                break;

            case 'D' :      /* OU active -> OK */
                nmr.state = ER035M_OU_ACTIVE;
                break;

            case 'E' :      /* OD active -> OK */
                nmr.state = ER035M_OD_ACTIVE;
                break;

            case 'F' :      /* Search active but just at search limit -> OK */
                nmr.state = ER035M_SEARCH_AT_LIMIT;
                break;
        }

    } while ( *bp++ != '\r' );

    /* Switch display on */

    er035m_command( "ED\r" );

    /* If the gaussmeter is already locked just get the field value, other-
       wise try to achieve locked state */

    if ( nmr.state != ER035M_LOCKED )
    {
        v = find_field( NULL );
        nmr.field = v->val.dval;
        vars_pop( v );
    }
    else
        nmr.field = er035m_get_field( );

    /* Find out the current settings of the search limits */

    nmr.upper_search_limit = er035m_get_upper_search_limit( );
    nmr.lower_search_limit = er035m_get_lower_search_limit( );

    return 1;
}


/*-----------------------------------------------------------------------*
 -----------------------------------------------------------------------*/

int
er035m_end_of_exp_hook( void )
{
    if ( ! nmr.is_needed )
        return 1;

    if ( nmr.device >= 0 )
        gpib_local( nmr.device );

    nmr.device = -1;

    return 1;
}


/**************************************************/
/*                                                */
/*              exported functions                */
/*                                                */
/**************************************************/


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
gaussmeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

Var_T *
gaussmeter_field( Var_T * v )
{
    return find_field( v );
}


/*----------------------------------------------------------------*
 * find_field() tries to get the gaussmeter into the locked state
 * and returns the current field value in a variable.
 *----------------------------------------------------------------*/

Var_T *
find_field( Var_T * v  UNUSED_ARG )
{
    char buffer[ 21 ];
    char *bp;
    long length;


    if ( FSC2_MODE == TEST )
        return vars_push( FLOAT_VAR, ER035M_TEST_FIELD );

    /* If gaussmeter is in oscillator up/down state or the state is unknown
       (i.e. it's standing somewhere but not searching) search for field.
       Starting with searching down is just as probable the wrong decision
       as searching up... */

    if (    nmr.state == ER035M_OU_ACTIVE
         || nmr.state == ER035M_OD_ACTIVE
         || nmr.state == ER035M_UNKNOWN )
         er035m_command( "SD\r" );

    /* Wait for gaussmeter to go into lock state (or FAILURE) */

    while ( nmr.state != ER035M_LOCKED )
    {
        stop_on_user_request( );

        /* Get status byte and check if lock was achieved */

        length = sizeof buffer;
        er035m_talk( "PS\r", buffer, &length );

        if ( length < 3 || ! strchr( buffer, '\r' ) )
        {
            print( FATAL, "Undocumented data received.\n" );
            THROW( EXCEPTION );
        }

        bp = buffer + 2;   /* skip first two chars of status byte */

        do     /* loop through remaining chars in status byte */
        {
            switch ( *bp )
            {
                case '4' : case '5' :  /* TRANS L-H or H-L -> test again */
                    break;

                case '7' : case '8' :  /* MOD POS or NEG -> just go on */
                    break;

                case '9' :      /* System in lock -> very good... */
                    nmr.state = ER035M_LOCKED;
                    break;

                case 'A' :      /* FIELD ? -> error */
                    print( FATAL, "Gaussmeter can't find the field "
                           "(\"FIELD?\").\n" );
                    THROW( EXCEPTION );

                case 'B' :      /* SU active -> OK */
                    nmr.state = ER035M_SU_ACTIVE;
                    break;

                case 'C' :      /* SD active */
                    nmr.state = ER035M_SD_ACTIVE;
                    break;

                case 'D' :      /* OU active -> error (should never happen) */
                    nmr.state = ER035M_OU_ACTIVE;
                    print( FATAL, "Unidentifiable device problem.\n" );
                    THROW( EXCEPTION );

                case 'E' :      /* OD active -> error (should never happen) */
                    nmr.state = ER035M_OD_ACTIVE;
                    print( FATAL, "Unidentifiable device problem.\n" );
                    THROW( EXCEPTION );

                case 'F' :      /* Search active but at a search limit -> OK*/
                    nmr.state = ER035M_SEARCH_AT_LIMIT;
                    break;
            }
        } while ( *bp++ != '\r' );
    };

    /* Finally  get current field value */

    return vars_push( FLOAT_VAR, er035m_get_field( ) );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_resolution( Var_T * v )
{
    double res;
    int i;
    int res_index = UNDEF_RES;


    if ( v == NULL )
    {
        if ( FSC2_MODE == PREPARATION && nmr.resolution == UNDEF_RES )
        {
            no_query_possible( );
            THROW( EXCEPTION );
        }

        if ( FSC2_MODE == TEST && nmr.resolution == UNDEF_RES )
            return vars_push( FLOAT_VAR, ER035M_TEST_RES );

        return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
    }

    res = get_double( v, "resolution" );

    too_many_arguments( v );

    if ( res <= 0 )
    {
        print( FATAL, "Invalid resolution of %.0f mG.\n", 1.0e3 * res );
        THROW( EXCEPTION );
    }

    /* Try to match the requested resolution, if necessary use the nearest
       possible value */

    for ( i = 0; i < 2; i++ )
        if ( res <= res_list[ i ] && res >= res_list[ i + 1 ] )
        {
            res_index = i +
                 ( ( res / res_list[ i ] > res_list[ i + 1 ] / res ) ? 0 : 1 );
            break;
        }

    if ( res_index == UNDEF_RES )
    {
        if ( res >= res_list[ LOW_RES ] )
            res_index = LOW_RES;
        if ( res <= res_list[ HIGH_RES ] )
            res_index = HIGH_RES;
    }

    if ( fabs( res_list[ res_index ] - res ) > 1.0e-2 * res_list[ res_index ] )
        print( WARN, "Can't set resolution to %.0f mG, using %.0f mG "
               "instead.\n", 1.0e3 * res, 1.0e3 * res_list[ res_index ] );

    fsc2_assert( res_index >= LOW_RES && res_index <= HIGH_RES );

    nmr.resolution = res_index;

    if ( FSC2_MODE == EXPERIMENT )
        er035m_set_resolution( nmr.resolution );

    return vars_push( FLOAT_VAR, res_list[ nmr.resolution ] );
}


/*--------------------------------------------------------*
 * While the manual claims something different neither setting nor querying
 * the modulation (and thereby the probe orientation) using the "MO" command
 * does really seem to work. The only way to figure out the modulation in
 * a reliable way seems to be to look at the status information using "PS".
 *--------------------------------------------------------*/

Var_T *
gaussmeter_probe_orientation( Var_T * v )
{
    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    if ( FSC2_MODE == TEST )
        return vars_push( INT_VAR, 1L );

    return vars_push( INT_VAR, ( long ) nmr.probe_orientation );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
gaussmeter_command( Var_T * v )
{
    static char *cmd;


    cmd = NULL;
    vars_check( v, STR_VAR );

    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            er035m_command( cmd );
            T_free( cmd );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( cmd );
            RETHROW;
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_wait( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE == EXPERIMENT && nmr.is_needed )
        fsc2_usleep( ( nmr.resolution * 10 ) * E2_US, UNSET );
    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_upper_search_limit( Var_T * v )
{
    double val;
    long ul;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, ( double ) nmr.upper_search_limit );

    val = get_double( v, "upper search limit" );
    ul = lrnd( ceil( val ) );

    if ( ul > upper_search_limits[ FSC2_MODE == TEST ?
                                   PROBE_TYPE_F1 : nmr.probe_type ] )
    {
        print( SEVERE, "Requested upper search limit too high, changing to "
               "%ld G.\n",
               upper_search_limits[ FSC2_MODE == TEST ?
                                    PROBE_TYPE_F1 : nmr.probe_type ] );
        ul = upper_search_limits[ FSC2_MODE == TEST ?
                                  PROBE_TYPE_F1 : nmr.probe_type ];
    }

    if ( ul <= nmr.lower_search_limit )
    {
        print( FATAL, "Requested upper search limit isn't higher than the "
               "currently active lower search limit.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        er035m_set_upper_search_limit( ul );
    nmr.upper_search_limit = ul;

    return vars_push( FLOAT_VAR, ( double ) ul );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

Var_T *
gaussmeter_lower_search_limit( Var_T * v )
{
    double val;
    long ll;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, ( double ) nmr.lower_search_limit );

    val = get_double( v, "lower search limit" );
    ll = lrnd( floor( val ) );

    if ( ll < lower_search_limits[ FSC2_MODE == TEST ?
                                   PROBE_TYPE_F0 : nmr.probe_type ] )
    {
        print( SEVERE, "Requested lower search limit too low, changing to "
               "%ld G.\n",
               lower_search_limits[ FSC2_MODE == TEST ?
                                    PROBE_TYPE_F0 : nmr.probe_type ] );
        ll = lower_search_limits[ FSC2_MODE == TEST ?
                                  PROBE_TYPE_F0 : nmr.probe_type ];
    }

    if ( ll >= nmr.upper_search_limit )
    {
        print( FATAL, "Requested lower search limit isn't lower than the "
               "currently active upper search limit.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        er035m_set_lower_search_limit( ll );
    nmr.lower_search_limit = ll;

    return vars_push( FLOAT_VAR, ( double ) ll );
}


/*******************************************************/
/*                                                     */
/*            internally used functions                */
/*                                                     */
/*******************************************************/


/*-----------------------------------------------------------------------*
 * er035m_get_field() returns the field value from the gaussmeter - it
 * will return the settled value but will report a failure if gaussmeter
 * isn't in lock state. Another reason for a failure is a field that is
 * too unstable to achieve the requested resolution eventhough the
 * gaussmeter is already in lock state.
 * Take care: If the gaussmeter isn't already in the lock state call
 *            the function find_field() instead.
 *-----------------------------------------------------------------------*/

double
er035m_get_field( void )
{
    char buffer[ 21 ];
    char *state_flag;
    long length;
    long tries = ER035M_MAX_RETRIES;


    /* Repeat asking for field value until it's correct up to LSD -
       it will give up after 'ER035M_S_MAX_RETRIES' retries to avoid
       getting into an infinite loop when the field is too unstable */

    do
    {
        stop_on_user_request( );

        /* Ask gaussmeter to send the current field and read result */

        length = sizeof buffer;
        er035m_talk( "PF\r", buffer, &length );

        /* Disassemble field value and flag showing the state */

        state_flag = strrchr( buffer, ',' );

        if ( ! state_flag )
        {
            print( FATAL, "Undocumented data received.\n" );
            THROW( EXCEPTION );
        }

        /* Report error if gaussmeter isn't in lock state */

        if ( *++state_flag >= '3' )
        {
            print( FATAL, "NMR gaussmeter can't lock on the field.\n" );
            THROW( EXCEPTION );
        }
    } while ( *state_flag != '0' && tries-- > 0 );

    /* If the maximum number of retries was exceeded we've got to give up */

    if ( tries < 0 )
    {
        print( FATAL, "Field is too unstable to be measured with the "
               "current resolution of %.0f mG.\n",
               1.0e3 * res_list[ nmr.resolution ] );
        THROW( EXCEPTION );
    }

    /* Finally interpret the field value string */

    *( state_flag - 1 ) = '\0';
    sscanf( buffer + 2, "%lf", &nmr.field );

    return nmr.field;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static int
er035m_get_resolution( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;


    er035m_talk( "RS\r", buffer, &length );

    if ( length >= 3 )
    {
        switch ( buffer[ 2 ] )
        {
            case '1' :
                return LOW_RES;

            case '2' :
                return MEDIUM_RES;

            case '3' :
                return HIGH_RES;
        }
    }

    print( FATAL, "Undocumented data received.\n" );
    THROW( EXCEPTION );

    return UNDEF_RES;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
er035m_set_resolution( int res_index )
{
    char buf[ 5 ];


    sprintf( buf, "RS%1d\r", res_index + 1 );
    er035m_command( buf );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static long
er035m_get_upper_search_limit( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;


    er035m_talk( "UL\r", buffer, &length );
    return T_atol( buffer );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static long
er035m_get_lower_search_limit( void )
{
    char buffer[ 20 ];
    long length = sizeof buffer;


    er035m_talk( "LL\r", buffer, &length );
    return T_atol( buffer );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
er035m_set_upper_search_limit( long ul )
{
    char cmd[ 40 ];


    snprintf( cmd, sizeof cmd, "UL%ld\r", ul );
    if ( gpib_write( nmr.device, cmd, strlen( cmd ) ) == FAILURE )
        er035m_failure( );
    fsc2_usleep( ER035M_WAIT, UNSET );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static void
er035m_set_lower_search_limit( long ll )
{
    char cmd[ 40 ];


    snprintf( cmd, sizeof cmd, "LL%ld\r", ll );
    if ( gpib_write( nmr.device, cmd, strlen( cmd ) ) == FAILURE )
        er035m_failure( );
    fsc2_usleep( ER035M_WAIT, UNSET );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
er035m_command( const char * cmd )
{
    if ( gpib_write( nmr.device, cmd, strlen( cmd ) ) == FAILURE )
        er035m_failure( );
    fsc2_usleep( ER035M_WAIT, UNSET );

    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
er035m_talk( const char * cmd,
             char *       reply,
             long *       length )
{
    if ( gpib_write( nmr.device, cmd, strlen( cmd ) ) == FAILURE )
        er035m_failure( );

    fsc2_usleep( ER035M_WAIT, UNSET );

    length -= 1;
    if ( gpib_read( nmr.device, reply, length ) == FAILURE )
        er035m_failure( );
    reply[ *length ] = '\0';

    return OK;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
er035m_failure( void )
{
    print( FATAL, "Can't access the NMR gaussmeter.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
