/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "hjs_fc.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Local functions */

static void hjs_fc_init_with_measured_data( void );

static void hjs_fc_init_with_calib_file( void );

static double hjs_fc_set_field( double field,
                                double error_margin );

static double hjs_fc_sweep_to( double new_field );

static double hjs_fc_get_field( void );

static double hjs_fc_field_check( double field );

static void hjs_fc_set_dac( double volts );


struct HJS_FC hjs_fc;


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*-------------------------------------------------------------------*
 * Function that gets called when the module is loaded. Since this
 * module does not control a real device but relies on the existence
 * of both the BNM12 gaussmeter and the home-built DA/AD converter
 * the main purpose of this function is to check that the modules
 * for these devices are already loaded and then to get a lock on
 * the DA-channel of the DA/AD converter. Of course, also some
 * internally used data are initialized by the function.
 *-------------------------------------------------------------------*/

int hjs_fc_init_hook( void )
{
    Var_T *func_ptr;
    int acc;
    Var_T *v;
    int dev_num;
    char *func = NULL;


    CLOBBER_PROTECT( func );

    /* Set the default values for the structure for the device */

    hjs_fc.B0V = HJS_FC_TEST_B0V;
    hjs_fc.slope = HJS_FC_TEST_SLOPE;

    hjs_fc.min_test_field = HUGE_VAL;
    hjs_fc.max_test_field = - HUGE_VAL;

    hjs_fc.is_field = UNSET;
    hjs_fc.is_field_step = UNSET;

    hjs_fc.use_calib_file = UNSET;
    hjs_fc.calib_file = NULL;

    hjs_fc.dac_func = NULL;
    hjs_fc.gm_gf_func = NULL;
    hjs_fc.gm_res_func = NULL;

    TRY
    {
        /* Check that the module for the BNM12 gaussmeter is loaded */

        if ( ( dev_num = exists_device( "bnm12" ) ) == 0 )
        {
            print( FATAL, "Can't find the module for the BNM12 gaussmeter "
                   "(bnm12) - it must be listed before this module.\n" );
            THROW( EXCEPTION );
        }

        if ( dev_num == 1 )
            hjs_fc.gm_gf_func = T_strdup( "gaussmeter_field" );
        else
            hjs_fc.gm_gf_func = get_string( "gaussmeter_field#%ld", dev_num );

        if ( ! func_exists( hjs_fc.gm_gf_func ) )
        {
            print( FATAL, "Function for getting the field from the gaussmeter "
                   "is missing.\n" );
            THROW( EXCEPTION );
        }

        /* Check that we can ask it for the field and its resolution. */

        if ( dev_num == 1 )
            hjs_fc.gm_res_func = T_strdup( "gaussmeter_resolution" );
        else
            hjs_fc.gm_res_func = get_string( "gaussmeter_resolution#%ld",
                                             dev_num );
        if ( ! func_exists( hjs_fc.gm_res_func ) )
        {
            print( FATAL, "Function for determining the field resolution of "
                   "the gaussmeter is missing.\n" );
            THROW( EXCEPTION );
        }

        /* Check that the module for the DA/AD converter is loaded.*/

        if ( ( dev_num = exists_device( "hjs_daadc" ) ) == 0 )
        {
            print( FATAL, "Can't find the module for the DA/AD converter "
                   "(hjs_daadc) - it must be listed before this module.\n" );
            THROW( EXCEPTION );
        }

        /* Get a lock on the DAC */

        func = get_string( "daq_reserve_dac#%d", dev_num );

        if ( ! func_exists( func ) ||
             ( func_ptr = func_get( func, &acc ) ) == NULL )
        {
            print( FATAL, "Function for reserving the DAC is missing.\n" );
            THROW( EXCEPTION );
        }

        func = CHAR_P T_free( func );
        vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

        v = func_call( func_ptr );

        if ( v->val.lval != 1 )
        {
            print( FATAL, "Can't reserve the DAC.\n" );
            THROW( EXCEPTION );
        }

        vars_pop( v );

        /* Get the name for the function for setting a value at the DAC and
           check if it exists */

        if ( dev_num == 1 )
            hjs_fc.dac_func = T_strdup( "daq_set_voltage" );
        else
            hjs_fc.dac_func = get_string( "daq_set_voltage#%ld", dev_num );

        if ( ! func_exists( hjs_fc.dac_func ) )
        {
            print( FATAL, "Function for setting the DAC is missing.\n" );
            THROW( EXCEPTION );
        }

        /* Finally determine the minimum and maximum output voltage and
           the voltage resolution of the DAC */

        func = get_string( "daq_dac_parameter#%d", dev_num );

        if ( ! func_exists( func ) ||
             ( func_ptr = func_get( func, &acc ) ) == NULL )
        {
            print( FATAL, "Function for determining the DAC parameters is "
                   "missing.\n" );
            THROW( EXCEPTION );
        }

        func = CHAR_P T_free( func );
        vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */

        v = func_call( func_ptr );

        if ( v->type != FLOAT_ARR || v->len != 3 )
        {
            print( FATAL, "Unexpected data received from DAC.\n" );
            THROW( EXCEPTION );
        }

        hjs_fc.dac_min_volts = v->val.dpnt[ 0 ];
        hjs_fc.dac_max_volts = v->val.dpnt[ 1 ];
        hjs_fc.dac_resolution = v->val.dpnt[ 2 ];

        vars_pop( v );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( func )
            T_free( func );
        if ( hjs_fc.gm_gf_func )
            hjs_fc.gm_gf_func = CHAR_P T_free( hjs_fc.gm_gf_func );
        if ( hjs_fc.gm_res_func )
            hjs_fc.gm_res_func = CHAR_P T_free( hjs_fc.gm_res_func );
        if ( hjs_fc.dac_func )
            hjs_fc.dac_func = CHAR_P T_free( hjs_fc.dac_func );
        RETHROW( );
    }

    return 1;
}


