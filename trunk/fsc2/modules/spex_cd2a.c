/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "spex_cd2a.h"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

SPEX_CD2A spex_cd2a, spex_cd2a_stored;



/*------------------------------------------------------------*/
/* Function that gets called when the module has been loaded. */
/*------------------------------------------------------------*/

int spex_cd2a_init_hook( void )
{
	/* Claim the serial port (throws exception on failure) */

	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	spex_cd2a.is_needed = SET;
	spex_cd2a.is_open = UNSET;

	/* Determine (from the configuration file) if the monochromator is
	   wave-number or wave-length driven */

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
			THROW( EXCEPTION );
	}

	/* If it's wave-length driven find out the units the controller expects
	   (either nanometer or Angstroem) */

	if ( spex_cd2a.mode == WL )
	{
		spex_cd2a.units = UNITS;

		if ( spex_cd2a.units != NANOMETER && spex_cd2a.units != ANGSTROEM )
		{
			print( FATAL, "Invalid setting for units in configuration file "
				   "for the device.\n" );
			THROW( EXCEPTION );
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
			print( FATAL, "Invalid setting for upper wave-number limit in "
				   "configuration file for the device.\n" );
		else
			print( FATAL, "Invalid setting for lower wave-length limit in "
				   "configuration file for the device.\n" );
		THROW( EXCEPTION );
	}			

	/* Find out which is the highest wavelength that can be set */

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a.lower_limit = spex_cd2a_wn2wl( LOWER_LIMIT );
	else if ( UNITS == NANOMETER )
		spex_cd2a.lower_limit = 1.0e-9 * LOWER_LIMIT;
	else
		spex_cd2a.lower_limit = 1.0e-10 * LOWER_LIMIT;

	if ( UPPER_LIMIT < 0 ||
		 ( spex_cd2a.mode & WN_MODES && UPPER_LIMIT <= 0.0 ) )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Invalid setting for lower wave-number limit in "
				   "configuration file for the device.\n" );
		else
			print( FATAL, "Invalid setting for upper wave-length limit in "
				   "configuration file for the device.\n" );
		THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES )
		spex_cd2a.upper_limit = spex_cd2a_wn2wl( UPPER_LIMIT );
	else if ( UNITS == NANOMETER )
		spex_cd2a.upper_limit = 1.0e-9 * UPPER_LIMIT;
	else
		spex_cd2a.upper_limit = 1.0e-10 * UPPER_LIMIT;

	/* Check consistency of settings for lower and upper limit */

	if ( spex_cd2a.upper_limit <= spex_cd2a.lower_limit )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Lower wave-number limit isn't larger than the "
				   "upper limit in configuration file for the device.\n" );
		else
			print( FATAL, "Upper wave-length limit isn't larger than the "
				   "lower limit in configuration file for the device.\n" );
		THROW( EXCEPTION );
	}

	/* Determine the number of grooves per meter for the installed grating */

	if ( GROOVE_DENSITY <= 0 )
	{
		print( FATAL, "Invalid setting for number of grooves per mm of "
			   "grating in configuration file for the device.\n" );
		THROW( EXCEPTION );
	}

	spex_cd2a.grooves = 1000 * GROOVE_DENSITY;

	if ( STANDARD_GROOVE_DENSITY <= 0 )
	{
		print( FATAL, "Invalid setting for number of grooves per mm of "
			   "standard grating in configuration file for the device.\n" );
		THROW( EXCEPTION );
	}

	/* Determine the number of grooves per meter for the standard grating */

	spex_cd2a.standard_grooves = 1000 * STANDARD_GROOVE_DENSITY;

	/* Get the minimum step size of the motor. It gets calculated from
	   the STEPS_PER_UNIT entry in the configuration file and the ratio
	   of the grooves of the installed and the standard grating. */

	if ( spex_cd2a.mode == WL )
	{
		if ( UNITS == NANOMETER )
			spex_cd2a.mini_step = 10e-9 * spex_cd2a.standard_grooves
								  / ( spex_cd2a.grooves * STEPS_PER_UNIT );
		else
			spex_cd2a.mini_step = 10e-10 * spex_cd2a.standard_grooves
								  / ( spex_cd2a.grooves * STEPS_PER_UNIT );
	}
	else
		spex_cd2a.mini_step = spex_cd2a.grooves /
							  ( spex_cd2a.standard_grooves * STEPS_PER_UNIT );

	if ( spex_cd2a.mini_step <= 0.0 )
	{
		print( FATAL, "Invalid setting for steps per unit with standard "
			   "grating in configuration file for the device.\n" );
		THROW( EXCEPTION );
	}

	/* Also the lower and upper limit must be corrected if the grating isn't
	   the standard grating. In this case it also seems not to be possible
	   to go to the limits, we should stay at least to steps away from them */

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
				spex_cd2a_wn2wl( spex_cd2a_wl2wn( spex_cd2a.lower_limit )
								 - 2 * spex_cd2a.mini_step );
			spex_cd2a.upper_limit =
				spex_cd2a_wn2wl(  spex_cd2a_wl2wn( spex_cd2a.upper_limit )
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
		THROW( EXCEPTION );
	}

	/* Determine if we need to send a checksum with commands */

	spex_cd2a.use_checksum = USE_CHECKSUM != NO_CHECKSUM;

	/* Determine if device sends a linefeed at the end of data */

	spex_cd2a.sends_lf = SENDS_LINEFEED != NO_LINEFEED;

	spex_cd2a.is_wavelength = UNSET;
	spex_cd2a.laser_wavenumber = 0.0;

	spex_cd2a.scan_is_init = UNSET;

	spex_cd2a.use_calib = 0;

	spex_cd2a.shutter_limits_are_set = UNSET;

	return 1;
}


