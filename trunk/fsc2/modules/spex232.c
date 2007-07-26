/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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
 */


#include "spex232.h"


/*--------------------------------*
 * Global variables of the module
 *--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


struct Spex232 spex232, spex232_stored;



/*------------------------------------------------------------*
 * Function that gets called when the module has been loaded.
 *------------------------------------------------------------*/

int spex232_init_hook( void )
{
    spex232.fatal_error = UNSET;

    /* Claim the serial port (throws exception on failure) */

    fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

    spex232.is_needed = SET;
    spex232.is_open = UNSET;

    /* Determine (from the configuration file) if the monochromator is
       wavenumber or wavelength driven */

    spex232.method = DRIVE_METHOD;

    switch ( spex232.method )
    {
        case WAVELENGTH :
            spex232.mode = WL;
            break;

        case WAVENUMBER :
            spex232.mode = WN_ABS;
            break;

        default :
            print( FATAL, "Invalid setting for drive method in "
                   "configuration file for device.\n" );
            SPEX232_THROW( EXCEPTION );
    }

#if defined SPEX232_MAX_OFFSET
    if ( fabs( spex232.offset ) > SPEX232_MAX_OFFSET )
    {
        const char *dn = fsc2_config_dir( );
        char *fn = get_string( "%s%s%s", dn, slash( dn ),
                               SPEX232_STATE_FILE );
        print( FATAL, "Offset setting in state file '%s' is "
               "unrealistically high.\n", fn );
        T_free( fn );
        SPEX232_THROW( EXCEPTION );
    }
#endif

    /* If it's wavelength driven find out the units the controller expects
       (either nanometer or Angstrom) */

    if ( spex232.mode == WL )
    {
        spex232.units = UNITS;

        if ( spex232.units != NANOMETER && spex232.units != ANGSTROEM )
        {
            print( FATAL, "Invalid setting for units in configuration file "
                   "for the device.\n" );
            SPEX232_THROW( EXCEPTION );
        }
    }

    /* Determine the number of grooves per meter for the installed grating */

    if ( GROOVE_DENSITY <= 0 )
    {
        print( FATAL, "Invalid setting for number of grooves per mm of "
               "grating in configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    spex232.grooves = 1000 * GROOVE_DENSITY;

    if ( STANDARD_GROOVE_DENSITY <= 0 )
    {
        print( FATAL, "Invalid setting for number of grooves per mm of "
               "standard grating in configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Determine the number of grooves per meter for the standard grating */

    spex232.standard_grooves = 1000 * STANDARD_GROOVE_DENSITY;

    /* Find out which is the lowest wavelength that can be set */

    if ( LOWER_LIMIT < 0 ||
         ( spex232.mode & WN_MODES && LOWER_LIMIT <= 0 ) )
    {
        if ( spex232.mode & WN_MODES )
            print( FATAL, "Invalid setting for upper wavenumber limit in "
                   "configuration file for the device.\n" );
        else
            print( FATAL, "Invalid setting for lower wavelength limit in "
                   "configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }           

    if ( spex232.mode & WN_MODES )
    {
        TRY
        {
            spex232.lower_limit = spex232_wn2wl( LOWER_LIMIT );
            TRY_SUCCESS;
        }
        CATCH( INVALID_INPUT_EXCEPTION )
        {
            print( FATAL, "Invalid setting for upper wavenumber limit in "
                   "configuration file for the device.\n" );
            SPEX232_THROW( EXCEPTION );
       }
    }
    else if ( UNITS == NANOMETER )
        spex232.lower_limit = 1.0e-9 * LOWER_LIMIT;
    else
        spex232.lower_limit = 1.0e-10 * LOWER_LIMIT;

    /* Find out which is the highest wavelength that can be set */

    if ( UPPER_LIMIT < 0 ||
         ( spex232.mode & WN_MODES && UPPER_LIMIT <= 0.0 ) )
    {
        if ( spex232.mode & WN_MODES )
            print( FATAL, "Invalid setting for lower wavenumber limit in "
                   "configuration file for the device.\n" );
        else
            print( FATAL, "Invalid setting for upper wavelength limit in "
                   "configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    if ( spex232.mode & WN_MODES )
    {
        TRY
        {
            spex232.upper_limit = spex232_wn2wl( UPPER_LIMIT );
            TRY_SUCCESS;
        }
        CATCH( INVALID_INPUT_EXCEPTION )
        {
            print( FATAL, "Invalid setting for lower wavenumber limit in "
                   "configuration file for the device.\n" );
            SPEX232_THROW( EXCEPTION );
        }
    }
    else if ( UNITS == NANOMETER )
        spex232.upper_limit = 1.0e-9 * UPPER_LIMIT;
    else
        spex232.upper_limit = 1.0e-10 * UPPER_LIMIT;

    /* Check consistency of settings for lower and upper limit */

    if ( spex232.upper_limit <= spex232.lower_limit )
    {
        if ( spex232.mode & WN_MODES )
            print( FATAL, "Lower wavenumber limit isn't larger than the "
                   "upper limit in configuration file for the device.\n" );
        else
            print( FATAL, "Upper wavelength limit isn't larger than the "
                   "lower limit in configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Get the minimum step size of the motor. It gets calculated from
       the STEPS_PER_UNIT entry in the configuration file and the ratio
       of the grooves of the installed and the standard grating. */

    if ( spex232.mode == WL )
    {
        if ( UNITS == NANOMETER )
            spex232.mini_step = 1.0e-9 * spex232.standard_grooves
                                / ( spex232.grooves * STEPS_PER_UNIT );
        else
            spex232.mini_step = 1.0e-10 * spex232.standard_grooves
                                / ( spex232.grooves * STEPS_PER_UNIT );
    }
    else
        spex232.mini_step = spex232.grooves
                            / ( spex232.standard_grooves * STEPS_PER_UNIT );

    if ( spex232.mini_step <= 0.0 )
    {
        print( FATAL, "Invalid setting for steps per unit with standard "
               "grating in configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Also the lower and upper limit must be corrected if the grating isn't
       the standard grating. In this case it also seems not to be possible
       to go to the limits, we should stay at least two steps away from them */

    if ( GROOVE_DENSITY != STANDARD_GROOVE_DENSITY )
    {
        spex232.lower_limit *= spex232.standard_grooves / spex232.grooves;
        spex232.upper_limit *= spex232.standard_grooves / spex232.grooves;

        if ( spex232.mode == WL )
        {
            spex232.lower_limit += 2 * spex232.mini_step;
            spex232.upper_limit -= 2 * spex232.mini_step;
        }
        else
        {
            spex232.lower_limit =
                            spex232_wn2wl( spex232_wl2wn( spex232.lower_limit )
                                           - 2 * spex232.mini_step );
            spex232.upper_limit =
                            spex232_wn2wl( spex232_wl2wn( spex232.upper_limit )
                                           + 2 * spex232.mini_step );
        }
    }

    /* Get the minimum and maximum number of steps per second */

    if ( MIN_STEPS_PER_SECOND <= 0 )
    {
        print( FATAL, "Invalid setting for minimum number of steps per "
               "second configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    if ( MAX_STEPS_PER_SECOND <= 0 )
    {
        print( FATAL, "Invalid setting for maximum number of steps per "
               "second in configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    if ( MIN_STEPS_PER_SECOND > MAX_STEPS_PER_SECOND )
    {
        print( FATAL, "Invalid setting for minimum and/or maximum number of "
               "steps per second in configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    spex232.min_rate = MIN_STEPS_PER_SECOND;
    spex232.max_rate = MAX_STEPS_PER_SECOND;

    if ( RAMP_TIME < MIN_RAMP_TIME || RAMP_TIME > MAX_RAMP_TIME )
    {
        print( FATAL, "Invalid setting for ramp time in configuration "
               "file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    spex232.ramp_time = RAMP_TIME;

    if ( BACKSLASH_STEPS < 0 )
    {
        print( FATAL, "Invalid setting for required backslash in "
               "configuration file for the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    spex232.backslash_steps = BACKSLASH_STEPS;

    /* We still need to correct the lower limit - when driving into the
       direction of the lower limit we need to apply backslash, so we
       need to drive to a still lower wavelength (or highter wavenumber) */

    spex232.abs_lower_limit = spex232.lower_limit;

    if ( spex232.mode & WN_MODES )
        spex232.lower_limit =
            spex232_wn2wl( spex232_wl2wn( spex232.lower_limit )
                           - spex232.backslash_steps * spex232.mini_step );
    else
        spex232.lower_limit += spex232.backslash_steps * spex232.mini_step;

    if ( spex232.lower_limit >= spex232.upper_limit )
    {
        print( FATAL, "Invalid setting for combination of upper and lower "
               "limit and required backslash in configuration file for "
               "the device.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Read in the file where the state of the monochromator gets stored
       to find out about the offset between the values displayed at the
       CD2A and the "true" wavelengths or -numbers and, for wavenumber-
       driven monochromators the setting of the laser line position as
       used in the previous invocation of the program. (The file has also
       to be read each time at the start of an experiment since it's
       contents are rewritten at the end of each experiment.) */

    spex232_read_state( );

    if ( spex232.mode & WN_MODES && spex232.laser_line != 0.0 )
        spex232.mode = WN_REL;

    /* Calculate the maximum motor position */

    spex232.max_motor_position = spex232_wl2p( spex232.upper_limit );

    spex232.is_wavelength = UNSET;
    spex232.scan_is_init = UNSET;
    spex232.need_motor_init = UNSET;
    spex232.do_enforce_position = UNSET;

    return 1;
}


/*-------------------------------------------------------------------------*
 * Function called (in the parents context) at the start of the experiment
 *-------------------------------------------------------------------------*/

int spex232_test_hook( void )
{
    spex232_stored = spex232;

    /* Read in the file where the state of the monochromator gets stored,
       the setting for the current wavelengths etc. are stored there */

    spex232_read_state( );

#if defined AUTOCALIBRATION_POSITION
	if ( spex232.need_motor_init )
    {
		if ( spex232.mode = WL )
			spex232.wavelength = AUTOCALIBRATION_POSITION;
        else
            spex232.wavelength = spex232_wl2wn( AUTOCALIBRATION_POSITION );
    }
    else
        spex232.wavelength = spex232_p2wl( spex232.motor_position );
#else
    spex232.wavelength = spex232_p2wl( spex232.motor_position );
#endif
    spex232.is_wavelength = SET;

    return 1;
}


/*-----------------------------------------------------------*
 * Function gets called always at the start of an experiment
 *-----------------------------------------------------------*/

int spex232_exp_hook( void )
{
    spex232 = spex232_stored;

#if defined SPEX232_MAX_OFFSET
    if ( fabs( spex232.offset ) > SPEX232_MAX_OFFSET )
    {
        print( FATAL, "Offset setting in state file '%s' is "
               "unrealistically high.\n" );
        SPEX232_THROW( EXCEPTION );
    }
#endif

    /* Open the serial port for the device */

    spex232_open( );

    /* Test if the device reacts */

    if ( ! spex232_init( ) )
    {
        print( FATAL, "Can't access the monochromator\n" );
        SPEX232_THROW( EXCEPTION );
    }

    return 1;
}


/*--------------------------------------------------------------*
 * Function gets called just before the child process doing the
 * experiment exits.
 *--------------------------------------------------------------*/

void spex232_child_exit_hook( void )
{
    if ( ! spex232.fatal_error )
        spex232_store_state( );
}


/*-----------------------------------------------------------------------*
 * Function called (in the parents context) at the end of the experiment
 *-----------------------------------------------------------------------*/

int spex232_end_of_exp_hook( void )
{
    spex232_close( );
    return 1;
}


/*----------------------------------------------*
 * Returns a string with the name of the device
 *----------------------------------------------*/

Var_T *monochromator_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------------------------*
 * Function allows to enforce an autocalibration for monochromators that
 * have this capability. For other monochromators this functions should
 * not be available. This function
 *-----------------------------------------------------------------------*/

#if defined AUTOCALIBRATION_POSITION
Var_T *monochromtor_init_motor( Var_T * v  UNUSED_ARG )
{
    spex232.need_init_motor = SET;
    return vars_push( INT_VAR, 1L );
}
#endif


/*---------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

Var_T *monochromator_enforce_wavelength( Var_T * v )
{
    double wl;


    if ( spex232.do_enforce_position )
    {
        print( WARN, "A wavelength or -number to enforce already has been "
               "set, disregarding this call.\n" );
        return vars_push( INT_VAR, 0L );
    }

    wl = get_double( v, "wavelength to enforce" );

    if ( wl < spex232.abs_lower_limit || wl > spex232.upper_limit )
    {
        print( FATAL, "Wavelength not within limits of monochromator.\n" );
        THROW( EXCEPTION );
    }

    spex232.do_enforce_position = SET;
    spex232.enforced_position = spex232_wl2p( wl );
 
    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

Var_T *monochromator_enforce_wavenumber( Var_T * v )
{
    double wl, wn;


    if ( spex232.do_enforce_position )
    {
        print( WARN, "A wavelength or -number to enforce already has been "
               "set, disregarding this call.\n" );
        return vars_push( INT_VAR, 0L );
    }

    wn = get_double( v, "wavenumber to enforce" );

    if ( wn <= 0.0 )
    {
        print( FATAL, "Invalid wavenumber of %g cm^-1.\n" );
        THROW( EXCEPTION );
    }

    wl = spex232_wn2wl( wn );

    if ( wl < spex232.abs_lower_limit || wl > spex232.upper_limit )
    {
        print( FATAL, "Wavenumber not within limits of monochromator.\n" );
        THROW( EXCEPTION );
    }

    spex232.do_enforce_position = SET;
    spex232.enforced_position = spex232_wl2p( wl );
 
    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------------------*
 * Returns an array with the scan limits in absolute wavenumbers (in cm^-1)
 * with the lower scan limit as the first and the upper limit as the second
 * array element.
 *--------------------------------------------------------------------------*/

Var_T *monochromator_wavenumber_scan_limits( Var_T *v  UNUSED_ARG )
{
    double vals[ 2 ] = { spex232_wl2Uwn( spex232.upper_limit ),
                         spex232_wl2Uwn( spex232.lower_limit ) };
    int i;


    if ( spex232.mode == WN_REL )
        for ( i = 0; i < 2; i++ )
            vals[ i ] = spex232.laser_line -  vals[ i ];

    return vars_push( FLOAT_ARR, vals, 2 );
}


/*----------------------------------------------------------------------*
 * Returns an array with the scan limits in wavelengths /in m) with the 
 * lower scan limit as the first and the upper limit as the second array
 * element.
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavelength_scan_limits( Var_T *v  UNUSED_ARG )
{
    double vals[ 2 ] = { spex232.lower_limit, spex232.upper_limit };
    return vars_push( FLOAT_ARR, vals, 2 );
}


/*----------------------------------------------------------*
 * Set or query the current wavelength of the monochromator
 * (always return the absolute wavelength in meter, even
 * when in wavenumber mode). If the monochromator is doing
 * a scan and a new wavelength is set the scan is aborted.
 *----------------------------------------------------------*/

Var_T *monochromator_wavelength( Var_T * v )
{
    double wl;


    CLOBBER_PROTECT( wl );

    if ( v == NULL )
    {
        if ( ! spex232.is_wavelength && ! spex232.scan_is_init )
        {
            print( FATAL, "Wavelength hasn't been set yet.\n ");
            SPEX232_THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR,
                          spex232_wl2Uwl( spex232.wavelength ) );
    }

    wl = get_double( v, "wavelength" );

    TRY
    {
        wl = spex232_Uwl2wl( wl );
        TRY_SUCCESS;
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        print( FATAL, "Invalid wavelength of %.5f nm.\n",
               1.0e9 * spex232_wl2Uwl( wl ) );
        SPEX232_THROW( EXCEPTION );
    }
    OTHERWISE
        SPEX232_RETHROW( );

    if ( wl < spex232.lower_limit )
    {
        print( FATAL, "Requested wavelength of %.5f nm is lower than the "
               "lower wavelength limit of %.5f nm.\n",
               1.0e9 * spex232_wl2Uwl( wl ),
               1.0e9 * spex232_wl2Uwl( spex232.lower_limit ) );
        SPEX232_THROW( EXCEPTION );
    }

    if ( wl > spex232.upper_limit )
    {
        print( FATAL, "Requested wavelength of %.5f nm is larger than the "
               "upper wavelength limit of %.5f nm.\n",
               1,0e9 * spex232_wl2Uwl( wl ),
               1.0e9 * spex232_wl2Uwl( spex232.upper_limit ) );
        SPEX232_THROW( EXCEPTION );
    }

    too_many_arguments( v );

    spex232.wavelength = spex232_set_wavelength( wl );
    spex232.is_wavelength = SET;
    spex232.in_scan = UNSET;

    return vars_push( FLOAT_VAR, spex232_wl2Uwl( spex232.wavelength ) );
}


/*----------------------------------------------------------------------*
 * Queries the current wavenumber (in cm^-1) of the monochromator. When
 * in wavenumber offset mode the difference to the laser line is used.
 * If the monochromator is currently doing scan and a new wavenumber is
 * set the currently running scan is aborted.
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavenumber( Var_T * v )
{
    double wl;


    CLOBBER_PROTECT( wl );

    if ( v == NULL )
    {
        if ( ! spex232.is_wavelength && ! spex232.scan_is_init )
        {
            print( FATAL, "Wavenumber hasn't been set yet.\n ");
            SPEX232_THROW( EXCEPTION );
        }

        return vars_push( FLOAT_VAR,
                          spex232_wl2Uwn( spex232.wavelength ) );
    }

    wl = get_double( v, "wavenumber" );

    TRY
    {
        wl = spex232_Uwn2wl( wl );
        TRY_SUCCESS;
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        print( FATAL, "Invalid wavenumber of %.4f cm^-1.\n",
               spex232_wl2Uwn( wl ) );
        SPEX232_THROW( EXCEPTION );
    }
    OTHERWISE
        SPEX232_RETHROW( );

    if ( wl < spex232.lower_limit )
    {
        print( FATAL, "Requested %s wavenumber of %.4f cm^-1 is %s than the "
               "%s wavenumber limit of %.4f cm^-1.\n",
               spex232.mode == WN_REL ? "(relative)" : "(absolute)",
               spex232_wl2Uwn( wl ),
               spex232.mode == WN_ABS ? "larger" : "lower",
               spex232.mode == WN_ABS ? "upper" : "lower",
               spex232_wl2Uwn( spex232.lower_limit ) );
        SPEX232_THROW( EXCEPTION );
    }

    if ( wl > spex232.upper_limit )
    {
        print( FATAL, "Requested %s wavenumber of %.4f cm^-1 is %s than the "
               "%s wavenumber limit of %.4f cm^-1.\n",
               spex232.mode == WN_REL ? "(relative)" : "(absolute)",
               spex232_wl2Uwn( wl ),
               spex232.mode == WN_ABS ? "lower" : "larger",
               spex232.mode == WN_ABS ? "lower" : "upper",
               spex232_wl2Uwn( spex232.upper_limit ) );
        SPEX232_THROW( EXCEPTION );
    }

    too_many_arguments( v );

    spex232.wavelength = spex232_set_wavelength( wl );
    spex232.is_wavelength = SET;
    spex232.in_scan = UNSET;

    return vars_push( FLOAT_VAR, spex232_wl2Uwn( spex232.wavelength ) );
}


/*-------------------------------------------------------------------------*
 * Function can be used to either determine the current settings for scans
 * (by calling it without an argument) or to set new scan parameters. In
 * the second case two arguments are required, the start position of the
 * scan and the scan step size. If a new scan setup is done while the
 * device is already doing a scan (is in scan mode) the currently running
 * scan is aborted. For wavelength driven monochromators the scan start
 * position and the step size must be given in meters while for wavenumber
 * driven ones in cm^-1. Of course, in queries both are also returned in
 * these units. For wavenumber driven monochromators we also have to deal
 * with a laser line being set: if it is the start wavenumber is taken to
 * in relative, otherwise in absolute units.
 *-------------------------------------------------------------------------*/

Var_T *monochromator_scan_setup( Var_T * v )
{
    double start = 0.0;
    double step;
    long int num_steps;
    double vals[ 2 ];


    CLOBBER_PROTECT( start );
    CLOBBER_PROTECT( v );

    /* Deal with queries */

    if ( v == NULL )
    {
        if ( ! spex232.scan_is_init )
        {
            print( FATAL, "No scan setup has been done.\n" );
            SPEX232_THROW( EXCEPTION );
        }

        if ( spex232.mode & WN_MODES )
            vals[ 0 ] = spex232_wl2Uwn( spex232.scan_start );
        else
            vals[ 0 ] = spex232_wl2Uwl( spex232.scan_start );

        vals[ 1 ] = spex232.scan_step;

        return vars_push( FLOAT_ARR, vals, 2 );
    }

    if ( v->next == NULL )
    {
        print( FATAL, "Missing scan step size argument.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Get the start point of the scan and check it */

    TRY
    {
        if ( spex232.mode & WN_MODES )
        {
            start = get_double( v, "start wavenumber" );
            start = spex232_Uwn2wl( start );
        }
        else
        {
            start = get_double( v, "start wavelength" );
            start = spex232_Uwl2wl( start );
        }

        TRY_SUCCESS;
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        if ( spex232.mode & WN_MODES )
            print( FATAL, "Invalid start wavenumber of %.4f cm^-1.\n",
                   spex232_wl2Uwn( start ) );
        else
            print( FATAL, "Invalid start wavelength of %.5f nm.\n",
                   1.0e9 * spex232_wl2Uwl( start ) );
        SPEX232_THROW( EXCEPTION );
    }
    OTHERWISE
        SPEX232_RETHROW( );

    if ( start < spex232.lower_limit )
    {
        if ( spex232.mode & WN_MODES )
            print( FATAL, "(Absolute) start wavenumber of %.4f cm^-1 is "
                   "higher than the upper limit of the device of %.4f "
                   "cm^-1.\n", spex232_wl2Uwn( start ),
                   spex232_wl2Uwn( spex232.lower_limit ) );
        else
            print( FATAL, "Start wavelength of %.5f nm is below the lower "
                   "limit of the device of %.5f nm.\n",
                   1.0e9 * spex232_wl2Uwl( start ),
                   1.0e9 * spex232_wl2Uwl( spex232.lower_limit ) );

        SPEX232_THROW( EXCEPTION );
    }

    if ( start > spex232.upper_limit )
    {
        if ( spex232.mode & WN_MODES )
            print( FATAL, "(Absolute) start wavenumber of %.4f cm^-1 is below "
                   "the the lower limit of the device of %.4f cm^-1.\n",
                   "cm^-1.\n", spex232_wl2Uwn( start ),
                   spex232_wl2Uwn( spex232.upper_limit ) );
        else
            print( FATAL, "Start wavelength of %.5f nm is higher than the "
                   "upper limit of the device of %.5f nm.\n",
                   1.0e9 * spex232_wl2Uwl( start ),
                   1.0e9 * spex232_wl2Uwl( spex232.upper_limit ) );

        SPEX232_THROW( EXCEPTION );
    }

    v = vars_pop( v );

    /* Get also the scan step size and check it */

    if ( spex232.mode & WN_MODES )
        step = get_double( v, "wavenumber step width" );
    else
        step = get_double( v, "wavelength step width" );

    /* All SPEX monochromators scan from low to high wavelengths and high
       to low (absolute) wavenumbers. */

    if ( step <= 0.0 ) 
    {
        print( FATAL, "Step size must be positive.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    if ( spex232.mode == WL && start + step > spex232.upper_limit )
    {
        print( FATAL, "Step size of %.5f nm is too large.\n",
               10e9 * step );
        SPEX232_THROW( EXCEPTION );
    }

    if ( spex232.mode & WN_MODES &&
         spex232_wn2wl( spex232_wl2wn( start ) - step ) > spex232.upper_limit )
    {
        print( FATAL, "Step size of %.4f cm^-1 is too large.\n", step );
        SPEX232_THROW( EXCEPTION );
    }

    if ( spex232.mode & WN_MODES &&
         lrnd( 1.0e4 * step ) < lrnd( 1.0e4 * spex232.mini_step ) )
    {
        print( FATAL, "Step size of %.4f cm^-1 is smaller than the "
               "minimum possible step size of %.4f cm^-1.\n",
               step, spex232.mini_step );
    }

    if ( spex232.mode == WL &&
         lrnd( 1.0e5 * step ) < lrnd( 1.0e5 * spex232.mini_step ) )
    {
        print( FATAL, "Step size of %.5f nm is smaller than the "
               "minimum possible step size of %.5f.\n",
               10e9 * step, 10e9 * spex232.mini_step );
        SPEX232_THROW( EXCEPTION );
    }

    if ( step < spex232.mini_step )
        step = spex232.mini_step;

    /* Finally make sure the step size is an integer multiple of the minimum
       step size and warn if the deviation is larger than 1% */

    num_steps = lrnd( step / spex232.mini_step );

    if ( fabs( num_steps - lrnd( step / spex232.mini_step ) ) > 0.01 )
    {
        if ( spex232.mode & WN_MODES )
            print( SEVERE, "Absolute value of step size of %.4f cm^-1 isn't "
                   "an integer multiple of the minimum step size of %.4f "
                   "cm^-1, changing it to %.4f cm^-1.\n", step,
                   spex232.mini_step, num_steps * spex232.mini_step );
        else
            print( SEVERE, "Step size of %.5f nm isn't an integer multiple of "
                   "the minimum step size of %.5f nm, changing it to %.5f "
                   "nm.\n", 10e9 * step, 1.0e9 * spex232.mini_step,
                   1.0e-9 * num_steps * spex232.mini_step );
    }

    too_many_arguments( v );

    spex232.scan_start = start;
    spex232.scan_step = num_steps * spex232.mini_step;
    spex232.scan_is_init = SET;
    spex232.in_scan = UNSET;

    if ( spex232.mode & WN_MODES )
        vals[ 0 ] = spex232_wl2Uwn( spex232.scan_start );
    else
        vals[ 0 ] = spex232_wl2Uwl( spex232.scan_start );

    vals[ 1 ] = spex232.scan_step;

    return vars_push( FLOAT_ARR, vals, 2 );
}


/*------------------------------------------------------------------*
 * Function for starting a new scan: the monochromator is driven to
 * the start position of the scan. If the monochromator was already
 * doing a scan the old scan is aborted.
 *------------------------------------------------------------------*/

Var_T *monochromator_start_scan( Var_T * v  UNUSED_ARG )
{
    /* Check that a scan setup has been done */

    if ( ! spex232.scan_is_init )
    {
        print( FATAL, "Missing scan setup.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Move monochromator to start position of the scan */

    spex232_scan_start( );

    if ( spex232.mode & WN_MODES )
        return vars_push( FLOAT_VAR,
                          spex232_wl2Uwn( spex232.wavelength ) );
    else
        return vars_push( FLOAT_VAR,
                          spex232_wl2Uwl( spex232.wavelength ) );
}


/*------------------------------------------------------------*
 * Function for doing a scan step, requires that a scan setup
 * has been done and a scan already has been started.
 *------------------------------------------------------------*/

Var_T *monochromator_scan_step( Var_T * v  UNUSED_ARG )
{
    /* Check that a scan setup has been done */

    if ( ! spex232.scan_is_init )
    {
        print( FATAL, "Missing scan setup.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Check that a scan has been started */

    if ( ! spex232.in_scan )
    {
        print( FATAL, "No scan has been started yet.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Check that the nex scan step can be done */

    if ( spex232.mode == WL &&
         spex232.wavelength + spex232.scan_step > spex232.upper_limit )
    {
        if ( FSC2_MODE == EXPERIMENT )
        {
            print( SEVERE, "Scan reached upper limit of wavelength range.\n" );
            return vars_push( FLOAT_VAR,
                              spex232_wl2Uwl( spex232.wavelength ) );
        }

        print( FATAL, "Scan reached upper limit of wavelength range.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    if ( spex232.mode & WN_MODES &&
         spex232_wn2wl( spex232_wl2wn( spex232.wavelength )
                        - spex232.scan_step ) > spex232.upper_limit )
    {
        if ( FSC2_MODE == EXPERIMENT )
        {
            print( SEVERE, "Scan reached lower limit of wavenumber range.\n" );
            return vars_push( FLOAT_VAR,
                              spex232_wl2Uwn( spex232.wavelength ) );
        }

        print( FATAL, "Scan reached lower limit of wavenumber range.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Move the monochromator for a scan step */

    spex232_scan_step( );

    return vars_push( FLOAT_VAR, spex232.mode == WL ?
                      spex232_wl2Uwl( spex232.wavelength ) :
                      spex232_wl2Uwn( spex232.wavelength ) );
}


/*----------------------------------------------------------------*
 * Function for setting the laser line position. This can only
 * be done for monochromators that are wavenumber driven and,
 * in contrast the driver for the CD2A, is only done in software.
 * When  a laser line has been set in the following only relative
 * wavenumbers can be used (i.e. differences between the laser
 * line position and absolute wavenumbers), and all functions
 * involving wavenumbers return relative positions. A laser
 * line position can switched of (reverting also to use of
 * absolute wavenumbers) by setting a zero or negative laser
 * line position. If called without an argument the function
 * returns the current setting of the laser line position
 * (always in absolute wavenumbers) or zero if no laser line
 * position has been set.
 *----------------------------------------------------------------*/

Var_T *monochromator_laser_line( Var_T * v )
{
    double wn;


    /* Setting a laser line (and thereby switching to wavenumber offset
       mode) only works when the device is in wavenumber mode */

    if ( spex232.mode == WL )
    {
        print( FATAL, "Laser line position can only be used in wavenumber "
               "mode of monochromator.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    if ( v == NULL )
        return vars_push( FLOAT_VAR,spex232.laser_line == 0.0 ? 0.0 :
                          spex232.laser_line );

    wn = get_double( v, "wavenumber of laser line" );

    /* A zero or negative laser line wavenumber switches delta wavelength
       mode off! */

    if ( wn <= 0.0 )
    {
        spex232.mode = WN_ABS;
        spex232.laser_line = 0.0;
    }
    else
    {
        spex232.mode = WN_REL;
        spex232.laser_line = wn;
    }

    return vars_push( FLOAT_VAR,spex232.laser_line );
}


/*-----------------------------------------------------------------*
 * Function returns the number of grooves per meter of the grating
 *-----------------------------------------------------------------*/

Var_T *monochromator_groove_density( Var_T * v )
{
    if ( v != NULL )
        print( WARN, "There's only one grating, argument is discarded.\n" );

    return vars_push( FLOAT_VAR, spex232.grooves );
}


/*--------------------------------------------------------------------*
 * Function for calibration of the monochromator. It tells the driver
 * 1. about an offset between what the driver reports to the user and
 *    where a line is supposed to be.
 * 2. (optionally) the width of a single pixel on the CCD camera
 *
 * The function is to be called at least with the difference between
 * the position where the line appears and the expected position (in
 * wavenumber modes in units of cm^-1 and in wavelength mode in nm).
 *
 * For wavelength driven monochromators the offset must be given in
 * wavelength units, while for wavenumber driven monochromators in
 * wavenumber units (i.e. cm^-1).
 *
 * If there's another argument it has to be the wavelength difference
 * between two pixels of the CCD camera (always to be given in wave-
 * length units since the monochromator is linear in wavelengths, not
 * wavenumbers). If not given the old value is retained.
 *
 * If called without an argument the function returns an array of two
 * values with the offset and the pixel difference. A value of 0 for
 * the pixel difference means that no calibration of this value has
 * been done.
 *--------------------------------------------------------------------*/

Var_T *monochromator_calibrate( Var_T * v )
{
    double pixel_diff;
    double offset;
    double new_offset;
    double field[ 2 ];


    if ( v == NULL )
    {
        field[ 0 ] = spex232.offset;
        field[ 1 ] = spex232.pixel_diff;
        return vars_push( FLOAT_ARR, field, 2 );
    }

    /* A new offset can only be set for wavenumber driven monochromators
       while in absolute wavenumber mode */

    if ( spex232.mode & WN_MODES )
    {
        new_offset = get_double( v, "wavenumber offset" );
        if ( spex232.mode == WN_REL )
            new_offset *= -1;
    }
    else
        new_offset = get_double( v, "wavelength offset" );

    offset = spex232.offset + new_offset;

#if defined SPEX232_MAX_OFFSET
    if ( spex232.mode & WN_MODES && offset > SPEX232_MAX_OFFSET )
    {
        print( FATAL, "Offset of %.4f cm^-1 is unrealistically high. If this "
               "isn't an error change the \"SPEX232_MAX_OFFSET\" setting in "
               "the device configuration file and recompile.\n", new_offset );
        SPEX232_THROW( EXCEPTION );
    }

    if ( spex232.mode == WL && fabs( 1.0e9 * offset )
                                                       > SPEX232_MAX_OFFSET )
    {
        print( FATAL, "Offset of %.5f nm is unrealistically high. If this "
               "isn't an error change the \"SPEX232_MAX_OFFSET\" setting in "
               "the device configuration file and recompile.\n",
               1.0e9 * new_offset );
        SPEX232_THROW( EXCEPTION );
    }
#endif

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        pixel_diff = get_double( v, "wavelength difference for one CCD "
                                 "camera pixel" );

        if ( pixel_diff <= 0.0 )
        {
            print( FATAL, "Invalid negative wavelength difference for one CCD "
                   "camera pixel.\n" );
            SPEX232_THROW( EXCEPTION );
        }
    }
    else
        pixel_diff = spex232.pixel_diff;

    too_many_arguments( v );

    spex232.offset = offset;
    spex232.pixel_diff = pixel_diff;

    field[ 0 ] =
              spex232.mode == WN_REL ? - spex232.offset : spex232.offset;
    field[ 1 ] = spex232.pixel_diff;

    return vars_push( FLOAT_ARR, field, 2 );
}


/*----------------------------------------------------------------------*
 * Function returns an array of two wavelength values that are suitable
 * for use as axis description parameters (start of axis and increment)
 * required by by the change_scale() function (if the camera uses
 * binning the second element may have to be multiplied by the x-binning
 * width). Please note: since the axis is not really linear the axis
 * displayed when using these values isn't absolutely correct!
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavelength_axis( Var_T * v )
{
    double wl = spex232.wavelength;
    Var_T *cv;
    long int num_pixels;
    int acc;


    CLOBBER_PROTECT( wl );

    if ( v != NULL )
    {
        wl = get_double( v, "wavelength" );

        TRY
        {
            wl = spex232_Uwl2wl( wl );
            TRY_SUCCESS;
        }
        CATCH( INVALID_INPUT_EXCEPTION )
        {
            print( FATAL, "Invalid center wavelength of %.5f nm.\n",
                   1.0e9 * spex232_wl2Uwl( wl ) );
            SPEX232_THROW( EXCEPTION );
        }
        OTHERWISE
            SPEX232_RETHROW( );
    }
    else
    {
        if ( ! spex232.is_wavelength )
        {
            print( FATAL, "Wavelength hasn't been set yet.\n ");
            SPEX232_THROW( EXCEPTION );
        }

        if ( spex232.is_wavelength )
            wl = spex232.wavelength;
    }

    too_many_arguments( v );

    /* Check if there's a CCD camera module loaded */

    if ( ! exists_device_type( "ccd camera" ) )
    {
        print( FATAL, "Function can only be used when the module for a "
               "CCD camera is loaded.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Get the width (in pixels) of the chip of the camera */

    if ( ! func_exists( "ccd_camera_pixel_area" ) )
    {
        print( FATAL, "CCD camera has no function for determining the "
               "size of the chip.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    cv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );

    if ( cv->type != INT_ARR ||
         cv->val.lpnt[ 0 ] <= 0 || cv->val.lpnt[ 1 ] <= 0 )
    {
        print( FATAL, "Function of CCD for determining the size of the chip "
               "does not return a useful value.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    num_pixels = cv->val.lpnt[ 0 ];
    vars_pop( cv );

    cv = vars_push( FLOAT_ARR, NULL, 2 );

    cv->val.dpnt[ 0 ] = spex232_wl2Uwl( wl - 0.5 * ( num_pixels - 1 )
                                        * spex232.pixel_diff );
    cv->val.dpnt[ 1 ] = spex232.pixel_diff;

    return cv;
}


/*----------------------------------------------------------------------*
 * Function returns an array of two wavenumber values that are suitable
 * for use as axis description parameters (start of axis and increment)
 * required by by the change_scale() function (if the camera uses
 * binning the second element may have to be multiplied by the x-binning
 * width). Please note: since the axis is not really linear the axis
 * displayed when using these values isn't absolutely correct!
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavenumber_axis( Var_T * v )
{
    double wl = spex232.wavelength;
    Var_T *cv;
    long int num_pixels;
    int acc;


    CLOBBER_PROTECT( wl );

    if ( v != NULL )
    {
        wl = get_double( v, "wavenumber" );

        TRY
        {
            wl = spex232_Uwn2wl( wl );
            TRY_SUCCESS;
        }
        CATCH( INVALID_INPUT_EXCEPTION )
        {
            print( FATAL, "Invalid wavenumber of %.4f cm^-1.\n",
                   spex232_wl2Uwn( wl ) );
            SPEX232_THROW( EXCEPTION );
        }
        OTHERWISE
            SPEX232_RETHROW( );
    }
    else
    {
        if ( ! spex232.is_wavelength && ! spex232.scan_is_init )
        {
            print( FATAL, "Wavenumber hasn't been set yet.\n ");
            SPEX232_THROW( EXCEPTION );
        }

        wl = spex232.wavelength;
    }

    too_many_arguments( v );

    /* Check that we can talk with the camera */

    if ( ! exists_device_type( "ccd camera" ) )
    {
        print( FATAL, "Function can only be used when the module for a "
               "CCD camera is loaded.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    /* Get the width (in pixels) of the chip of the camera */

    if ( ! func_exists( "ccd_camera_pixel_area" ) )
    {
        print( FATAL, "CCD camera has no function for determining the "
               "size of the chip.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    cv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );

    if ( cv->type != INT_ARR ||
         cv->val.lpnt[ 0 ] <= 0 || cv->val.lpnt[ 1 ] <= 0 )
    {
        print( FATAL, "Function of CCD for determining the size of the chip "
               "does not return a useful value.\n" );
        SPEX232_THROW( EXCEPTION );
    }

    num_pixels = cv->val.lpnt[ 0 ];
    vars_pop( cv );

    cv = vars_push( FLOAT_ARR, NULL, 2 );

    cv->val.dpnt[ 0 ] = spex232_wl2Uwn( wl + 0.5 * ( num_pixels - 1 )
                                        * spex232.pixel_diff );

    cv->val.dpnt[ 1 ] = ( spex232_wl2Uwn( wl - 0.5 * ( num_pixels - 1 )
                                          * spex232.pixel_diff )
                          - cv->val.dpnt[ 0 ] ) / ( num_pixels - 1 );

    if ( spex232.mode == WN_ABS )
        cv->val.dpnt[ 1 ] *= -1.0;

    return cv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
