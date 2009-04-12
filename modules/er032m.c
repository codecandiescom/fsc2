/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 * 
 * 
 *  Programming this field controller became a bit of a mess because of the
 *  number of conditions to be taken into consideration:
 * 
 *  1. Field range is -50 G to 23000 G
 *  2. CF must be set with a resolution of 50 mG
 *  3. Sweep range limits 0 G to 16000 G
 *  4. Sweep range resolution: 100 mG
 *  5. CF plus or minus 0.5 time sweep range may never excced the field
 *     range limits
 *  6. Sweep range is not truely symmetric (in contrast to what the manual
 *     claims), SWA settings can range from 0 to 4095, the generated field
 *     is equal to CF for SWA = 2048
 *  7. Repeatability of CF setting: 5 mG
 */


#include "fsc2_module.h"
#include "gpib_if.h"


/* Include configuration information for the device */

#include "er032m.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#define ER032M_TEST_FIELD  3480.0   /* returned as current field in test run */

#define MIN_SWA            0
#define CENTER_SWA         2048
#define MAX_SWA            4095


/* The maximum field resolution: this is, according to the manual the best
   result that can be achieved when repeatedly setting the center field and
   it doesn't seem to make too much sense to set a new field when the
   difference from the current field is below this resolution. */

#define ER032M_RESOLUTION   5.0e-3


/* Maximum resolution that can be used for sweep ranges */

#define ER032M_SW_RESOLUTION 0.1


/* As Mr. Antoine Wolff from Bruker pointed out to me the center field
   can only be set with a resolution of 50 mG. */

#define ER032M_CF_RESOLUTION 5.0e-2


/* Defines the time before we test again if it is found that the overload
   LED is still on. */

#define ER032M_WAIT_TIME   100000     /* in us */


/* Define the maximum number of retries before giving up if the
   overload LED is on. */

#define ER032M_MAX_RETRIES   300


/* When setting a new center field or sweep width the value send to the
   device is read back to check if it really got set. If the values differ
   the ER032M_MAX_SET_RETRIES times the value is again set and also checked
   before we give up. */

#define ER032M_MAX_SET_RETRIES 3


#define SEARCH_UP   1
#define SEARCH_DOWN 0


/* Exported functions */

int er032m_init_hook(        void );
int er032m_test_hook(        void );
int er032m_end_of_test_hook( void );
int er032m_exp_hook(         void );
int er032m_end_of_exp_hook(  void );


Var_T * magnet_name(            Var_T * v );
Var_T * magnet_setup(           Var_T * v );
Var_T * magnet_field(           Var_T * v );
Var_T * set_field(              Var_T * v );
Var_T * get_field(              Var_T * v );
Var_T * magnet_field_step_size( Var_T * v );
Var_T * sweep_up(               Var_T * v );
Var_T * sweep_down(             Var_T * v );
Var_T * magnet_sweep_up(        Var_T * v );
Var_T * magnet_sweep_down(      Var_T * v );
Var_T * reset_field(            Var_T * v );
Var_T * magnet_reset_field(     Var_T * v );
Var_T * magnet_command(         Var_T * v );


static void er032m_init( void );

static void er032m_field_check( double field );

static void er032m_start_field( void );

static double er032m_get_field( void );

static double er032m_set_field( double field );

static void er032m_change_field_and_set_sw( double field );

static void er032m_change_field_and_sw( double field );

static void er032m_change_field_and_keep_sw( double field );

static bool er032m_guess_sw( double field_diff );

static double er032m_set_cf( double center_field );

static double er032m_get_cf( void );

static double er032m_set_sw( double sweep_width );

static double er032m_get_sw( void );

static int er032m_set_swa( int sweep_address );

#if 0
static int er032m_get_swa( void );        /* currently not needed */
#endif

static void er032m_test_leds( void );

static bool er032m_command( const char *cmd );

static bool er032m_talk( const char * cmd,
                         char *       reply,
                         long *       length );

static void er032m_failure( void );

static int er032m_best_fit_search( double * cf,
                                   int *    swa,
                                   bool     dir,
                                   int fac );


static struct
{
    int device;

    double act_field;       /* the real current field */

    double cf;              /* the current CF setting */
    double sw;              /* the current SW setting */
    int swa;                /* the current SWA setting */

    bool is_sw;             /* set if SW is known */
    double max_sw;          /* maximum sweep width */

    double swa_step;        /* field step between two SWAs */
    int step_incr;          /* how many SWAs to step for a sweep step */

    double start_field;     /* the start field given by the user */
    double field_step;      /* the field steps to be used */

    bool is_init;           /* flag, set if magnet_setup() has been called */

    double max_field_dev;   /* maximum field deviation (in test run) */
} magnet;


#define DEVIATION( f )                                                 \
    do {                                                               \
        double d = fabs( ( f ) - magnet.cf                             \
                   - ( magnet.swa - CENTER_SWA ) * magnet.swa_step );  \
        if ( d > magnet.max_field_dev )                                \
            magnet.max_field_dev = d;                                  \
    } while ( 0 )


/*----------------------------------------------------------------*
 *----------------------------------------------------------------*/

