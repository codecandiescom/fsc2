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


#include "fsc2_module.h"
#include "gentec_maestro.conf"
#include <sys/time.h>
#include <float.h>

#if defined USE_SERIAL && defined USE_LAN
#error "Error in configuration file, on;e one of 'USE_SERIAL' and 'USE_LAN' can defined."
#endif

#if ! defined USE_SERIAL && ! defined USE_LAN
#error "Neither 'USE_SERIAL' nor 'USE_LAN' are defined in configuration file."
#endif


#if defined USE_SERIAL
#include "serial.h"

#define SERIAL_BAUDRATE B115200

enum {
    SERIAL_INIT,
    SERIAL_READ,
    SERIAL_WRITE,
    SERIAL_EXIT
};

static bool gentec_maestro_serial_comm( int type,
                                        ... );

#else                    /* USE_LAN defined */
#include "lan.h"
#endif


/* Hook functions */

int gentec_maestro_init_hook( void );
int gentec_maestro_test_hook( void );
int gentec_maestro_end_of_test_hook( void );
int gentec_maestro_exp_hook( void );
int gentec_maestro_end_of_exp_hook( void );


/* EDL functions */

Var_T * powermeter_name( Var_T * v  UNUSED_ARG );
Var_T * powermeter_detector_name( Var_T * v  UNUSED_ARG );
Var_T * powermeter_scale( Var_T * v );
Var_T * powermeter_get_scale_limits( Var_T * v );
Var_T * powermeter_autoscale( Var_T * v );
Var_T * powermeter_trigger_level( Var_T * v );
Var_T * powermeter_wavelength( Var_T * v );
Var_T * powermeter_get_wavelength_limits( Var_T * v );
Var_T * powermeter_get_reading( Var_T * v );
Var_T * powermeter_anticipation( Var_T * v );
Var_T * powermeter_attenuator( Var_T * v );
Var_T * powermeter_zero_offset( Var_T * v );
Var_T * powermeter_multiplier( Var_T * v );
Var_T * powermeter_offset( Var_T * v );
Var_T * powermeter_get_laser_repetition_frequency( Var_T * v );
Var_T * powermeter_analog_output( Var_T * v );
Var_T * powermeter_statistics( Var_T * v );


/* Internal functions */

static bool gentec_maestro_init( void );
static double gentec_maestro_set_scale( int index );
static double gentec_maestro_get_scale( void );
static bool gentec_maestro_set_autoscale( bool on_off );
static bool gentec_maestro_get_autoscale( void );
static double gentec_maestro_set_trigger_level( double level );
static double gentec_maestro_get_trigger_level( void );
static int gentec_maestro_get_mode( void );
static double gentec_maestro_get_current_value( void );
static bool gentec_maestro_check_for_new_value( void );
static bool gentec_maestro_continuous_transmission( bool on_off );
static double gentec_maestro_get_laser_frequency( void );
static bool gentec_maestro_set_joulemeter_binary_mode( bool on_off );
#if 0
static bool gentec_maestro_get_joulemeter_binary_mode( void );
#endif
static bool gentec_maestro_set_analog_output( bool on_off );
static double gentec_maestro_set_wavelength( long int wl );
static double gentec_maestro_get_wavelength( void );
static bool gentec_maestro_set_anticipation( bool on_off );
static bool gentec_maestro_get_anticipation( void );
static bool gentec_maestro_zero_offset( bool on_off );
static bool gentec_maestro_test_zero_offset( void );
static double gentec_maestro_set_user_multiplier( double mul );
static double gentec_maestro_get_user_multiplier( void );
static double gentec_maestro_set_user_offset( double offset );
static double gentec_maestro_get_user_offset( void );
static void gentec_maestro_get_statistics( double * data );
#if 0
static bool gentec_maestro_set_energy_mode( bool on_off );
#endif
static bool gentec_maestro_set_attenuator( bool on_off );
static bool gentec_maestro_get_attenuator( void );
static unsigned long gentec_maestro_status_entry_to_int( const char * e );
static double gentec_maestro_status_entry_to_float( const char * e );
static char * gentec_maestro_status_entry_to_string( const char * e );
static void gentec_maestro_get_extended_status( void );
static double gentec_maestro_index_to_scale( int index );
static int gentec_maestro_scale_to_index( double   scale,
                                          bool   * no_fit );
static char const * gentec_maestro_format( double v );
static const char * gentec_maestro_pretty_print_scale( int index );
static void gentec_maestro_open( void );
static void gentec_maestro_close( void );
static void gentec_maestro_command( const char * cmd );
static long gentec_maestro_talk( const char * cmd,
                                 char       * reply,
					             long         length );
static long gentec_maestro_read( char * buf,
                                 long   length );
static void gentec_maestro_failure( void );


/* Global variables */

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

typedef struct
{
    int handle;

    int mode;
    bool mode_has_been_set;

    int scale_index;
    int min_scale_index;
    int max_scale_index;

    bool scale_index_has_been_set;
    int min_test_scale_index;
    int max_test_scale_index;

    bool att_is_available;
    bool att_is_on;
    bool att_has_been_set;

    long int wavelength;           /* in nm */
    bool wavelength_has_been_set;

    long int min_wavelength;
    long int max_wavelength;
    long int min_wavelength_with_att;
    long int max_wavelength_with_att;

    long int min_test_wavelength;
    long int max_test_wavelength;
    long int min_test_wavelength_with_att;
    long int max_test_wavelength_with_att;
    long int min_test_wavelength_unknown;
    long int max_test_wavelength_unknown;
    
    char head_name[ 33 ];

    double trigger_level;      /* between 0.1 and 99.9 (in percent) */
    bool trigger_level_has_been_set;

    bool autoscale_is_on;
    bool autoscale_has_been_set;

    bool anticipation_is_on;
    bool anticipation_has_been_set;

    bool zero_offset_is_on;
    bool zero_offset_has_been_set;

    double user_multiplier;
    bool user_multiplier_has_been_set;

    double user_offset;
    bool user_offset_has_been_set;

    bool analog_output_is_on;
    bool analog_output_has_been_set;

    bool continuous_transmission_is_on;
    bool energy_mode_is_on;

#if defined USE_SERIAL
    struct termios * tio;        /* serial port terminal interface structure */
#endif
} Gentec_Maestro;


static Gentec_Maestro gentec_maestro,
                      gentec_maestro_test;
static Gentec_Maestro * gm;


/* Defines */

#define MAX_SCALE            41
#define MIN_TRIGGER_LEVEL    0.1
#define MAX_TRIGGER_LEVEL    99.9
#define MIN_USER_MULTIPLIER  -FLT_MAX      /* not documented */
#define MAX_USER_MULTIPLIER  FLT_MAX       /* not documented */
#define MIN_USER_OFFSET      -FLT_MAX      /* not documented */
#define MAX_USER_OFFSET      FLT_MAX       /* not documented */


enum {
    POWER_MODE  = 0,        /* power in W  */
    ENERGY_MODE = 1,        /* energy in J */
    SSE_MODE    = 2,        /* single shot energy in J */
    DBM_MODE    = 6,        /* power in dBm */
    NO_DETECTOR = 7         /* no probe head connected */
};


#define TEST_MODE             ENERGY_MODE
#define TEST_VALUE            2.45e-4
#define TEST_SCALE_INDEX      17
#define TEST_WAVELENGTH       650
#define TEST_HEAD_NAME        "QE12LP-S-MB"
#define TEST_TRIGGER_LEVEL    2.0
#define TEST_USER_MULTIPLIER  1.0
#define TEST_USER_OFFSET      0.0
#define TEST_LASER_FREQUENCY  100.0


/*---------------------------------------------------*
 * Init hook, called when the module is loaded
 *---------------------------------------------------*/

int
gentec_maestro_init_hook( void )
{
    gm = &gentec_maestro;

    /* If communication is via USB request the necessary serial port (throws
       exception on failure), for communication via LAN set global variable
       to indicate that the device is controlled via LAN */

#if defined USE_SERIAL
    gm->handle = fsc2_request_serial_port( DEVICE_FILE, DEVICE_NAME );
#else
    Need_LAN = true;
    gm->handle = -1;
#endif

    /* Initialize the structure for the device as good as possible */

    gm->mode = TEST_MODE;
    gm->mode_has_been_set = false;

    gm->scale_index = TEST_SCALE_INDEX;;
    gm->min_scale_index = 0;
    gm->max_scale_index = MAX_SCALE;

    gm->scale_index_has_been_set = false;
    gm->min_test_scale_index = MAX_SCALE;
    gm->max_test_scale_index = 0;

    gm->att_is_available    = true;;
    gm->att_is_on        = false;
    gm->att_has_been_set = false;

    gm->wavelength = TEST_WAVELENGTH;
    gm->min_wavelength = 0;
    gm->max_wavelength = LONG_MAX;
    gm->min_wavelength_with_att = 0;
    gm->max_wavelength_with_att = LONG_MAX;

    gm->wavelength_has_been_set = false;
    gm->min_test_wavelength = LONG_MAX;
    gm->max_test_wavelength = 0;
    gm->min_test_wavelength_with_att = LONG_MAX;
    gm->max_test_wavelength_with_att = 0;
    gm->min_test_wavelength_unknown = LONG_MAX;
    gm->max_test_wavelength_unknown = 0;
    
    strcpy( gm->head_name, TEST_HEAD_NAME );

    gm->trigger_level = TEST_TRIGGER_LEVEL;
    gm->trigger_level_has_been_set = false;

    gm->autoscale_is_on = true;
    gm->autoscale_has_been_set = false;

    gm->anticipation_is_on = false;
    gm->anticipation_has_been_set = false;

    gm->zero_offset_is_on = false;
    gm->zero_offset_has_been_set = false;

    gm->user_multiplier = TEST_USER_MULTIPLIER;
    gm->user_multiplier_has_been_set = false;

    gm->user_offset = TEST_USER_OFFSET;
    gm->user_offset_has_been_set = false;

    gm->analog_output_is_on = false;
    gm->analog_output_has_been_set = false;

    return 1;
}