/*-------------------------------------------------------------------------*/
/* Function called (in the parents context) at the start of the experiment */
/*-------------------------------------------------------------------------*/

int spex_cd2a_test_hook( void )
{
	spex_cd2a_stored = spex_cd2a;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int spex_cd2a_exp_hook( void )
{
	spex_cd2a = spex_cd2a_stored;

	/* Open the serial port for the device */

	spex_cd2a_open( );

	/* Test if the device reacts */

	spex_cd2a_init( );

	return 1;
}


/*--------------------------------------------------------------*/
/* Function gets called just before the child process doing the */
/* experiment exits.                                            */
/*--------------------------------------------------------------*/

void spex_cd2a_child_exit_hook( void )
{
	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );
}


/*-----------------------------------------------------------------------*/
/* Function called (in the parents context) at the end of the experiment */
/*-----------------------------------------------------------------------*/

int spex_cd2a_end_of_exp_hook( void )
{
	spex_cd2a_close( );
	return 1;
}


/*----------------------------------------------*/
/* Returns a string with the name of the device */
/*----------------------------------------------*/

Var *monochromator_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-------------------------------------------------------------------------*/
/* Function can be used to either determine the current settings for scans */
/* (by calling it without an argument) or to set new scan parameters. In   */
/* the second case two arguments are required, the start position of the   */
/* scan and the scan step size. If a new scan setup is done while the      */
/* device is already doing a scan (is in scan mode) the currently running  */
/* scan is aborted.                                                        */
/*-------------------------------------------------------------------------*/