int
er032m_init_hook( void )
{
    /* Set global variable to indicate that GPIB bus is needed. */

    Need_GPIB = SET;

    /* Initialize some variables in the magnets structure. */

    magnet.device = -1;
    magnet.is_init = UNSET;
    magnet.is_sw = UNSET;
    magnet.max_field_dev = 0.0;

    /* Make sure the maximum sweep width isn't larger than the difference
       between the maximum and minimum field. */

    magnet.max_sw = ER032M_MAX_SWEEP_WIDTH;
    if ( magnet.max_sw > ER032M_MAX_FIELD - ER032M_MIN_FIELD )
        magnet.max_sw = ER032M_MAX_FIELD - ER032M_MIN_FIELD;

    return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int
er032m_test_hook( void )
{
    double rem;


    if ( magnet.is_init )
        er032m_start_field( );
    else
    {
        magnet.act_field = ER032M_TEST_FIELD;

        rem = ( lrnd( magnet.act_field / ER032M_MIN_FIELD_STEP )
                % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP )
              ) * ER032M_MIN_FIELD_STEP;

        if ( rem <= 2 * ER032M_MIN_FIELD_STEP )
        {
            magnet.swa = er032m_set_swa( CENTER_SWA );
            magnet.sw = er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( magnet.act_field );
        }
        else
        {
            er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( magnet.act_field - rem );
            er032m_set_swa( magnet.swa = CENTER_SWA + 1 );
            magnet.sw = er032m_set_sw( MAX_SWA * rem );
        }
    }

    magnet.act_field = magnet.is_init ? magnet.start_field : ER032M_TEST_FIELD;
    return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int
er032m_end_of_test_hook( void )
{
    /* Tell user if field maximum field deviation was more than 1% of the
       field step width (if one was set) or was larger than the resolution
       reasonably to be expected */

    if (    (    magnet.is_init
              && magnet.max_field_dev / magnet.field_step >= 0.01 )
         || (    ! magnet.is_init
              && floor( magnet.max_field_dev / ER032M_RESOLUTION ) >= 1 ) )
        print( NO_ERROR, "Maximum field error during test run was %.0f mG.\n",
                magnet.max_field_dev * 1.0e3 );
    magnet.max_field_dev = 0.0;
    return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

int
er032m_exp_hook( void )
{
    er032m_init( );
    return 1;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

int
er032m_end_of_exp_hook( void )
{
    /* Tell user if field maximum field deviation was more than 1% of the
       field step width (if one was set) or was larger than the resolution
       reasonably to be expected */

    if (    (    magnet.is_init
              && magnet.max_field_dev / magnet.field_step >= 0.01 )
         || (    ! magnet.is_init
              && floor( magnet.max_field_dev / ER032M_RESOLUTION ) >= 1 ) )
        print( NO_ERROR, "Maximum field error during experiment was "
               "%.0f mG.\n", magnet.max_field_dev * 1.0e3 );
    magnet.max_field_dev = 0.0;

    if ( magnet.device >= 0 )
        gpib_local( magnet.device );
    return 1;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------*
 * Function for registering the start field and the
 * field step size during the PREPARATIONS section.
 *--------------------------------------------------*/

Var_T *
magnet_setup( Var_T * v )
{
    double start_field;
    double field_step;
    long rem;


    /* Check that both the variables, i.e. the start field and the field step
       size are reasonable. */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    start_field = lrnd( get_double( v, "start field" )
                        / ER032M_MIN_FIELD_STEP ) * ER032M_MIN_FIELD_STEP;

    er032m_field_check( start_field );

    /* Get the field step size and round it to the next allowed value
       (resulting in a sweep step resolution of about 25 uG with the
       given setting for the sweep width resolution of 0.1 G) */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing field step size.\n" );
        THROW( EXCEPTION );
    }

    field_step = get_double( v, "field step width" );

    if ( field_step < ER032M_MIN_FIELD_STEP )
    {
        print( FATAL, "Field sweep step size (%f G) too small, minimum is "
               "%f G.\n", VALUE( v ), ER032M_MIN_FIELD_STEP );
        THROW( EXCEPTION );
    }

    field_step = lrnd( MAX_SWA * field_step / ER032M_SW_RESOLUTION )
                 * ER032M_SW_RESOLUTION / MAX_SWA;

    too_many_arguments( v );

    /* We would get into problems with setting the field exactly for start
       fields not being multiples of the center field resolution and field
       steps that are larger than the center field resolution when we have
       to shift the center field during a longer sweep. Thus we readjust
       the start field in these cases. */

    rem = lrnd( start_field / ER032M_MIN_FIELD_STEP )
          % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP );

    if (    rem > 0
         && lrnd( field_step / ER032M_MIN_FIELD_STEP ) %
                    lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP ) == 0 )
    {
        start_field = lrnd( start_field / ER032M_CF_RESOLUTION )
                      * ER032M_CF_RESOLUTION;
        print( SEVERE, "Readjusting start field to %.3f G.\n",
               start_field );
        THROW( EXCEPTION );
    }

    magnet.start_field = start_field;
    magnet.field_step = field_step;
    magnet.is_init = SET;

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_sweep_up( Var_T * v )
{
    return sweep_up( v );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
sweep_up( Var_T * v  UNUSED_ARG )
{
    int steps;
    int new_swa;
    double new_cf;
    double diff;


    if ( ! magnet.is_init )
    {
        print( FATAL, "No sweep step size has been set - you must call "
               "the function magnet_setup() to be able to do sweeps.\n" );
        THROW( EXCEPTION );
    }

    /* Check that the new field value is still within the allowed range */

    er032m_field_check( magnet.act_field + magnet.field_step );

    /* If the new field is still within the sweep range set the new field by
       changing the SWA, otherwise shift the center field up by a quarter of
       the total sweep width. If this would move the upper sweep limit above
       the maximum allowed field move the center field only as far as
       possible. */

    if ( magnet.swa + magnet.step_incr <= MAX_SWA )
        er032m_set_swa( magnet.swa += magnet.step_incr );
    else
    {
        if ( magnet.cf + 0.75 * magnet.sw < ER032M_MAX_FIELD )
            steps = MAX_SWA / 4;
        else
            steps = irnd( floor( ( ER032M_MAX_FIELD - magnet.cf )
                                 / magnet.swa_step ) ) - CENTER_SWA;

        /* We also need the center field to be a multiple of the center field
           resolution. If necessary shift the center field by as many swep
           steps until this condition is satisfied (with a deviation of not
           more than 2 mG). And while we're at it also try to eliminate
           differences between the actual field and the field the user is
           expecting. */

        new_swa = magnet.swa - steps + magnet.step_incr;
        diff = magnet.cf + ( magnet.swa - CENTER_SWA ) * magnet.swa_step
               - magnet.act_field;
        new_cf = magnet.cf - diff + steps * magnet.swa_step;

        /* When we're extremely near to the maximum field it may happen that
           the field can't be set with a useful combination of CF and SWA. */

        if (    new_swa > MAX_SWA
             || new_cf + 0.5 * magnet.sw > ER032M_MAX_FIELD )
        {
            print( FATAL, "Can't set field of %.3f G, too near to maximum "
                   "field.\n", magnet.act_field + magnet.field_step );
            THROW( EXCEPTION );
        }

        er032m_best_fit_search( &new_cf, &new_swa, SEARCH_UP, 2 );

        fsc2_assert(    new_swa >= MIN_SWA && new_swa <= MAX_SWA
                     && new_cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD
                     && new_cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD );

        er032m_set_swa( magnet.swa = new_swa );
        magnet.cf = er032m_set_cf( new_cf );
    }

    er032m_test_leds( );

    magnet.act_field += magnet.field_step;
    DEVIATION( magnet.act_field );
    return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_sweep_down( Var_T * v )
{
    return sweep_down( v );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
sweep_down( Var_T * v  UNUSED_ARG )
{
    int steps;
    int new_swa;
    double new_cf;
    double diff;


    if ( ! magnet.is_init )
    {
        print( FATAL, "No sweep step size has been set - you must call "
               "function magnet_setup() to be able to do sweeps.\n" );
        THROW( EXCEPTION );
    }

    er032m_field_check( magnet.act_field - magnet.field_step );

    /* If the new field is still within the sweep range set the new field by
       changing the SWA, otherwise shift the center field down by a quarter of
       the total sweep width. If this would move the lower sweep limit below
       the minimum allowed field move the center field only as far as
       possible. */

    if ( ( magnet.swa - magnet.step_incr ) >= MIN_SWA )
        er032m_set_swa( magnet.swa -= magnet.step_incr );
    else
    {
        if ( magnet.cf - 0.75 * magnet.sw > ER032M_MIN_FIELD )
            steps = MAX_SWA / 4;
        else
            steps = irnd( floor( ( magnet.cf - ER032M_MIN_FIELD )
                                 / magnet.swa_step ) ) - CENTER_SWA;

        /* We also need the center field to be a multiple of the center field
           resolution. If necessary shift the center field by as many swep
           steps until this condition is satisfied (with a deviation of not
           more than 2 mG) */

        new_swa = magnet.swa + steps - magnet.step_incr;
        diff = magnet.cf + ( magnet.swa - CENTER_SWA ) * magnet.swa_step
               - magnet.act_field;
        new_cf = magnet.cf - diff - steps * magnet.swa_step;

        /* When we're extremely near to the minimum field it may happen that
           the field can't be set with a useful combination of CF and SWA. */

        if (    new_swa < MIN_SWA
             || new_cf - 0.5 * magnet.sw < ER032M_MIN_FIELD )
        {
            print( FATAL, "Can't set field of %.3f G, too near to minimum "
                   "field.\n", magnet.act_field + magnet.field_step );
            THROW( EXCEPTION );
        }

        er032m_best_fit_search( &new_cf, &new_swa, SEARCH_DOWN, 2 );

        fsc2_assert(    new_swa >= MIN_SWA && new_swa <= MAX_SWA
                     && new_cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD
                     && new_cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD );

        er032m_set_swa( magnet.swa = new_swa );
        magnet.cf = er032m_set_cf( new_cf );
    }

    er032m_test_leds( );

    magnet.act_field -= magnet.field_step;
    DEVIATION( magnet.act_field );
    return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_reset_field( Var_T * v )
{
    return reset_field( v );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
reset_field( Var_T * v  UNUSED_ARG )
{
    if ( ! magnet.is_init )
    {
        print( FATAL, "Start field has not been defined  - you must call "
               "function magnet_setup() before.\n" );
        THROW( EXCEPTION );
    }

    er032m_start_field( );

    return vars_push( FLOAT_VAR, magnet.act_field );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_field( Var_T *v )
{
    return v == NULL ? get_field( v ) : set_field( v );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
get_field( Var_T * v  UNUSED_ARG )
{
    return vars_push( FLOAT_VAR, er032m_get_field( ) );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
set_field( Var_T * v )
{
    double field;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    field = get_double( v, "magnetic field" );

    if ( ( v = vars_pop( v ) ) != NULL )
        print( SEVERE, "Can't use a maximum field error.\n" );

    too_many_arguments( v );

    er032m_field_check( field );

    return vars_push( FLOAT_VAR, er032m_set_field( field ) );
}


/*----------------------------------------------------------------*
 * Function returns the minimum field step size if called without
 * an argument and the possible field step size nearest to the
 * argument.
 *----------------------------------------------------------------*/

Var_T *
magnet_field_step_size( Var_T * v )
{
    double field_step;
    long steps;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, ER032M_RESOLUTION );

    field_step = get_double( v, "field step size" );

    too_many_arguments( v );

    if ( field_step < 0.0 )
    {
        print( FATAL, "Invalid negative field step size.\n" );
        THROW( EXCEPTION );
    }

    if ( ( steps = lrnd( field_step / ER032M_RESOLUTION ) ) == 0 )
        steps++;

    return vars_push( FLOAT_VAR, steps * ER032M_RESOLUTION );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
magnet_command( Var_T * v )
{
    char *cmd = NULL;


    CLOBBER_PROTECT( cmd );

    vars_check( v, STR_VAR );
    
    if ( FSC2_MODE == EXPERIMENT )
    {
        TRY
        {
            cmd = translate_escape_sequences( T_strdup( v->val.sptr ) );
            er032m_command( cmd );
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


/*------------------------------*
 * Initialization of the device
 *------------------------------*/

static void
er032m_init( void )
{
    double rem;


    if ( gpib_init_device( DEVICE_NAME, &magnet.device ) == FAILURE )
    {
        magnet.device = -1;
        print( FATAL, "Initialization of device failed: %s\n",
               gpib_error_msg );
        THROW( EXCEPTION );
    }

    /* Switch to computer mode. */

    er032m_command( "CO\r" );

    /* Enable Hall controller. */

    er032m_command( "EC\r" );

    /* Switch to mode 0. */

    er032m_command( "MO0\r" );

    /* Switch off service requests. */

    er032m_command( "SR0\r" );

    /* Set IM0 sweep mode (we don't use it, just to make sure we don't
       trigger a sweep start inadvertently). */

    er032m_command( "IM0\r" );

    /* Check the status of the LEDs. */

    er032m_test_leds( );

    /* Set the start field and the field step if magnet_setup() has been
       called, otherwise measure current field and make CF identical to
       the current field. */

    if ( magnet.is_init )
        er032m_start_field( );
    else
    {
        magnet.act_field = er032m_get_field( );

        rem = ( lrnd( magnet.act_field / ER032M_MIN_FIELD_STEP )
                % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP )
              ) * ER032M_MIN_FIELD_STEP;

        if ( rem <= 2 * ER032M_MIN_FIELD_STEP )
        {
            magnet.swa = er032m_set_swa( CENTER_SWA );
            magnet.sw = er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( magnet.act_field );
        }
        else
        {
            er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( magnet.act_field - rem );
            er032m_set_swa( magnet.swa = CENTER_SWA + 1 );
            magnet.sw = er032m_set_sw( MAX_SWA * rem );
        }

        DEVIATION( magnet.act_field );
        er032m_test_leds( );
    }
}


/*--------------------------------------------------------------------*
 * Function sets up the magnet in case magnet_setp() had been called,
 * assuming that field will be changed via sweep_up() or sweep_down()
 * calls, in which case the sweep usually can be done via changes of
 * the SWA and without changing the center field. The center field is
 * set to the required start field, so that up and down sweeps can be
 * done without changing the center field.
 *--------------------------------------------------------------------*/

static void
er032m_start_field( void )
{
    int factor;
    int shift;
    double cur_cf;
    double cur_sw;


    magnet.cf = magnet.start_field;
    magnet.sw = MAX_SWA * magnet.field_step;
    magnet.swa = CENTER_SWA;
    magnet.swa_step = magnet.field_step;
    magnet.step_incr = 1;

    /* Make sure that the sweep width isn't larger than the maximum sweep
       width otherwise divide it by two as often as needed and remember
       that we now have to step not by one SWA but by a power of 2 SWAs. */

    if ( magnet.sw > magnet.max_sw )
    {
        factor = irnd( ceil( magnet.sw / magnet.max_sw ) );

        magnet.sw /= factor;
        magnet.swa_step /= factor;
        magnet.step_incr *= factor;
    }

    /* Sweep range must be a multiple of ER032M_SW_RESOLUTION and we adjust
       it silently to fit this requirement - hopefully, this isn't going to
       lead to any real field precision problems. */

    magnet.sw = ER032M_SW_RESOLUTION
                * lrnd( magnet.sw / ER032M_SW_RESOLUTION );
    magnet.swa_step = magnet.sw / MAX_SWA;
    magnet.field_step = magnet.step_incr * magnet.swa_step;

    /* Make sure the sweep range is still within the allowed field range,
       otherwise shift the center field and calculate the SWA that's needed
       in this case. When the start field is extremely near to the limits
       it can happen that it's not possible to set the start field. */

    if ( magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD )
    {
        shift = irnd( ceil( ( magnet.cf + 0.5 * magnet.sw
                              - ER032M_MAX_FIELD ) / magnet.swa_step ) );
        magnet.swa += shift;
        magnet.cf -= shift * magnet.swa_step;
    }
    else if ( magnet.cf - 0.5 * magnet.sw < ER032M_MIN_FIELD )
    {
        shift = irnd( ceil( ( ER032M_MIN_FIELD
                              - ( magnet.cf - 0.5 * magnet.sw ) )
                            / magnet.swa_step ) );
        magnet.swa -= shift;
        magnet.cf += shift * magnet.swa_step;
    }

    if (    magnet.swa > MAX_SWA || magnet.swa < MIN_SWA
         || magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD
         || magnet.cf - 0.5 * magnet.sw < ER032M_MIN_FIELD )
    {
        print( FATAL, "Can't set field of %.3f G, too neat to maximum or "
               "minimum field.\n", magnet.start_field );
        THROW( EXCEPTION );
    }

    /* Now readjust the SWA count a bit to make sure that the center field is
       a multiple of the maximum CF resolution (at least with in accuracy of
       2 mG). This should always work out correctly because we made sure that
       either CF is alrady a multiple of the required resolution or the step
       size is below this resolution. So we can set the correct start field
       with a combination of a slightly shifted CF plus not too large number
       of SWA steps (typically not more than 100). */

    er032m_best_fit_search( &magnet.cf, &magnet.swa, magnet.cf >=
                            0.5 * ( ER032M_MAX_FIELD - ER032M_MIN_FIELD ), 2 );

    fsc2_assert(    magnet.swa >= MIN_SWA && magnet.swa <= MAX_SWA
                 && magnet.cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD
                 && magnet.cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD );

    /* Set the new field but avoid that center field plus/minus half the
       sweep range to ever exceed the field limits. */

    cur_cf  = er032m_get_cf( );
    cur_sw  = er032m_get_sw( );

    if (    magnet.cf + 0.5 * cur_sw <= ER032M_MAX_FIELD
         && magnet.cf - 0.5 * cur_sw >= ER032M_MIN_FIELD )
    {
        er032m_set_cf( magnet.cf );
        er032m_set_sw( magnet.sw );
        er032m_set_swa( magnet.swa );
    }
    else if (    cur_cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD
              && cur_cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD )
    {
        er032m_set_sw( magnet.sw );
        er032m_set_cf( magnet.cf );
        er032m_set_swa( magnet.swa );
    }
    else
    {
        er032m_set_sw( 0.5 * d_min( ER032M_MAX_FIELD -
                                    d_min( magnet.cf, cur_cf ),
                                    d_min( magnet.cf, cur_cf )
                                    - ER032M_MIN_FIELD ) );
        er032m_set_cf( magnet.cf );
        er032m_set_swa( magnet.swa );
        er032m_set_sw( magnet.sw );
    }

    er032m_test_leds( );

    magnet.is_sw = SET;
    magnet.act_field = magnet.start_field;
    DEVIATION( magnet.act_field );
}


/*---------------------------------------------------------------------*
 * This function is called when the user asks for setting a new field.
 * One important point here is to change the center field only if
 * absolutely necessary. We have to distinguish several cases:
 * 1. There is no sweep step size set and thus the sweep width is set
 *    to zero. In this case we try to guess a step size from the
 *    difference between the currrent field and the target field. This
 *    is used to set a sweep width, and if we're lucky (i.e. the user
 *    does not require random field changes in the future) we can use
 *    the SWAs also for later field changes.
 * 2. If there is a sweep width set but there was no magnet_setup()
 *    call we try to set the field using SWAs if one of them fits the
 *    the new field value. If not we have to shift the center field,
 *    but we first try to keep the change as small as possible.
 *    Because our guess about the typical field changes didn't work
 *    out we also have to adapt the sweep width.
 * 3. If magnet_setup() has been called changing the sweep width can't
 *    be done. If possible field change is done via changing the SWA,
 *    otherwise a combination of changing the SWA and shifting the
 *    center field is used.
 *---------------------------------------------------------------------*/

static double
er032m_set_field( double field )
{
    fsc2_assert( field >= ER032M_MIN_FIELD && field <= ER032M_MAX_FIELD );

    /* We don't try to set a new field when the change is extremely small,
       i.e. lower than the accuracy for setting a center field. */

    if ( fabs( magnet.act_field - field ) <= ER032M_RESOLUTION )
        return magnet.act_field;

    if ( ! magnet.is_sw )
        er032m_change_field_and_set_sw( field );
    else
    {
        if ( ! magnet.is_init )
            er032m_change_field_and_sw( field );
        else
            er032m_change_field_and_keep_sw( field );
    }

    er032m_test_leds( );
    DEVIATION( field );
    return magnet.act_field = field;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

static void
er032m_change_field_and_set_sw( double field )
{
    double rem;


    /* Try to determine a sweep width so that we can set the field without
       changing the center field, i.e. alone by setting a SWA. */

    magnet.is_sw = er032m_guess_sw( fabs( field - magnet.cf ) );

    /* If this fails we have to change the center field, otherwise we use the
       newly calculated sweep width. */

    if ( ! magnet.is_sw )
    {
        rem = ( lrnd( field / ER032M_MIN_FIELD_STEP )
                % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP ) )
              * ER032M_MIN_FIELD_STEP;
        if ( rem > ER032M_MIN_FIELD_STEP )
        {
            magnet.cf = er032m_set_cf( field - rem );
            er032m_set_swa( magnet.swa = CENTER_SWA + 1 );
            magnet.sw = er032m_set_sw( MAX_SWA * rem );
        }
        else
        {
            magnet.sw = er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( field );
        }
    }
    else
    {
        magnet.swa += irnd( ( field - magnet.cf ) / magnet.swa_step );

        er032m_best_fit_search( &magnet.cf, &magnet.swa, magnet.cf >=
                                0.5 * ( ER032M_MAX_FIELD - ER032M_MIN_FIELD ),
                                2 );

        if (    magnet.swa < MIN_SWA || magnet.swa > MAX_SWA
             || magnet.cf - 0.5 * magnet.sw < ER032M_MIN_FIELD
             || magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD
             || field - ( magnet.cf + ( magnet.swa - CENTER_SWA )
                          * magnet.swa_step ) > ER032M_RESOLUTION )
        {
            rem = ( lrnd( field / ER032M_MIN_FIELD_STEP )
                    % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP ) )
                  * ER032M_MIN_FIELD_STEP;
            er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( field - rem );
            er032m_set_swa( magnet.swa = CENTER_SWA + 1 );
            magnet.sw = er032m_set_sw( MAX_SWA * rem );
        }
        else
        {
            er032m_set_sw( 0.0 );
            magnet.cf = er032m_set_cf( magnet.cf );
            er032m_set_swa( magnet.swa );
            magnet.sw = er032m_set_sw( magnet.sw );
        }
    }

}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

static void
er032m_change_field_and_sw( double field )
{
    int steps;


    /* If the new field value can be reached (within an accuracy of 1% of the
       SWA step size) by setting a SWA do just this. */

    steps = irnd( ( field - magnet.act_field ) / magnet.swa_step );

    if (    fabs( fabs( field - magnet.act_field )
                  - magnet.swa_step * abs( steps ) ) < 0.01 * magnet.swa_step
         && magnet.swa + steps <= MAX_SWA
         && magnet.swa + steps >= MIN_SWA )
    {
        er032m_set_swa( magnet.swa += steps );
        return;
    }

    er032m_change_field_and_set_sw( field );
}


/*------------------------------------------------------------------*
 * This function gets called for the set_field() EDL function when
 * the magnet_setup() EDL function had also been called during the
 * PREPARATION stage. In this case we can't change the sweep width
 * and have to do our best trying to achieve the requested field
 * with just changing the center field and the SWA count. Sometimes
 * we will only be able to set the field with a certain deviation
 * from the requested field.
 *------------------------------------------------------------------*/

static void
er032m_change_field_and_keep_sw( double field )
{
    int steps;
    long rem;


    /* First check if the new field value can be reached (within an accuracy
       of 1% of the SWA step size) by setting a SWA. If yes do just this. */

    steps = irnd( ( field - magnet.act_field ) / magnet.swa_step );

    if (    fabs( fabs( field - magnet.act_field )
                  - magnet.swa_step * abs( steps ) ) < 1e-2 * magnet.swa_step
         && magnet.swa + steps <= MAX_SWA
         && magnet.swa + steps >= MIN_SWA )
    {
        er032m_set_swa( magnet.swa += steps );
        return;
    }

    /* Otherwise we start of with the center field set to the requested
       field */

    magnet.swa = CENTER_SWA;
    magnet.cf  = field;

    /* Make sure we don't get over the field limits (including halve the
       sweep range) */

    if ( magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD )
    {
        steps = irnd( ceil( ( magnet.cf + 0.5 * magnet.sw - ER032M_MAX_FIELD )
                            / magnet.swa_step ) );

        magnet.swa += steps;
        magnet.cf  -= steps * magnet.swa_step;
    }
    else if ( magnet.cf - 0.5 * magnet.sw < ER032M_MIN_FIELD )
    {
        steps = irnd( ceil( ( ER032M_MIN_FIELD - magnet.cf + 0.5 * magnet.sw )
                            / magnet.swa_step ) );

        magnet.cf += steps * magnet.swa_step;
        magnet.swa -= steps;
    }

    /* When we're extremely near to the limits it's possible that there is
       no combination of CF and SWA that can be used. */

    if (    magnet.swa > MAX_SWA || magnet.swa < MIN_SWA
         || magnet.cf + 0.5 * magnet.sw > ER032M_MAX_FIELD
         || magnet.cf - 0.5 * magnet.sw < ER032M_MIN_FIELD )
    {
        print( FATAL, "Can't set field of %.3f G, to near to minimum or "
               "maximum field.\n", field );
        THROW( EXCEPTION );
    }

    /* Now we again got to deal with cases where the resulting center field
       isn't a multiple of the CF resoultion... */

    rem = lrnd( magnet.cf / ER032M_MIN_FIELD_STEP )
          % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP );

    if ( rem > 0 )
    {
        /* If the new CF value isn't a multiple of the CF resolution but the
           step size is we can't adjust the field to the required value and
           must use the nearest possible field instead. */

        if ( lrnd( magnet.swa_step / ER032M_MIN_FIELD_STEP ) %
             lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP ) == 0 )
            magnet.cf = lrnd( magnet.cf / ER032M_CF_RESOLUTION )
                        * ER032M_CF_RESOLUTION;
        else
        {
            /* Otherwise try to adjust the field by a few additional SWA
               steps in the right direction */

            er032m_best_fit_search( &magnet.cf, &magnet.swa, magnet.cf >=
                            0.5 * ( ER032M_MAX_FIELD - ER032M_MIN_FIELD ), 2 );

            fsc2_assert( magnet.swa >= MIN_SWA && magnet.swa <= MAX_SWA
                         && magnet.cf - 0.5 * magnet.sw >= ER032M_MIN_FIELD
                         && magnet.cf + 0.5 * magnet.sw <= ER032M_MAX_FIELD );
        }
    }

    magnet.cf = er032m_set_cf( magnet.cf );
    er032m_set_swa( magnet.swa );
}


/*-----------------------------------------------------------------------*
 * This function tries to guess a useful sweep range from the difference
 * between the current center field and the target field so that the
 * jump can be done by setting a SWA. If it isn't possible to find such
 * a setting the function returns FAIL, otherwise both the entries sw,
 * swa and swa_step in the magnet structure get set and OK is returned.
 *-----------------------------------------------------------------------*/

static bool
er032m_guess_sw( double field_diff )
{
    int i;
    double swa_step;
    double sw;
    double factor;


    /* For very small or huge changes we can't deduce a sweep range. */

    if (    field_diff * MAX_SWA < ER032M_SW_RESOLUTION
         || field_diff > magnet.max_sw / 2 )
        return FAIL;

    /* This also doesn't work if the center field is nearer to one of the
       limits than to the new field value. */

    if (    ER032M_MAX_FIELD - magnet.cf < field_diff
         || magnet.cf - ER032M_MIN_FIELD < field_diff )
        return FAIL;

    /* Now start with the step size set to the field difference */

    swa_step = field_diff;
    sw = MAX_SWA * field_diff;

    /* The following reduces the sweep width to the maximum possible sweep
       width and will always happen for field changes above 3.90625 G. */

    if ( sw > magnet.max_sw )
    {
        factor = irnd( ceil( sw / magnet.max_sw ) );

        sw /= factor;
        swa_step /= factor;
    }

    /* If with this corrected SWA step size the field change can't be achieved
       give up. */

    if ( swa_step * ( CENTER_SWA - 1 ) < field_diff )
        return FAIL;

    /* We also can't use a sweep range that exceeds the maximum or minimum
       field (keeping the center field) thus we must reduce it further as
       far as necessary */

    for ( i = 1; magnet.cf + 0.5 * sw / i > ER032M_MAX_FIELD; i++ )
        /* empty */ ;
    sw /= i;
    swa_step /= i;

    for ( i = 1; magnet.cf - 0.5 * sw / i < ER032M_MIN_FIELD; i++ )
        /* empty */ ;
    sw /= i;
    swa_step /= i;

    sw = ER032M_SW_RESOLUTION * lrnd( sw / ER032M_SW_RESOLUTION );
    swa_step = sw / MAX_SWA;

    if ( swa_step * ( CENTER_SWA - 1 ) < field_diff )
        return FAIL;

    magnet.sw = sw;
    magnet.swa_step = swa_step;
    magnet.swa = CENTER_SWA;

    return OK;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

static void
er032m_field_check( double field )
{
    if ( field > ER032M_MAX_FIELD )
    {
        print( FATAL, "Field of %f G is too high, maximum field is %f G.\n",
               field, ER032M_MAX_FIELD );
        THROW( EXCEPTION );
    }

    if ( field < ER032M_MIN_FIELD )
    {
        print( FATAL, "Field of %f G is too low, minimum field is %f G.\n",
               field, ER032M_MIN_FIELD );
        THROW( EXCEPTION );
    }
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

static void
er032m_test_leds( void )
{
    char buf[ 20 ];
    long length;
    char *bp;
    int max_retries = ER032M_MAX_RETRIES;
    bool is_overload;
    bool is_remote;


    if ( FSC2_MODE != EXPERIMENT )
        return;

    while ( 1 )
    {
        stop_on_user_request( );           /* give the user a chance to stop */

        is_overload = is_remote = UNSET;

        length = sizeof buf;
        er032m_talk( "LE\r", buf, &length );

        bp = buf;
        while ( *bp && *bp != '\r' )
        {
            switch ( *bp )
            {
                case '1' :
                    is_overload = SET;
                    break;

                case '2' :
                    print( FATAL, "Probehead thermostat not in "
                           "equilibrilum.\n" );
                    THROW( EXCEPTION );
                    break;

                case '4' :
                    is_remote = SET;
                    break;

                default :
                    break;
            }

            bp++;
        }

        /* If remote LED isn't on we're out of luck... */

        if ( ! is_remote )
        {
            print( FATAL, "Device isn't in remote state.\n" );
            THROW( EXCEPTION );
        }

        /* If there's no overload we're done, otherwise we retry several
           times before giving up. */

        if ( ! is_overload )
            break;

        if ( max_retries-- > 0  )
            fsc2_usleep( ER032M_WAIT_TIME, UNSET );
        else
        {
            print( FATAL, "Field regulation loop not balanced.\n" );
            THROW( EXCEPTION );
        }
    }
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static double
er032m_get_field( void )
{
    char buf[ 30 ];
    long length = sizeof buf;


    if ( FSC2_MODE != EXPERIMENT )
        return magnet.act_field;

    er032m_talk( "FC\r", buf, &length );
    buf[ length ] = '\0';
    return T_atod( buf + 2 );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static double
er032m_set_cf( double center_field )
{
    char buf[ 30 ];
    int i;


    center_field = ER032M_CF_RESOLUTION
                   * lrnd( center_field / ER032M_CF_RESOLUTION );

    fsc2_assert(    center_field >= ER032M_MIN_FIELD
                 && center_field <= ER032M_MAX_FIELD );

    if ( FSC2_MODE != EXPERIMENT )
        return center_field;

    sprintf( buf, "CF%.3f\r", center_field );

    for ( i = ER032M_MAX_SET_RETRIES; i > 0; i-- )
    {
        er032m_command( buf );
        if ( fabs( center_field - er032m_get_cf( ) ) < ER032M_CF_RESOLUTION )
            break;
    }

    if ( i == 0 )
    {
        print( FATAL, "Failed to set center field.\n" );
        THROW( EXCEPTION );
    }

    return center_field;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static double
er032m_get_cf( void )
{
    char buf[ 30 ];
    long len = sizeof buf;


    if ( FSC2_MODE != EXPERIMENT )
        return ER032M_TEST_FIELD;

    er032m_talk( "CF\r", buf, &len );
    buf[ len ] = '\0';
    return T_atod( buf + 2 );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static double
er032m_set_sw( double sweep_width )
{
    char buf[ 30 ];
    int i;


    fsc2_assert( sweep_width >= 0.0 && sweep_width <= magnet.max_sw );

    sweep_width = ER032M_SW_RESOLUTION
                  * lrnd( sweep_width / ER032M_SW_RESOLUTION );

    if ( lrnd( sweep_width / ER032M_SW_RESOLUTION ) != 0 )
    {
        magnet.swa_step = sweep_width / MAX_SWA;
        magnet.is_sw = SET;
    }
    else
    {
        magnet.swa_step = 0.0;
        magnet.is_sw = UNSET;
    }

    if ( FSC2_MODE != EXPERIMENT )
        return sweep_width;

    sprintf( buf, "SW%.3f\r", sweep_width );

    for ( i = ER032M_MAX_SET_RETRIES; i > 0; i-- )
    {
        er032m_command( buf );
        if ( fabs( sweep_width - er032m_get_sw( ) ) < ER032M_SW_RESOLUTION )
            break;
    }

    if ( i == 0 )
    {
        print( FATAL, "Failed to set sweep width.\n" );
        THROW( EXCEPTION );
    }

    return sweep_width;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static double
er032m_get_sw( void )
{
    char buf[ 30 ];
    long len = sizeof buf;


    if ( FSC2_MODE != EXPERIMENT )
        return CENTER_SWA;

    er032m_talk( "SW\r", buf, &len );
    buf[ len ] = '\0';
    return T_atod( buf + 2 );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static int er032m_set_swa( int sweep_address )
{
    char buf[ 30 ];


    fsc2_assert( sweep_address >= MIN_SWA && sweep_address <= MAX_SWA );

    if ( FSC2_MODE != EXPERIMENT )
        return sweep_address;

    sprintf( buf, "SS%d\r", sweep_address );
    er032m_command( buf );
    return sweep_address;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

#if 0
static int
er032m_get_swa( void )
{
    char buf[ 30 ];
    long len = sizeof buf;


    if ( FSC2_MODE != EXPERIMENT )
        return magnet.swa;

    er032m_talk( "SA\r", buf, &len );
    buf[ len ] = '\0';
    return T_atoi( buf + 2 );
}
#endif


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
er032m_command( const char * cmd )
{
    if ( gpib_write( magnet.device, cmd, strlen( cmd ) ) == FAILURE )
        er032m_failure( );
    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
er032m_talk( const char * cmd,
             char *       reply,
             long *       length )
{
    if (    gpib_write( magnet.device, cmd, strlen( cmd ) ) == FAILURE
         || gpib_read( magnet.device, reply, length ) == FAILURE )
        er032m_failure( );
    return OK;
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

static void
er032m_failure( void )
{
    print( FATAL, "Can't access the field controller.\n" );
    THROW( EXCEPTION );
}


/*-------------------------------------------------------*
 *-------------------------------------------------------*/

#define MAX_RECURSION 20          /* deepest level of recursion */
#define MAX_ADD_STEPS 100         /* maximum number of additional SWA steps */


static int
er032m_best_fit_search( double * cf,
                        int *    swa,
                        bool     dir,
                        int      fac )
{
    long rem;
    int add_steps = 0;
    double new_cf = *cf;
    int new_swa = *swa;
    static int recursion_count = 0;
    static bool dir_change = UNSET;


    fsc2_assert(    new_swa >= MIN_SWA && new_swa <= MAX_SWA
                 && new_cf - 0.5 * magnet.swa_step >= ER032M_MIN_FIELD
                 && new_cf + 0.5 * magnet.swa_step <= ER032M_MAX_FIELD );

    if ( dir == SEARCH_UP )
    {
        if ( new_swa == MAX_SWA )
            return MAX_ADD_STEPS;

        while (    new_swa < MAX_SWA
                && new_cf - 0.5 * magnet.swa_step > ER032M_MIN_FIELD )
        {
            rem = lrnd( fabs( new_cf ) / ER032M_MIN_FIELD_STEP )
                  % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP );
            if ( rem <= fac || ++add_steps >= MAX_ADD_STEPS )
                break;
            new_swa++;
            new_cf -= magnet.swa_step;
        }
    }
    else
    {
        if ( new_swa == MIN_SWA )
            return MAX_ADD_STEPS;

        while (    new_swa > MIN_SWA
                && new_cf + 0.5 * magnet.swa_step < ER032M_MAX_FIELD )
        {
            rem = lrnd( fabs( new_cf ) / ER032M_MIN_FIELD_STEP )
                  % lrnd( ER032M_CF_RESOLUTION / ER032M_MIN_FIELD_STEP );
            if ( rem <= fac || ++add_steps >= MAX_ADD_STEPS )
                break;
            new_swa--;
            new_cf += magnet.swa_step;
        }
    }

    if ( dir_change == UNSET )
    {
        if (    new_swa <= MIN_SWA || new_swa >= MAX_SWA
             || new_cf - 0.5 * magnet.sw < ER032M_MIN_FIELD
             || new_cf + 0.5 * magnet.sw > ER032M_MAX_FIELD
             || add_steps >= MAX_ADD_STEPS )
        {
            new_cf = *cf;
            new_swa = *swa;

            dir_change = SET;
            add_steps = er032m_best_fit_search( &new_cf, &new_swa,
                                                ! dir, fac );
            dir_change = UNSET;
        }

        if (    new_swa <= MIN_SWA || new_swa >= MAX_SWA
             || new_cf - 0.5 * magnet.sw < ER032M_MIN_FIELD
             || new_cf + 0.5 * magnet.sw > ER032M_MAX_FIELD
             || add_steps >= MAX_ADD_STEPS )
        {
            if ( recursion_count >= MAX_RECURSION )
                return MAX_ADD_STEPS;

            new_cf = *cf;
            new_swa = *swa;

            recursion_count++;
            er032m_best_fit_search( &new_cf, &new_swa, dir, fac + 1 );
            recursion_count--;
        }
    }

    *cf = lrnd( new_cf / ER032M_CF_RESOLUTION ) * ER032M_CF_RESOLUTION;
    *swa = new_swa;

    return add_steps;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