/*---------------------------------------------------*
 * Test hook, called at the start of the test run
 *---------------------------------------------------*/

int
gentec_maestro_test_hook( void )
{
    gentec_maestro_test = gentec_maestro;
    strcpy( gentec_maestro_test.head_name, gentec_maestro.head_name );

    gm = &gentec_maestro_test;

    gm->mode_has_been_set = false;
    gm->scale_index_has_been_set = false;
    gm->att_has_been_set = false;
    gm->wavelength_has_been_set = false;
    gm->trigger_level_has_been_set = false;
    gm->anticipation_has_been_set = false;
    gm->zero_offset_has_been_set = false;
    gm->user_multiplier_has_been_set = false;
    gm->user_offset_has_been_set = false;
    gm->analog_output_has_been_set = false;

    return 1;
}


/*---------------------------------------------------*
 * End-of-test hook, called after the end of the test run
 *---------------------------------------------------*/

int
gentec_maestro_end_of_test_hook( void )
{
    gm = &gentec_maestro;
    return 1;
}


/*---------------------------------------------------*
 * Exp hook, called at the start of the experiment
 *---------------------------------------------------*/

int
gentec_maestro_exp_hook( void )
{
    bool is_ok = false;

    TRY
    {
        is_ok = gentec_maestro_init( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        is_ok = false;
        gentec_maestro_close( );
    }

    return is_ok;
}


/*---------------------------------------------------*
 * End-of-exp hook, called at the end of the experiment
 *---------------------------------------------------*/

int
gentec_maestro_end_of_exp_hook( void )
{
    gentec_maestro_close( );
    return 1;
}


/*-------------------------------------------------------*
 * Function returns a string with the name of the device
 *-------------------------------------------------------*/

Var_T *
powermeter_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}



/*-------------------------------------------------------*
 * Function returns a string with the name of the device
 *-------------------------------------------------------*/

Var_T *
powermeter_detector_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, gm->head_name );
}


/*-------------------------------------------------------*
 * Sets a new scale (note: this switches autoscaling off)
 *-------------------------------------------------------*/

Var_T *
powermeter_scale( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->scale_index_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_scale( );
        return vars_push( FLOAT_VAR,
                          gentec_maestro_index_to_scale( gm->scale_index ) );
    }

    double scale = get_double( v, "scale" );
    too_many_arguments( v );

    if ( scale <= 0 )
    {
        print( FATAL, "Invalid negativ or zero scale.\n" );
        THROW( EXCEPTION );
    }

    bool no_fit = true;
    int idx = gentec_maestro_scale_to_index( scale, &no_fit );

    if ( idx > MAX_SCALE )
    {
        print( FATAL, "Requested scale larger than possible, maximum scale "
               "for all heads is %s.\n",
               gentec_maestro_pretty_print_scale( MAX_SCALE ) );
        THROW( EXCEPTION );
    }

    if ( no_fit )
        print( WARN, "Requested scale doesn't exist, using nearest larger "
               "one of %s.\n", gentec_maestro_pretty_print_scale( idx ) );

    if ( idx < gm->min_scale_index || idx > gm->max_scale_index )
    {
        char smin[ 10 ];

        strcpy( smin,
                gentec_maestro_pretty_print_scale( gm->min_scale_index ) );

        print( FATAL, "Invalid scale for connected head, must be between %s "
               "and %s.\n", smin,
               gentec_maestro_pretty_print_scale( gm->max_scale_index ) );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_scale( idx );
    else
    {
        gm->scale_index = idx;
        gm->scale_index_has_been_set = true;

        gm->min_scale_index = d_min( gm->min_scale_index, idx );
        gm->max_scale_index = d_max( gm->max_scale_index, idx );
    }

    return vars_push( FLOAT_VAR,
                      gentec_maestro_index_to_scale( gm->scale_index ) );
}


/*-------------------------------------------------------*
 * Returns an array with the minimum and maximum scale setting
 * for the currently connected head.
 *-------------------------------------------------------*/

Var_T *
powermeter_get_scale_limits( Var_T * v )
{
    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    double mm[ ] = { gentec_maestro_index_to_scale( gm->min_scale_index ),
                     gentec_maestro_index_to_scale( gm->max_scale_index ) };

    return vars_push( FLOAT_ARR, mm, 2L );
}


/*-------------------------------------------------------*
 * Switch autoscale on or off

XXXXXXXXX  It's unclear from the documention how the scale setting
XXXXXXXXX  interacts with the autoscale setting

 *-------------------------------------------------------*/

Var_T *
powermeter_autoscale( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->autoscale_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_autoscale( );

        return vars_push( INT_VAR, gm->autoscale_is_on ? 1L : 0L );
    }

    bool as = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_autoscale( as );
    else  
  {
        gm->autoscale_is_on = as;
        gm->autoscale_has_been_set = true;
    }

    return vars_push( INT_VAR, gm->autoscale_is_on ? 1L : 0L );
}


/*-------------------------------------------------------*
 * Query or set trigger level (arhument must be in percent)
 *-------------------------------------------------------*/

Var_T *
powermeter_trigger_level( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->trigger_level_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_trigger_level( );
        return vars_push( FLOAT_VAR, gm->trigger_level );
    }

    double level = get_double( v, NULL );
    too_many_arguments( v );

    if ( level < 0 || level > 100 )
    {
        print( FATAL, "Invalid trigger level percentage.\n" );
        THROW( EXCEPTION );
    }

    if ( level < MIN_TRIGGER_LEVEL )
        print( WARN, "Trigger level too low, adjusting to %.1f%%.\n",
               level = MIN_TRIGGER_LEVEL );
    if ( level > MAX_TRIGGER_LEVEL )
        print( WARN, "Trigger level too high, adjusting to %.1f%%.\n",
               level = MAX_TRIGGER_LEVEL );

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_trigger_level( level );
    else
    {
        gm->trigger_level = 0.1 * lrnd( 10 * level );
        gm->trigger_level_has_been_set = true;
    }

    if ( fabs( level - gm->trigger_level ) > 0.01 )
        print( WARN, "Adjusted trigger level to %.1f%%.\n",
               gm->trigger_level );

    return vars_push( FLOAT_VAR, gm->trigger_level );
}


/*-------------------------------------------------------*
 * Query or set wavelength
 *-------------------------------------------------------*/