Var *monochromator_scan_setup( Var *v )
{
	double start, step;
	long num_steps;
	double vals[ 2 ];


	if ( v == NULL )
	{
		if ( ! spex_cd2a.scan_is_init )
		{
			print( FATAL, "No scan setup has been done.\n" );
			THROW( EXCEPTION );
		}

		if ( spex_cd2a.mode && WN_MODES )
			vals[ 0 ] = spex_cd2a_wl2mwn( spex_cd2a.scan_start );
		else
			vals[ 0 ] = spex_cd2a.scan_start;
		vals[ 1 ] = spex_cd2a.scan_step;

		return vars_push( FLOAT_ARR, vals, 2 );
	}

	if ( v->next == NULL )
	{
		print( FATAL, "Missing scan step size argument.\n" );
		THROW( EXPERIMENT );
	}

	if ( spex_cd2a.mode & WN_MODES )
	{
		start = get_double( v, "start wave-number" );
		if ( spex_cd2a.mode == WND )
			start = spex_cd2a.laser_wavenumber - start;
		start = spex_cd2a_wn2wl( start );
	}
	else
		start = get_double( v, "start wave-length" );

	if ( start <= 0.0 )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Invalid start wave-number of %.4f cm^-1.\n",
				   spex_cd2a_wl2mwn( start ) );
		else
			print( FATAL, "Invalid start wave-length of %.5f nm.\n",
				   1.0e9 * start );
		THROW( EXCEPTION );
	}

	if ( start < spex_cd2a.lower_limit )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Start wave-number of %.4f cm^-1 is higher than "
				   "the upper limit of the device of %.4f cm^-1.\n",
				   spex_cd2a_wl2mwn( start ),
				   spex_cd2a_wl2mwn( spex_cd2a.lower_limit ) );
		else
			print( FATAL, "Start wave-length of %.5f nm is below the lower "
				   "limit of the device of %.5f nm.\n",
				   1.0e9 * start, 1.0e9 * spex_cd2a.lower_limit );
		THROW( EXCEPTION );
	}

	if ( start > spex_cd2a.upper_limit )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Start wave-number of %.4f cm^-1 is below the "
				   "the lower limit of the device of %.4f cm^-1.\n",
				   spex_cd2a_wl2mwn( start ),
				   spex_cd2a_wl2mwn( spex_cd2a.upper_limit ) );
		else
			print( FATAL, "Start wave-length of %.5f nm is higher than the "
				   "upper limit of the device of %.5f nm.\n",
				   1.0e9 * start, 1.0e9 * spex_cd2a.upper_limit );
		THROW( EXCEPTION );
	}

	v = vars_pop( v );

	if ( spex_cd2a.mode & WN_MODES )
		step = get_double( v, "wave-number step width" );
	else
		step = get_double( v, "wave-length step width" );

	/* All SPEX monochromators scan from low to high wavelengths and high
	   to low wavenumbers. */

	if ( spex_cd2a.mode == WL && step <= 0.0 ) 
	{
		print( FATAL, "Invalid step size of %.5f nm, must be positive.\n",
			   10e9 * step );
		THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode & WN_MODES && step >= 0.0 )
	{
		print( FATAL, "Invalid step size of %.4f cm^-1, must be negative.\n",
			   step );
		THROW( EXCEPTION );
	}

	if ( ( spex_cd2a.mode == WL && start + step > spex_cd2a.upper_limit ) ||
		 ( spex_cd2a.mode & WN_MODES &&
		   spex_cd2a_wn2wl( spex_cd2a_wl2wn( start ) + step )
													> spex_cd2a.upper_limit ) )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Absolute value of step size of %.4f cm^-1 is too "
				   "large.\n", - step );
		else
			print( FATAL, "Step size of %.5f nm is too large.\n",
				   10e9 * step );
		THROW( EXCEPTION );
	}

	step = fabs( step );

	if ( step < spex_cd2a.mini_step )
	{
		if ( spex_cd2a.mode & WN_MODES )
			print( FATAL, "Absolute value of step size of %.4f cm^-1 is "
				   "smaller than the minimum possible step size of %.4f "
				   "cm^-1.\n", step, spex_cd2a.mini_step );
		else
			print( FATAL, "Step size of %.5f nm is smaller than the "
				   "minimum possible step size of %.5f.\n",
				   10e9 * step, 10e9 * spex_cd2a.mini_step );
		THROW( EXCEPTION );
	}

	/* Finally make sure the step size is an integer multiple of the minimum
	   step size */

	num_steps = lrnd( step / spex_cd2a.mini_step );

	if ( fabs( num_steps - lrnd( step / spex_cd2a.mini_step ) )
		 > 0.01 )
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
	if ( spex_cd2a.mode && WND )
		spex_cd2a.scan_step *= -1.0;
	spex_cd2a.scan_is_init = SET;

	/* Stop a possibly already running scan, then set the new start position
	   and step size */

	spex_cd2a_halt( );
	if ( FSC2_MODE == EXPERIMENT )
	{
		spex_cd2a_scan_start( );
		spex_cd2a_scan_step( );
	}

	if ( spex_cd2a.mode && WN_MODES )
		vals[ 0 ] = spex_cd2a_wl2mwn( spex_cd2a.scan_start );
	else
		vals[ 0 ] = spex_cd2a.scan_start;
	vals[ 1 ] = spex_cd2a.scan_step;

	return vars_push( FLOAT_ARR, vals, 2 );
}


/*----------------------------------------------------------*/
/* Set or query the current wavelength of the monochromator */
/* (always return the absolute wavelength in m, even when   */
/* in wave-number mode). If the monochromator is currently  */
/* doing scan and a new wavelength is set the currently     */
/* running scan is aborted.                                 */
/*----------------------------------------------------------*/

