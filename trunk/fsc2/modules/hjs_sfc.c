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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "hjs_sfc.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/* Local functions */

static double hjs_sfc_field_check( double field );

static double hjs_sfc_set_field( double field );


struct HJS_SFC hjs_sfc, hjs_sfc_stored;


/*-------------------------------------------*/
/*                                           */
/*            Hook functions                 */
/*                                           */
/*-------------------------------------------*/

/*---------------------------------------------------------------------*
 * Initialize the structure for the module, then lock the required DAC
 * and check that the function for setting the DAC can be found.
 *---------------------------------------------------------------------*/

int
hjs_sfc_init_hook( void )
{
    Var_T *func_ptr;
    int acc;
    Var_T *v;
    int dev_num;
    char *func;


    /* Set the default values for the structure for the device (the field for
       a DAC setting and the slope get read from the configuration file */

    hjs_sfc.calib_file = NULL;

    hjs_sfc.B0V_has_been_used = UNSET;
    hjs_sfc.slope_has_been_used = UNSET;

    hjs_sfc.is_field = UNSET;
    hjs_sfc.is_field_step = UNSET;

    hjs_sfc.dac_func = NULL;
    hjs_sfc.act_field = UNSET;

    /* Check that the module for the DA/AD converter is loaded.*/

    if ( ( dev_num = exists_device( "hjs_daadc" ) ) == 0 )
    {
        print( FATAL, "Can't find the module for the DA/AD converter "
               "(hjs_daadc) - it must be listed before this module.\n" );
        THROW( EXCEPTION );
    }

    /* Get a lock on the DAC */

    if ( dev_num )
        func = T_strdup( "daq_reserve_dac" );
    else
        func = get_string( "daq_reserve_dac#%d", dev_num );

    if (    ! func_exists( func )
         || ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        T_free( func );
        print( FATAL, "Function for reserving the DAC is missing.\n" );
        THROW( EXCEPTION );
    }

    T_free( func );
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
        hjs_sfc.dac_func = T_strdup( "daq_set_voltage" );
    else
        hjs_sfc.dac_func = get_string( "daq_set_voltage#%d", dev_num );

    if ( ! func_exists( hjs_sfc.dac_func ) )
    {
        hjs_sfc.dac_func = T_free( hjs_sfc.dac_func );
        print( FATAL, "Function for setting the DAC is missing.\n" );
        THROW( EXCEPTION );
    }

    /* Finally determine the minimum and maximum output voltage and
       the voltage resolution of the DAC */

    func = get_string( "daq_dac_parameter#%d", dev_num );

    if (    ! func_exists( func )
         || ( func_ptr = func_get( func, &acc ) ) == NULL )
    {
        print( FATAL, "Function for determining the DAC parameters is "
               "missing.\n" );
        THROW( EXCEPTION );
    }

    func = T_free( func );
    vars_push( STR_VAR, DEVICE_NAME );    /* push the new pass-phrase */
    
    v = func_call( func_ptr );

    if ( v->type != FLOAT_ARR || v->len != 3 )
    {
        print( FATAL, "Unexpected data received from DAC.\n" );
        THROW( EXCEPTION );
    }

    hjs_sfc.dac_range = v->val.dpnt[ 1 ] - v->val.dpnt[ 0 ];
    vars_pop( v );

    return 1;
}


/*----------------------------------------------------------*
 * Just store the current settings in case they get changed
 * during the test run.
 *----------------------------------------------------------*/

int
hjs_sfc_test_hook( void )
{
    /* Now is the last moment to read in the calibration file with the
       data of the field at the minimum and maximum DAC output voltage */

    hjs_sfc_read_calibration( );

    /* When manget_setup() had been called we couldn't check if the field
       was within the allowed range because we need the information from
       the calibration file to do so. So do the check now if necessary. */

    if ( hjs_sfc.is_field )
        hjs_sfc_field_check( hjs_sfc.field );

    hjs_sfc_stored = hjs_sfc;

    if ( hjs_sfc.is_field )
    {
        hjs_sfc.act_field = hjs_sfc_set_field( hjs_sfc.field );
        hjs_sfc.is_act_field = SET;
    }

    return 1;
}


/*-------------------------------------------------------------*
 * Restore the settings back to what they were after executing
 * the preparations section and then set the start field (if
 * one had been set.
 *-------------------------------------------------------------*/

int
hjs_sfc_exp_hook( void )
{
    hjs_sfc = hjs_sfc_stored;

    if ( hjs_sfc.is_field )
    {
        hjs_sfc.act_field = hjs_sfc_set_field( hjs_sfc.field );
        hjs_sfc.is_act_field = SET;
    }

    return 1;
}


/*--------------------------------------------------------------*
 * Function that is called just before the module gets unloaded
 *--------------------------------------------------------------*/

