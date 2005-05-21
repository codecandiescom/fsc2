/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2005 Jens Thoms Toerring
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


#include "spex_cd2a.h"


/*--------------------------------*
 * Global variables of the module
 *--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


struct Spex_CD2A spex_cd2a, spex_cd2a_stored;



/*------------------------------------------------------------*
 * Function that gets called when the module has been loaded.
 *------------------------------------------------------------*/

int spex_cd2a_init_hook( void )
{
	spex_cd2a.fatal_error = UNSET;

	/* Claim the serial port (throws exception on failure) */

	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	spex_cd2a.is_needed = SET;
	spex_cd2a.is_open = UNSET;

	/* Determine (from the configuration file) if the monochromator is
	   wavenumber or wavelength driven */

	spex_cd2a.method = DRIVE_METHOD;

	switch ( spex_cd2a.method )
	{
		case WAVELENGTH :
			spex_cd2a.mode = WL;
			break;

		case WAVENUMBER :
			spex_cd2a.mode = WN;
			break;

		default :
			print( FATAL, "Invalid setting for drive method in "
				   "configuration file for device.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Read in the file where the state of the monochromator gets stored
	   to find out about the offset between the values displayed at the
	   CD2A and the "true" wavelengths or -numbers and, for wavenumber-
	   driven monochromators the setting of the laser line position as
	   used in the previous invocation of the program. (The file has also
	   to be read each time at the start of an experiment since it's
	   contents are rewritten at the end of each experiment.) */

	spex_cd2a_read_state( );

#if defined SPEX_CD2A_MAX_OFFSET
	if ( fabs( spex_cd2a.offset ) > SPEX_CD2A_MAX_OFFSET )
	{
		const char *dn = fsc2_config_dir( );
		char *fn = get_string( "%s%s%s", dn, slash( dn ),
							   SPEX_CD2A_STATE_FILE );
		print( FATAL, "Offset setting in state file '%s' is "
			   "unrealistically high.\n", fn );
		T_free( fn );
		SPEX_CD2A_THROW( EXCEPTION );
	}
#endif

	if ( spex_cd2a.mode & WN_MODES && spex_cd2a.laser_wavenumber != 0.0 )
		spex_cd2a.mode = WND;

	/* If it's wavelength driven find out the units the controller expects
	   (either nanometer or Angstrom) */

	if ( spex_cd2a.mode == WL )
	{
		spex_cd2a.units = UNITS;

		if ( spex_cd2a.units != NANOMETER && spex_cd2a.units != ANGSTROEM )
		{
			print( FATAL, "Invalid setting for units in configuration file "
				   "for the device.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
		}
	}

	/* Find out  if the monochromator has a shutter for PMT protection
	   installed */

	spex_cd2a.has_shutter = HAS_SHUTTER == 1;

	/* Find out which is the lowest wavelength that can be set */

	if ( LOWER_LIMIT < 0 ||
		 ( spex_cd2a.mode & WN_MODES && LOWER_LIMIT <= 0 ) )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Invalid setting for upper wavenumber limit in "
				   "configuration file for the device.\n" );
		else
			print( FATAL, "Invalid setting for lower wavelength limit in "
				   "configuration file for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}			

	if ( spex_cd2a.mode & WN_MODES )
	{
		TRY
		{
			spex_cd2a.lower_limit = spex_cd2a_Awn2wl( LOWER_LIMIT );
			TRY_SUCCESS;
		}
		CATCH( INVALID_INPUT_EXCEPTION )
		{
			print( FATAL, "Invalid setting for upper wavenumber limit in "
				   "configuration file for the device.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
	   }
	}
	else if ( UNITS == NANOMETER )
		spex_cd2a.lower_limit = 1.0e-9 * LOWER_LIMIT;
	else
		spex_cd2a.lower_limit = 1.0e-10 * LOWER_LIMIT;

	/* Find out which is the highest wavelength that can be set */

	if ( UPPER_LIMIT < 0 ||
		 ( spex_cd2a.mode & WN_MODES && UPPER_LIMIT <= 0.0 ) )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Invalid setting for lower wavenumber limit in "
				   "configuration file for the device.\n" );
		else
			print( FATAL, "Invalid setting for upper wavelength limit in "
				   "configuration file for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES )
	{
		TRY
		{
			spex_cd2a.upper_limit = spex_cd2a_Awn2wl( UPPER_LIMIT );
			TRY_SUCCESS;
		}
		CATCH( INVALID_INPUT_EXCEPTION )
		{
			print( FATAL, "Invalid setting for lower wavenumber limit in "
				   "configuration file for the device.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
		}
	}
	else if ( UNITS == NANOMETER )
		spex_cd2a.upper_limit = 1.0e-9 * UPPER_LIMIT;
	else
		spex_cd2a.upper_limit = 1.0e-10 * UPPER_LIMIT;

	/* Check consistency of settings for lower and upper limit */

	if ( spex_cd2a.upper_limit <= spex_cd2a.lower_limit )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Lower wavenumber limit isn't larger than the "
				   "upper limit in configuration file for the device.\n" );
		else
			print( FATAL, "Upper wavelength limit isn't larger than the "
				   "lower limit in configuration file for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Determine the number of grooves per meter for the installed grating */

	if ( GROOVE_DENSITY <= 0 )
	{
		print( FATAL, "Invalid setting for number of grooves per mm of "
			   "grating in configuration file for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	spex_cd2a.grooves = 1000 * GROOVE_DENSITY;

	if ( STANDARD_GROOVE_DENSITY <= 0 )
	{
		print( FATAL, "Invalid setting for number of grooves per mm of "
			   "standard grating in configuration file for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Determine the number of grooves per meter for the standard grating */

	spex_cd2a.standard_grooves = 1000 * STANDARD_GROOVE_DENSITY;

	/* Get the minimum step size of the motor. It gets calculated from
	   the STEPS_PER_UNIT entry in the configuration file and the ratio
	   of the grooves of the installed and the standard grating. */

	if ( spex_cd2a.mode == WL )
	{
		if ( UNITS == NANOMETER )
			spex_cd2a.mini_step = 1.0e-9 * spex_cd2a.standard_grooves
								  / ( spex_cd2a.grooves * STEPS_PER_UNIT );
		else
			spex_cd2a.mini_step = 1.0e-10 * spex_cd2a.standard_grooves
								  / ( spex_cd2a.grooves * STEPS_PER_UNIT );
	}
	else
		spex_cd2a.mini_step = spex_cd2a.grooves /
							  ( spex_cd2a.standard_grooves * STEPS_PER_UNIT );

	if ( spex_cd2a.mini_step <= 0.0 )
	{
		print( FATAL, "Invalid setting for steps per unit with standard "
			   "grating in configuration file for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Also the lower and upper limit must be corrected if the grating isn't
	   the standard grating. In this case it also seems not to be possible
	   to go to the limits, we should stay at least two steps away from them */

	if ( GROOVE_DENSITY != STANDARD_GROOVE_DENSITY )
	{
		spex_cd2a.lower_limit *=
								spex_cd2a.standard_grooves / spex_cd2a.grooves;
		spex_cd2a.upper_limit *=
								spex_cd2a.standard_grooves / spex_cd2a.grooves;

		if ( spex_cd2a.mode == WL )
		{
			spex_cd2a.lower_limit += 2 * spex_cd2a.mini_step;
			spex_cd2a.upper_limit -= 2 * spex_cd2a.mini_step;
		}
		else
		{
			spex_cd2a.lower_limit =
				spex_cd2a_Awn2wl( spex_cd2a_wl2Awn( spex_cd2a.lower_limit )
								  - 2 * spex_cd2a.mini_step );
			spex_cd2a.upper_limit =
				spex_cd2a_Awn2wl(  spex_cd2a_wl2Awn( spex_cd2a.upper_limit )
								   + 2 * spex_cd2a.mini_step );
		}
	}

	/* Determine the format data are send by the device */

	spex_cd2a.data_format = DATA_FORMAT;
	if ( spex_cd2a.data_format != STANDARD &&
		 spex_cd2a.data_format != DATALOGGER )
	{
		print( FATAL, "Invalid setting for data format in configuration file "
			   "for the device.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Determine if we need to send a checksum with commands */

	spex_cd2a.use_checksum = USE_CHECKSUM != NO_CHECKSUM;

	/* Determine if device sends a linefeed at the end of data */

	spex_cd2a.sends_lf = SENDS_LINEFEED != NO_LINEFEED;

	spex_cd2a.is_wavelength = UNSET;
	spex_cd2a.scan_is_init = UNSET;
	spex_cd2a.shutter_limits_are_set = UNSET;

	return 1;
}


/*-------------------------------------------------------------------------*
 * Function called (in the parents context) at the start of the experiment
 *-------------------------------------------------------------------------*/

int spex_cd2a_test_hook( void )
{
	spex_cd2a_stored = spex_cd2a;
	return 1;
}


/*-----------------------------------------------------------*
 * Function gets called always at the start of an experiment
 *-----------------------------------------------------------*/

int spex_cd2a_exp_hook( void )
{
	spex_cd2a = spex_cd2a_stored;

	/* Read in the file where the state of the monochromator gets stored,
	   it might have been changed during a previous experiment */

	spex_cd2a_read_state( );

#if defined SPEX_CD2A_MAX_OFFSET
	if ( fabs( spex_cd2a.offset ) > SPEX_CD2A_MAX_OFFSET )
	{
		print( FATAL, "Offset setting in state file '%s' is "
			   "unrealistically high.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}
#endif

	/* Open the serial port for the device */

	spex_cd2a_open( );

	/* Test if the device reacts */

	spex_cd2a_init( );

	return 1;
}


/*--------------------------------------------------------------*
 * Function gets called just before the child process doing the
 * experiment exits.
 *--------------------------------------------------------------*/

void spex_cd2a_child_exit_hook( void )
{
	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );
	if ( ! spex_cd2a.fatal_error )
		spex_cd2a_store_state( );
}


/*-----------------------------------------------------------------------*
 * Function called (in the parents context) at the end of the experiment
 *-----------------------------------------------------------------------*/

int spex_cd2a_end_of_exp_hook( void )
{
	spex_cd2a_close( );
	return 1;
}


/*----------------------------------------------*
 * Returns a string with the name of the device
 *----------------------------------------------*/

Var_T *monochromator_name( UNUSED_ARG Var_T *v )
{
	return vars_push( STR_VAR, DEVICE_NAME );
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

Var_T *monochromator_scan_setup( Var_T *v )
{
	double start = 0.0;
	double step;
	long num_steps;
	double vals[ 2 ];


	CLOBBER_PROTECT( v );
	CLOBBER_PROTECT( start );

	if ( v == NULL )
	{
		if ( ! spex_cd2a.scan_is_init )
		{
			print( FATAL, "No scan setup has been done.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
		}

		if ( spex_cd2a.mode & WN_MODES )
			vals[ 0 ] = spex_cd2a_SAwn2UMwn( spex_cd2a.scan_start );
		else
			vals[ 0 ] = spex_cd2a_Swl2Uwl( spex_cd2a.scan_start );

		vals[ 1 ] = spex_cd2a.scan_step;

		return vars_push( FLOAT_ARR, vals, 2 );
	}

	if ( v->next == NULL )
	{
		print( FATAL, "Missing scan step size argument.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	TRY
	{
		if ( spex_cd2a.mode & WN_MODES )
		{
			start = get_double( v, "start wavenumber" );
			start = spex_cd2a_UMwn2Swl( start );
		}
		else
		{
			start = get_double( v, "start wavelength" );
			start = spex_cd2a_Uwl2Swl( start );
		}

		TRY_SUCCESS;
	}
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Invalid start wavenumber of %.4f cm^-1.\n", start );
		else
			print( FATAL, "Invalid start wavelength of %.5f nm.\n",
				   1.0e9 * start );
		SPEX_CD2A_THROW( EXCEPTION );
	}
	OTHERWISE
		SPEX_CD2A_RETHROW( );

	if ( start < spex_cd2a.lower_limit )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "(Absolute) start wavenumber of %.4f cm^-1 is "
				   "higher than the upper limit of the device of %.4f "
				   "cm^-1.\n",
				   spex_cd2a_Swl2UMwn( start ),
				   spex_cd2a_Swl2UMwn( spex_cd2a.lower_limit ) );
		else
			print( FATAL, "Start wavelength of %.5f nm is below the lower "
				   "limit of the device of %.5f nm.\n",
				   1.0e9 * spex_cd2a_Swl2Uwl( start ),
				   1.0e9 * spex_cd2a_Swl2Uwl( spex_cd2a.lower_limit ) );

		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( start > spex_cd2a.upper_limit )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "(Absolute) start wavenumber of %.4f cm^-1 is below "
				   "the the lower limit of the device of %.4f cm^-1.\n",
				   spex_cd2a_Swl2UMwn( start ),
				   spex_cd2a_Swl2UMwn( spex_cd2a.upper_limit ) );
		else
			print( FATAL, "Start wavelength of %.5f nm is higher than the "
				   "upper limit of the device of %.5f nm.\n",
				   1.0e9 * spex_cd2a_Swl2Uwl( start ),
				   1.0e9 * spex_cd2a_Swl2Uwl( spex_cd2a.upper_limit ) );

		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES )
		start = spex_cd2a_wl2Awn( start );

	v = vars_pop( v );

	if ( spex_cd2a.mode & WN_MODES )
		step = get_double( v, "wavenumber step width" );
	else
		step = get_double( v, "wavelength step width" );

	/* All SPEX monochromators scan from low to high wavelengths and high
	   to low (absolute) wavenumbers. */

	if ( step <= 0.0 ) 
	{
		print( FATAL, "Step size must be positive.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode == WL && start + step > spex_cd2a.upper_limit )
	{
		print( FATAL, "Step size of %.5f nm is too large.\n",
			   10e9 * step );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES &&
		 spex_cd2a_Awn2wl( start - step ) > spex_cd2a.upper_limit )
	{
		print( FATAL, "Step size of %.4f cm^-1 is too large.\n", step );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES &&
		 lrnd( 1.0e4 * step ) < lrnd( 1.0e4 * spex_cd2a.mini_step ) )
	{
		print( FATAL, "Step size of %.4f cm^-1 is smaller than the "
			   "minimum possible step size of %.4f cm^-1.\n",
			   step, spex_cd2a.mini_step );
	}

	if ( spex_cd2a.mode == WL &&
		 lrnd( 1.0e5 * step ) < lrnd( 1.0e5 * spex_cd2a.mini_step ) )
	{
		print( FATAL, "Step size of %.5f nm is smaller than the "
			   "minimum possible step size of %.5f.\n",
			   10e9 * step, 10e9 * spex_cd2a.mini_step );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( step < spex_cd2a.mini_step )
		step = spex_cd2a.mini_step;

	/* Finally make sure the step size is an integer multiple of the minimum
	   step size */

	num_steps = lrnd( step / spex_cd2a.mini_step );

	if ( fabs( num_steps - lrnd( step / spex_cd2a.mini_step ) ) > 0.01 )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( SEVERE, "Absolute value of step size of %.4f cm^-1 isn't "
				   "an integer multiple of the minimum step size of %.4f "
				   "cm^-1, changing it to %.4f cm^-1.\n", step,
				   spex_cd2a.mini_step, num_steps * spex_cd2a.mini_step );
		else
			print( SEVERE, "Step size of %.5f nm isn't an integer multiple of "
				   "the minimum step size of %.5f nm, changing it to %.5f "
				   "nm.\n", 10e9 * step, 1.0e9 * spex_cd2a.mini_step,
				   1.0e-9 * num_steps * spex_cd2a.mini_step );
	}

	too_many_arguments( v );

	spex_cd2a.scan_start = start;
	spex_cd2a.scan_step = num_steps * spex_cd2a.mini_step;
	spex_cd2a.scan_is_init = SET;

	/* Stop a possibly already running scan, then set the new start position
	   and step size */

	spex_cd2a_halt( );

	if ( FSC2_MODE == EXPERIMENT )
	{
		spex_cd2a_scan_start( );
		spex_cd2a_scan_step( );
	}

	if ( spex_cd2a.mode & WN_MODES )
		vals[ 0 ] = spex_cd2a_SAwn2UMwn( spex_cd2a.scan_start );
	else
		vals[ 0 ] = spex_cd2a_Swl2Uwl( spex_cd2a.scan_start );

	vals[ 1 ] = spex_cd2a.scan_step;

	return vars_push( FLOAT_ARR, vals, 2 );
}


/*----------------------------------------------------------*
 * Set or query the current wavelength of the monochromator
 * (always return the absolute wavelength in meter, even
 * when in wavenumber mode). If the monochromator is doing
 * a scan and a new wavelength is set the scan is aborted.
 *----------------------------------------------------------*/

Var_T *monochromator_wavelength( Var_T *v )
{
	double wl;


	CLOBBER_PROTECT( wl );

	if ( v == NULL )
	{
		if ( ! spex_cd2a.is_wavelength && ! spex_cd2a.scan_is_init )
		{
			print( FATAL, "Wavelength hasn't been set yet.\n ");
			SPEX_CD2A_THROW( EXCEPTION );
		}

		return vars_push( FLOAT_VAR,
						  spex_cd2a_Swl2Uwl( spex_cd2a.wavelength ) );
	}

	wl = get_double( v, "wavelength" );

	TRY
	{
		wl = spex_cd2a_Uwl2Swl( wl );
		TRY_SUCCESS;
	}
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		print( FATAL, "Invalid wavelength of %.5f nm.\n", 1.0e9 * wl );
		SPEX_CD2A_THROW( EXCEPTION );
	}
	OTHERWISE
		SPEX_CD2A_RETHROW( );

	if ( wl < spex_cd2a.lower_limit )
	{
		print( FATAL, "Requested wavelength of %.5f nm is lower than the "
			   "lower wavelength limit of %.5f nm.\n",
			   1.0e9 * spex_cd2a_Swl2Uwl( wl ),
			   1.0e9 * spex_cd2a_Swl2Uwl( spex_cd2a.lower_limit ) );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( wl > spex_cd2a.upper_limit )
	{
		print( FATAL, "Requested wavelength of %.5f nm is larger than the "
			   "upper wavelength limit of %.5f nm.\n",
			   1,0e9 * spex_cd2a_Swl2Uwl( wl ),
			   1.0e9 * spex_cd2a_Swl2Uwl( spex_cd2a.upper_limit ) );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	too_many_arguments( v );

	spex_cd2a.wavelength = wl;
	spex_cd2a.is_wavelength = SET;

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	spex_cd2a_set_wavelength( );

	return vars_push( FLOAT_VAR, spex_cd2a_Swl2Uwl( spex_cd2a.wavelength ) );
}


/*----------------------------------------------------------------------*
 * Queries the current wavenumber (in cm^-1) of the monochromator. When
 * in wavenumber offset mode the difference to the laser line is used.
 * If the monochromator is currently doing scan and a new wavenumber is
 * set the currently running scan is aborted.
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavenumber( Var_T *v )
{
	double wl;


	CLOBBER_PROTECT( wl );

	if ( v == NULL )
	{
		if ( ! spex_cd2a.is_wavelength && ! spex_cd2a.scan_is_init )
		{
			print( FATAL, "Wavenumber hasn't been set yet.\n ");
			SPEX_CD2A_THROW( EXCEPTION );
		}

		return vars_push( FLOAT_VAR,
						  spex_cd2a_Swl2UMwn( spex_cd2a.wavelength ) );
	}

	wl = get_double( v, "wavenumber" );

	TRY
	{
		wl = spex_cd2a_UMwn2Swl( wl );
		TRY_SUCCESS;
	}
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		print( FATAL, "Invalid wavenumber of %.4f cm^-1.\n", wl );
		SPEX_CD2A_THROW( EXCEPTION );
	}
	OTHERWISE
		SPEX_CD2A_RETHROW( );

	if ( wl < spex_cd2a.lower_limit )
	{
		print( FATAL, "Requested (absolute) wavenumber of %.4f cm^-1 is "
			   "larger than the upper wavenumber limit of %.4f cm^-1.\n",
			   spex_cd2a_Swl2UAwn( wl ),
			   spex_cd2a_Swl2UAwn( spex_cd2a.lower_limit ) );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( wl > spex_cd2a.upper_limit )
	{
		print( FATAL, "Requested (absolute) wavenumber of %.4f cm^-1 is "
			   "lower than the lower wavenumber limit of %.4f cm^-1.\n",
			   spex_cd2a_Swl2UAwn( wl ),
			   spex_cd2a_Swl2UAwn( spex_cd2a.upper_limit ) );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	too_many_arguments( v );

	spex_cd2a.wavelength = wl;
	spex_cd2a.is_wavelength = SET;

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	spex_cd2a_set_wavelength( );

	return vars_push( FLOAT_VAR, spex_cd2a_Swl2UMwn( spex_cd2a.wavelength ) );
}


/*------------------------------------------------------------------*
 * Function for starting a new scan: the monochromator is driven to
 * the start position of the scan. If the monochromator was already
 * doing a scan the old scan is aborted.
 *------------------------------------------------------------------*/

Var_T *monochromator_start_scan( UNUSED_ARG Var_T *v )
{
	if ( ! spex_cd2a.scan_is_init )
	{
		print( FATAL, "Missing scan setup.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	spex_cd2a_start_scan( );

	if ( spex_cd2a.mode & WN_MODES )
		return vars_push( FLOAT_VAR,
						  spex_cd2a_Swl2UMwn( spex_cd2a.wavelength ) );
	else
		return vars_push( FLOAT_VAR,
						  spex_cd2a_Swl2Uwl( spex_cd2a.wavelength ) );
}


/*------------------------------------------------------------*
 * Function for doing a scan step, requires that a scan setup
 * has been done and a scan already has been started.
 *------------------------------------------------------------*/

Var_T *monochromator_scan_step( UNUSED_ARG Var_T *v )
{
	if ( ! spex_cd2a.scan_is_init )
	{
		print( FATAL, "Missing scan setup.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( ! spex_cd2a.in_scan )
	{
		print( FATAL, "No scan has been started yet.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode == WL &&
		 spex_cd2a.wavelength + spex_cd2a.scan_step > spex_cd2a.upper_limit )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( SEVERE, "Scan reached upper limit of wavelength range.\n" );
			return vars_push( FLOAT_VAR,
							  spex_cd2a_Swl2Uwl( spex_cd2a.wavelength ) );
		}

		print( FATAL, "Scan reached upper limit of wavelength range.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES &&
		 spex_cd2a_Awn2wl( spex_cd2a_wl2Awn( spex_cd2a.wavelength )
						   - spex_cd2a.scan_step ) > spex_cd2a.upper_limit )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( SEVERE, "Scan reached lower limit of wavenumber range.\n" );
			return vars_push( FLOAT_VAR,
							  spex_cd2a_Swl2UMwn( spex_cd2a.wavelength ) );
		}

		print( FATAL, "Scan reached lower limit of wavenumber range.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	spex_cd2a_trigger( );

	if ( spex_cd2a.mode == WL )
	{
		spex_cd2a.wavelength += spex_cd2a.scan_step;
		return vars_push( FLOAT_VAR,
						  spex_cd2a_Swl2Uwl( spex_cd2a.wavelength ) );
	}

	spex_cd2a.wavelength =
					 spex_cd2a_Awn2wl( spex_cd2a_wl2Awn( spex_cd2a.wavelength )
									   - spex_cd2a.scan_step );

	return vars_push( FLOAT_VAR,
					  spex_cd2a_Swl2UMwn( spex_cd2a.wavelength ) );
}


/*-------------------------------------------------------------*
 * Function for setting the laser line position. This can only
 * be done for monochromators that are wavenumber driven. When
 * a laser line has been set in the following only relative
 * wavenumbers can be used (i.e. differences between the laser
 * line position and absolute wavenumbers), and all functions
 * involving wavenumbers return relative positions. A laser
 * line position can switched of (reverting also to use of
 * absolute wavenumbers) by setting a zero or negative laser
 * line position. If called without an argument the function
 * returns the current setting of the laser line position
 * (always in absolute wavenumbers) or zero if no laser line
 * position has been set.
 *-------------------------------------------------------------*/

Var_T *monochromator_laser_line( Var_T *v )
{
	double wn, wl;


	/* Setting a laser line (and thereby switching to wavenumber offset
	   mode) only works when the device is in wavenumber mode */

	if ( spex_cd2a.mode == WL )
	{
		print( FATAL, "Laser line position can only be used in wavenumber "
			   "mode of monochromator.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( v == NULL )
		return vars_push( FLOAT_VAR,spex_cd2a.laser_wavenumber == 0.0 ? 0.0 :
						  spex_cd2a_SAwn2UAwn( spex_cd2a.laser_wavenumber ) );

	wn = get_double( v, "wavenumber of laser line" );

	/* A zero or negative laser line wavenumber switches delta wavelength
	   mode off! */

	if ( wn <= 0.0 )
	{
		spex_cd2a.mode = WN;
		spex_cd2a.laser_wavenumber = 0.0;
	}
	else
	{
		spex_cd2a.mode = WND;
		
		TRY
		{
			wl = spex_cd2a_UAwn2Swl( wn );
			TRY_SUCCESS;
		}
		CATCH( INVALID_INPUT_EXCEPTION )
		{
			print( FATAL, "Invalid laser line of %.4f cm^-1.\n", wn );
			SPEX_CD2A_THROW( EXCEPTION );
		}
		OTHERWISE
			SPEX_CD2A_RETHROW( );

		/* Abort if the monochromator has a shutter, the laser line is within
		   it's range but the laser line is outside of the shutter range. */

		if ( spex_cd2a.has_shutter && spex_cd2a.shutter_limits_are_set &&
			 wl >= spex_cd2a.lower_limit && wl <= spex_cd2a.upper_limit &&
			 ( wl < spex_cd2a.shutter_low_limit ||
			   wl > spex_cd2a.shutter_high_limit ) )
		{
			print( FATAL, "Laser line position is not within the shutter "
				   "range.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
		}

		spex_cd2a.laser_wavenumber = spex_cd2a_UAwn2SAwn( wn );
	}

	spex_cd2a_set_laser_line( );

	return vars_push( FLOAT_VAR,spex_cd2a.laser_wavenumber == 0.0 ? 0.0 :
					  spex_cd2a_SAwn2UAwn( spex_cd2a.laser_wavenumber ) );
}


/*-----------------------------------------------------------------*
 * Function returns the number of grooves per meter of the grating
 *-----------------------------------------------------------------*/

Var_T *monochromator_groove_density( Var_T *v )
{
	if ( v != NULL )
		print( WARN, "There's only one grating, argument is discarded.\n" );

	return vars_push( FLOAT_VAR, spex_cd2a.grooves );
}


/*-----------------------------------------------------------------------*
 * Function for querying setting the limits for shutters (if installed).
 * To set new limits the function expects two arguments, first the lower
 * limit and the upper limit (where the lower limit must not be a larger
 * number than the upper limit).
 *-----------------------------------------------------------------------*/

Var_T *monochromator_shutter_limits( Var_T *v )
{
	double l[ 2 ];
	double tl[ 2 ];
	double lwl;


	if ( ! spex_cd2a.has_shutter )
	{
		print( FATAL, "Device hasn't a shutter.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( v == NULL )
	{
		if ( ! spex_cd2a.shutter_limits_are_set )
		{
			print( FATAL, "Shutter limits haven't been set yet.\n" );
			SPEX_CD2A_THROW( EXCEPTION );
		}
	}
	else
	{
		l[ 0 ] = get_double( v, "lower shutter limit" );

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			print( FATAL, "Missing upper shutter limit.\n ");
			SPEX_CD2A_THROW( EXCEPTION );
		}

		l[ 1 ] = get_double( v, "uppper shutter limit" );
		
		too_many_arguments( v );

		if ( l[ 0 ] > l[ 1 ] )
		{
			print( FATAL, "Lower shutter limit larger than upper shutter "
				   "limit.\n ");
			SPEX_CD2A_THROW( EXCEPTION );
		}

		TRY
		{
			if ( spex_cd2a.mode & WN_MODES )
			{
				tl[ 0 ] = spex_cd2a_UMwn2Swl( l[ 1 ] );
				tl[ 1 ] = spex_cd2a_UMwn2Swl( l[ 0 ] );
			}
			else
			{
				tl[ 0 ]	= spex_cd2a_Uwl2Swl( l[ 0 ] );
				tl[ 1 ]	= spex_cd2a_Uwl2Swl( l[ 1 ] );
			}

			TRY_SUCCESS;
		}
		CATCH( INVALID_INPUT_EXCEPTION )
		{
			print( FATAL, "Invalid shutter linits (%.4f cm^-1, %.4f cm^-1).\n",
				   l[ 0 ], l[ 1 ] );
			SPEX_CD2A_THROW( EXCEPTION );
		}
		OTHERWISE
			SPEX_CD2A_RETHROW( );

		if ( tl[ 0 ] < spex_cd2a.lower_limit )
		{
			if ( spex_cd2a.mode == WN )
				print( WARN, "Upper shutter limit larger than upper "
					   "wavenumber limit of monochromator.\n" );
			else if ( spex_cd2a.mode == WND )
				print( WARN, "Lower shutter limit lower than lower "
					   "wavenumber limit of monochromator.\n" );
			else
				print( WARN, "Lower shutter limit lower than lower "
					   "wavelength limit of monochromator.\n" );
			tl[ 0 ] = spex_cd2a.lower_limit;
		}

		if ( tl[ 1 ] > spex_cd2a.upper_limit )
		{
			if ( spex_cd2a.mode == WN )
				print( WARN, "Lower shutter limit lower than lower "
					   "wavenumber limit of monochromator.\n" );
			else if ( spex_cd2a.mode == WND )
				print( WARN, "Upper shutter limit larger than upper "
					   "wavenumber limit of monochromator.\n" );
			else
				print( WARN, "Upper shutter limit larger than upper "
					   "wavelength limit of monochromator.\n" );
			tl[ 1 ] = spex_cd2a.upper_limit;
		}

		if ( spex_cd2a.mode == WND )
		{
			lwl = spex_cd2a_Awn2wl( spex_cd2a.laser_wavenumber );
			if ( lwl < tl[ 0 ] || lwl > tl[ 1 ] )
			{
				print( FATAL, "Shutter limits don't cover the laser line "
					   "position.\n" );
				SPEX_CD2A_THROW( EXCEPTION );
			}
		}

		spex_cd2a.shutter_low_limit = tl[ 0 ];
		spex_cd2a.shutter_high_limit = tl[ 1 ];
		spex_cd2a.shutter_limits_are_set = SET;

		spex_cd2a_set_shutter_limits( );
	}

	if ( spex_cd2a.mode & WN_MODES )
	{
		l[ 0 ] = spex_cd2a_Swl2UMwn( spex_cd2a.shutter_high_limit );
		l[ 1 ] = spex_cd2a_Swl2UMwn( spex_cd2a.shutter_low_limit );
	}
	else
	{
		l[ 0 ] = spex_cd2a_Swl2Uwl( spex_cd2a.shutter_low_limit );
		l[ 1 ] = spex_cd2a_Swl2Uwl( spex_cd2a.shutter_high_limit );
	}

	return vars_push( FLOAT_ARR, l, 2 );
}


/*--------------------------------------------------------------------*
 * Function for calibration of the monochromator. It tells the driver
 * 1. about an offset between what the driver reports to the user and
 *    where a line is supposed to be.
 * 2. (optionally) the width of a single pixel on the CCD camera
 *
 * The function is to be called at least with the difference between
 * the expected position and the position where the line really appears
 * according the values reported by the driver. If, for example a sweep
 * is done from 400 nm to 404 nm, with the laser line appearing at 401 nm
 * while it is expected to be at 402 nm the function has to be called
 * with a value of +1 nm to to correct for this offset. Since the wave-
 * lengths or wavenumbers the user is dealing with are already offset-
 * corrected the argument isn't taken to be the offset itself but is
 * subtracted from the existing value of the offset.
 * For wavelength driven monochromators the offset must be given in
 * wavelength units, while for wavenumber driven monochromators in
 * wavenumber units (i.e. cm^-1).
 *
 * If there's another argument it has to be the wavelength or wave-
 * number (depending on the type of the monochromator) difference
 * between two pixels of the CCD camera. If not given the old value
 * is retained.
 *
 * If called without an argument the function returns an array of two
 * values with the offset and the pixel difference. A value of 0 for
 * the pixel difference means that no calibration of this value has
 * been done.
 *--------------------------------------------------------------------*/

Var_T *monochromator_calibrate( Var_T *v )
{
	double pixel_diff;
	double offset;
	double new_offset;
	double field[ 2 ];


	if ( v == NULL )
	{
		field[ 0 ] = spex_cd2a.offset;
		field[ 1 ] = spex_cd2a.pixel_diff;
		return vars_push( FLOAT_ARR, field, 2 );
	}

	/* A new offset can only be set for wavenumber driven monochromators
	   while in absolute wavenumber mode */

	if ( spex_cd2a.mode == WND )
	{
		print( FATAL, "Monochromator offset can't be set while in relative "
			   "wavenumber mode. Set the laser line position to 0 before "
			   "attempting to change the calibration.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES )
		new_offset = get_double( v, "wavenumber offset" );
	else
		new_offset = get_double( v, "wavelength offset" );

	offset = spex_cd2a.offset - new_offset;

#if defined SPEX_CD2A_MAX_OFFSET
	if ( spex_cd2a.mode & WN_MODES && offset > SPEX_CD2A_MAX_OFFSET )
	{
		print( FATAL, "Offset of %.4f cm^-1 is unrealistically high. If this "
			   "isn't an error change the \"SPEX_CD2A_MAX_OFFSET\" setting in "
			   "the device configuration file and recompile.\n", new_offset );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode == WL && fabs( 1.0e9 * offset )
		 											   > SPEX_CD2A_MAX_OFFSET )
	{
		print( FATAL, "Offset of %.5f nm is unrealistically high. If this "
			   "isn't an error change the \"SPEX_CD2A_MAX_OFFSET\" setting in "
			   "the device configuration file and recompile.\n",
			   1.0e9 * new_offset );
		SPEX_CD2A_THROW( EXCEPTION );
	}
#endif

	if ( ( v = vars_pop( v ) ) != NULL )
	{
		if ( spex_cd2a.mode & WN_MODES )
			pixel_diff = get_double( v, "wavelength difference for one CCD "
									 "camera pixel" );
		else
			pixel_diff = get_double( v, "wavenumber difference for one CCD "
									 "camera pixel" );

		if ( pixel_diff < 0.0 )
		{
			print( FATAL, "Invalid negative wave%s difference for one CCD "
				   "camera pixel.\n",
				   spex_cd2a.mode & WN_MODES ? "number" : "length" );
			SPEX_CD2A_THROW( EXCEPTION );
		}
	}
	else
		pixel_diff = spex_cd2a.pixel_diff;

	too_many_arguments( v );

	spex_cd2a.offset = offset;
	spex_cd2a.pixel_diff = pixel_diff;

	field[ 0 ] = spex_cd2a.offset;
	field[ 1 ] = spex_cd2a.pixel_diff;
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

Var_T *monochromator_wavelength_axis( Var_T *v )
{
	double wl = spex_cd2a.wavelength;
	Var_T *cv;
	long num_pixels;
	int acc;


	CLOBBER_PROTECT( wl );

	if ( v != NULL )
	{
		wl = get_double( v, "wavelength" );

		TRY
		{
			wl = spex_cd2a_Uwl2Swl( wl );
			TRY_SUCCESS;
		}
		CATCH( INVALID_INPUT_EXCEPTION )
		{
			print( FATAL, "Invalid center wavelength of %.5f nm.\n",
				   1.0e9 * wl );
			SPEX_CD2A_THROW( EXCEPTION );
		}
		OTHERWISE
			SPEX_CD2A_RETHROW( );
	}
	else if ( ! spex_cd2a.is_wavelength && ! spex_cd2a.scan_is_init )
	{
		print( FATAL, "Wavelength hasn't been set yet.\n ");
		SPEX_CD2A_THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* Check if there's a CCD camera module loaded */

	if ( ! exists_device_type( "ccd camera" ) )
	{
		print( FATAL, "Function can only be used when the module for a "
			   "CCD camera is loaded.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Get the width (in pixels) of the chip of the camera */

	if ( ! func_exists( "ccd_camera_pixel_area" ) )
	{
		print( FATAL, "CCD camera has no function for determining the "
			   "size of the chip.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	cv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );

	if ( cv->type != INT_ARR ||
		 cv->val.lpnt[ 0 ] <= 0 || cv->val.lpnt[ 1 ] <= 0 )
	{
		print( FATAL, "Function of CCD for determining the size of the chip "
			   "does not return a useful value.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	num_pixels = cv->val.lpnt[ 0 ];
	vars_pop( cv );

	cv = vars_push( FLOAT_ARR, NULL, 2 );

	if ( spex_cd2a.mode == WL )
	{
		cv->val.dpnt[ 0 ] = spex_cd2a_Swl2Uwl( wl - 0.5 * ( num_pixels - 1 )
											   * spex_cd2a.pixel_diff );
		cv->val.dpnt[ 1 ] = spex_cd2a.pixel_diff;
	}
	else
	{
		cv->val.dpnt[ 0 ] =	spex_cd2a_SAwn2Uwl( spex_cd2a_wl2Awn( wl )
												+ 0.5 * ( num_pixels - 1 )
												* spex_cd2a.pixel_diff );
		cv->val.dpnt[ 1 ] = ( spex_cd2a_SAwn2Uwl( spex_cd2a_wl2Awn( wl )
												  - 0.5 * ( num_pixels - 1 )
												  * spex_cd2a.pixel_diff )
							  - cv->val.dpnt[ 0 ] ) / ( num_pixels - 1 );
	}

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

Var_T *monochromator_wavenumber_axis( Var_T *v )
{
	double wl = spex_cd2a.wavelength;
	Var_T *cv;
	long num_pixels;
	int acc;


	CLOBBER_PROTECT( wl );

	if ( v != NULL )
	{
		wl = get_double( v, "wavenumber" );

		TRY
		{
			wl = spex_cd2a_UMwn2Swl( wl );
			TRY_SUCCESS;
		}
		CATCH( INVALID_INPUT_EXCEPTION )
		{
			print( FATAL, "Invalid wavenumber of %.4f cm^-1.\n", wl );
			SPEX_CD2A_THROW( EXCEPTION );
		}
		OTHERWISE
			SPEX_CD2A_RETHROW( );
	}
	else if ( ! spex_cd2a.is_wavelength && ! spex_cd2a.scan_is_init )
	{
		print( FATAL, "Wavenumber hasn't been set yet.\n ");
		SPEX_CD2A_THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* Check that we can talk with the camera */

	if ( ! exists_device_type( "ccd camera" ) )
	{
		print( FATAL, "Function can only be used when the module for a "
			   "CCD camera is loaded.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	/* Get the width (in pixels) of the chip of the camera */

	if ( ! func_exists( "ccd_camera_pixel_area" ) )
	{
		print( FATAL, "CCD camera has no function for determining the "
			   "size of the chip.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	cv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );

	if ( cv->type != INT_ARR ||
		 cv->val.lpnt[ 0 ] <= 0 || cv->val.lpnt[ 1 ] <= 0 )
	{
		print( FATAL, "Function of CCD for determining the size of the chip "
			   "does not return a useful value.\n" );
		SPEX_CD2A_THROW( EXCEPTION );
	}

	num_pixels = cv->val.lpnt[ 0 ];
	vars_pop( cv );

	cv = vars_push( FLOAT_ARR, NULL, 2 );

	if ( spex_cd2a.mode == WL )
	{
		cv->val.dpnt[ 0 ] = spex_cd2a_SAwn2Uwl( spex_cd2a_wl2Awn( wl )
												+ 0.5 * ( num_pixels - 1 )
												* spex_cd2a.pixel_diff );
		cv->val.dpnt[ 1 ] = ( spex_cd2a_SAwn2Uwl( spex_cd2a_wl2Awn( wl )
												  - 0.5 * ( num_pixels - 1 )
												  * spex_cd2a.pixel_diff )
							  - cv->val.dpnt[ 0 ] ) / ( num_pixels - 1 );
	}
	else
	{
		cv->val.dpnt[ 0 ] = spex_cd2a_SAwn2UMwn( spex_cd2a_wl2Awn( wl )
												 - 0.5 * ( num_pixels - 1 )
												 * spex_cd2a.pixel_diff );
		cv->val.dpnt[ 1 ] = spex_cd2a.pixel_diff;
		if ( spex_cd2a.mode == WN )
			cv->val.dpnt[ 1 ] *= -1.0;
	}

	return cv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