Var *monochromator_wavelength( Var *v )
{
	double wl;


	if ( v == NULL )
	{
		if ( ! spex_cd2a.is_wavelength && ! spex_cd2a.scan_is_init )
		{
			print( FATAL, "Wavelength hasn't been set yet.\n ");
			THROW( EXCEPTION );
		}
		return vars_push( FLOAT_VAR, spex_cd2a.wavelength );
	}

	wl = get_double( v, "wavelength" );

	if ( wl < 0.0 )
	{
		print( FATAL, "Invalid wave-length of %.5f nm.\n", 1.0e9 * wl );
		THROW( EXCEPTION );
	}

	if ( wl < spex_cd2a.lower_limit )
	{
		print( FATAL, "Requested wave-length of %.5f nm is lower than the "
			   "lower wave-length limit of %.5f nm.\n",
			   1.0e9 * wl, 1.0e9 * spex_cd2a.lower_limit );
		THROW( EXCEPTION );
	}

	if ( wl > spex_cd2a.upper_limit )
	{
		print( FATAL, "Requested wave-length of %.5f nm is larger than the "
			   "upper wave-length limit of %.5f nm.\n",
			   1,0e9 * wl, 1.0e9 * spex_cd2a.upper_limit );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	spex_cd2a.wavelength = wl;
	spex_cd2a.is_wavelength = SET;

	if ( spex_cd2a.in_scan )
		spex_cd2a_halt( );

	spex_cd2a_set_wavelength( );

	return vars_push( FLOAT_VAR, spex_cd2a.wavelength );
}


/*----------------------------------------------------------------------*/
/* Queries the current wave-number (in cm^-1 of the monochromator. When */
/* in wave-number offset mode the difference to the laser line is used. */
/* If the monochromator is currently doing scan and a new wavenumber is */
/* set the currently running scan is aborted.                           */
/*----------------------------------------------------------------------*/

Var *monochromator_wavenumber( Var *v )
{
	double wn, wl;


	if ( v != NULL )
	{
		wn = get_double( v, "wave-number" );

		if ( spex_cd2a.mode == WND )
			wn = spex_cd2a.laser_wavenumber - wn;

		if ( wn <= 0.0 )
		{
			print( FATAL, "Invalid (absolute) wave-number of %.4f cm^-1.\n",
				   wn );
			THROW( EXCEPTION );
		}

		wl = spex_cd2a_wn2wl( wn );

		if ( wl < spex_cd2a.lower_limit )
		{
			print( FATAL, "Requested wave-number of %.4f cm^-1 is larger "
				   "than the upper wave-number limit of %.4f "
				   "cm^-1.\n", spex_cd2a_wl2mwn( wl ),
				   spex_cd2a_wl2mwn( spex_cd2a.lower_limit ) );
			THROW( EXCEPTION );
		}

		if ( wl > spex_cd2a.upper_limit )
		{
			print( FATAL, "Requested wave-number of %.4f cm^-1 is lower than "
				   "the lower wave-number limit of %.4f cm^-1.\n",
				   spex_cd2a_wl2mwn( wl ),
				   spex_cd2a_wl2mwn( spex_cd2a.upper_limit ) );
			THROW( EXCEPTION );
		}

		too_many_arguments( v );

		spex_cd2a.wavelength = wl;
		spex_cd2a.is_wavelength = SET;

		if ( spex_cd2a.in_scan )
			spex_cd2a_halt( );

		spex_cd2a_set_wavelength( );
	}
	else
	{
		if ( ! spex_cd2a.is_wavelength && ! spex_cd2a.scan_is_init )
		{
			print( FATAL, "Wavenumber hasn't been set yet,\n ");
			THROW( EXCEPTION );
		}
	}

	return vars_push( FLOAT_VAR, spex_cd2a_wl2mwn( spex_cd2a.wavelength ) );
}


/*------------------------------------------------------------------*/
/* Function for starting a new scan: the monocromator is driven to  */
/* the start position of the scan. If the monochromator was already */
/* doing a scan the old scan is aborted.                            */
/*------------------------------------------------------------------*/

Var *monochromator_start_scan( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! spex_cd2a.scan_is_init )
	{
		print( FATAL, "Missing scan setup.\n" );
		THROW( EXCEPTION );
	}

	spex_cd2a_start_scan( );

	if ( spex_cd2a.mode & WN_MODES )
		return vars_push( FLOAT_VAR,
						  spex_cd2a_wl2mwn( spex_cd2a.wavelength ) );
	else
		return vars_push( FLOAT_VAR, spex_cd2a.wavelength );
}