void
hjs_sfc_exit_hook( void )
{
    if ( hjs_sfc.calib_file != NULL )
        hjs_sfc.calib_file = T_free( hjs_sfc.calib_file );

    if ( hjs_sfc.dac_func )
        T_free( hjs_sfc.dac_func );
}


/*-------------------------------------------*/
/*                                           */
/*             EDL functions                 */
/*                                           */
/*-------------------------------------------*/

/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *
magnet_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------*
 * Function for registering the start field and the field step size.
 *-------------------------------------------------------------------*/

Var_T *
magnet_setup( Var_T * v )
{
    double start_field;
    double field_step;


    hjs_sfc.B0V_has_been_used = hjs_sfc.slope_has_been_used = SET;

    /* Check that both variables are reasonable */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    start_field = get_double( v, "magnetic field" );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing field step size.\n" );
        THROW( EXCEPTION );
    }

    field_step = get_double( v, "field step width" );

    too_many_arguments( v );

    hjs_sfc.field = start_field;
    hjs_sfc.field_step = field_step;
    hjs_sfc.is_field = hjs_sfc.is_field_step = SET;

    return vars_push( INT_VAR, 1L );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_field( Var_T * v )
{
    return v == NULL ? get_field( v ) : set_field( v );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
set_field( Var_T * v )
{
    double field;


    if ( v != NULL )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }

    field = get_double( v, "field" );

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        print( SEVERE, "This module does not allow setting a field precision, "
               "discarding second argument.\n" );
        too_many_arguments( v );
    }

    field = hjs_sfc_field_check( field );

    hjs_sfc.act_field = hjs_sfc_set_field( field );
    hjs_sfc.is_act_field = SET;

    return vars_push( FLOAT_VAR, hjs_sfc.act_field );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
