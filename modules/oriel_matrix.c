/*
 * Newport Oriel MMS Spectrometer Driver
 *
 * Notes:
 *    File requires oriel_matrix.h
 *    Needs root privileges to access USB devices
 *.   This driver has been modified [hacked :-)] to work with fsc2.
 *
 * Last updated August 19, 2008
 *
 * This driver was developed using the ClearShotII - USB port interface
 * communications and control information specification provided by Centice
 * Corporation (http://www.centice.com)
 *
 * Copyright (c) 2008-2009, Clinton McCrowey <clinton.mccrowey.18@csun.edu>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Clinton McCrowey ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Clinton McCrowey BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "oriel_matrix.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

struct ORIEL_MATRIX oriel_matrix,
                    oriel_matrix_saved;


/*-------------------------------------------------------------*
 * Hook function that's run when the module gets loaded - just
 * set the variable that tells that the libusb must be initia-
 * lized and set up a few internal variables.
 *-------------------------------------------------------------*/

int oriel_matrix_init_hook( void )
{
    Need_USB = SET;

    oriel_matrix.udev                  = NULL;

    oriel_matrix.has_shutter_support   = SET;

    oriel_matrix.is_temp               = UNSET;
    oriel_matrix.has_temp_control      = SET;
    oriel_matrix.requires_temp_control = UNSET;

    oriel_matrix.min_test_temp         = HUGE_VAL;
    oriel_matrix.max_test_temp         = - HUGE_VAL;

    oriel_matrix.is_exp_time           = UNSET;

    oriel_matrix.min_test_exp_time     = HUGE_VAL;
    oriel_matrix.max_test_exp_time     = - HUGE_VAL;

    return 1;
}


/*-------------------------------------------------------------*
 * Hook function run at the start of a test. Save the internal
 * data that might contain information about things that need
 * to be set up at the start of the experiment
 *-------------------------------------------------------------*/

int oriel_matrix_test_hook( void )
{
    oriel_matrix_saved = oriel_matrix;
    return 1;
}


/*------------------------------------------------------------*
 * Hook function run at the start of the experiment. Needs to
 * initialize communication with the device and set it up.
 *------------------------------------------------------------*/