Var_T *
powermeter_wavelength( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_wavelength( );
        return vars_push( FLOAT_VAR, 1.0e-9 * gm->wavelength );
    }

    double wl = get_double( v, NULL );
    too_many_arguments( v );

    if ( wl <= 0 )
    {
        print( FATAL, "Invalid negativ or zero wavelength.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT && gentec_maestro.att_is_available )
        gentec_maestro_get_attenuator( );

    long int wl_nm = lrnd( 1.0e9 * wl );

    if ( fabs( wl_nm - 1.0e9 * wl ) > 0.1 )
        print( WARN, "Wavelength has been adjusted to %ld nm.\n", wl_nm );

    if ( FSC2_MODE == EXPERIMENT )
    {
        if (    ! gm->att_is_on
             && ( wl_nm < gm->min_wavelength || wl_nm > gm->max_wavelength ) )
        {
            print( FATAL, "Requested wavelength is out of range, must be "
                   "(with attenuator off) between %ld nm and %ld nm.\n",
                   gm->min_wavelength, gm->max_wavelength );
            THROW( EXCEPTION );
        }

        if (    gm->att_is_on
             && (    wl_nm < gm->min_wavelength_with_att 
                  || wl_nm > gm->max_wavelength_with_att ) )
        {
            print( FATAL, "Requested wavelength is out of range, must be "
                   "(with attenuator on) between %ld nm and %ld nm.\n",
                   gm->min_wavelength_with_att, gm->max_wavelength_with_att );
            THROW( EXCEPTION );
        }

        gentec_maestro_set_wavelength( wl_nm );
    }
    else
    {
        gm->wavelength = wl_nm;

        if ( ! gm->att_has_been_set )
        {
            gm->min_test_wavelength_unknown =
                                l_min( wl_nm, gm->min_test_wavelength_unknown );
            gm->max_test_wavelength_unknown =
                                l_max( wl_nm, gm->max_test_wavelength_unknown );
        }
        else if ( ! gm->att_is_on )
        {
            gm->min_test_wavelength = l_min( wl_nm, gm->min_test_wavelength );
            gm->max_test_wavelength = l_max( wl_nm, gm->max_test_wavelength );
        }
        else
        {
            gm->min_test_wavelength_with_att =
                              l_min( wl_nm, gm->min_test_wavelength_with_att );
            gm->max_test_wavelength_with_att =
                              l_max( wl_nm, gm->max_test_wavelength_with_att );
        }

        gm->wavelength_has_been_set = true;
    }

    return vars_push( FLOAT_VAR, 1.0e-9 * wl_nm );
}


/*---------------------------------------------------*
 * Returns an array with the minimum and maximum wavelength
 * for the head. If a true argument is passed to the function
 * and an attenuator is avaialble the wavelength limits with
 * attenuator on are returned.
 *---------------------------------------------------*/

Var_T *
powermeter_get_wavelength_limits( Var_T * v )
{
    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    bool with_att = false;
    if ( v )
        with_att = get_boolean( v );

    if ( with_att && ! gm->att_is_available )
    {
        print( FATAL, "No attenuator available.\n" );
        THROW( EXCEPTION );
    }

    double wls[ 2 ];

    if ( ! with_att || ! gm->att_is_available )
    {
        wls[ 0 ] = 1.0e-9 * gm->min_wavelength;
        wls[ 1 ] = 1.0e-9 * gm->max_wavelength;
    }
    else
    {
        wls[ 0 ] = 1.0e-9 * gm->min_wavelength_with_att;
        wls[ 1 ] = 1.0e-9 * gm->max_wavelength_with_att;
    }

    return vars_push( FLOAT_ARR, wls, 2L );
}


/*---------------------------------------------------*
 * Get a value from the device - if called with a true
 * argument make sure it's a new value. An optional
 * third argument is a maximum time (in seconds) we're
 * prepared to wait for a new value.
 *---------------------------------------------------*/

Var_T *
powermeter_get_reading( Var_T * v )
{
    bool wait_for_new = false;
    long max_wait = 0;
    bool upper_wait_limit = false;

    if ( v )
    {
        wait_for_new = true;

        double mw = get_double( v, NULL );
        if ( mw > 0 )
        {
            max_wait = lrnd( 1000000 * mw );
            upper_wait_limit = true;
        }

        too_many_arguments( v );
    }

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, TEST_VALUE );

    /* If were supposed to wait for a new value and there's none
       recheck with twice the laser frequency (or all 100 ms if
       the laser frequency is 0), but never slower than every
       100 ms. Also give the user a chance to abort. */

    if ( wait_for_new )
    {
        long delay = 100000;
        double lf;

        if ( ( lf = gentec_maestro_get_laser_frequency( ) ) != 0 )
            delay = 500000 / lf;      /* half the time between laser shots */

        delay = l_min( delay, 100000 );
        
        if ( upper_wait_limit && max_wait < delay )
            delay = max_wait;

        struct timeval before;
        gettimeofday( &before, NULL );

        while ( ! gentec_maestro_check_for_new_value( ) )
        {
            struct timeval after;
            gettimeofday( &after, NULL );
            max_wait -=   1000000 * ( after.tv_sec  - before.tv_sec )
                        +           ( after.tv_usec - before.tv_usec );
            before = after;

            if ( upper_wait_limit && max_wait <= 0 )
            {
                print( FATAL, "Device didn't measure a new value within "
                       "the requested time interval of %.3f ms.\n",
                       1.0e-3 * max_wait );
                THROW( EXCEPTION );
            }

            delay = l_min( delay, max_wait );
            fsc2_usleep( delay, false );
            stop_on_user_request( );
        }
    }

    return vars_push( FLOAT_VAR, gentec_maestro_get_current_value( ) );
}


/*-------------------------------------------------------*
 * Switch anticipation on or off
 *-------------------------------------------------------*/

Var_T *
powermeter_anticipation( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->anticipation_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_anticipation( );
        return vars_push( INT_VAR, gm->anticipation_is_on ? 1L : 0L );
    }

    bool ac = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_anticipation( ac );
    else  
  {
        gm->anticipation_is_on = ac;
        gm->anticipation_has_been_set = true;
    }

    return vars_push( INT_VAR, gm->anticipation_is_on ? 1L : 0L );
}


/*-------------------------------------------------------*
 * Switch attenuation on or off (if possible). Also check
 * if wavelength is in range
 *-------------------------------------------------------*/