get_field( Var_T * v  UNUSED_ARG )
{
    if ( ! hjs_sfc.is_act_field )
    {
        print( FATAL, "Field hasn't been set yet, can't determine current "
               "field.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, hjs_sfc.act_field );
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
    double field;


    if ( ! hjs_sfc.is_field_step )
    {
        print( FATAL, "Sweep step size has not been defined.\n" );
        THROW( EXCEPTION );
    }

    
    field = hjs_sfc_field_check( hjs_sfc.act_field + hjs_sfc.field_step );

    hjs_sfc.act_field = hjs_sfc_set_field( field );
    hjs_sfc.is_act_field = SET;

    return vars_push( FLOAT_VAR, field );
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
    double field;


    if ( ! hjs_sfc.is_field_step )
    {
        print( FATAL, "Sweep step size has not been defined.\n" );
        THROW( EXCEPTION );
    }

    
    field = hjs_sfc_field_check( hjs_sfc.act_field - hjs_sfc.field_step );

    hjs_sfc.act_field = hjs_sfc_set_field( field );
    hjs_sfc.is_act_field = SET;

    return vars_push( FLOAT_VAR, field );
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
    if ( ! hjs_sfc.is_field )
    {
        print( FATAL, "Start field has not been defined.\n" );
        THROW( EXCEPTION );
    }

    hjs_sfc.act_field = hjs_sfc_set_field( hjs_sfc.field );
    hjs_sfc.is_act_field = SET;
    
    return vars_push( FLOAT_VAR, hjs_sfc.act_field );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_B0( Var_T * v )
{
    double B0V;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, hjs_sfc.B0V );

    if ( FSC2_MODE == EXPERIMENT )
    {
        print( FATAL, "Field for DAC output of 0 V can only be set during the "
               "PREPARATIONS section.\n" );
        THROW( EXCEPTION );
    }

    if ( hjs_sfc.B0V_has_been_used )
    {
        print( FATAL, "Field for DAC output of 0 V has already been used, "
               "can't be changed anymore.\n" );
        THROW( EXCEPTION );
    }

    B0V = get_double( v, "field for DAC output of 0 V" );

    if ( B0V < 0.0 )
    {
        print( FATAL, "Invalid negative field for DAC output of 0 V.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    hjs_sfc.B0V = B0V;
            
    return vars_push( FLOAT_VAR, hjs_sfc.B0V );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_slope( Var_T * v )
{
    double slope;


    if ( v == NULL )
        return vars_push( FLOAT_VAR, hjs_sfc.slope );

    if ( FSC2_MODE == EXPERIMENT )
    {
        print( FATAL, "Field change for 1 V DAC voltage increment can only "
               "be set during the PREPARATIONS section.\n" );
        THROW( EXCEPTION );
    }

    if ( hjs_sfc.slope_has_been_used )
    {
        print( FATAL, "Field change for 1 V DAC voltage increment has "
               "already been used, can't be changed anymore.\n" );
        THROW( EXCEPTION );
    }

    slope = get_double( v, "field change for 1 V DAC voltage incement" );

    if ( slope == 0.0 )
    {
        print( FATAL, "Invalid zero field change for 1 V DAC voltage "
               "increment.\n" );
        THROW( EXCEPTION );
    }

    too_many_arguments( v );

    hjs_sfc.slope = slope;
            
    return vars_push( FLOAT_VAR, hjs_sfc.slope );
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

Var_T *
magnet_calibration_file( Var_T * v )
{
    char *buf;


    if ( hjs_sfc.calib_file != NULL )
    {
        print( SEVERE, "A Calibration file has already been read, keeping as "
               "'%s'.\n", hjs_sfc.calib_file );
        return vars_push( STR_VAR, hjs_sfc.calib_file );
    }

    if ( v == NULL )
        return vars_push( STR_VAR, hjs_sfc.calib_file != NULL ?
                          hjs_sfc.calib_file : DEFAULT_CALIB_FILE );

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Argument isn't a file name.\n" );
        THROW( EXCEPTION );
    }

    if ( *v->val.sptr == '/' )
        hjs_sfc.calib_file = T_strdup( v->val.sptr );
    else if ( *v->val.sptr == '~' )
        hjs_sfc.calib_file = get_string( "%s%s%s", getenv( "HOME" ),
                                         v->val.sptr[ 1 ] != '/' ? "/" : "",
                                         v->val.sptr + 1 );
    else
    {
        buf = T_malloc( PATH_MAX );

        if ( getcwd( buf, PATH_MAX ) == NULL )
        {
            print( FATAL, "Can't determine current working directory.\n" );
            T_free( buf );
            THROW( EXCEPTION );
        }

        TRY
        {
            hjs_sfc.calib_file = get_string( "%s%s%s", buf, slash( buf ),
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

    return vars_push( STR_VAR, hjs_sfc.calib_file );
}


/*-------------------------------------------*/
/*                                           */
/*           Low level functions             */
/*                                           */
/*-------------------------------------------*/

/*-----------------------------------------------------*
 * Tests if a field value is with the admissible range
 *-----------------------------------------------------*/

static double
hjs_sfc_field_check( double field )
{
    /* When checking the field we must take into consideration that for some
       magnets the field the minimum DAC output voltage is the highest
       possible field while for others it's lowest field. */

    if ( hjs_sfc.slope < 0.0 )
    {
        if ( field > hjs_sfc.B0V )
        {
            if ( FSC2_MODE == EXPERIMENT )
            {
                print( FATAL, "Field of %.2lf G is too high, reducing to "
                   "maximum field of %.2lf G.\n", field, hjs_sfc.B0V );
                return hjs_sfc.B0V;
            }

            print( FATAL, "Field of %.2lf G is too high, maximum is "
                   "%.2lf G.\n", field, hjs_sfc.B0V );
            THROW( EXCEPTION );
        }

        if ( field < hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope )
        {
            if ( FSC2_MODE == EXPERIMENT )
            {
                print( FATAL, "Field of %.2lf G is too low, increasing to "
                       "minimum of %.2lf G.\n", field,
                       hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope );
                return hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope;
            }

            print( FATAL, "Field of %.2lf G is too low, minimum is %.2lf G.\n",
                   field, hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( field < hjs_sfc.B0V )
        {
            if ( FSC2_MODE == EXPERIMENT )
            {
                print( FATAL, "Field of %.2lf G is too low, increasing to "
                       "minimum of %.2lf G.\n", field, hjs_sfc.B0V );
                return hjs_sfc.B0V;
            }

            print( FATAL, "Field of %.2lf G is too low, minimum is %.2lf G.\n",
                   field, hjs_sfc.B0V );
            THROW( EXCEPTION );
        }

        if ( field > hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope )
        {
            if ( FSC2_MODE == EXPERIMENT )
            {
                print( FATAL, "Field of %.2lf G is too high, reducing to "
                       "maximum of %.2lf G.\n", field,
                       hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope );
                return hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope;
            }

            print( FATAL, "Field of %.2lf G is too high, maximum is "
                   "%.2lf G.\n", field,
                   hjs_sfc.B0V + hjs_sfc.dac_range * hjs_sfc.slope );
            THROW( EXCEPTION );
        }
    }

    return field;
}


/*---------------------------------------------------------*
 * Set a new field by outputting a new voltage at the DAC.
 *---------------------------------------------------------*/

static double
hjs_sfc_set_field( double field )
{
    double v_step;
    Var_T *func_ptr;
    int acc;


    v_step = ( field - hjs_sfc.B0V ) / hjs_sfc.slope;

    if ( ( func_ptr = func_get( hjs_sfc.dac_func, &acc ) ) == NULL )
    {
        print( FATAL, "Internal error detected at %s:%d.\n",
               __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

    vars_push( STR_VAR, DEVICE_NAME );         /* push the pass-phrase */
    vars_push( FLOAT_VAR, v_step );
    vars_pop( func_call( func_ptr ) );

    return hjs_sfc.B0V + v_step * hjs_sfc.slope;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
