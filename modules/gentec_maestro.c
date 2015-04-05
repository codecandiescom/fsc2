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
#include "lan.h"
#include <float.h>


/* Hook functions */

int gentec_maestro_init_hook( void );
int gentec_maestro_test_hook( void );
int gentec_maestro_end_of_test_hook( void );
int gentec_maestro_exp_hook( void );
int gentec_maestro_end_of_exp_hook( void );

/* EDL functions */

Var_T * powermeter_name( Var_T * v  UNUSED_ARG );
Var_T * powermeter_scale( Var_T * v );
Var_T * powermeter_get_scale_limits( Var_T * v );
Var_T * powermeter_autocale( Var_T * v );
Var_T * powermeter_trigger_level( Var_T * v );
Var_T * powermeter_wavelength( Var_T * v );
Var_T * powermeter_attenuator( Var_T * v );
Var_T * powermeter_wavelength_range( Var_T * v );
Var_T * powermeter_get_reading( Var_T * v );
Var_T * powermeter_anticipation( Var_T * v );
Var_T * powermeter_attenuation( Var_T * v );
Var_T * powermeter_zero_offset( Var_T * v );
Var_T * powermeter_multiplier( Var_T * v );
Var_T * powermeter_offset( Var_T * v );

/* Internal functions */

static bool gentec_maestro_init( const char * dev_name );
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
static bool gentec_maestro_get_joulemeter_binary_mode( void );
static bool gentec_maestro_set_analog_output( bool on_off );
static double gentec_maestro_set_wavelength( double wavelength );
static double gentec_maestro_get_wavelength( void );
static bool gentec_maestro_set_anticipation( bool on_off );
static bool gentec_maestro_get_anticipation( void );
static bool gentec_maestro_set_zero_offset( void );
static bool gentec_maestro_clear_zero_offset( void );
static bool gentec_maestro_test_zero_offset( void );
static double gentec_maestro_set_user_multiplier( double mul );
static double gentec_maestro_get_user_multiplier( void );
static double gentec_maestro_set_user_offset( double offset );
static double gentec_maestro_get_user_offset( void );
static bool gentec_maestro_set_energy_mode( bool on_off );
static bool gentec_maestro_set_attenuator( bool on_off );
static bool gentec_maestro_get_attenuator( void );
static unsigned long gentec_maestro_status_entry_to_int( const char * e );
static double gentec_maestro_status_entry_to_float( const char * e );
static char * gentec_maestro_status_entry_to_string( const char * e );
static void gentec_maestro_get_extended_status( void );
static void gentec_maestro_command( const char * cmd );
static long gentec_maestro_talk( const char * cmd,
                                 char       * reply,
					             long         length );
static void gentec_maestro_failure( void );
static double gentec_maestro_index_to_scale( int index );
static int gentec_maestro_scale_to_index( double   scale,
                                          bool   * no_fit );
static char const * gentec_maestro_format( double v );
static const char * gentec_maestro_pretty_print_scale( int index );


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

    double trigger_level;      /* between 0.1 and 0.99 */
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

    bool continuous_transmission_is_on;
    bool energy_mode_is_on;
} Gentec_Maestro;


static Gentec_Maestro gentec_maestro,
                      gentec_maestro_test;
static Gentec_Maestro * gm;


/* Defines */

#define MAX_SCALE            41
#define MIN_TRIGGER_LEVEL    0.1
#define MAX_TRIGGER_LEVEL    99.9
#define MIN_USER_MULTIPLIER  -DBL_MAX      /* not documented */
#define MAX_USER_MULTIPLIER  DBL_MAX       /* not documented */
#define MIN_USER_OFFSET      -DBL_MAX      /* not documented */
#define MAX_USER_OFFSET      DBL_MAX       /* not documented */


#define POWER_MODE     0     /* power in W  */
#define ENERGY_MODE    1     /* energy in J */
#define SSE_MODE       2     /* single shot energy in J */
#define DBM_MODE       6     /* power in dBm */
#define NO_DETECTOR    7