Var_T *
powermeter_attenuator( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->att_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
        {
            if ( ! gm->att_is_available )
                return vars_push( INT_VAR, -1L );

            gentec_maestro_get_attenuator( );
        }

        return vars_push( INT_VAR, gm->att_is_on ? 1L : 0L );
    }

    bool att = get_boolean( v );
    too_many_arguments( v );

    /* When the experiment is running we definitely know if an attenuator
       is avialable, so first test if it is. We also don't need to bother
       with further tests if it's already in the requested state. */

    long int wl;

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( ! gm->att_is_available )
        {
            if ( att )
            {
                print( FATAL, "No attenuator is available.\n" );
                THROW( EXCEPTION );
            }

            print( WARN, "No attenuator is available.\n" );
            return vars_push( INT_VAR, 0L );
        }

        if ( att == gentec_maestro_get_attenuator( ) )
            return vars_push( INT_VAR, att ? 1L : 0L );

        wl = gentec_maestro_get_wavelength( );
    }
    else
        wl = gm->wavelength;

    if (    (    att
              && (    wl < gm->min_wavelength_with_att
                   || wl < gm->max_wavelength_with_att ) )
         || (    ! att
              && (    wl < gm->min_wavelength
                   || wl > gm->max_wavelength ) ) )
    {
        print( FATAL, "Can't switch attenuator %s, currently selected "
               "wavelength out of range.\n", att ? "on" : "off" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_attenuator( att );
    else
    {
        gm->att_is_on = att;
        gm->att_has_been_set = true;
    }

    return vars_push( INT_VAR, gm->att_is_on ? 1L : 0L );
}


/*---------------------------------------------------*
 * Switch zero offset on or off or query if it's on or off

XXXXXXXXX How to set zero offset for different detector types
XXXXXXXXX is unclear (or how to detect what kind of detector)
XXXXXXXXX on has. There also doesn't seem to be a commond to
XXXXXXXXX unset it for photodiodes...

 *---------------------------------------------------*/

Var_T *
powermeter_zero_offset( Var_T * v )
{
    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->zero_offset_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_test_zero_offset( );

        return vars_push( INT_VAR, gm->zero_offset_is_on ? 1L : 0L );
    }

    bool zo = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION && zo )
    {
        print( FATAL, "Zero offset can't be switched on from VARAUBLES or "
               "PREPARATION sectrion.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_zero_offset( zo );
    else
    {
        gm->zero_offset_is_on = zo;
        gm->zero_offset_has_been_set = true;
    }

    return vars_push( INT_VAR, gm->zero_offset_is_on ? 1L : 0L );
}


/*---------------------------------------------------*
 * Set or query the user multiplier

XXXXXXXXX  It's undocumented if there are any restriction on
XXXXXXXXX  the range the multiplier can be set to - needs to be
XXXXXXXXX  tested (would make sense if only positive values would
XXXXXXXXX  be allowed

 *---------------------------------------------------*/

Var_T *
powermeter_multiplier( Var_T * v )
{
    if ( !v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->user_multiplier_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_user_multiplier( );

        return vars_push( FLOAT_VAR, gm->user_multiplier );
    }

    double mul = get_double( v, NULL );
    too_many_arguments( v );

    if ( mul < MIN_USER_MULTIPLIER || mul > MAX_USER_MULTIPLIER )
    {
        print( FATAL, "Multiplier out of range, must be between %g and %g.\n",
               MIN_USER_MULTIPLIER, MAX_USER_MULTIPLIER );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_user_multiplier( mul );
    else
    {
        gm->user_multiplier = strtod( gentec_maestro_format( mul ), NULL );
        gm->user_multiplier_has_been_set = true;
    }

    if ( mul != 0 && fabs( ( mul - gm->user_multiplier ) / mul ) >= 0.01 )
        print( WARN, "Multiplier was ajusted to %g.\n", gm->user_multiplier );

    return vars_push( FLOAT_VAR, gm->user_multiplier );
}


/*---------------------------------------------------*
 * Set or query the user offset

XXXXXXXXX  It's undocumented if there are any restriction on
XXXXXXXXX  the range the offset can be set to - needs to be
XXXXXXXXX  tested

 *---------------------------------------------------*/

Var_T *
powermeter_offset( Var_T * v )
{
    if ( !v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->user_offset_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_user_offset( );

        return vars_push( FLOAT_VAR, gm->user_offset );
    }

    double offset = get_double( v, NULL );
    too_many_arguments( v );

    if ( offset < MIN_USER_OFFSET || offset > MAX_USER_OFFSET )
    {
        print( FATAL, "Offset out of range, must be between %g and %g.\n",
               MIN_USER_OFFSET, MAX_USER_OFFSET );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_user_offset( offset );
    else
    {
        gm->user_offset = strtod( gentec_maestro_format( offset ), NULL );
        gm->user_offset_has_been_set = true;
    }

    if ( offset != 0 && fabs( ( offset - gm->user_offset ) / offset ) >= 0.01 )
        print( WARN, "Offset was ajusted to %g.\n", gm->user_offset );

    return vars_push( FLOAT_VAR, gm->user_offset );
}


/*---------------------------------------------------*
 * Returns the laser repetition frequency
 *---------------------------------------------------*/

Var_T *
powermeter_get_laser_repetition_frequency( Var_T * v )
{
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        return vars_push( FLOAT_VAR, gentec_maestro_get_laser_frequency( ) );

    return vars_push( FLOAT_VAR, TEST_LASER_FREQUENCY );
}


/*---------------------------------------------------*
 * Queries or sets the analog output (a voltage proportional
 * to power or enerhy)
 *---------------------------------------------------*/

Var_T *
powermeter_analog_output( Var_T * v )
{
    if ( ! v )
    {
        print( FATAL, "State of analog output can't be queried.\n" );
        THROW( EXCEPTION );
    }

    bool on_off = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_analog_output( on_off );
    else
    {
        gm->analog_output_is_on = on_off;
        gm->analog_output_has_been_set = true;
    }

    return vars_push( INT_VAR, on_off ? 1L : 0L );
}


/*---------------------------------------------------*
 * Returns the statistics data the device collected as
 * an (floating point) array with 12 elements
 *---------------------------------------------------*/

Var_T *
powermeter_statistics( Var_T * v  UNUSED_ARG )
{
    double data[ 12 ] = { 1.8e-5,      // current value
                          3.2e-5,      // maximum
                          1.2e-6,      // minimum
                          1.7e-5,      // average
                          2.1e-6,      // standard deviation
                          1.0e-7,      // RMS stability
                          8.2e-6,      // PTP stability
                          2132,        // pulse number
                          10000,       // total pulses
                          1.7e-5,      // average power
                          20.012,      // rep. rate
                          0.000018     // uncorrected
                        };

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_get_statistics( data );
    return vars_push( FLOAT_ARR, data, 12 );
}


/*---------------------------------------------------*
 * Sets a new scale given the corresponding index
 *---------------------------------------------------*/

static
bool
gentec_maestro_init( void )
{
    Gentec_Maestro * gmt = &gentec_maestro_test;

    /* Try to open a connection to the device (throws exception on failure) */

    gentec_maestro_open( );

    /* Store all properties that might have been set during the preparation
       phase in the structure used for tests */

    gmt->mode                = gm->mode;
    gmt->scale_index         = gm->scale_index;
    gmt->att_is_on           = gm->att_is_on;
    gmt->wavelength          = gm->wavelength;
    gmt->trigger_level       = gm->trigger_level;
    gmt->autoscale_is_on     = gm->autoscale_is_on;
    gmt->anticipation_is_on  = gm->anticipation_is_on;
    gmt->zero_offset_is_on   = gm->zero_offset_is_on;
    gmt->user_multiplier     = gm->user_multiplier;
    gmt->user_offset         = gm->user_offset;
    gmt->analog_output_is_on = gm->analog_output_is_on;

    /* Disable continupus transmission of data, binary transfers and get the
       currect settings of the device */
    
    gentec_maestro_continuous_transmission( false );
    gentec_maestro_set_joulemeter_binary_mode( false );
    gentec_maestro_get_extended_status( );

    /* Check if there's a detector head connected at all - without one
       all the rest is futile. */

    gentec_maestro_get_mode( );
    if ( gm->mode == NO_DETECTOR )
    {
        print( FATAL, "No detector head is connected.\n" );
        return false;
    }

    /* Now check if settings made in the preparation phase were valid.
       First check the scale setting.*/

    if (    gm->scale_index_has_been_set
         && (    gmt->scale_index < gm->min_scale_index
              || gmt->scale_index > gm->max_scale_index ) )
    {
        print( FATAL, "During preparations a scale was requested that "
               "can't be set for the connected head.\n" );
        return false;
    }

    /* Check if the attenuator was set, but there isn't one. */

    if ( ! gm->att_is_available && gm->att_has_been_set )
    {
        print( FATAL, "During preparations the attenuator was set, "
               "but there's no attenuator.\n" );
        return false;
    }   

    /* Check if the last set wavelength fits the allowed wavelength range
       (here we have to distinguish between attenuator on or off and if
       the attenuatior has been set during the preparations or not) */

    if ( gm->wavelength_has_been_set )
    {
        if (    ( ! gm->att_has_been_set && gm->att_is_on )
             || (   gm->att_has_been_set && gmt->att_is_on ) )
        {
            if (    gmt->wavelength < gm->min_wavelength_with_att
                 || gmt->wavelength > gm->min_wavelength_with_att )
            {
                print( FATAL, "During preparations a wavelength (with "
                       "attenuator on) was set that is out of range.\n" );
                return false;
            }
        }
        else if (    gmt->wavelength < gm->min_wavelength
                  || gmt->wavelength > gm->min_wavelength )
        {
            print( FATAL, "During preparations a wavelength (with "
                   " attenuator off) was set that is out of range.\n" );
            return false;
        }
    }

    /* Now the checks for things set during the test phase. Again we start
       with the scale setting. */

    if (    gmt->scale_index_has_been_set
         && (    gmt->min_test_scale_index < gm->min_scale_index
              || gmt->max_test_scale_index > gm->max_scale_index ) )
    {
        print( FATAL, "During test a scale was requested that can't be set "
               "for the connected head.\n" );
        return false;
    }

    /* Check for the attenuator having been set despite none available */

    if ( ! gm->att_is_available && gmt->att_has_been_set )
    {
        print( FATAL, "Durinf the the attenuator was set, but there's no "
               " attenuator.\n" );
        return false;
    }   

    if (    gmt->max_test_wavelength != 0
         && (    gmt->min_test_wavelength < gm->min_wavelength
              || gmt->max_test_wavelength > gm->max_wavelength ) )
    {
        print( FATAL, "During tests an out-of-range wavelength was "
               "requested.\n" );
        return false;
    }

    /* Wavelength checks:
       a) If gmt->max_test_wavelength is not 0 a wavelength with the attenuator
          explicitely set to off has been set.
       b) If gmt->max_test_wavelength_with_att is not 0 a wavelength with
          the attenuator explicitely set to on has been set.
       c) If gmt->max_test_wavelength_unknown is not 0 a wavelength has been
          set without setting the attenuator - for checking we need to test
          if the attenuator is on or will be set on due to a preparations
          request. */

    if (    gmt->max_test_wavelength != 0
         && (    gmt->min_test_wavelength < gm->min_wavelength
              || gmt->max_test_wavelength > gm->max_wavelength ) )
    {
        print( FATAL, "During tests an out of range wavelength was "
               "requested.\n" );
        return false;
    }

    if (    gmt->max_test_wavelength_with_att != 0
         && (    gmt->min_test_wavelength_with_att <
                                                gm->min_wavelength_with_att
              || gmt->max_test_wavelength_with_att >
                                                gm->max_wavelength_with_att ) )
    {
        print( FATAL, "During tests an out of range wavelength was "
               "requested.\n" );
        return false;
    }

    if ( gmt->max_test_wavelength_unknown != 0 )
    {
        if (    ( ! gm->att_has_been_set && gm->att_is_on )
             || ( gm->att_has_been_set && gmt->att_is_on ) )
        {
            if (    gmt->min_test_wavelength_unknown <
                                                gm->min_wavelength_with_att
                 || gmt->max_test_wavelength_unknown <
                                                 gm->max_wavelength_with_att )

            {
                print( FATAL, "During tests an out-of-range wavelength was "
                       "requested.\n" );
                return false;
            }
        }
        else if (    gmt->min_test_wavelength_unknown < gm->min_wavelength
                  || gmt->max_test_wavelength_unknown > gm->max_wavelength )
        {
            print( FATAL, "During tests an out-of-range wavelength was "
                   "requested.\n" );
            return false;
        }
    }

    /* Ok, everything seems to be fine;-) Start setting up the device,
       starting with the  autoscaling and scaling. Scale is set first if
       autoscale will be switched off (to avoid potential out of scale
       energies), otherwise autoscale is set first, then the scale. */

    bool set_autoscale =    gm->autoscale_has_been_set
                         && gm->autoscale_is_on != gmt->autoscale_is_on;
    bool set_scale     =    gm->scale_index_has_been_set
                         && gm->scale_index != gmt->scale_index;

    if ( set_autoscale && ! set_scale )
        gentec_maestro_set_autoscale( false );
    else if ( ! set_autoscale && set_scale )
        gentec_maestro_set_scale( gm->scale_index );
    else if ( set_autoscale && set_scale )
    {
        if ( gm->autoscale_is_on )
        {
            gentec_maestro_set_scale( gmt->scale_index );
            gentec_maestro_set_autoscale( false );
        }
        else
        {
            gentec_maestro_set_autoscale( true );
            gentec_maestro_set_scale( gmt->scale_index );
        }
    }

    /* Next we set wavelength and attenuator. Since not all wavelengths
       might be allowed with the current and requested attenuator setting
       we have to make sure we do it in the right sequence. */

    bool set_wavelength =    gm->wavelength_has_been_set
                          && gm->wavelength != gmt->wavelength;
    bool set_att        =    gm->att_has_been_set
                          && gm->att_is_on != gmt->att_is_on;

    if ( set_wavelength && ! set_att )
        gentec_maestro_set_wavelength( gmt->wavelength );
    else if ( ! set_wavelength && set_att )
        gentec_maestro_set_attenuator( gmt->att_is_on );
    else if ( set_wavelength && set_att )
    {
        if (    (    gm->att_is_on
                  && (    gm->wavelength < gm->min_wavelength
                       || gm->wavelength > gm->max_wavelength ) )
             || (    ! gm->att_is_on
                  && (    gm->wavelength < gm->min_wavelength_with_att
                       || gm->wavelength > gm->max_wavelength_with_att ) ) )
        {
            gentec_maestro_set_wavelength( gmt->wavelength );
            gentec_maestro_set_attenuator( gmt->att_is_on );
        }
        else
        {
            gentec_maestro_set_attenuator( gmt->att_is_on );
            gentec_maestro_set_wavelength( gmt->wavelength );
        }
    }

    if ( gm->trigger_level_has_been_set
         && gm->trigger_level != gmt->trigger_level )
        gentec_maestro_set_trigger_level( gmt->trigger_level );

    if (    gm->anticipation_has_been_set
         && gm->anticipation_is_on != gmt->anticipation_is_on )
        gentec_maestro_set_anticipation( gmt->anticipation_is_on );

    if (    gm->user_multiplier_has_been_set
         && gm->user_multiplier != gmt->user_multiplier )
        gentec_maestro_set_user_multiplier( gmt->user_multiplier );

    if (    gm->user_offset_has_been_set
         && gm->user_offset != gmt->user_offset )
        gentec_maestro_set_user_offset( gmt->user_offset );

    if ( gm->analog_output_has_been_set )
        gentec_maestro_set_analog_output( gmt->analog_output_is_on );

    return true;
}


/*---------------------------------------------------*
 * Sets a new scale given the corresponding index
 *---------------------------------------------------*/

static
double
gentec_maestro_set_scale( int index )
{
	fsc2_assert(    index >= gentec_maestro.min_scale_index
                 && index <= gentec_maestro.max_scale_index );

	char cmd[ 10 ];
	sprintf( cmd, "*SCS%02d", index );
	gentec_maestro_command( cmd );

    /* Note: setting a scale unsets autoscaling */

    gentec_maestro.autoscale_is_on = false;
	return gentec_maestro_index_to_scale( gentec_maestro.scale_index = index );
}


/*---------------------------------------------------*
 * Returns the current scale setting (in J or Watt)
 *---------------------------------------------------*/

static
double
gentec_maestro_get_scale( void )
{
	char reply[ 20 ];
	if (    gentec_maestro_talk( "*GCR", reply, sizeof reply ) < 8
         || strncmp( reply, "Range: ", 7 ) )
		gentec_maestro_failure( );

    char *ep;
	long int index = strtol( reply + 7, &ep, 10 );

	if (    *ep
         || index < gentec_maestro.min_scale_index
         || index > gentec_maestro.max_scale_index )
		gentec_maestro_failure( );

	gentec_maestro.scale_index = index;
	return gentec_maestro_index_to_scale( index );
}


/*---------------------------------------------------*
 * Switches autoscale mode on or off
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_autoscale( bool on_off )
{
	char cmd[ 10 ];
	sprintf( cmd, "*SAS%c", on_off ? '1' : '0' );
	gentec_maestro_command( cmd );

	return gentec_maestro.autoscale_is_on = on_off;
}


/*---------------------------------------------------*
 * Returns if the device is in autoscale mode
 *---------------------------------------------------*/

static
bool
gentec_maestro_get_autoscale( void )
{
	char reply[ 15 ];

	if (    gentec_maestro_talk( "*GAS", reply, sizeof reply ) != 12
		 || strncmp( reply, "AutoScale: ", 11 )
		 || ( reply[ 11 ] != '0' && reply[ 11 ] != '1' )
		 || reply[ 12 ] != '\0' )
		gentec_maestro_failure( );

	return gentec_maestro.autoscale_is_on = reply[ 11 ] == '1';
}


/*---------------------------------------------------*
 * Set a new trigger level, argument must be a percentage
 * between 0.1 and 99.9.
 *---------------------------------------------------*/

static
double
gentec_maestro_set_trigger_level( double level )
{
	fsc2_assert( level >= 0.05 && level < 99.95 );

	char cmd[ 100 ];
	sprintf( cmd, "*STL%.*f", level >= 9.95 ? 1 : 2, level );
	gentec_maestro_command( cmd );

    return gentec_maestro.trigger_level = strtod( cmd + 4, NULL );
}


/*---------------------------------------------------*
 * Returns te trigger level, a number in percent between
 * 0.1 and 99.9.
 *---------------------------------------------------*/

static
double
gentec_maestro_get_trigger_level( void )
{
	char reply[ 30 ];
    if (    gentec_maestro_talk( "*GTL", reply, sizeof reply ) < 16
         || strncmp( reply, "Trigger Level: ", 15 ) )
        gentec_maestro_failure( );

    char *ep;
    double level = strtod( reply + 15, &ep );

    if (    *ep
         || level < MIN_TRIGGER_LEVEL
         || level > MAX_TRIGGER_LEVEL )
        gentec_maestro_failure( );

    return gentec_maestro.trigger_level = level;
}


/*---------------------------------------------------*
 * Returns the current display mode, which depends on
 * the connected proobe head.
 *---------------------------------------------------*/

static
int
gentec_maestro_get_mode( void )
{
    char reply[ 10 ];

    if (    gentec_maestro_talk( "*GMD", reply, sizeof reply ) != 7
         || strncmp( reply, "Mode: ", 6 )
         || reply[ 7 ] != '\0' )
        gentec_maestro_failure( );

    if ( reply[ 6 ] - '0' == POWER_MODE )
            return gentec_maestro.mode = POWER_MODE;
    else if ( reply[ 6 ] - '0' == ENERGY_MODE )
            return gentec_maestro.mode = ENERGY_MODE;
    else if ( reply[ 6 ] - '0' == SSE_MODE )
            return gentec_maestro.mode = SSE_MODE;
    else if ( reply[ 6 ] - '0' == DBM_MODE )
            return gentec_maestro.mode = DBM_MODE;
    else if ( reply[ 6 ] - '0' == NO_DETECTOR )
            return gentec_maestro.mode = NO_DETECTOR;

    gentec_maestro_failure( );

    /* We'll never get here... */

    return -1;
}


/*---------------------------------------------------*
 * Requests a data point from the device (note that it
 * will return an already sent value if no new data
 * point as been measured since.
 *---------------------------------------------------*/

static
double
gentec_maestro_get_current_value( void )
{
    char reply[ 20 ];
    if ( gentec_maestro_talk( "*CVU", reply, sizeof reply ) < 1 )
        gentec_maestro_failure( );

    char *ep;
    double val = strtod( reply, &ep );

    if (    *ep
         || ( ( val == HUGE_VAL || val == -HUGE_VAL ) && errno == ERANGE ) )
        gentec_maestro_failure( );

    return val;
}


/*---------------------------------------------------*
 * Returns if a newly measured value is available
 *---------------------------------------------------*/

static
bool
gentec_maestro_check_for_new_value( void )
{
    char reply[ 30 ];
    gentec_maestro_talk( "*NVU", reply, sizeof reply );

    if ( ! strcmp( reply, "New Data Available" ) )
        return 1;
    else if ( ! strcmp( reply, "New Data Not Available" ) )
        return 0;

    gentec_maestro_failure( );
    return 0;                    /* never executed */
}


/*---------------------------------------------------*
 * Switches te mode were the device constantly sends new
 * data points when they become available on and off
 *---------------------------------------------------*/

static
bool
gentec_maestro_continuous_transmission( bool on_off )
{
    gentec_maestro_command( on_off ? "*CAU" : "*CSU" );
    return gentec_maestro.continuous_transmission_is_on = on_off;
}


/*---------------------------------------------------*
 * Returns the repetition frequency of the laser
 *---------------------------------------------------*/

static
double
gentec_maestro_get_laser_frequency( void )
{
    char reply[ 20 ];
    if ( gentec_maestro_talk( "*GRR", reply, sizeof reply ) < 1 )
        gentec_maestro_failure( );

    char *ep;
    double freq = strtod( reply, &ep );

    if (    *ep
         || ( ( freq == HUGE_VAL || freq == -HUGE_VAL ) && errno == ERANGE )
         || freq < 0 )
        gentec_maestro_failure( );

    return freq;
}


/*---------------------------------------------------*
 * Switches binary transmission mode for enery measurements
 * on or off.
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_joulemeter_binary_mode( bool on_off )
{
    char cmd[  ] = "*SS1*";
    cmd[ 4 ] = on_off ? '1' : '0';

    gentec_maestro_command( cmd );
    return on_off;
}


/*---------------------------------------------------*
 * Returns if device is in binary transmission mode
 * for energy measurements.
 *---------------------------------------------------*/

#if 0
static
bool
gentec_maestro_get_joulemeter_binary_mode( void )
{
    char reply[ 30 ];

    if (    gentec_maestro_talk( "*GBM", reply, sizeof reply ) != 24
         || strncmp( reply, "Binary Joulemeter Mode: ", 24 )
         || ( reply[ 24 ] != '0' && reply[ 24 ] != '1' )
         || reply[ 25 ] != '\0' )
        gentec_maestro_failure( );

    return reply[ 24 ] != '0';
}
#endif


/*---------------------------------------------------*
 * Switches analog output of measured values on or off
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_analog_output( bool on_off )
{
    char cmd[ ] = "*ANO*";
    cmd[ 4 ] = on_off ? '1' : '0';

    gentec_maestro_command( cmd );
    return on_off;
}


/*---------------------------------------------------*
 * Sets a new "personal wavelength" (i.e. the wavelength
 * usd for corrections using a table built into the probe
 * head)
 *---------------------------------------------------*/

static
double
gentec_maestro_set_wavelength( long int wl )
{
    fsc2_assert(    (    ! gentec_maestro.att_is_on
                      && wl >= gentec_maestro.min_wavelength
                      && wl <= gentec_maestro.max_wavelength )
                 || (    gentec_maestro.att_is_on
                      && wl >= gentec_maestro.min_wavelength_with_att
                      && wl <= gentec_maestro.max_wavelength_with_att ) );

    char cmd[ 30 ];
    snprintf( cmd, sizeof cmd, "*PWC%05ld", wl );
    gentec_maestro_command( cmd );

    gentec_maestro.wavelength = wl;
    return 1.0e-9 * wl;
}


/*---------------------------------------------------*
 * Returns the currently set wavelength
 *---------------------------------------------------*/

static
double
gentec_maestro_get_wavelength( void )
{
    char reply[ 20 ];
    if (    gentec_maestro_talk( "*GWL", reply, sizeof reply ) < 6
         || strncmp( reply, "PWC: ", 5 ) )
        gentec_maestro_failure( );


    char *ep;
    long int wl = strtol( reply + 5, &ep, 10 );

    if (    *ep
         || (    ! gentec_maestro.att_is_on
               && wl < gentec_maestro.min_wavelength
               && wl > gentec_maestro.max_wavelength )
         || (    gentec_maestro.att_is_on
              && wl < gentec_maestro.min_wavelength_with_att
              && wl > gentec_maestro.max_wavelength_with_att ) )
        gentec_maestro_failure( );

    gentec_maestro.wavelength = wl;
    return 1.0e-9 * wl;
}


/*---------------------------------------------------*
 * Switches the "anticipation" feature on or off
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_anticipation( bool on_off )
{
    char cmd[ ] = "*ANT*";
    cmd[ 4 ] = on_off ? '1' : '0';

    gentec_maestro_command( cmd );
    return on_off;
}


/*---------------------------------------------------*
 * Returns if the anticpation feature is on or off
 *---------------------------------------------------*/

static
bool
gentec_maestro_get_anticipation( void )
{
    char reply[ 20 ];

    if (    gentec_maestro_talk( "*GAN", reply, sizeof reply ) != 15
         || strncmp( reply, "Anticipation: ", 14 )
         || ( reply[ 14 ] != '0' && reply[ 14 ] != '1' )
         || reply[ 15 ] != '\0' )
        gentec_maestro_failure( );

    return gentec_maestro.anticipation_is_on = reply[ 14 ] != '0';
}


/*---------------------------------------------------*
 * Switches "zero offset" on (in whic case the current
 * ata value will be subtracted from future measurements)
 * or off.
 *---------------------------------------------------*/

static
bool
gentec_maestro_zero_offset( bool on_off )
{
    const char *cmd = on_off ? "*DOU" : "*COU";

    gentec_maestro_command( cmd );
    return gentec_maestro.zero_offset_is_on = on_off;
}


/*---------------------------------------------------*
 * Returns if a zero offset is applied
 *---------------------------------------------------*/

static
bool
gentec_maestro_test_zero_offset( void )
{
    char reply[ 10 ];

    if (    gentec_maestro_talk( "*GZO", reply, sizeof reply ) != 7
         || strncmp( reply, "Zero: ", 6 )
         || ( reply[ 6 ] != '0' && reply[ 6 ] != '1' )
         || reply[ 7 ] != '\0' )
        gentec_maestro_failure( );

    return gentec_maestro.zero_offset_is_on = reply[ 6 ] != '0';
}


/*---------------------------------------------------*
 * Sets a new user multiplier
 *---------------------------------------------------*/

static
double
gentec_maestro_set_user_multiplier( double mul )
{
    fsc2_assert( mul >= MIN_USER_MULTIPLIER && mul <= MAX_USER_MULTIPLIER );

    char cmd[ 13 ] = "*MUL";
    strcat( cmd, gentec_maestro_format( mul ) );
    gentec_maestro_command( cmd );

    return gentec_maestro.user_multiplier = strtod( cmd + 4, NULL );
}


/*---------------------------------------------------*
 * Returns the current value of the user multiplier
 *---------------------------------------------------*/

static
double
gentec_maestro_get_user_multiplier( void )
{
    char reply[ 40 ];
    if (    gentec_maestro_talk( "*GUM", reply, sizeof reply ) < 18
         || strncmp( reply, "User Multiplier: ", 17 ) )
        gentec_maestro_failure( );

    char *ep;
    double mul = strtod( reply + 17, &ep );

    if ( *ep || mul < MIN_USER_MULTIPLIER || mul > MAX_USER_MULTIPLIER )
        gentec_maestro_failure( );

    return gentec_maestro.user_multiplier = mul;
}


/*---------------------------------------------------*
 * Sets the user offset, a value subtracted from all data points
 *---------------------------------------------------*/

static
double
gentec_maestro_set_user_offset( double offset )
{
    fsc2_assert( offset >= MIN_USER_OFFSET && offset <= MAX_USER_OFFSET );

    char cmd[ 13 ] = "*OFF";
    strcat( cmd, gentec_maestro_format( offset ) );
    gentec_maestro_command( cmd );

    return gentec_maestro.user_offset = strtod( cmd + 4, NULL );
}


/*---------------------------------------------------*
 * Returns the current value of the user offset
 *---------------------------------------------------*/

static
double
gentec_maestro_get_user_offset( void )
{
    char reply[ 40 ];
    if (    gentec_maestro_talk( "*GUO", reply, sizeof reply ) < 14
         || strncmp( reply, "User Offset: ", 13 ) )
        gentec_maestro_failure( );

    char *ep;
    double offset = strtod( reply + 13, &ep );

    if (    *ep
         || offset < MIN_USER_OFFSET
         || offset > MAX_USER_OFFSET )
        gentec_maestro_failure( );

    return gentec_maestro.user_offset = offset;
}


/*---------------------------------------------------*
 * Requests the current statistics values from the device.
 * The command and what it returns is undocumented and this
 * is just derived from what was observed...
 *---------------------------------------------------*/

static
void
gentec_maestro_get_statistics( double * data )
{
    char const *labels[ ] = { "Current Value:",
                              "Maximum:",
                              "Minimum:"
                              "Average:",
                              "Standard Deviation:",
                              "RMS stability:",
                              "PTP stability:"
                              "Pulse Number:",
                              "Total Pulses:",
                              "Average Power:",
                              "Rep Rate:",
                              "Uncorrected Value:" };

    char reply[ 500 ];
    gentec_maestro_talk( "*vsu", reply, sizeof reply );

    char *cp = reply;

    for ( size_t i = 0; i < sizeof labels / sizeof *labels; i++ )
    {
        if ( strncmp( cp, labels[ i ], strlen( labels[ i ]  ) ) )
            gentec_maestro_failure( );

        char *ep;
        errno = 0;
        data[ i ] = strtod( cp, &ep );
        if (    ep == cp
             || errno == ERANGE )
            gentec_maestro_failure( );

        cp = ep;
        while ( isspace( ( int ) *cp ) )
            cp++;
    }

    if ( *cp )
        gentec_maestro_failure( );
}


/*---------------------------------------------------*
 * Switches single shot energy mode on or off
 *---------------------------------------------------*/

#if 0
static
bool
gentec_maestro_set_energy_mode( bool on_off )
{
    char cmd[ ] = "*SSE*";
    cmd[ 4 ] = on_off ? '1' : '0';

    gentec_maestro_command( cmd );
    return gentec_maestro.energy_mode_is_on = on_off;
}
#endif


/*---------------------------------------------------*
 * Switches the attenuator (if available) on or off
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_attenuator( bool on_off )
{
    fsc2_assert( gentec_maestro.att_is_available );

    char cmd[ ] = "*ATT*";
    cmd[ 4 ] = on_off ? '1' : '0';

    gentec_maestro_command( cmd );
    return gentec_maestro.att_is_on = on_off;
}


/*---------------------------------------------------*
 * Returns if the attenuator is on or off (if available)
 *---------------------------------------------------*/

static
bool
gentec_maestro_get_attenuator( void )
{
    fsc2_assert( gentec_maestro.att_is_available );

    char reply[ 16 ];

    if (    gentec_maestro_talk( "*GAT", reply, sizeof reply ) != 13
         || strncmp( reply, "Attenuator: ", 12 )
         || ( reply[ 12 ] != '0' && reply[ 12 ] != '1' )
         || reply[ 13 ] != '\0' )
        gentec_maestro_failure( );

    return gentec_maestro.att_is_on = reply[ 12 ] != '0';
}


/*---------------------------------------------------*
 * Converts a status entry to a 4-byte integer
 *---------------------------------------------------*/

static
unsigned long
gentec_maestro_status_entry_to_int( const char * e )
{
    char str[ 9 ] = "";
    strncpy( str + 4, e + 6, 4 );
    strncpy( str, e + 18, 4 );

    char *ep;
    unsigned long int res = strtoul( str, &ep, 16 );

    if ( *ep )
        gentec_maestro_failure( );

    return res;
}
    

/*---------------------------------------------------*
 * Converts a status entry from IEEE 754 single-precision binary
 * floating-point format (encoded as a hex string)
 *---------------------------------------------------*/

static
double
gentec_maestro_status_entry_to_float( const char * e )
{
    unsigned long int v = 0;

    for ( int i = 18; i > 0; i -= 12 )
        for ( int j = 0; j < 4; j++ )
        {
            unsigned char c = e[ i + j ];

            v <<= 4;
            v |= c <= '9' ? ( c - '0' ) : ( c - 'A' + 10 );
        }

    int sign  = v & 0x80000000UL ? -1 : 1;

    unsigned char offset_expo = ( v & 0x7F800000UL ) >> 23;

    if ( offset_expo == 0xFF )                /* NaN or infinity? */
        gentec_maestro_failure( );

    int expo = offset_expo - 127;

    v &= 0x7FFFFFUL;

    double res = 1.0;
    double x = 1.0 / 8388608;         /* 2^-23 */

    for ( int i = 0; i < 23; v >>= 1, x *= 2, i++ )
        res += ( v & 1 ) * x;

    return sign * res * pow( 2, expo );
}


/*---------------------------------------------------*
 * Converts a status entry into a string
 *---------------------------------------------------*/

static
char *
gentec_maestro_status_entry_to_string( const char * e )
{
    static char name[ 33 ];      /* maximum possible length is 32 */

    int i;
    for ( i = 0; i < 16; ++i )
    {
        unsigned long v = strtoul( e + 12 * i + 6, NULL, 16 );

        if (    ( name[ 2 * i     ] = v & 0xFF ) == 0
             || ( name[ 2 * i + 1 ] = v >> 8   ) == 0 )
            break;
    }

    if ( i == 16 )
        name[ 32 ] = '\0';

    return name;
}


/*---------------------------------------------------*
 * Reads in the "extended status" and sets up the
 * structure describing the state of the device
 * accordingly.
 *---------------------------------------------------*/

static
void
gentec_maestro_get_extended_status( void )
{
    gentec_maestro_command( "*ST2" );

    /* Read all the lines into our buffer and do basic sanity checks */

    char reply[ 0x3a * 12 ];
    char * r = reply;

    for ( int i = 0; i <= 0x39; r += 12, i++ )
    {
        if ( gentec_maestro_read( r, 12 ) != 12 )
            gentec_maestro_failure( );

        char b[ 7 ];
        sprintf( b, ":0%04X", i );

        if ( strncmp( r, b, 6 ) || strncmp( r + 10, "\r\n", 2 ) )
            gentec_maestro_failure( );

        for ( int j = 6; j < 10; ++j )
            if ( ! isxdigit( ( int ) r[ j ] ) )
                gentec_maestro_failure( );
    }

    /* Get the final line */

    if (    gentec_maestro_read( r, 12 ) != 12
         || strncmp( r, ":100000000\r\n", 12 ) )
        gentec_maestro_failure( );

    /* Now extract all the relevant values */

    long int v = gentec_maestro_status_entry_to_int( reply + 0x04 * 12 );

    if (    v != POWER_MODE
         && v != ENERGY_MODE
         && v != SSE_MODE
         && v != DBM_MODE
         && v != NO_DETECTOR )
        gentec_maestro_failure( );
    gentec_maestro.mode = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x06 * 12 );
    if ( v < 0 || v > MAX_SCALE )
        gentec_maestro_failure( );
    gentec_maestro.scale_index = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x08 * 12 );
    if ( v < 0 || v > MAX_SCALE )
        gentec_maestro_failure( );
    gentec_maestro.max_scale_index = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x0a * 12 );
    if ( v < 0 || v > gentec_maestro.max_scale_index )
        gentec_maestro_failure( );
    gentec_maestro.min_scale_index = v;

    if (    gentec_maestro.scale_index < gentec_maestro.min_scale_index
         || gentec_maestro.scale_index > gentec_maestro.max_scale_index )
        gentec_maestro_failure( );

    v = gentec_maestro_status_entry_to_int( reply + 0x0c * 12 );
    if ( v < 0 )
        gentec_maestro_failure( );
    gentec_maestro.wavelength = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x0e * 12 );
    if ( v < 0 )
        gentec_maestro_failure( );
    gentec_maestro.max_wavelength = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x10 * 12 );
    if ( v < 0 )
        gentec_maestro_failure( );
    gentec_maestro.min_wavelength = v;

    if (    gentec_maestro.wavelength < gentec_maestro.min_wavelength
         || gentec_maestro.wavelength > gentec_maestro.max_wavelength )
        gentec_maestro_failure( );

    v = gentec_maestro_status_entry_to_int( reply + 0x12 * 12 );
    if ( v != 0 && v != 1 )
        gentec_maestro_failure( );
    gentec_maestro.att_is_available = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x14 * 12 );
    if ( v != 0 && v != 1 )
        gentec_maestro_failure( );
    gentec_maestro.att_is_on = v;

    if ( ! gentec_maestro.att_is_available && gentec_maestro.att_is_on )
        gentec_maestro_failure( );
    
    v = gentec_maestro_status_entry_to_int( reply + 0x16 * 12 );
    if ( v < 0 )
        gentec_maestro_failure( );
    gentec_maestro.max_wavelength_with_att = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x18 * 12 );
    if ( v < 0 )
        gentec_maestro_failure( );
    gentec_maestro.min_wavelength_with_att = v;

    if (    gentec_maestro.att_is_on
         && (    gentec_maestro.wavelength <
                                     gentec_maestro.min_wavelength_with_att
              || gentec_maestro.wavelength >
                                     gentec_maestro.max_wavelength_with_att ) )
        gentec_maestro_failure( );

    strcpy( gentec_maestro.head_name,
            gentec_maestro_status_entry_to_string( reply + 0x1a * 12 ) );

    /* Note: in the extended status the trigger level gets returned as a
       fraction, not a percentage! */

    double d =   0.1 * lrnd( 1000
               * gentec_maestro_status_entry_to_float( reply + 0x2e * 12 ) ); 
   if ( d < 0.1 || d > 99.9 )
        gentec_maestro_failure( );
    gentec_maestro.trigger_level = d;

    v = gentec_maestro_status_entry_to_int( reply + 0x30 * 12 );
    if ( v != 0 && v != 1 )
        gentec_maestro_failure( );
    gentec_maestro.autoscale_is_on = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x32 * 12 );
    if ( v != 0 && v != 1 )
        gentec_maestro_failure( );
    gentec_maestro.anticipation_is_on = v;

    v = gentec_maestro_status_entry_to_int( reply + 0x34 * 12 );
    if ( v != 0 && v != 1 )
        gentec_maestro_failure( );
    gentec_maestro.zero_offset_is_on = v;

    d = gentec_maestro_status_entry_to_float( reply + 0x36 * 12 );
    if ( d < MIN_USER_MULTIPLIER || d > MAX_USER_MULTIPLIER )
        gentec_maestro_failure( );
    gentec_maestro.user_multiplier = d;

    d = gentec_maestro_status_entry_to_float( reply + 0x38 * 12 );
    if ( d < MIN_USER_OFFSET || d > MAX_USER_OFFSET )
        gentec_maestro_failure( );
    gentec_maestro.user_offset = d;
}