/*------------------------------------------------------------*/
/* Function for doing a scan step, requires that a scan setup */
/* has been done and a scan already has been started.         */
/*------------------------------------------------------------*/

Var *monochromator_scan_step( Var *v )
{
	UNUSED_ARGUMENT( v );

	if ( ! spex_cd2a.scan_is_init )
	{
		print( FATAL, "Missing scan setup.\n" );
		THROW( EXCEPTION );
	}

	if ( ! spex_cd2a.in_scan )
	{
		print( FATAL, "No scan has been started yet.\n" );
		THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode == WL &&
		 spex_cd2a.wavelength + spex_cd2a.scan_step > spex_cd2a.upper_limit )
	{
		print( FATAL, "Scan reached upper limit of wave-length range.\n" );
		THROW( EXCEPTION );
	}

	if ( spex_cd2a.mode == WL &&
		 spex_cd2a_wn2wl( spex_cd2a_wl2wn( spex_cd2a.wavelength )
						  + spex_cd2a.scan_step ) > spex_cd2a.upper_limit )
	{
		print( FATAL, "Scan reached lower limit of wave-number range.\n" );
		THROW( EXCEPTION );
	}

	spex_cd2a_trigger( );

	if ( spex_cd2a.mode == WL )
	{
		spex_cd2a.wavelength += spex_cd2a.scan_step;
		return vars_push( FLOAT_VAR, spex_cd2a.wavelength );
	}

	spex_cd2a.wavelength =
					   spex_cd2a_wn2wl( spex_cd2a_wl2wn( spex_cd2a.wavelength )
										+ spex_cd2a.scan_step );
	return vars_push( FLOAT_VAR, spex_cd2a_wl2mwn( spex_cd2a.wavelength ) );
}


/*-------------------------------------------------------------*/
/* Function for setting the laser line position. This can only */
/* be done for monochromators that are wavenumber driven. When */
/* a laser line has been set in the following only relative    */
/* wavenumbers can be used (i.e. differences between the laser */
/* line position and absolute wavenumbers), and all functions  */
/* involving wavenumbers return relative positions. A laser    */
/* line position can switched of (reverting also to use of     */
/* absolute wave-numbers) by setting a zero or negatibe laser  */
/* line position. If called without an argument the function   */
/* returns the current setting of the laser line position      */
/* (always in absolute wavenumbers) or zero if no laser line   */
/* position has been set.                                      */
/*-------------------------------------------------------------*/

Var *monochromator_laser_line( Var *v )
{
	double wn, wl;


	/* Setting a laser line (and thereby switching to wave-number offset
	   mode) only works when the device is in wave-number mode */

	if ( spex_cd2a.mode != WL )
	{
		print( FATAL, "Laser line position can only be used in wave-number "
			   "mode of monochromator.\n" );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
		return vars_push( FLOAT_VAR, spex_cd2a.laser_wavenumber );

	wn = get_double( v, "wave-number of laser line" );

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
		spex_cd2a.laser_wavenumber = wn;

		/* Abort if the monochromator has a shutter, the laser line is within
		   it's range but the laser line is outside of the shutter range. */

		wl = spex_cd2a_wn2wl( wn );

		if ( spex_cd2a.has_shutter && spex_cd2a.shutter_limits_are_set &&
			 wl >= spex_cd2a.lower_limit && wl <= spex_cd2a.upper_limit &&
			 ( wl < spex_cd2a.shutter_low_limit ||
			   wl > spex_cd2a.shutter_high_limit ) )
		{
			print( FATAL, "Laser line position is outside of shutter "
				   "range.\n" );
			THROW( EXCEPTION );
		}
	}

	spex_cd2a_set_laser_line( );

	return vars_push( FLOAT_VAR, spex_cd2a.laser_wavenumber );
}


/*------------------------------------------------------------------*/
/* Function returns the number of grooves per meter of the grating. */
/*------------------------------------------------------------------*/

Var *monochromator_groove_density( Var *v )
{
	if ( v != NULL )
		print( WARN, "There's only one grating, argument is discarded.\n" );

	return vars_push( FLOAT_VAR, spex_cd2a.grooves );
}


/*-----------------------------------------------------------------------*/
/* Function for querying setting the limits for shutters (if installed). */
/* To set new limits the function expects two arguments, first the lower */
/* limit and the upper limit (where the lower limit must not be a larger */
/* number than the upper limit).                                         */
/*-----------------------------------------------------------------------*/