/*----------------------------------------------------------*
 *----------------------------------------------------------*/

int hjs_fc_test_hook( void )
{
    if ( hjs_fc.is_field )
        hjs_fc.target_field = hjs_fc.act_field = hjs_fc.field;
    else
        hjs_fc.target_field = hjs_fc.act_field = HJS_FC_TEST_FIELD;

    return 1;
}


/*---------------------------------------------------------------*
 * Function gets called when the experiment is started. First of
 * all we need to do some initialization. This mainly includes
 * setting the field to some positions, covering the whole range
 * of possible values, to figure out the maximum field range and
 * the relationship between the DAC output voltage and the
 * resulting field. When we then know the field range we have to
 * check that during the test run the field never was set to a
 * value that can't be reached. If the function magnet_setup()
 * had been called during the PREPARATIONS section we also have
 * to make sure that the sweep step size isn't smaller than the
 * minimum field step we can set with the DAC resolution.
 *---------------------------------------------------------------*/

int hjs_fc_exp_hook( void )
{
    /* Try to figure out what the field for the minimum and maximum DAC
       output voltage is and how (and how fast) the field changes with
       the DAC output voltage. This is either done by ea direct measurement
       or, if the user told us to load a calibration file, by reading the
       clibration file. */

    TRY
    {
        if ( ! hjs_fc.use_calib_file )
            hjs_fc_init_with_measured_data( );
        else
            hjs_fc_init_with_calib_file( );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        hjs_fc_child_exit_hook( );
        RETHROW( );
    }

    /* If we found in the test run that the requested field is going to
       be not within the limits of the field we can set give up */

    if ( hjs_fc.max_test_field > hjs_fc.min_test_field )
    {
        if ( hjs_fc.max_test_field > hjs_fc.B_max )
        {
            print( FATAL, "During test run a field of %.2lf G was requested "
                   "but the maximum field is %.2lf G.\n",
                   hjs_fc.max_test_field, hjs_fc.B_max );
            THROW( EXCEPTION );
        }

        if ( hjs_fc.min_test_field < hjs_fc.B_min )
        {
            print( FATAL, "During test run a field of %.2lf G was requested "
                   "but the minimum field is %.2lf G.\n",
                   hjs_fc.min_test_field, hjs_fc.B_min );
            THROW( EXCEPTION );
        }
    }

    /* Check that the field step size (if it has been given) isn't that
       small that it can't be produced with the voltage resolution of
       the DAC. */

    if ( hjs_fc.is_field_step &&
         fabs( hjs_fc.field_step / hjs_fc.slope ) < hjs_fc.dac_resolution )
    {
        print( FATAL, "Field step size for sweeps is too small, the minimum "
               "possible step size is %.2lf G.\n",
               hjs_fc.dac_resolution * hjs_fc.slope );
        THROW( EXCEPTION );
    }

    /* If a start field had been defined by a call of magnet_setup() set
       it now. */

    if ( hjs_fc.is_field )
    {
        hjs_fc.act_field = hjs_fc_set_field( hjs_fc.field, 0.0 );
        hjs_fc.target_field = hjs_fc.field;
    }

    return 1;
}