/*---------------------------------------------------*
 * Converts a scale index (must be between 0 and 41) to
 * the corresponding scale
 *---------------------------------------------------*/

static
double
gentec_maestro_index_to_scale( int index )
{
	fsc2_assert( index >= 0 && index <= MAX_SCALE );
	return ( ( index & 1 ) ? 3 : 1 ) * 1.0e-12 * pow( 10, index / 2 );
}


/*---------------------------------------------------*
 * Converts a scale value (between 1pW or pJ and 300 MW
 * or MJ) to the corresponding index. Returns -1 if no
 * conversion is possible.
 *---------------------------------------------------*/

static
int
gentec_maestro_scale_to_index( double   scale,
                               bool   * no_fit )
{
	int i;
    *no_fit = false;

	for ( i = 0; i <= MAX_SCALE; i++ )
	{
		double r = gentec_maestro_index_to_scale( i );

		if ( scale < 0.9999 * r )
        {
            *no_fit = true;
			return i;
        }

		if ( scale < 1.0001 * r )
			return i;
	}

	return i;
}


/*---------------------------------------------------*
 * Converts a double value into the best (meaning the
 * maximum number of relevant digits) string with 8
 * characters (with no '+' after the 'e' in exponential
 * notation).
 *---------------------------------------------------*/