#define TEST_MODE             ENERGY_MODE
#define TEST_VALUE            2.45e-4
#define TEST_SCALE_INDEX      17
#define TEST_WAVELENGTH       650
#define TEST_HEAD_NAME        "QE12LP-S-MB"
#define TEST_TRIGGER_LEVEL    0.02
#define TEST_USER_MULTIPLIER  1.0
#define TEST_USER_OFFSET      0.0
#define TEST_LASER_FREQUENCY  100.0


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
gentec_maestro_init_hook( void )
{
    gm = &gentec_maestro;

    /* Set global variable to indicate that the device is controlled via LAN */

    Need_LAN = SET;

    /* Initialize the structure for the device as good as possible */

    gm->mode = TEST_MODE;
    gm->mode_has_been_set = UNSET;

    gm->scale_index = TEST_SCALE_INDEX;;
    gm->min_scale_index = 0;
    gm->max_scale_index = MAX_SCALE;

    gm->scale_index_has_been_set = UNSET;
    gm->min_test_scale_index = MAX_SCALE;
    gm->max_test_scale_index = 0;

    gm->att_is_available    = SET;;
    gm->att_is_on        = UNSET;
    gm->att_has_been_set = UNSET;

    gm->wavelength = TEST_WAVELENGTH;
    gm->min_wavelength = 0;
    gm->max_wavelength = LONG_MAX;
    gm->min_wavelength_with_att = 0;
    gm->max_wavelength_with_att = LONG_MAX;

    gm->wavelength_has_been_set = UNSET;
    gm->min_test_wavelength = LONG_MAX;
    gm->max_test_wavelength = 0;
    gm->min_test_wavelength_with_att = LONG_MAX;
    gm->max_test_wavelength_with_att = 0;
    gm->min_test_wavelength_unknown = LONG_MAX;
    gm->max_test_wavelength_unknown = 0;
    
    strcpy( gm->head_name, TEST_HEAD_NAME );

    gm->trigger_level = TEST_TRIGGER_LEVEL;
    gm->trigger_level_has_been_set = UNSET;

    gm->autoscale_is_on = SET;
    gm->autoscale_has_been_set = UNSET;

    gm->anticipation_is_on = UNSET;
    gm->anticipation_has_been_set = UNSET;

    gm->zero_offset_is_on = UNSET;
    gm->zero_offset_has_been_set = UNSET;

    gm->user_multiplier = TEST_USER_MULTIPLIER;
    gm->user_multiplier_has_been_set = UNSET;

    gm->user_offset = TEST_USER_OFFSET;
    gm->user_offset_has_been_set = UNSET;

    return 1;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
gentec_maestro_test_hook( void )
{
    gentec_maestro_test = gentec_maestro;
    gm = &gentec_maestro;

    gm->mode_has_been_set = UNSET;
    gm->scale_index_has_been_set = UNSET;
    gm->att_has_been_set = UNSET;
    gm->wavelength_has_been_set = UNSET;
    gm->trigger_level_has_been_set = UNSET;
    gm->anticipation_has_been_set = UNSET;
    gm->zero_offset_has_been_set = UNSET;
    gm->user_multiplier_has_been_set = UNSET;
    gm->user_offset_has_been_set = UNSET;

    return 1;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
gentec_maestro_end_of_test_hook( void )
{
    gm = &gentec_maestro;
    return 1;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
gentec_maestro_exp_hook( void )
{
    bool is_ok;


    CLOBBER_PROTECT( is_ok );

    TRY
    {
        is_ok = gentec_maestro_init( DEVICE_NAME );
        TRY_SUCCESS;
    }
    CATCH( EXCEPTION )
        is_ok = UNSET;

    if ( ! is_ok && gentec_maestro.handle >= 0 )
    {
        fsc2_lan_close( gentec_maestro.handle );
        gentec_maestro.handle = -1;
    }

    return is_ok;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

int
gentec_maestro_end_of_exp_hook( void )
{
    if ( gentec_maestro.handle >= 0 )
    {
        fsc2_lan_close( gentec_maestro.handle );
        gentec_maestro.handle = -1;
    }

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
 * Sets a new scale
 *-------------------------------------------------------*/

Var_T *
powermeter_scale( Var_T * v )
{
    double scale;
    int idx;
    bool no_fit;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->scale_index_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_scale( );
        return vars_push( FLOAT_VAR,
                          gentec_maestro_index_to_scale( gm->scale_index ) );
    }

    scale = get_double( v, "scale" );
    too_many_arguments( v );

    if ( scale <= 0 )
    {
        print( FATAL, "Invalid negativ or zero scale.\n" );
        THROW( EXCEPTION );
    }

    idx = gentec_maestro_scale_to_index( scale, &no_fit );

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
        gm->scale_index_has_been_set = SET;

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
    double min_max[ 2 ];

    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    min_max[ 0 ] = gentec_maestro_index_to_scale( gm->min_scale_index );
    min_max[ 1 ] = gentec_maestro_index_to_scale( gm->max_scale_index );

    return vars_push( FLOAT_ARR, min_max, 4 );
}


/*-------------------------------------------------------*
 * Switch autoscale on or off
 *-------------------------------------------------------*/

Var_T *
powermeter_autocale( Var_T * v )
{
    bool as;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->autoscale_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_autoscale( );

        return vars_push( INT_VAR, gm->autoscale_is_on ? 1L : 0L );
    }

    as = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_autoscale( as );
    else  
  {
        gm->autoscale_is_on = as;
        gm->autoscale_has_been_set = SET;
    }

    return vars_push( INT_VAR, gm->autoscale_is_on ? 1L : 0L );
}


/*-------------------------------------------------------*
 * Query or set trigger level (arhument must be in percent)
 *-------------------------------------------------------*/

Var_T *
powermeter_trigger_level( Var_T * v )
{
    double level;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->trigger_level_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_trigger_level( );
        return vars_push( FLOAT_VAR, 100 * gm->trigger_level );
    }

    level = get_double( v, "level" );
    too_many_arguments( v );

    if ( level < 0 || level > 100 )
    {
        print( FATAL, "Invalid trigger level percentage.\n" );
        THROW( EXCEPTION );
    }

    if ( level < MIN_TRIGGER_LEVEL )
        print( WARN, "Trigger level too low, adjusting to %f%%.\n",
            level = MIN_TRIGGER_LEVEL );
    if ( level > MAX_TRIGGER_LEVEL )
        print( WARN, "Trigger level too high, adjusting to %f%%.\n",
            level = MAX_TRIGGER_LEVEL );
 
    
    level *= 0.01;

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_trigger_level( level );
    else
    {
        gm->trigger_level = 0.0001 * lrnd( 10000 * level );
        gm->trigger_level_has_been_set = SET;
    }

    if ( fabs( level - gm->trigger_level ) > 0.0001 )
        print( WARN, "djuted trigger level to %.*f%%.\n",
               gm->trigger_level < 0.1 ? 2 : 1, 100 * gm->trigger_level );

    return vars_push( FLOAT_VAR, 100 * gm->trigger_level );
}


/*-------------------------------------------------------*
 * Query or set wavelength
 *-------------------------------------------------------*/

Var_T *
powermeter_wavelength( Var_T * v )
{
    long int wl_nm;
    double wl;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_wavelength( );
        return vars_push( FLOAT_VAR, 1.0e-9 * gm->wavelength );
    }

    wl = get_double( v, "wavelength" );
    too_many_arguments( v );

    if ( wl <= 0 )
    {
        print( FATAL, "Invalid negativ or zero wavelength.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_get_attenuator( );

    wl_nm = lrnd( 1.0e9 * wl );

    if (    ! gm->att_is_on
         && ( wl_nm < gm->min_wavelength || wl_nm > gm->max_wavelength ) )
    {
        print( FATAL, "Requested wavelength is out of range, must be (with "
               "attenuator off) between %ld nm and %ld nm.\n",
               gm->min_wavelength, gm->max_wavelength );
        THROW( EXCEPTION );
    }

    if (    gm->att_is_on
         && (    wl_nm < gm->min_wavelength_with_att 
              || wl_nm > gm->max_wavelength_with_att ) )
    {
        print( FATAL, "Requested wavelength is out of range, must be (with "
               "attenuator on) between %ld nm and %ld nm.\n",
               gm->min_wavelength_with_att, gm->max_wavelength_with_att );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == TEST )
        gentec_maestro_set_wavelength( wl_nm );
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

        gm->wavelength_has_been_set = SET;
    }

    if ( fabs( wl_nm - 1.0e9 ) > 0.1 )
        print( WARN, "Wavelength has been adjusted to %ld nm.\n", wl_nm );

    return vars_push( FLOAT_VAR, 1.0e-9 * wl_nm );
}


/*---------------------------------------------------*
 * Queries or sets the attenuator (if one is available).
 * If not available a query for the attenuator state
 * returns -1.
 *---------------------------------------------------*/

Var_T *
powermeter_attenuator( Var_T * v )
{
    bool state;


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

    if ( FSC2_MODE == EXPERIMENT && ! gm->att_is_available )
    {
        print( FATAL, "No attenuator available.\n" );
        THROW( EXCEPTION );
    }

    state = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( ! gm->att_is_available )
        {
            print( FATAL, "No attenuator available.\n" );
            THROW( EXCEPTION );
        }

        gentec_maestro_get_wavelength( );

        if (    (    state
                  && (    gm->wavelength < gm->min_wavelength_with_att
                       || gm->wavelength > gm->max_wavelength_with_att ) )
             || (    ! state
                  && (    gm->wavelength < gm->min_wavelength
                       || gm->wavelength > gm->max_wavelength ) ) )
        {
            print( FATAL, "Wavelength is ut of range when switching "
                   "attenuator %s.\n", state ? "on" : "off" );
            THROW( EXCEPTION );
        }

        gentec_maestro_set_attenuator( state );
    }
    else
    {
        gm->att_is_on = state;
        gm->att_has_been_set = SET;
    }

    return vars_push( INT_VAR, gm->att_is_on ? 1L : 0L );
}


/*---------------------------------------------------*
 * Returns an array with the minimum and maximum wavelength
 * for the head. If a true argument is passed to the function
 * and an attenuator is avaialble the wavelength limits with
 * attenuator on are returned.
 *---------------------------------------------------*/

Var_T *
powermeter_wavelength_range( Var_T * v )
{
    bool with_att = UNSET;
    double wls[ 2 ];


    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    if ( v )
        with_att = get_boolean( v );

    if ( with_att && ! gm->att_is_available )
    {
        print( FATAL, "No attenuator available.\n" );
        THROW( EXCEPTION );
    }

    if ( ! with_att || gm->att_is_available )
    {
        wls[ 0 ] = 1.0e-9 * gm->min_wavelength;
        wls[ 1 ] = 1.0e-9 * gm->max_wavelength;
    }
    else
    {
        wls[ 0 ] = 1.0e-9 * gm->min_wavelength_with_att;
        wls[ 1 ] = 1.0e-9 * gm->max_wavelength_with_att;
    }

    return vars_push( FLOAT_ARR, wls, 2 );
}


/*---------------------------------------------------*
 * Get a value from the device - if called with a true
 * argument make sure it's a new value.
 *---------------------------------------------------*/

Var_T *
powermeter_get_reading( Var_T * v )
{
    double lf;
    bool wait_for_new = UNSET;

    if ( v )
        wait_for_new = get_boolean( v );
    too_many_arguments( v );

    /* If were supposed to wait for a new value and there's none
       recheck with twice the laser frequency (or all 100 ms if
       the laser frequency is 0), but never slower than every
       100 ms. Also give the user a chance to abort. */

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( FLOAT_VAR, TEST_VALUE );

    if ( wait_for_new )
    {
        if ( ! gentec_maestro_check_for_new_value( ) )
        {
            unsigned long delay = 100000;

            lf = gentec_maestro_get_laser_frequency( );

            if ( lf != 0 )
                delay = 500000 / lf;

            delay = l_min( delay, 100000 );

            do
            {
                stop_on_user_request( );
                fsc2_usleep( delay, UNSET );
            } while ( ! gentec_maestro_check_for_new_value( ) );
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
    bool ac;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->anticipation_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_anticipation( );
        return vars_push( INT_VAR, gm->anticipation_is_on ? 1L : 0L );
    }

    ac = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_anticipation( ac );
    else  
  {
        gm->anticipation_is_on = ac;
        gm->anticipation_has_been_set = SET;
    }

    return vars_push( INT_VAR, gm->anticipation_is_on ? 1L : 0L );
}


/*-------------------------------------------------------*
 * Switch attenuation on or off (if possible). Also check
 * if wavelength is in range
 *-------------------------------------------------------*/

Var_T *
powermeter_attenuation( Var_T * v )
{
    bool att;
    long int wl;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->att_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_attenuator( );

        return vars_push( INT_VAR, gm->att_is_on ? 1L : 0L );
    }

    att = get_boolean( v );
    too_many_arguments( v );

    /* When the experiment is running we definitely know if an attenuator
       is avialable, so first test if it is. We also don't need to bother
       with further tests if it's already in the requested state. */

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
        gm->att_has_been_set = SET;
    }

    return vars_push( INT_VAR, gm->att_is_on ? 1L : 0L );
}