int
oriel_matrix_exp_hook( void )
{
    struct exposure *exposure;

    oriel_matrix = oriel_matrix_saved;

    /* Open connection to device and intialize it */

    oriel_matrix_init( );

    /* Check if in the PREPARATIONS section or during the test run a
       temperature control command was used. Bail out if the camera
       doesn't support temperature regulation. */

    if (    (    oriel_matrix.requires_temp_control
              || oriel_matrix_saved.requires_temp_control )
         && ! oriel_matrix.has_temp_control )
    {
        print( FATAL, "Temperature regulation not supported for this "
               "camera.\n" );
        THROW( EXCEPTION );
    }

    /* Check temperature setting done in the PREPARATIONS section and
       during the test run */

    if (    oriel_matrix.is_temp
         && (    lrnd( 100 * oriel_matrix.temp ) <
                                     lrnd( 100 * oriel_matrix.min_setpoint )
              || lrnd( 100 * oriel_matrix.temp ) >
                                     lrnd( 100 * oriel_matrix.min_setpoint ) ) )
    {
        print( FATAL, "Temperature of %.2f K requested in PREPARATIONS section "
               "is not within the possible range of %.2 - %.2f K.\n",
               oriel_matrix.temp, oriel_matrix.min_setpoint,
               oriel_matrix.max_setpoint );
        THROW( EXCEPTION );
    }

    if (    oriel_matrix.min_test_temp <= oriel_matrix.max_test_temp
         && (    lrnd( 100 * oriel_matrix_saved.min_test_temp ) <
                                     lrnd( 100 * oriel_matrix.min_setpoint )
              || lrnd( 100 * oriel_matrix_saved.max_test_temp ) >
                                     lrnd( 100 * oriel_matrix.min_setpoint ) ) )
    {
        print( FATAL, "Detected that during test run a temperature was "
               "requested that's not within the possible range of %.2 - "
               "%.2f K.\n", oriel_matrix.min_setpoint,
               oriel_matrix.max_setpoint );
        THROW( EXCEPTION );
    }

    /* Set a temperature if one was asked for in the PREPARATIONS section */

    if ( oriel_matrix.is_temp )
        oriel_matrix_set_CCD_temp( oriel_matrix.temp );

    /* Check if requested exposure times are within the limits */

    if (    oriel_matrix.is_exp_time
         && (    lrnd( 1000 * oriel_matrix.exp_time ) <
                                   lrnd( 1000 * oriel_matrix.min_exp_time )
              || lrnd( 1000 * oriel_matrix.exp_time ) >
                                   lrnd( 1000 * oriel_matrix.max_exp_time ) ) )
    {
        print( FATAL, "Exposure time of %.3g s requested in PREPARATIONS "
               "section is not within the possible limits of %.3g - %.3g s.\n",
               oriel_matrix.exp_time, oriel_matrix.min_exp_time,
               oriel_matrix.max_exp_time );
        THROW( EXCEPTION );
    }

    if ( oriel_matrix.min_test_exp_time <= oriel_matrix.max_test_exp_time
         && (    lrnd( 1000 * oriel_matrix.min_test_exp_time ) <
                                    lrnd( 1000 * oriel_matrix.min_exp_time )
              || lrnd( 1000 * oriel_matrix.max_test_exp_time ) <
                                    lrnd( 1000 * oriel_matrix.max_exp_time ) ) )
    {
        print( FATAL, "Detected that in test run an exposuretime was requested "
               "that's not within the possible limits of %.3g - %.3g s.\n",
               oriel_matrix.min_exp_time, oriel_matrix.max_exp_time );
        THROW( EXCEPTION );
    }

    /* Set either a default exposure time or the one the user asked for
       in the PREPARATIONS section */

    if ( oriel_matrix.is_exp_time )
        oriel_matrix_set_exposure_time( oriel_matrix.is_exp_time );
    else
    {
        fsc2_assert(    lrnd( 1000 * DEFAULT_EXPOSURE_TIME ) >=
                                    lrnd( 1000 * oriel_matrix.min_exp_time )
                     && lrnd( 1000 * DEFAULT_EXPOSURE_TIME ) <=
                                    lrnd( 1000 * oriel_matrix.max_exp_time ) );
        oriel_matrix_set_exposure_time( DEFAULT_EXPOSURE_TIME );
    }

    /* Take a dark exposure to determine the baseline */

    oriel_matrix_start_exposure( SHUTTER_OPEN, EXPOSURE_TYPE_DARK );

    /* Wait until the camera is ready - but give the user a chance to
       bail out if (s)he feels it takes too long */

    while ( oriel_matrix_query_exposure( ) != EXPOSURE_IS_AVAILABLE )
        stop_on_user_request( );

   /* Get the data and immediately throw them away (no need to check
       for success, on failure we won't get here since an exception
       will have been thrown) */

    exposure = oriel_matrix_get_exposure( );
    T_free( exposure->image );

    oriel_matrix_end_exposure( SHUTTER_CLOSED );

    return 1;
}


/*---------------------------------------------------------*
 * Hook function run at the end of the experiment. Closes
 * the connection to the device.
 *---------------------------------------------------------*/