static
char const *
gentec_maestro_format( double v )
{
	static char buf[ 20 ];
	bool is_neg = v < 0;

	v = fabs( v );

	if ( v >= 99999999.5 )
	{
		int expo = log10( v + 0.5 );
		int expo_digits = log10( expo ) + 1;

		sprintf( buf, "%s%.*fe%d", is_neg ? "-" : "",
				 5 - expo_digits - is_neg, v * pow( 10, -expo ), expo );

		if ( buf[ 1 + is_neg ] != '.' )
		{
			expo_digits = log10( ++expo ) + 1;
			sprintf( buf, "%s%.*fe%d", is_neg ? "-" : "",
					 5 - expo_digits - is_neg, v * pow( 10, -expo ), expo );
		}

	}
	else if ( v < 0.0099995 )
	{
		int expo = - log10( v ) + 1;
		int expo_digits = log10( expo ) + 1;

		printf( "%d\n", expo );

		sprintf( buf, "%s%.*fe-%d", is_neg ? "-" : "",
				 4 - expo_digits - is_neg, v * pow( 10, expo ), expo );

		if ( buf[ 1 + is_neg ] != '.' )
		{
			expo_digits = log10( --expo ) + 1;
			sprintf( buf, "%s%.*fe%-d", is_neg ? "-" : "",
					 5 - expo_digits - is_neg, v * pow( 10, expo ), expo );
		}

	}
	else
	{
		int expo = log10( v + 0.5 );
		sprintf( buf, "%s%.*f", is_neg ? "-" : "", 6 - expo - is_neg, v );
	}

    fsc2_assert( strlen( buf ) == 8 );

	return buf;
}