Var *monochromator_shutter_limits( Var *v )
{
	double l[ 2 ];
	double tl[ 2 ];
	double lwl;


	if ( ! spex_cd2a.has_shutter )
	{
		print( FATAL, "Device hasn't a shutter.\n" );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
	{
		if ( ! spex_cd2a.shutter_limits_are_set )
		{
			print( FATAL, "Shutter limits haven't been set yet.\n" );
			THROW( EXCEPTION );
		}
	}
	else
	{
		l[ 0 ] = get_double( v, "lower shutter limit" );

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			print( FATAL, "Missing upper shutter limit.\n ");
			THROW( EXCEPTION );
		}

		l[ 1 ] = get_double( v, "uppper shutter limit" );
		
		too_many_arguments( v );

		if ( l[ 0 ] > l[ 1 ] )
		{
			print( FATAL, "Lower shutter limit larger than upper shutter "
				   "limit.\n ");
			THROW( EXCEPTION );
		}

		if ( spex_cd2a.mode & WND )
		{
			tl[ 0 ] = spex_cd2a_wn2wl( l[ 1 ] );
			tl[ 1 ] = spex_cd2a_wn2wl( l[ 0 ] );
		}
		else if ( spex_cd2a.mode == WND )
		{
			tl[ 0 ] = spex_cd2a_wn2wl( spex_cd2a.laser_wavenumber - l[ 0 ] );
			tl[ 1 ] = spex_cd2a_wn2wl( spex_cd2a.laser_wavenumber - l[ 1 ] );
		}
		else
		{
			tl[ 0 ]	= l[ 0 ];
			tl[ 1 ]	= l[ 1 ];
		}

		if ( tl[ 0 ] < spex_cd2a.lower_limit )
		{
			if ( spex_cd2a.mode == WN )
				print( WARN, "Upper shutter limit larger than upper "
					   "wave-number limit of monochromator.\n" );
			else if ( spex_cd2a.mode == WND )
				print( WARN, "Lower shutter limit lower than lower "
					   "wave-number limit of monochromator.\n" );
			else
				print( WARN, "Lower shutter limit lower than lower "
					   "wave-length limit of monochromator.\n" );
			tl[ 0 ] = spex_cd2a.lower_limit;
		}

		if ( tl[ 1 ] > spex_cd2a.upper_limit )
		{
			if ( spex_cd2a.mode == WN )
				print( WARN, "Lower shutter limit lower than lower "
					   "wave-number limit of monochromator.\n" );
			else if ( spex_cd2a.mode == WND )
				print( WARN, "Upper shutter limit larger than upper "
					   "wave-number limit of monochromator.\n" );
			else
				print( WARN, "Upper shutter limit larger than upper "
					   "wave-length limit of monochromator.\n" );
			tl[ 1 ] = spex_cd2a.upper_limit;
		}

		if ( spex_cd2a.mode == WND )
		{
			lwl = spex_cd2a_wn2wl( spex_cd2a.laser_wavenumber );
			if ( lwl < tl[ 0 ] || lwl > tl[ 1 ] )
			{
				print( FATAL, "New shutter limits do not cover the laser "
					   "line position.\n" );
				THROW( EXCEPTION );
			}
		}

		spex_cd2a.shutter_low_limit = tl[ 0 ];
		spex_cd2a.shutter_high_limit = tl[ 1 ];
		spex_cd2a.shutter_limits_are_set = SET;

		spex_cd2a_set_shutter_limits( );
	}

	if ( spex_cd2a.mode == WN )
	{
		l[ 0 ] = spex_cd2a_wl2wn( spex_cd2a.shutter_high_limit );
		l[ 1 ] = spex_cd2a_wl2wn( spex_cd2a.shutter_low_limit );
	}
	else if ( spex_cd2a.mode == WND )
	{
		l[ 0 ] = spex_cd2a_wl2mwn( spex_cd2a.shutter_low_limit );
		l[ 1 ] = spex_cd2a_wl2mwn( spex_cd2a.shutter_high_limit );
	}
	else
	{
		l[ 0 ] = spex_cd2a.shutter_low_limit;
		l[ 1 ] = spex_cd2a.shutter_high_limit;
	}

	return vars_push( FLOAT_ARR, l, 2 );
}



/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