/*--------------------------------------------------------------*
 * Function that is called just before the module gets unloaded
 *--------------------------------------------------------------*/

void hjs_fc_exit_hook( void )
{
    if ( hjs_fc.calib_file != NULL )
        hjs_fc.calib_file = CHAR_P T_free( hjs_fc.calib_file );
    if ( hjs_fc.dac_func )
        T_free( hjs_fc.dac_func );
    if ( hjs_fc.gm_gf_func )
        T_free( hjs_fc.gm_gf_func );
    if ( hjs_fc.gm_res_func )
        T_free( hjs_fc.gm_res_func );
}


/*--------------------------------------------------------------*
 * Function gets executed just before the child process commits
 * suicide to bring the field back to the value it has for a
 * DAC output voltage of 0 V.
 *--------------------------------------------------------------*/

void hjs_fc_child_exit_hook( void )
{
    double cur_volts;
    double mini_step;


    if ( ( cur_volts = hjs_fc.cur_volts ) == 0.0 )
        return;

    mini_step = HJS_FC_FIELD_SET_MULTIPLIER * hjs_fc.dac_resolution;

    /* If the constants for the field at a DAC voltage of 0 V and for
       the change of the field with the DAC voltage as measured during
       intitalization are correct we should arrive at the target field
       with just one try. But if this isn't the case we retry by
       adjusting the output voltage according to the measured difference
       from the target field (at least as the deviation is larger than
       'error_margin'). */

    while ( cur_volts > 0.0 )
    {
        if ( ( cur_volts -= mini_step ) < 0.0 )
            cur_volts = 0.0;

        hjs_fc_set_dac( cur_volts );
        hjs_fc.cur_volts = 0.0;

        if ( HJS_FC_FIELD_SET_TIMEOUT > 0 )
            fsc2_usleep( HJS_FC_FIELD_SET_TIMEOUT, UNSET );
    }
}


/*-------------------------------------------*/
/*                                           */
/*             EDL functions                 */
/*                                           */
/*-------------------------------------------*/

/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *magnet_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------*
 * Function for registering the start field and the field step size.
 *-------------------------------------------------------------------*/