/*---------------------------------------------------*
 * Returns a user readable string for a scale index
 *---------------------------------------------------*/

static
const char *
gentec_maestro_pretty_print_scale( int index )
{
    static char ret[ 10 ];

    fsc2_assert( index >= 0 && index <= MAX_SCALE );
    
    char *c = ret;
    const char *units[ ] = { "pJ/pW", "nJ/nW", "uJ/uW", "mJ/mW",
                             "J/W",   "kJ/kW", "MJ/MW" };

    *c++ = index & 1 ? '3' : '1';
    if ( index & 6 )
        *c++ = '0';
    if ( index & 4 )
        *c++ = '0';
    *c++ = ' ';
    strcpy( c, units[ index / 6 ] );

    return ret;
}   


/*---------------------------------------------------*
 * Opens a connection to the device
 *---------------------------------------------------*/

static
void
gentec_maestro_open( void )
{
#if defined USE_SERIAL
    if ( ! gentec_maestro_serial_comm( SERIAL_INIT ) )
#else
    if ( ( gentec_maestro.handle = fsc2_lan_open( DEVICE_NAME, NETWORK_ADDRESS,
                                                  PORT, OPEN_TIMEOUT,
                                                  false ) ) == -1 )
#endif
    {
        print( FATAL, "ailed to connect to device.\n" );
        THROW( EXCEPTION );
    }
}