/*---------------------------------------------------*
 * Switch zero offset on or off or query if it's on or off
 *---------------------------------------------------*/

Var_T *
powermeter_zero_offset( Var_T * v )
{
    bool zo;


    if ( ! v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->zero_offset_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_test_zero_offset( );

        return vars_push( INT_VAR, gm->zero_offset_is_on ? 1L : 0L );
    }

    zo = get_boolean( v );
    too_many_arguments( v );

    if ( FSC2_MODE == PREPARATION && zo )
    {
        print( FATAL, "Zero offset can't be switched on from VARAUBLES or "
               "PREPARATION sectrion.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        if ( zo )
            gentec_maestro_set_zero_offset( );
        else
            gentec_maestro_clear_zero_offset( );
    }
    else
    {
        gm->zero_offset_is_on = zo;
        gm->zero_offset_has_been_set = SET;
    }

    return vars_push( INT_VAR, gm->zero_offset_is_on ? 1L : 0L );
}


/*---------------------------------------------------*
 * Set or query the user multiplier
 *---------------------------------------------------*/

Var_T *
powermeter_multiplier( Var_T * v )
{
    double mul;

    if ( !v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->user_multiplier_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_user_multiplier( );

        return vars_push( FLOAT_VAR, gm->user_multiplier );
    }

    mul = get_double( v, "multiplier" );
    too_many_arguments( v );

    if ( mul < MIN_USER_MULTIPLIER || mul > MAX_USER_MULTIPLIER )
    {
        print( FATAL, "Multiplier out of range, must be between %g and %d.\n",
               MIN_USER_MULTIPLIER, MAX_USER_MULTIPLIER );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_user_multiplier( mul );
    else
    {
        gm->user_multiplier = strtod( gentec_maestro_format( mul ), NULL );
        gm->user_multiplier_has_been_set = SET;
    }

    if ( mul != 0 && fabs( ( mul - gm->user_multiplier ) / mul ) >= 0.01 )
        print( WARN, "Multiplier was ajusted to %g.\n", gm->user_multiplier );

    return vars_push( FLOAT_VAR, gm->user_multiplier );
}


/*---------------------------------------------------*
 * Set or query the user offset
 *---------------------------------------------------*/

Var_T *
powermeter_offset( Var_T * v )
{
    double offset;

    if ( !v )
    {
        if ( FSC2_MODE == PREPARATION && ! gm->user_offset_has_been_set )
            no_query_possible( );

        if ( FSC2_MODE == EXPERIMENT )
            gentec_maestro_get_user_offset( );

        return vars_push( FLOAT_VAR, gm->user_offset );
    }

    offset = get_double( v, "offset" );
    too_many_arguments( v );

    if ( offset < MIN_USER_OFFSET || offset > MAX_USER_OFFSET )
    {
        print( FATAL, "Offset out of range, must be between %g and %d.\n",
               MIN_USER_OFFSET, MAX_USER_OFFSET );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
        gentec_maestro_set_user_offset( offset );
    else
    {
        gm->user_offset = strtod( gentec_maestro_format( offset ), NULL );
        gm->user_offset_has_been_set = SET;
    }

    if ( offset != 0 && fabs( ( offset - gm->user_offset ) / offset ) >= 0.01 )
        print( WARN, "Offset was ajusted to %g.\n", gm->user_offset );

    return vars_push( FLOAT_VAR, gm->user_offset );
}





/*---------------------------------------------------*
 * Sets a new scale given the corresponding index
 *---------------------------------------------------*/

static
bool
gentec_maestro_init( const char * dev_name )
{
    Gentec_Maestro * gmt = &gentec_maestro_test;
    bool set_autoscale,
         set_scale,
         set_wavelength,
         set_att;


    /* Try to open a connection to the device */

    if ( ( gentec_maestro.handle = fsc2_lan_open( dev_name, NETWORK_ADDRESS,
                                                  PORT, OPEN_TIMEOUT,
                                                  UNSET ) ) == -1 )
    {
        print( FATAL, "Failed to connect to device.\n" );
        return UNSET;
    }

    /* Store all properties that might have been set during the preparation
       phase in the structure used for tests */

    gmt->mode               = gm->mode;
    gmt->scale_index        = gm->scale_index;
    gmt->att_is_on          = gm->att_is_on;
    gmt->wavelength         = gm->wavelength;
    gmt->trigger_level      = gm->trigger_level;
    gmt->autoscale_is_on    = gm->autoscale_is_on;
    gmt->anticipation_is_on = gm->anticipation_is_on;
    gmt->zero_offset_is_on  = gm->zero_offset_is_on;
    gmt->user_multiplier    = gm->user_multiplier;
    gmt->user_offset        = gm->user_offset;

    /* Disable continupus transmission of data, binary transfers and get the
       currect settings of the device */
    
    gentec_maestro_set_joulemeter_binary_mode( UNSET );
    gentec_maestro_continuous_transmission( UNSET );
    gentec_maestro_get_extended_status( );

    /* Check if there's a detector head connected at all - without one
       all the rest is futile. */

    if ( gm->mode == NO_DETECTOR )
    {
        print( FATAL, "No detector head is connected.\n" );
        return UNSET;
    }

    /* Now check if settings made in the preparation phase were valid.
       First check the scale setting.*/

    if (    gm->scale_index_has_been_set
         && (    gmt->scale_index < gm->min_scale_index
              || gmt->scale_index > gm->max_scale_index ) )
    {
        print( FATAL, "During preparations a scale was requested that "
               "can't be set for the connected head.\n" );
        return UNSET;
    }

    /* Check if the attenuator was set, but there isn't one. */

    if ( ! gm->att_is_available && gm->att_has_been_set )
    {
        print( FATAL, "During preparations the attenuator was set, "
               "but there's no attenuator.\n" );
        return UNSET;
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
                return UNSET;
            }
        }
        else if (    gmt->wavelength < gm->min_wavelength
                  || gmt->wavelength > gm->min_wavelength )
        {
            print( FATAL, "During preparations a wavelength (with "
                   " attenutaor off) was set that is out of range.\n" );
            return UNSET;
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
        return UNSET;
    }

    /* Check for the attenuator having been set despite none available */

    if ( ! gm->att_is_available && gmt->att_has_been_set )
    {
        print( FATAL, "Durinf the the attenuator was set, but there's no "
               " attenuator.\n" );
        return UNSET;
    }   

    if (    gmt->max_test_wavelength != 0
         && (    gmt->min_test_wavelength < gm->min_wavelength
              || gmt->max_test_wavelength > gm->max_wavelength ) )
    {
        print( FATAL, "During tests an ot of range wavelength was "
               "requested.\n" );
        return UNSET;
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
        return UNSET;
    }

    if (    gmt->max_test_wavelength_with_att != 0
         && (    gmt->min_test_wavelength_with_att <
                                                gm->min_wavelength_with_att
              || gmt->max_test_wavelength_with_att >
                                                gm->max_wavelength_with_att ) )
    {
        print( FATAL, "During tests an out of range wavelength was "
               "requested.\n" );
        return UNSET;
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
                print( FATAL, "During tests an ot of range wavelength was "
                       "requested.\n" );
                return UNSET;
            }
        }
        else if (    gmt->min_test_wavelength_unknown < gm->min_wavelength
                  || gmt->max_test_wavelength_unknown < gm->max_wavelength )
        {
            print( FATAL, "During tests an ot of range wavelength was "
                   "requested.\n" );
            return UNSET;
        }
    }

    /* Ok, everything seems to be fine;-) Start setting up the device,
       starting with the  autoscaling and scaling. Scale is set first if
       autoscale will be switched off (to avoid potential out of scale
       energies), otherwise autoscale is set first, then the scale. */

    set_autoscale =    gm->autoscale_has_been_set
                    && gm->autoscale_is_on != gmt->autoscale_is_on;
    set_scale     =    gm->scale_index_has_been_set
                    && gm->scale_index != gmt->scale_index;

    if ( set_autoscale && ! set_scale )
        gentec_maestro_set_autoscale( UNSET );
    else if ( ! set_autoscale && set_scale )
        gentec_maestro_set_scale( gmt->scale_index );
    else if ( set_autoscale && set_scale )
    {
        if ( gm->autoscale_is_on )
        {
            gentec_maestro_set_scale( gmt->scale_index );
            gentec_maestro_set_autoscale( UNSET );
        }
        else
        {
            gentec_maestro_set_autoscale( SET );
            gentec_maestro_set_scale( gmt->scale_index );
        }
    }

    /* Next we set wavelength and attenuator. Since not all wavelengths
       might be allowed with the current and requested attenuator setting
       we have to make sure we do it in the right sequence. */

    set_wavelength =    gm->wavelength_has_been_set
                     && gm->wavelength != gmt->wavelength;
    set_att        =    gm->att_has_been_set
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

    return SET;
}