int oriel_matrix_end_of_exp_hook()
{
    oriel_matrix_close( );
    return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *
ccd_camera_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------*
 * EDL function used to take a CCD image and return
 * the hardware decoded spectural reconstructed image
 *
 * Input: Var_T *v - Not used
 *
 * Return value: a fsc2 floating point array which contains
 * the reconstructed intensity array
 *----------------------------------------------------------*/

Var_T *
ccd_camera_get_spectrum( Var_T * v  UNUSED_ARG )
{
    double *array;
    Var_T *nv;
    struct exposure *exposure;
    struct reconstruction *recon;
    size_t len;
    size_t i;
    

    if ( FSC2_MODE == TEST )
    {
        array = T_malloc(   TEST_PIXEL_WIDTH * TEST_PIXEL_HEIGHT
                          * sizeof *array );
        for ( i = 0; i < TEST_PIXEL_WIDTH * TEST_PIXEL_HEIGHT; i++ )
            array[ i ] = 0.0;
        nv = vars_push( FLOAT_ARR, array,
                        TEST_PIXEL_WIDTH * TEST_PIXEL_HEIGHT );
        nv->flags |= IS_DYNAMIC;
        return nv;
    }

    oriel_matrix_start_exposure( SHUTTER_OPEN, EXPOSURE_TYPE_LIGHT );

    /* Wait until the camera is ready - but give the user a chance to bail
       out if it should be taking too long */

    while( oriel_matrix_query_exposure( ) != EXPOSURE_IS_AVAILABLE )
        stop_on_user_request( );

    /* Get the data and immediately throw them away (no need to check
       for success, on failure we won't get here since an exception
       will have been thrown) */

    exposure = oriel_matrix_get_exposure( );
    T_free( exposure->image );

    oriel_matrix_end_exposure( SHUTTER_CLOSED );

    recon = oriel_matrix_get_reconstruction( RECON_TYPE_LIGHT );

    len = recon->response_size  / sizeof( float );
    array = T_malloc( len * sizeof *array );

    for ( i = 0; i < len; i++ )
        array[ i ] = recon->intensity[ i ];

    T_free( recon->intensity );
  
    nv = vars_push( FLOAT_ARR, array, len );
    nv->flags |= IS_DYNAMIC;

    return nv;
}


/*-----------------------------------------------------------------------*
 * EDL function which returns the exposure time if v is NULL. 
 * If v is a float this function will set that as the CCD exposure time.
 *
 * Input:Var_T * v - a fsc2 variable which is either NULL or contains
 * a float
 *
 * Return value: Returns a fsc2 float which is the exposure time.
 *-----------------------------------------------------------------------*/

Var_T *
ccd_camera_exposure_time( Var_T * v )
{
    double et;


    if ( v == NULL )
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! oriel_matrix.is_exp_time )
                    no_query_possible( );
                else
                    return vars_push( FLOAT_VAR, oriel_matrix.exp_time );

            case TEST :
                if ( oriel_matrix.is_exp_time )
                    return vars_push( FLOAT_VAR, oriel_matrix.exp_time );
                else
                    return vars_push( FLOAT_VAR, TEST_EXPOSURE_TIME );

            case EXPERIMENT :
                return vars_push( FLOAT_VAR,
                                  oriel_matrix_get_exposure_time( ) );
        }

    et = get_double( v, "exposure time" );

    too_many_arguments( v );

    if ( et <= 0.0 )
    {
        print( FATAL, "Invalid zero or negative exposure time.\n" );
        THROW( EXCEPTION );
    }

    /* If this isn't a real experiment just store that setting a
       exposure time was attempted. */

    if ( FSC2_MODE != EXPERIMENT )
    {
        oriel_matrix.is_exp_time = SET;
        oriel_matrix.exp_time = et;
        if ( et < oriel_matrix.min_test_exp_time )
            oriel_matrix.min_test_exp_time = et;
        if ( et > oriel_matrix.max_test_exp_time )
            oriel_matrix.max_test_exp_time = et;
        return vars_push( FLOAT_VAR, et );
    }

    /* Check that the exposure time can be set */

    if ( lrnd( 1000 * et ) < lrnd( 1000 * oriel_matrix.min_exp_time ) )
    {
        print( FATAL, "Requested exposure time of %g s is shorter than "
               "lower limit of %g s.\n", et, oriel_matrix.min_exp_time );
        THROW( EXCEPTION );
    }

    if ( lrnd( 1000 * et ) > lrnd( 1000 * oriel_matrix.max_exp_time ) )
    {
        print( FATAL, "Requested exposure time of %g s is longer than "
               "upper limit of %g s.\n", et, oriel_matrix.max_exp_time );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, oriel_matrix_set_exposure_time( et ) );
}