Var_T *magnet_setup( Var_T * v )
{
    hjs_fc.field = get_double( v, "magnetic field" );
    hjs_fc.field_step = get_double( v->next, "field step width" );

    hjs_fc.min_test_field = hjs_fc.max_test_field = hjs_fc.field;

    hjs_fc.is_field = hjs_fc.is_field_step = SET;
    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *magnet_calibration_file( Var_T * v )
{
    char *buf;


    if ( hjs_fc.calib_file != NULL )
    {
        print( SEVERE, "A Calibration file has already been read, keeping as "
               "'%s'.\n", hjs_fc.calib_file );
        return vars_push( STR_VAR, hjs_fc.calib_file );
    }

    if ( v == NULL )
    {
        hjs_fc.use_calib_file = SET;
        return vars_push( STR_VAR, DEFAULT_CALIB_FILE );
    }

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Argument isn't a file name.\n" );
        THROW( EXCEPTION );
    }

    if ( *v->val.sptr == '/' )
        hjs_fc.calib_file = T_strdup( v->val.sptr );
    else if ( *v->val.sptr == '~' )
        hjs_fc.calib_file = get_string( "%s%s%s", getenv( "HOME" ),
                                        v->val.sptr[ 1 ] != '/' ? "/" : "",
                                        v->val.sptr + 1 );
    else
    {
        buf = CHAR_P T_malloc( PATH_MAX );

        if ( getcwd( buf, PATH_MAX ) == NULL )
        {
            print( FATAL, "Can't determine current working directory.\n" );
            T_free( buf );
            THROW( EXCEPTION );
        }

        TRY
        {
            hjs_fc.calib_file = get_string( "%s%s%s", buf, slash( buf ),
                                            v->val.sptr );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( buf );
            RETHROW( );
        }

        T_free( buf );
    }

    too_many_arguments( v );

    hjs_fc.use_calib_file = SET;

    return vars_push( STR_VAR, hjs_fc.calib_file );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *magnet_field( Var_T * v )
{
    return v == NULL ? get_field( v ) : set_field( v );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *set_field( Var_T * v )
{
    double field;
    double error_margin = 0.0;


    if ( v == NULL )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    field = get_double( v, "field" );

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        error_margin = get_double( v, "field precision" );
        too_many_arguments( v );

        if ( error_margin < 0.0 )
        {
            print( FATAL, "Invalid negative field precision.\n ");
            THROW( EXCEPTION );
        }

        if ( error_margin > 0.1 * fabs( field ) )
            print( SEVERE, "Field precision is larger than 10% of field "
                   "value.\n" );
    }

    hjs_fc.target_field = hjs_fc_field_check( field );
    hjs_fc.act_field = hjs_fc_set_field( hjs_fc.target_field, error_margin );

    return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*---------------------------------------------------------*
 * Function asks the used gaussmeter for the current field
 *---------------------------------------------------------*/

Var_T *get_field( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != TEST )
        hjs_fc.act_field = hjs_fc_get_field( );

    return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

Var_T *magnet_sweep_up( Var_T * v )
{
    return sweep_up( v );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

Var_T *sweep_up( Var_T * v  UNUSED_ARG )
{
    if ( ! hjs_fc.is_field_step )
    {
        print( FATAL, "Sweep step size has not been defined.\n" );
        THROW( EXCEPTION );
    }

    hjs_fc.target_field =
                 hjs_fc_field_check( hjs_fc.target_field + hjs_fc.field_step );
    hjs_fc.act_field = hjs_fc_sweep_to( hjs_fc.target_field );

    return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

Var_T *magnet_sweep_down( Var_T * v )
{
    return sweep_down( v );
}

/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

Var_T *sweep_down( Var_T * v  UNUSED_ARG )
{
    if ( ! hjs_fc.is_field_step )
    {
        print( FATAL, "Sweep step size has not been defined.\n" );
        THROW( EXCEPTION );
    }

    hjs_fc.target_field =
                 hjs_fc_field_check( hjs_fc.target_field - hjs_fc.field_step );
    hjs_fc.act_field = hjs_fc_sweep_to( hjs_fc.target_field );

    return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

Var_T *magnet_reset_field( Var_T * v )
{
    return reset_field( v );
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

Var_T *reset_field( Var_T * v  UNUSED_ARG )
{
    if ( ! hjs_fc.is_field )
    {
        print( FATAL, "Start field has not been defined.\n" );
        THROW( EXCEPTION );
    }

    hjs_fc.act_field = hjs_fc_set_field( hjs_fc.field, 0.0 );
    hjs_fc.target_field = hjs_fc.field; 

    return vars_push( FLOAT_VAR, hjs_fc.act_field );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *magnet_B0( Var_T * v )
{
    if ( v != NULL )
    {
        print( FATAL, "Field for DAC output of 0 V can't be set for this "
               "driver, only query is possible.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == PREPARATION )
    {
        print( FATAL, "Field for DAC output of 0 V can only be queried during "
               "the EXPERIMENT section.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, hjs_fc.B0V );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *magnet_slope( Var_T * v )
{
    if ( v != NULL )
    {
        print( FATAL, "Field change for 1 V DAC voltage increment can't be "
               "set for this driver, only query is possible.\n" );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == PREPARATION )
    {
        print( FATAL, "Field change for 1 V DAC voltage increment can only be "
               "queried during the EXPERIMENT section.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, hjs_fc.slope );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*--------------------------------------------------------------------*
 * In the initialization process with use of measured data we need to
 * figure out the field for an output voltage of the DAC of 0 V and
 * how the field changes with the DAC voltage.
 *--------------------------------------------------------------------*/

static void hjs_fc_init_with_measured_data( void )
{
    size_t i;
    double test_volts[ ] = { hjs_fc.dac_min_volts, 1.0, 2.0, 7.5,
                             hjs_fc.dac_max_volts };
    size_t num_test_data = NUM_ELEMS( test_volts );
    double test_gauss[ num_test_data + 1 ];
    double cur_volts;
    double mini_step;


    /* We can be sure that the output of the DAC is set to 0 V */

    hjs_fc.B0V = test_gauss[ 0 ] = hjs_fc_get_field( );

    cur_volts = hjs_fc.dac_min_volts;
    mini_step = HJS_FC_FIELD_SET_MULTIPLIER * hjs_fc.dac_resolution;

    /* For the test voltages set in 'test_volts' get the magnetic field */

    for ( i = 1; i < num_test_data; i++ )
    {
        if ( test_volts[ i ] < hjs_fc.dac_min_volts ||
             test_volts[ i ] > hjs_fc.dac_max_volts ||
             ( i > 0 && fabs( test_volts[ i - 1 ] - test_volts[ i ] )
                        < hjs_fc.dac_resolution ) )
        {
            print( FATAL, "Internal error detected at %s:%d.\n",
                   __FILE__, __LINE__ );
            THROW( EXCEPTION );
        }

        while ( cur_volts < test_volts[ i ] )
        {
            cur_volts += mini_step;
            if ( cur_volts > test_volts[ i ] )
                cur_volts = test_volts[ i ];

            hjs_fc_set_dac( cur_volts );

            hjs_fc.cur_volts = cur_volts;

            if ( HJS_FC_FIELD_SET_TIMEOUT > 0 )
            {
                fsc2_usleep( HJS_FC_FIELD_SET_TIMEOUT, SET );
                stop_on_user_request( );
            }
        }

        /* Get a consistent reading */

        fsc2_usleep( 1000000L, SET );
        stop_on_user_request( );

        test_gauss[ i ] = hjs_fc_get_field( );
    }

    hjs_fc.slope = ( test_gauss[ num_test_data - 1 ] - hjs_fc.B0V ) /
                   ( test_volts[ num_test_data - 1 ] - test_volts[ 0 ] );

    if ( hjs_fc.slope == 0.0 )         /* this should be impossible... */
    {
        print( FATAL, "Tests of field sweep indicate that the field can't be "
               "swept at all.\n" );
        print( FATAL, "Tests of field sweep failed, field did not change.\n" );
        THROW( EXCEPTION );
    }

    /* Check that we at least have a field resolution of 0.1 G with the
       value we got from our tests */

    if ( hjs_fc.dac_resolution / hjs_fc.slope > 0.1 )
    {
        print( FATAL, "Minimum field resolution is higher than 0.1 G.\n" );
        THROW( EXCEPTION );
    }

    if ( hjs_fc.slope >= 0.0 )
    {
        hjs_fc.B_min = hjs_fc.B0V;
        hjs_fc.B_max = test_gauss[ num_test_data - 1 ];
    }
    else
    {
        hjs_fc.B_max = hjs_fc.B0V;
        hjs_fc.B_min = test_gauss[ num_test_data - 1 ];
    }

    hjs_fc.act_field = test_gauss[ num_test_data - 1 ];
}


/*-------------------------------------------------------------------*
 * In the initialization process using a calibration file we have to
 * read in the calibration file and measure the actual field.
 *-------------------------------------------------------------------*/

static void hjs_fc_init_with_calib_file( void )
{
    /* Try to read the calibration file */

    hjs_fc_read_calibration( );

    hjs_fc.cur_volts = hjs_fc.dac_min_volts;

    /* Check that we at least have a field resolution of 0.1 G with the
       value we got from our tests */

    if ( hjs_fc.dac_resolution / hjs_fc.slope > 0.1 )
    {
        print( FATAL, "Minimum field resolution is higher than 0.1 G.\n" );
        THROW( EXCEPTION );
    }

    if ( hjs_fc.slope >= 0.0 )
    {
        hjs_fc.B_min = hjs_fc.B0V;
        hjs_fc.B_max = hjs_fc.B0V + hjs_fc.slope * hjs_fc.dac_max_volts;
    }
    else
    {
        hjs_fc.B_max = hjs_fc.B0V;
        hjs_fc.B_min = hjs_fc.B0V + hjs_fc.slope * hjs_fc.dac_max_volts;
    }

    /* Finally measure the current field and check that it's not too far
       (5%) outside the limits specified in the calibration field */

    hjs_fc.act_field = hjs_fc_get_field( );

    if ( hjs_fc.act_field <
            hjs_fc.B_min - 0.05 * hjs_fc.slope * hjs_fc.dac_max_volts ||
         hjs_fc.act_field >
            hjs_fc.B_max + 0.05 * hjs_fc.slope * hjs_fc.dac_max_volts )
    {
        print( FATAL, "Measured field of %.2f G too far outside the range "
               "specified in calibration file '%s'.\n", hjs_fc.act_field,
               hjs_fc.calib_file );
        THROW( EXCEPTION );
    }
}


/*---------------------------------------------------------------*
 * Function tries to set a new field and checks if the field was
 * reached by measuring the new field via the BNM12 gaussmeter.
 *---------------------------------------------------------------*/

static double hjs_fc_set_field( double field,
                                double error_margin )
{
    double v_step = 0.0;
    double cur_field = hjs_fc.B0V;
    double cur_volts;
    double mini_step;


    if ( error_margin < 0.2 )
        error_margin = 0.2;

    mini_step = HJS_FC_FIELD_SET_MULTIPLIER * hjs_fc.dac_resolution;

    /* If the constants for the field at a DAC voltage of 0 V and for
       the change of the field with the DAC voltage as measured during
       intitalization are correct we should arrive at the target field
       with just one try. But if this isn't the case we retry by
       adjusting the output voltage according to the measured difference
       from the target field (at least as long as the deviation is larger
       than 'error_margin'). */

    do
    {
        v_step += ( field - cur_field ) / hjs_fc.slope;
        cur_volts = hjs_fc.cur_volts;

        if ( v_step > cur_volts )
            mini_step = fabs( mini_step );
        else
            mini_step = - fabs( mini_step );

        while ( ( mini_step > 0.0 && cur_volts < v_step ) ||
                ( mini_step < 0.0 && cur_volts > v_step ) )
        {
            cur_volts += mini_step;

            if ( ( mini_step > 0.0 && cur_volts > v_step ) ||
                 ( mini_step < 0.0 && cur_volts < v_step ) )
                cur_volts = v_step;

            if ( ( cur_volts < hjs_fc.dac_min_volts && hjs_fc.slope > 0.0 ) ||
                 ( cur_volts > hjs_fc.dac_max_volts && hjs_fc.slope < 0.0 ) )
            {
                print( FATAL, "Can't sweep field to %.2f G, it's too low.\n",
                       field );
                THROW( EXCEPTION );
            }
            else if ( ( cur_volts < hjs_fc.dac_min_volts &&
                        hjs_fc.slope < 0.0 ) ||
                      ( cur_volts > hjs_fc.dac_max_volts &&
                        hjs_fc.slope > 0.0 ) )
            {
                print( FATAL, "Can't sweep field to %.2f G, it's too high.\n",
                       field );
                THROW( EXCEPTION );
            }

            hjs_fc_set_dac( cur_volts );
            hjs_fc.cur_volts = cur_volts;

            if ( FSC2_MODE == TEST )
            {
                hjs_fc.cur_volts = v_step;
                break;
            }

            if ( HJS_FC_FIELD_SET_TIMEOUT > 0 )
            {
                fsc2_usleep( HJS_FC_FIELD_SET_TIMEOUT, UNSET );
                stop_on_user_request( );
            }
        }

        cur_field = hjs_fc_get_field( );

        if ( FSC2_MODE == TEST )
            cur_field = field;

    } while ( lrnd( 10.0 * fabs( cur_field - field ) ) >
              lrnd( 10.0 * error_margin ) );

    return cur_field;
}


/*-------------------------------------------------------------------*
 * Function for sweeping the field, i.e. just setting a new value of
 * the DAC output voltage but without checking the field using the
 * gaussmeter.
 *-------------------------------------------------------------------*/

static double hjs_fc_sweep_to( double new_field )
{
    double v_step;
    double cur_volts;
    double mini_step;


    if ( FSC2_MODE == TEST )
        return new_field;

    mini_step = HJS_FC_FIELD_SET_MULTIPLIER * hjs_fc.dac_resolution;

    /* Calculate the DAC voltage for the new field. Before we then set it we
       check that this wouldn't require setting the DAC to a value outside
       its range...*/

    cur_volts = hjs_fc.cur_volts;
    v_step = cur_volts + ( new_field - hjs_fc.act_field ) / hjs_fc.slope;

    if ( ( v_step < hjs_fc.dac_min_volts && hjs_fc.slope > 0.0 ) ||
         ( v_step > hjs_fc.dac_max_volts && hjs_fc.slope < 0.0 ) )
    {
        print( FATAL, "Can't sweep field to %.2f G, it's too low.\n",
               new_field );
        THROW( EXCEPTION );
    }
    else if ( ( v_step < hjs_fc.dac_min_volts && hjs_fc.slope < 0.0 ) ||
              ( v_step > hjs_fc.dac_max_volts && hjs_fc.slope > 0.0 ) )
    {
        print( FATAL, "Can't sweep field to %.2f G, it's too high.\n",
               new_field );
        THROW( EXCEPTION );
    }

    /* Now set the new field by stepping into its direction in small steps
       to keep the gussmeter from loosing its lock on the field */

    if ( v_step > cur_volts )
        mini_step = fabs( mini_step );
    else
        mini_step = - fabs( mini_step );

    while ( ( mini_step > 0.0 && cur_volts < v_step ) ||
            ( mini_step < 0.0 && cur_volts > v_step ) )
    {
        cur_volts += mini_step;

        if ( ( mini_step > 0.0 && cur_volts > v_step ) ||
             ( mini_step < 0.0 && cur_volts < v_step ) )
            cur_volts = v_step;

        hjs_fc_set_dac( cur_volts );
        hjs_fc.cur_volts = cur_volts;

        if ( HJS_FC_FIELD_SET_TIMEOUT > 0 )
        {
            fsc2_usleep( HJS_FC_FIELD_SET_TIMEOUT, UNSET );
            stop_on_user_request( );
        }
    }

    return new_field;
}


/*-------------------------------------------------------------------*
 * Function tries to get a consistent reading from the gaussmeter by
 * repeatedly fetching new values until it stays unchanged for at
 * least MIN_NUM_IDENTICAL_READINGS times. If the readings stay
 * unstable for more than MAX_GET_FIELD_RETRIES times the function
 * throws an exception. Between fetching new values from the gauss-
 * meter the function pauses for WAIT_LENGTH microseconds. Each of
 * the three constants need to be defined at the top of this file.
 *-------------------------------------------------------------------*/

static double hjs_fc_get_field( void )
{
    Var_T *v;
    int acc;
    long cur_field;
    long old_field;
    int repeat_count;
    int ident_count;


    /* Repeatedly read the field until we get a consistent reading (i.e.
       the same field value for MIN_NUM_IDENTICAL_READINGS times) or we
       have to decide that we can't get one... */

    for ( repeat_count = 0, ident_count = 0, old_field = LONG_MIN;
          ident_count < MIN_NUM_IDENTICAL_READINGS &&
          repeat_count < MAX_GET_FIELD_RETRIES; repeat_count++ )
    {
        if ( repeat_count )
            fsc2_usleep( WAIT_LENGTH, UNSET );

        stop_on_user_request( );

        v = func_call( func_get( hjs_fc.gm_gf_func, &acc ) );
        cur_field = lrnd( 10.0 * v->val.dval );
        vars_pop( v );

        if ( cur_field != old_field )
        {
            old_field = cur_field;
            ident_count = 0;
        }
        else
            ident_count++;
    }

    if ( repeat_count >= MAX_GET_FIELD_RETRIES )
    {
        print( FATAL, "Impossible to get a consistent field reading.\n" );
        THROW( EXCEPTION );
    }

    return 0.1 * cur_field;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

static double hjs_fc_field_check( double field )
{
    if ( FSC2_MODE == TEST )
    {
        hjs_fc.max_test_field = d_max( hjs_fc.max_test_field, field );
        hjs_fc.min_test_field = d_min( hjs_fc.min_test_field, field );
        return field;
    }

    if ( field > hjs_fc.B_max )
    {
        print( SEVERE, "Field of %.2lf G is too high, using maximum field of "
               "%.2lf G.\n", field, hjs_fc.B_max );
        field = hjs_fc.B_max;
    }
    else if ( field < hjs_fc.B_min )
    {
        print( SEVERE, "Field of %.2lf G is too low, using minimum field of "
               "%.2lf G.\n", field, hjs_fc.B_min );
        field = hjs_fc.B_min;
    }

    return field;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

static void hjs_fc_set_dac( double volts )
{
    Var_T *func_ptr;
    int acc;


    if ( ( func_ptr = func_get( hjs_fc.dac_func, &acc ) ) == NULL )
    {
        print( FATAL, "Internal error detected at %s:%d.\n",
               __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

    vars_push( STR_VAR, DEVICE_NAME );       /* push the pass-phrase */
    vars_push( FLOAT_VAR, volts );
    vars_pop( func_call( func_ptr ) );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