/*---------------------------------------------------*
 * Closes the connection to the device
 *---------------------------------------------------*/

static
void
gentec_maestro_close( void )
{
#if defined USE_SERIAL
    gentec_maestro_serial_comm( SERIAL_EXIT );
#else
    if ( gentec_maestro.handle != -1 )
    {
        fsc2_lan_close( gentec_maestro.handle );
        gentec_maestro.handle = -1;
    }
#endif
}


/*---------------------------------------------------*
 * Sends a string a command to the device, no reply expected
 *---------------------------------------------------*/

static
void
gentec_maestro_command( const char * cmd )
{
#if defined USE_SERIAL
    if ( ! gentec_maestro_serial_comm( SERIAL_WRITE, cmd ) )
#else
    long len = strlen( cmd );

    if ( fsc2_lan_write( gentec_maestro.handle, cmd, len,
                         WRITE_TIMEOUT, false ) != len )
#endif
        gentec_maestro_failure( );
}


/*---------------------------------------------------*
 * Sends a single string to the device and then reads
 * in the device's single line reply. The input buffer
 * must have room for the replY (including a carriage
 * return and a line-feed at the end). The CR and LF
 * are removed and the returned string is nul-terminated.
 *---------------------------------------------------*/

static
long
gentec_maestro_talk( const char * cmd,
					 char       * reply,
					 long         length )
{
    fsc2_assert( length >= 3 );

    gentec_maestro_command( cmd );
    length = gentec_maestro_read( reply, length );
    reply[ length -= 2 ] = '\0';
    return length;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
long
gentec_maestro_read( char * reply,
                     long   length )
{
#if defined USE_SERIAL
    if (    ! gentec_maestro_serial_comm( SERIAL_READ, reply, &length )
#else
    if (    ( length = fsc2_lan_read( gentec_maestro.handle, reply, length,
                                      READ_TIMEOUT, false ) ) < 3
#endif
         || length < 3
         || strncmp( reply + length - 2, "\r\n", 2 ) )
        gentec_maestro_failure( );

    return length;
}


/*---------------------------------------------------*
 * Called whenever the communication with the device fails,
 * either because the device doesn't react in time or since
 * it sent invalid data.
 *---------------------------------------------------*/

static
void
gentec_maestro_failure( void )
{
    print( FATAL, "Communication with powermeter failed.\n" );
    THROW( EXCEPTION );
}


/*---------------------------------------------------*
 * Function for opening and closing a connection as well
 * as reading or writing data when the the device is
 * connected via the serial port or USB (the latter just
 * being a USB-to-serial converter)
 *---------------------------------------------------*/

#if defined USE_SERIAL
static bool
gentec_maestro_serial_comm( int type,
                            ... )
{
    va_list ap;
    char * buf;
    ssize_t len;
    size_t * lptr;

    switch ( type )
    {
        case SERIAL_INIT :
            /* Open the serial port for reading and writing. */

            if ( ( gentec_maestro.tio = fsc2_serial_open( gentec_maestro.handle,
                                                          O_RDWR ) ) == NULL )
                return false;

            /* Use 8-N-1, ignore flow control and modem lines, enable
               reading and set the baud rate. */

            gentec_maestro.tio->c_cflag = CS8 | CLOCAL | CREAD;
            gentec_maestro.tio->c_iflag = IGNBRK;
            gentec_maestro.tio->c_oflag = 0;
            gentec_maestro.tio->c_lflag = 0;

            cfsetispeed( gentec_maestro.tio, SERIAL_BAUDRATE );
            cfsetospeed( gentec_maestro.tio, SERIAL_BAUDRATE );

            fsc2_tcflush( gentec_maestro.handle, TCIOFLUSH );
            fsc2_tcsetattr( gentec_maestro.handle, TCSANOW,
                            gentec_maestro.tio );
            break;

        case SERIAL_EXIT :
            fsc2_serial_close( gentec_maestro.handle );
            break;

        case SERIAL_WRITE :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            va_end( ap );

            len = strlen( buf );
            if ( fsc2_serial_write( gentec_maestro.handle, buf, len,
                                    WRITE_TIMEOUT, true ) != len )
            {
                if ( len == 0 )
                    stop_on_user_request( );
                return false;
            }
            break;

        case SERIAL_READ :
            va_start( ap, type );
            buf = va_arg( ap, char * );
            lptr = va_arg( ap, size_t * );
            va_end( ap );

            /* Try to read from the device (reads need to stop on "\r\n") */

            if ( ( *lptr = fsc2_serial_read( gentec_maestro.handle, buf, *lptr,
                                             "\r\n", READ_TIMEOUT, false ) )
                 <= 0 )
            {
                if ( *lptr == 0 )
                    stop_on_user_request( );

                return false;
            }
            break;

        default :
            print( FATAL, "INTERNAL ERROR detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
    }

    return true;
}
#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