/*--------------------------------------------------------------*
 * EDL function which returns the temperature if v is NULL. 
 * If v is a float this function will set that as the
 * temperature.
 *
 * Input:Var_T * v - a fsc2 variable which is either NULL or
 * contains a float
 *
 * Return value: Returns a fsc2 float which is the temperature.
 *--------------------------------------------------------------*/

Var_T *
ccd_camera_temperature( Var_T * v )
{
    double temp;
    struct temperature *st;


    /* Make sure the device supports temperature regulation */

    if ( ! oriel_matrix.has_temp_control )
    {
        print( FATAL, "Temperature regulation not supported for "
               "this camera.\n" );
        THROW( EXCEPTION );
    }

    oriel_matrix.requires_temp_control = SET;

    if ( v == 0 )
    {
        switch ( FSC2_MODE )
        {
            case PREPARATION :
                if ( ! oriel_matrix.is_temp )
                    no_query_possible( );
                else
                    return vars_push( FLOAT_VAR, oriel_matrix.temp );

            case TEST :
                if ( ! oriel_matrix.is_temp )
                    return vars_push( FLOAT_VAR, TEST_CAMERA_TEMPERATURE );
                else
                    return vars_push( FLOAT_VAR, oriel_matrix.temp );

            case EXPERIMENT :
                st =  oriel_matrix_get_CCD_temp( );
                return vars_push( FLOAT_VAR,
                                  ( double ) st->current_temp + 273.15 );
        }
    }

    temp = get_double( v, "camera temperature" );

    too_many_arguments( v );

    if ( temp <= 0.0 )
    {
        print( FATAL, "Invalid zer or negative temperature.\n" );
        THROW( EXCEPTION );
    }

    /* If this isn't a real experiment just store that setting a
       temperature was attempted. */

    if ( FSC2_MODE != EXPERIMENT )
    {
        oriel_matrix.is_temp = SET;
        oriel_matrix.temp = temp;
        if ( temp < oriel_matrix.min_test_temp )
            oriel_matrix.min_test_temp = temp;
        if ( temp > oriel_matrix.max_test_temp )
            oriel_matrix.max_test_temp = temp;
        return vars_push( FLOAT_VAR, temp );
    }

    /* Check that the temperature can be set */

    if ( lrnd( 100 * temp ) < lrnd( 100 * oriel_matrix.min_setpoint ) )
    {
        print( FATAL, "Requested temperature of %.2f K is lower than the "
               "lower limit of %.2f K.\n", temp, oriel_matrix.min_setpoint );
        THROW( EXCEPTION );
    }

    if ( lrnd( 100 * temp ) > lrnd( 100 * oriel_matrix.max_setpoint ) )
    {
        print( FATAL, "Requested temperature of %.2f K is higher than the "
               "upper limit of %.2f K.\n", temp, oriel_matrix.max_setpoint );
        THROW( EXCEPTION );
    }

    temp = lrnd( 100 * temp );

    return vars_push( FLOAT_VAR,
                      oriel_matrix_set_CCD_temp( temp - 273.15 ) + 273.15 );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *
ccd_camera_pixel_area( Var_T * v  UNUSED_ARG )
{
    Var_T *cv;


    if ( FSC2_MODE == PREPARATION )
        no_query_possible( );

    if ( FSC2_MODE == EXPERIMENT )
    {
        cv = vars_push( INT_ARR, NULL, 2 );
        cv->val.lpnt[ 0 ] = oriel_matrix.pixel_width;
        cv->val.lpnt[ 1 ] = oriel_matrix.pixel_height;
    }
    else
    {
        cv = vars_push( INT_ARR, NULL, 2 );
        cv->val.lpnt[ 0 ] = TEST_PIXEL_WIDTH;
        cv->val.lpnt[ 1 ] = TEST_PIXEL_HEIGHT;
    }

    return cv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