/*---------------------------------------------------*
 * Sets a new scale given the corresponding index
 *---------------------------------------------------*/

static
double
gentec_maestro_set_scale( int index )
{
	char cmd[ 10 ];

	fsc2_assert(    index >= gentec_maestro.min_scale_index
                 && index <= gentec_maestro.max_scale_index );

	sprintf( cmd, "*SCS%02d", index );
	gentec_maestro_command( cmd );

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
	long int index;
	char *eptr;


	if (    gentec_maestro_talk( "*GCR", reply, sizeof reply ) < 10
         || strncmp( reply, "Range: ", 7 ) )
		gentec_maestro_failure( );

	index = strtol( reply + 7, &eptr, 10 );
	if (    strcmp( eptr, "\r\n" )
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

	if (    gentec_maestro_talk( "*GAS", reply, sizeof reply ) != 14
		 || strncmp( reply, "AutoScale: ", 11 )
		 || ( reply[ 11 ] != '0' && reply[ 11 ] != '1' )
		 || strcmp( reply + 12, "\r\n" ) )
		gentec_maestro_failure( );

	return gentec_maestro.autoscale_is_on = reply[ 11 ] == '1';
}


/*---------------------------------------------------*
 * Set a new trigger level
 *---------------------------------------------------*/

static
double
gentec_maestro_set_trigger_level( double level )
{
	char cmd[ 100 ];


	fsc2_assert( level >= 0.0005 && level < 0.9995 );

	printf( cmd, "*STL%.*f", level < 0.0995 ? 2 : 1, 100 * level );
	gentec_maestro_command( cmd );

    return gentec_maestro.trigger_level = strtod( cmd + 4, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_get_trigger_level( void )
{
	char reply[ 30 ];
    double level;
    char *ep;

    if (    gentec_maestro_talk( "*GTL", reply, sizeof reply ) < 18
         || strncmp( reply, "Trigger Level: ", 15 ) )
        gentec_maestro_failure( );

    level = strtod( reply, &ep );
    if (    strcmp( ep, "\r\n" )
         || level < MIN_TRIGGER_LEVEL
         || level > MAX_TRIGGER_LEVEL )
        gentec_maestro_failure( );

    return gentec_maestro.trigger_level = 0.01 * level;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
int
gentec_maestro_get_mode( void )
{
    char reply[ 10 ];


    if (    gentec_maestro_talk( "*GMD", reply, sizeof reply ) != 9
         || strncmp( reply, "Mode: ", 6 )
         || strcmp( reply + 7, "\r\n" )
         || ! isdigit( ( int ) reply[ 6 ] ) )
        gentec_maestro_failure( );

    switch ( reply[ 6 ] )
    {
        case '0':
            return gentec_maestro.mode = POWER_MODE;

        case '1' :
            return gentec_maestro.mode = ENERGY_MODE;

        case '2' :
            return gentec_maestro.mode = SSE_MODE;

        case '6' :
            return gentec_maestro.mode = DBM_MODE;

        case '7' :
            return gentec_maestro.mode = NO_DETECTOR;

        default :
            gentec_maestro_failure( );
    }

    /* We never should end up here... */

    return -1;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_get_current_value( void )
{
    char reply[ 20 ];
    double val;
    char *ep;


    if ( gentec_maestro_talk( "*CVU", reply, sizeof reply ) < 3 )
        gentec_maestro_failure( );

    val = strtod( reply, &ep );
    if (    strcmp( ep, "\r\n" )
         || ( ( val == HUGE_VAL || val == -HUGE_VAL ) && errno == ERANGE ) )
        gentec_maestro_failure( );

    return val;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_check_for_new_value( void )
{
    char reply[ 30 ];


    gentec_maestro_talk( "*NVU", reply, sizeof reply );

    if ( ! strcmp( reply, "New Data Available\r\n" ) )
        return 1;
    else if ( ! strcmp( reply, "New Data Not Available\r\n" ) )
        return 0;

    gentec_maestro_failure( );
    return 0;                    /* never executed */
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_continuous_transmission( bool on_off )
{
    gentec_maestro_command( on_off ? "*CAU" : "*CSU" );
    return gentec_maestro.continuous_transmission_is_on = on_off;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_get_laser_frequency( void )
{
    char reply[ 20 ];
    double freq;
    char *ep;

    if ( gentec_maestro_talk( "*GRR", reply, sizeof reply ) < 3 )
        gentec_maestro_failure( );

    freq = strtod( reply, &ep );
    if (    strcmp( ep, "\r\n" )
         || ( ( freq == HUGE_VAL || freq == -HUGE_VAL ) && errno == ERANGE )
         || freq < 0 )
        gentec_maestro_failure( );

    return freq;
}


/*---------------------------------------------------*
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
 *---------------------------------------------------*/

static
bool
gentec_maestro_get_joulemeter_binary_mode( void )
{
    char reply[ 30 ];


    if (    gentec_maestro_talk( "*GBM", reply, sizeof reply ) != 26
         || strncmp( reply, "Binary Joulemeter Mode: ", 24 )
         || ( reply[ 24 ] != '0' && reply[ 24 ] != '1' )
         || strcmp( reply + 25, "\r\n" ) )
        gentec_maestro_failure( );

    return reply[ 24 ] != '0';
}


/*---------------------------------------------------*
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
 *---------------------------------------------------*/

static
double
gentec_maestro_set_wavelength( double wavelength )
{
    char cmd[ 30 ];
    long wl = lrnd( 1.0e9 * wavelength );


    fsc2_assert(    (    ! gentec_maestro.att_is_on
                      && wl >= gentec_maestro.min_wavelength
                      && wl <= gentec_maestro.max_wavelength )
                 || (    gentec_maestro.att_is_on
                      && wl >= gentec_maestro.min_wavelength_with_att
                      && wl <= gentec_maestro.max_wavelength_with_att ) );

    snprintf( cmd, sizeof cmd, "*PWC%ld", wl );
    gentec_maestro_command( cmd );

    gentec_maestro.wavelength = wl;
    return 1.0e-9 * wl;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_get_wavelength( void )
{
    char reply[ 20 ];
    long wl;
    char *ep;


    if (    gentec_maestro_talk( "*GWL", reply, sizeof reply ) < 8
         || strncmp( reply, "PWC: ", 5 ) )
        gentec_maestro_failure( );

    wl = strtol( reply + 5, &ep, 10 );
    if (     (    ! gentec_maestro.att_is_on
               && wl >= gentec_maestro.min_wavelength
               && wl <= gentec_maestro.max_wavelength )
         || (    gentec_maestro.att_is_on
              && wl >= gentec_maestro.min_wavelength_with_att
              && wl <= gentec_maestro.max_wavelength_with_att )
          || strcmp( ep, "\r\n" ) )
        gentec_maestro_failure( );

    gentec_maestro.wavelength = wl;
    return 1.0e-9 * wl;
}


/*---------------------------------------------------*
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
 *---------------------------------------------------*/

static
bool
gentec_maestro_get_anticipation( void )
{
    char reply[ 20 ];


    if (    gentec_maestro_talk( "*GAN", reply, sizeof reply ) != 17
         || strncmp( reply, "Anticipation: ", 14 )
         || ( reply[ 14 ] != '0' && reply[ 14 ] != '1' )
         || strcmp( reply + 15, "\r\n" ) )
        gentec_maestro_failure( );

    return gentec_maestro.anticipation_is_on = reply[ 14 ] != '0';
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_zero_offset( void )
{
    const char *cmd;


    /* There are different commands for setting the zero offset for
       photodiodes and other detectors. Pick the correct one by
       checking the dector name - photodiode names start with either
       "PH" or "PE" */

    if (    strncmp( gentec_maestro.head_name, "PH", 2 )
         && strncmp( gentec_maestro.head_name, "PE", 2 ) )
        cmd = "*SOU";
    else
        cmd = "*SDZ";

    gentec_maestro_command( cmd );

    return gentec_maestro.zero_offset_is_on = SET;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_clear_zero_offset( void )
{
    gentec_maestro_command( "*COU" );
    return gentec_maestro.zero_offset_is_on = UNSET;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_test_zero_offset( void )
{
    char reply[ 10 ];


    if (    gentec_maestro_talk( "*GZO", reply, sizeof reply ) != 9
         || strncmp( reply, "Zero: ", 6 )
         || ( reply[ 6 ] != '0' && reply[ 6 ] != '1' )
         || strcmp( reply + 7, "\r\n" ) )
        gentec_maestro_failure( );

    return gentec_maestro.zero_offset_is_on = reply[ 6 ] != '0';
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_set_user_multiplier( double mul )
{
    char cmd[ 13 ] = "*MUL";


    fsc2_assert( mul >= MIN_USER_MULTIPLIER && mul <= MAX_USER_MULTIPLIER );

    strcat( cmd, gentec_maestro_format( mul ) );
    gentec_maestro_command( cmd );
    return gentec_maestro.user_multiplier = strtod( cmd + 4, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_get_user_multiplier( void )
{
    char reply[ 40 ];
    double mul;
    char *ep;

    ;
    if (    gentec_maestro_talk( "*GUM", reply, sizeof reply ) < 20
         || strncmp( reply, "User Multiplier: ", 17 ) )
        gentec_maestro_failure( );

    mul = strtod( reply + 17, &ep );
    if (    strcmp( ep, "\r\n" )
         || mul < MIN_USER_MULTIPLIER
         || mul > MAX_USER_MULTIPLIER )
        gentec_maestro_failure( );

    return gentec_maestro.user_multiplier = mul;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_set_user_offset( double offset )
{
    char cmd[ 13 ] = "*OFF";


    fsc2_assert( offset >= MIN_USER_OFFSET && offset <= MAX_USER_OFFSET );

    strcat( cmd, gentec_maestro_format( offset ) );
    gentec_maestro_command( cmd );
    return gentec_maestro.user_offset = strtod( cmd + 4, NULL );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
double
gentec_maestro_get_user_offset( void )
{
    char reply[ 40 ];
    double offset;
    char *ep;

    ;
    if (    gentec_maestro_talk( "*GUO", reply, sizeof reply ) < 16
         || strncmp( reply, "User Offset: ", 13 ) )
        gentec_maestro_failure( );

    offset = strtod( reply + 13, &ep );
    if (    strcmp( ep, "\r\n" )
         || offset < MIN_USER_OFFSET
         || offset > MAX_USER_OFFSET )
        gentec_maestro_failure( );

    return gentec_maestro.user_offset = offset;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_energy_mode( bool on_off )
{
    char cmd[ ] = "*SSE*";


    cmd[ 4 ] = on_off ? '1' : '0';
    gentec_maestro_command( cmd );
    return gentec_maestro.energy_mode_is_on = on_off;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_set_attenuator( bool on_off )
{
    char cmd[ ] = "*ATT*";


    fsc2_assert( gentec_maestro.att_is_available );

    cmd[ 4 ] = on_off ? '1' : '0';
    gentec_maestro_command( cmd );
    return gentec_maestro.att_is_on = on_off;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
bool
gentec_maestro_get_attenuator( void )
{
    char reply[ 16 ];


    fsc2_assert( gentec_maestro.att_is_available );

    if (    gentec_maestro_talk( "*GAT", reply, sizeof reply ) != 15
         || strncmp( reply, "Attenuator: ", 12 )
         || ( reply[ 12 ] != '0' && reply[ 12 ] != '1' )
         || strcmp( reply + 13, "\r\n" ) )
        gentec_maestro_failure( );

    return gentec_maestro.att_is_on = reply[ 12 ] != '0';
}


/*---------------------------------------------------*
 * Converts a status entry too a 4-byte integer
 *---------------------------------------------------*/

static
unsigned long
gentec_maestro_status_entry_to_int( const char * e )
{
    char str[ 9 ] = "";
    unsigned long int res;
    char *ep;

    strncpy( str + 4, e + 6, 4 );
    strncpy( str, e + 18, 4 );
    res = strtoul( str, &ep, 16 );
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
    double res = 1.0;
    unsigned long int v = 0;
    int sign;
    int expo = -127;
    unsigned char offset_expo;
    double x = 1.0 / 8388608;         /* 2^-23 */
	int i;

    for ( i = 18; i > 0; i -= 12 )
    {
        int j;

        for ( j = 0; j < 4; j++ )
        {
            unsigned char c = e[ i + j ];

            v <<= 4;
            v |= c <= '9' ? ( c - '0' ) : ( c - 'A' + 10 );
        }
    }

    sign  = v & 0x80000000UL ? -1 : 1;

    offset_expo = ( v & 0x7F800000UL ) >> 23;
    if ( offset_expo == 0xFF )                /* NaN or infinity? */
        gentec_maestro_failure( );
    expo += offset_expo;

    v    &= 0x7FFFFFUL;

    for ( i = 0; i < 23; v >>= 1, x *= 2, i++ )
        res += ( v & 1 ) * x;

    return sign * res * pow( 2, expo );
}


/*---------------------------------------------------*
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
 *---------------------------------------------------*/

static
void
gentec_maestro_get_extended_status( void )
{
    char reply[ 709 ];
    long int v;
    double d;
    int i;


    if ( gentec_maestro_talk( "*ST2", reply, sizeof reply ) != 708 )
        gentec_maestro_failure( );

    /* Check for basic layout */

    for ( i = 0; i <= 0x39; i++ )
    {
        char r[ 7 ];
        int j;

        sprintf( r, ":0%04X", i );
        if (    strncmp( reply + 12 * i, r, 6 )
             || strncmp( reply + 12 * i + 10, "\r\n", 2 ) )
            gentec_maestro_failure( );

        for ( j = 12 * i + 6; j < 12 * i + 10; ++j )
            if ( ! isxdigit( ( int ) reply[ j ] ) )
                gentec_maestro_failure( );
    }

    /* Now extract all the relevant values */

    if ( strcmp( reply + 0x40 * 12, ":100000000\r\n" ) )
        gentec_maestro_failure( );

    /* Now extract all the values */

    v = gentec_maestro_status_entry_to_int( reply + 0x04 * 12 );

#define POWER_MODE     0     /* power in W  */
#define ENERGY_MODE    1     /* energy in J */
#define SSE_MODE       2     /* single shot energy in J */
#define DBM_MODE       6     /* power in dBm */
#define NO_DETECTOR    7

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
    if ( v )
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
    if ( v )
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

    d = gentec_maestro_status_entry_to_float( reply + 0x2e * 12 );
    if ( v < 0.001 || v > 0.999 )
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
 *---------------------------------------------------*/

static
void
gentec_maestro_command( const char * cmd )
{
    long len = strlen( cmd );

    if ( fsc2_lan_write( gentec_maestro.handle, cmd, strlen( cmd ),
                         WRITE_TIMEOUT, UNSET ) != len )
        gentec_maestro_failure( );
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
long
gentec_maestro_talk( const char * cmd,
					 char       * reply,
					 long         length )
{
    gentec_maestro_command( cmd );
    if ( ( length = fsc2_lan_read( gentec_maestro.handle, reply, length - 1,
                                   READ_TIMEOUT, UNSET ) ) < 3 )
        gentec_maestro_failure( );
    reply[ length ] = '\0';
    return length;
}


/*---------------------------------------------------*
 *---------------------------------------------------*/

static
void
gentec_maestro_failure( void )
{
    print( FATAL, "Communication with powermeter failed.\n" );
    THROW( EXCEPTION );
}


/*---------------------------------------------------*
 * Converts a scale index (must be between 0 and 41) to
 * the corresponding scale
 *---------------------------------------------------*/

static
double
gentec_maestro_index_to_scale( int index )
{
	fsc2_assert( index < 0 || index > MAX_SCALE );
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


    *no_fit = UNSET;
	for ( i = 0; i <= MAX_SCALE; i++ )
	{
		double r = gentec_maestro_index_to_scale( i );

		if ( scale < 0.9999 * r )
        {
            *no_fit = SET;
			return i;
        }

		if ( scale < 1.0001 *r )
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
 *---------------------------------------------------*/

static
const char *
gentec_maestro_pretty_print_scale( int index )
{
    static char ret[ 10 ];
    char *c = ret;
    const char *units[ ] = { "pJ/pW", "nJ/nW", "uJ/uW", "mJ/mW",
                             "J/W",   "kJ/kW", "MJ/MW" };

    fsc2_assert( index >= 0 && index <= MAX_SCALE );
    
    *c++ = index & 1 ? '3' : '1';
    if ( index & 6 )
        *c++ = '0';
    if ( index & 4 )
        *c++ = '0';
    *c++ = ' ';
    strcpy( c, units[ index / 6 ] );

    return ret;
}   


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
