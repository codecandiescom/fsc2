/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2004 Jens Thoms Toerring
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


#include "spectrapro_300i.h"


/*--------------------------------*/
/* global variables of the module */
/*--------------------------------*/

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


struct SPECTRAPRO_300I spectrapro_300i;



static void spectrapro_300i_arr_conv( long gn, double cwl, long num_pixels,
									  double pixel_width,
									  Var_T *src, Var_T *dest );
static double spectrapro_300i_conv( long gn, double cwl, long num_pixels,
									double pixel_width, double px );


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int spectrapro_300i_init_hook( void )
{
	int i;


	/* Claim the serial port (throws exception on failure) */

	fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );

	spectrapro_300i.is_needed = SET;
	spectrapro_300i.is_open = UNSET;

	for ( i = 0; i < MAX_GRATINGS; i++ )
	{
		spectrapro_300i.grating[ i ].is_installed = SET;
		spectrapro_300i.grating[ i ].grooves = 1200000;
		spectrapro_300i.grating[ i ].blaze = 7.5e-7;
		spectrapro_300i.grating[ i ].init_offset = 0.0;
		spectrapro_300i.grating[ i ].init_adjust = 0.0;
		spectrapro_300i.grating[ i ].used_in_test = UNSET;
		spectrapro_300i.grating[ i ].installed_in_test = UNSET;
	}

	spectrapro_300i.tn = 0;
	spectrapro_300i.current_gn = 0;
	spectrapro_300i.wavelength = 5.0e-7;
	spectrapro_300i.is_wavelength = UNSET;
	spectrapro_300i.use_calib = 0;

	return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int spectrapro_300i_exp_hook( void )
{
	int i;


	if ( ! spectrapro_300i.is_needed )
		return 1;

	/* We don't know yet which gratings are installed */

	for ( i = 0; i < MAX_GRATINGS; i++ )
		spectrapro_300i.grating[ i ].is_installed = SET;

	spectrapro_300i_open( );

	/* Check that gratings used in the test run is installed (or at least
	   the function for installing the grating was called) */

	for ( i = 0; i < MAX_GRATINGS; i++ )
		if ( spectrapro_300i.grating[ i ].used_in_test &&
			 ! spectrapro_300i.grating[ i ].is_installed  &&
			 ! spectrapro_300i.grating[ i ].installed_in_test )
		{
			print( FATAL, "Found during test run that grating #%ld is used "
				   "which isn't installed.\n", i + 1 );
			THROW( EXCEPTION );
		}

	/* If a wavelength was set during the PREPARATIONS section set it now */

	if ( spectrapro_300i.is_wavelength )
		spectrapro_300i_set_wavelength( spectrapro_300i.wavelength );

	/* No calibration file has been read yet */

	spectrapro_300i.use_calib = 0;

	return 1;
}


/*-----------------------------------------------------------------------*
 *-----------------------------------------------------------------------*/

int spectrapro_300i_end_of_exp_hook( void )
{
	if ( ! spectrapro_300i.is_needed || ! spectrapro_300i.is_open )
		return 1;

	spectrapro_300i_close( );

	return 1;
}


/*----------------------------------------------*
 * Returns a string with the name of the device
 *----------------------------------------------*/

Var_T *monochromator_name( UNUSED_ARG Var_T *v )
{
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------------------------*
 * Function for setting or quering the currently used grating
 *------------------------------------------------------------*/

Var_T *monochromator_grating( Var_T *v )
{
	long grating;
	long gn;


	if ( v == 0 )
		return vars_push( INT_VAR, spectrapro_300i.current_gn + 1 );

	grating = get_strict_long( v, "grating number" );

	if ( grating < 1 || grating > MAX_GRATINGS )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid grating number, valid range is 1 to %ld.\n",
				   MAX_GRATINGS );
			THROW( EXCEPTION );
		}

		print( SEVERE,  "Invalid grating number, keeping grating #%ld\n",
			   spectrapro_300i.current_gn + 1 );
		return vars_push( INT_VAR, spectrapro_300i.current_gn + 1 );
	}

	gn = grating - 1;

	if ( FSC2_MODE == TEST )
		spectrapro_300i.grating[ gn ].used_in_test = SET;

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ! spectrapro_300i.grating[ gn ].is_installed )
		{
			print( SEVERE,  "No grating #%ld is installed, keeping grating "
				   "#%ld\n", grating, spectrapro_300i.current_gn + 1 );
			return vars_push( INT_VAR, spectrapro_300i.current_gn + 1 );
		}

		if ( gn - spectrapro_300i.tn * 3 < 0 ||
			 gn - spectrapro_300i.tn * 3 > 2 )
		{
			print( FATAL, "Can't switch to grating #ld while turret #ld is "
				   "in use.\n", grating, spectrapro_300i.tn + 1 );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_grating( gn );

	spectrapro_300i.current_gn = gn;

	return vars_push( INT_VAR, grating );
}


/*----------------------------------------------------------------------*
 * Function for setting or quering the current turret. If the turret is
 * changed also the grating is set to the one at the same position on
 * the new turret (if this is installed) or to one of the gratings
 * installed on the new turret. If there is no grating installed for
 * the new turret the function will return an error.
 *----------------------------------------------------------------------*/

Var_T *monochromator_turret( Var_T *v )
{
	long turret;
	long tn;
	long new_gn;
	long i;


	if ( v == 0 )
		return vars_push( INT_VAR, spectrapro_300i.tn + 1 );

	turret = get_strict_long( v, "turret number" );

	if ( turret < 1 || turret > MAX_TURRETS )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid turret number, valid range is 1 to %ld.\n",
				   MAX_TURRETS );
			THROW( EXCEPTION );
		}

		print( SEVERE,  "Invalid turret number, keeping turret #%ld\n",
			   spectrapro_300i.tn + 1 );
		return vars_push( INT_VAR, spectrapro_300i.tn + 1 );
	}

	tn = turret - 1;

	/* During the test run we don't have to do anything */

	if ( FSC2_MODE != EXPERIMENT )
	{
		spectrapro_300i.tn = tn;
		return vars_push( INT_VAR, turret );
	}

	/* Same if the new turret is identical to the current turret */

	if ( tn == spectrapro_300i.tn )
		return vars_push( INT_VAR, turret );

	/* Figure out the new grating to use */

	new_gn = spectrapro_300i.current_gn + 3 * ( tn - spectrapro_300i.tn );

	if ( ! spectrapro_300i.grating[ new_gn ].is_installed )
	{
		for ( i = 3 * tn; i < 3 * ( tn + 1 ); i++ )
			if ( spectrapro_300i.grating[ i ].is_installed )
				break;

		if ( i >= 3 * ( tn + 1 ) )
		{
			print( FATAL, "Can't switch to turret #%ld, no grating installed "
				   "for this turret.\n", turret );
			THROW( EXCEPTION );
		}

		new_gn = i;
	}

	spectrapro_300i_set_turret( tn );
	spectrapro_300i.tn = tn;

	spectrapro_300i_set_grating( new_gn );
	spectrapro_300i.current_gn = new_gn;	

	return vars_push( INT_VAR, turret );
}


/*-----------------------------------------------------------------*
 * Function either returns the currently set wavelength (if called
 * with no arguments) or moves the grating to a new center wave-
 * length as specified (in meters) by the only argument.
 *-----------------------------------------------------------------*/

Var_T *monochromator_wavelength( Var_T *v )
{
	double wl;


	CLOBBER_PROTECT( wl );

	if ( v == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			spectrapro_300i.wavelength = spectrapro_300i_get_wavelength( );
		return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
	}

	wl = get_double( v, "wavelength" );

	if ( wl < 0.0 )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid negative wavelength.\n" );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Invalid negative wavelength, using 0 nm instead.\n" );
		wl = 0.0;
	}

	/* Check that the wavelength isn't too large (also dealing with rounding
	   errors) */

	if ( wl > MAX_WAVELENGTH + 1.0e-12 )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Wavelength of %.3f nm is too large, maximum "
				   "wavelength is %.3f nm.\n",
				   wl * 1.0e9, MAX_WAVELENGTH * 1.0e9 );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Wavelength of %.3f nm is too large, using %.3f nm "
			   "instead.\n", wl * 1.0e9, MAX_WAVELENGTH * 1.0e9 );
		wl = MAX_WAVELENGTH;
	}

	if ( wl > MAX_WAVELENGTH )
		wl = MAX_WAVELENGTH;

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_wavelength( wl );

	spectrapro_300i.wavelength = wl;
	if ( FSC2_MODE == PREPARATION )
		spectrapro_300i.is_wavelength = SET;

	return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
}


/*-----------------------------------------------------------------*
 * Function either returns the currently set wavenumber (if called
 * with no arguments) or moves the grating to a new center wave-
 * number as specified (in cm^-1) by the only argument.
 *-----------------------------------------------------------------*/

Var_T *monochromator_wavenumber( Var_T *v )
{
	double wl, wn;


	CLOBBER_PROTECT( wl );

	if ( v == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			spectrapro_300i.wavelength = spectrapro_300i_get_wavelength( );
		return vars_push( FLOAT_VAR,
						 spectrapro_300i_wl2wn( spectrapro_300i.wavelength ) );
	}

	wn = get_double( v, "wavenumber" );

	if ( wn <= 0.0 )
	{
		if ( FSC2_MODE == TEST )
		{
			print( FATAL, "Invalid zero or negative wavenumber.\n" );
			THROW( EXCEPTION );
		}

		print( SEVERE, "Invalid zero or negative wavenumber, using %lf cm^-1 "
			   "instead.\n", spectrapro_300i_wl2wn( MAX_WAVELENGTH ) );
		wl = MAX_WAVELENGTH;
	}
	else
		wl = spectrapro_300i_wn2wl( wn );

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_wavelength( wl );

	spectrapro_300i.wavelength = wl;
	if ( FSC2_MODE == PREPARATION )
		spectrapro_300i.is_wavelength = SET;

	return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
}


/*--------------------------------------------------------------------*
 * Function for installing or de-installing a grating. First argument
 * must be a valid grating number. The second argument is either the
 * string "UNINSTALL" to de-install a grating (in which case the
 * grating number must be the number of an already installed grating)
 * or a string with the part number of the grating, i.e. something
 * like "1-120-750". A part number consists of a single digit, a
 * hyphen, a three-digit number for the grooves density, another
 * hyphen and either a number for the blaze wavelength or a string
 * consisting of letters. The string may have not conssist of more
 * than 10 characters. Of course, to install a grating there can't be
 * already a grating installed at the same position.
 *--------------------------------------------------------------------*/

Var_T *monochromator_install_grating( Var_T *v )
{
	long grating;
	long gn;


	grating = get_strict_long( v->next, "grating position" );

	if ( grating < 1 || grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating position, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	gn = grating - 1;

	if ( FSC2_MODE == EXPERIMENT && ! strcmp( v->val.sptr, "UNINSTALL" ) &&
		 ! spectrapro_300i.grating[ gn ].is_installed )
	{
		print( SEVERE,  "Grating #%ld is not installed, can't uninstall "
			   "it.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
	{
		if ( ! strcmp( v->val.sptr, "UNINSTALL" ) )
			spectrapro_300i.grating[ gn ].used_in_test = SET;
		else
			spectrapro_300i.grating[ gn ].installed_in_test = SET;
	}

	v = vars_pop( v );

	if ( v->type != STR_VAR )
	{
		print( FATAL, "First variable must be a string with the part "
			   "number, e.g. \"1-120-750\", or \"UNINSTALL\".\n" );
		THROW( EXCEPTION );
	}

	/* Do some minimal checks on the part number */

	if ( strcmp( v->val.sptr, "UNINSTALL" ) &&
		 ( ! isdigit( v->val.sptr[ 0 ] ) ||
		   v->val.sptr[ 0 ] != '-'       ||
		   ! isdigit( v->val.sptr[ 2 ] ) ||
		   ! isdigit( v->val.sptr[ 3 ] ) ||
		   ! isdigit( v->val.sptr[ 4 ] ) ||
		   ! v->val.sptr[ 5 ] != '-' || 
		   strlen( v->val.sptr ) > 10 ) )
	{
		print( FATAL, "First argument doesn't look like a valid part "
			   "number.\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ! strcmp( v->val.sptr, "UNINSTALL" ) )
		{
			spectrapro_300i_uninstall_grating( gn );
			spectrapro_300i.grating[ gn ].is_installed = UNSET;
		}
		else
		{
			spectrapro_300i_install_grating( gn, v->val.sptr );
			spectrapro_300i.grating[ gn ].is_installed = SET;
		}
	}

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------*
 * Function returns the number of grooves per meter of a grating.
 * The only argument must be the number of an installed grating.
 *----------------------------------------------------------------*/

Var_T *monochromator_groove_density( Var_T *v )
{
	long gn;


	if ( v == NULL )
		gn = spectrapro_300i.current_gn;
	else
	{
		gn = get_strict_long( v, "grating number" ) - 1;

		if ( gn < 0 || gn >= MAX_GRATINGS )
		{
			print( FATAL, "Invalid grating number, must be in range between "
				   "1 and %d.\n", MAX_GRATINGS );
			THROW( EXCEPTION );
		}

		too_many_arguments( v );
	}
	
	if ( ! spectrapro_300i.grating[ gn ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", gn + 1 );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		spectrapro_300i.grating[ gn ].used_in_test = SET;

	return vars_push( FLOAT_VAR,
					  ( double ) spectrapro_300i.grating[ gn ].grooves );
}


/*----------------------------------------------------------------------*
 * The function loads a calibration for the monochromator from a file.
 * If no argument is passed to the function the tries to open a default
 * calibration file with a name that can be set via the definition of
 * "DEFAULT_CALIB_FILE" in the configuration file for the device. If
 * this file name is not an absolute path (i.e. starts with a slash)
 * name of the directory where also the library for the monochromator
 * resides is prepended.
 * If there is an argument this must be a string with the name of the
 * calibration file.
 * A calibration file may contain entries for one or more gratings. For
 * each grating the offset, inclusion angle, focal length and detector
 * angle must be defined. Take a luck at the default calibration file
 * for the details of the syntax.
 *----------------------------------------------------------------------*/

Var_T *monochromator_load_calibration( Var_T * v )
{
	char *calib_file = NULL;
	FILE *cfp = NULL;


	CLOBBER_PROTECT( cfp );
	CLOBBER_PROTECT( calib_file );

	if ( v == NULL )
	{
		if ( DEFAULT_CALIB_FILE[ 0 ] ==  '/' )
			calib_file = T_strdup( DEFAULT_CALIB_FILE );
		else
			calib_file = get_string( "%s%s%s", libdir, slash( libdir ),
									 DEFAULT_CALIB_FILE );

		TRY
		{
			cfp = spectrapro_300i_open_calib( calib_file );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( calib_file );
			RETHROW( );
		}
	}
	else
	{
		vars_check( v, STR_VAR );

		calib_file = T_strdup( v->val.sptr );

		too_many_arguments( v );

		TRY
		{
			cfp = spectrapro_300i_find_calib( calib_file );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( calib_file );
			RETHROW( );
		}
	}

	TRY
	{
		spectrapro_300i_read_calib( cfp, calib_file );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fclose( cfp );
		T_free( calib_file );
		RETHROW( );
	}

	fclose( cfp );
	T_free( calib_file );

	spectrapro_300i.use_calib = SET;

	return vars_push( INT_VAR, 1L );
}


/*----------------------------------------------------------------------*
 * Function returns an array of two wavelength values that are suitable
 * for use as axis description parameters (start of axis and increment)
 * required by by the change_scale() function (if the camera uses
 * binning the second element may have to be multiplied by the
 * x-binning width). Please note: since the axis is not really
 * completely linear, the axis that gets displyed when using these
 * values is not 100% correct!
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavelength_axis( Var_T *v )
{
	Var_T *cv;
	double pixel_width;
	long num_pixels;
	long gn;
	double wl;
	double wl_low;
	double wl_hi;
	int acc;


	/* Check if there's a CCD camera module loaded */

	if ( ! exists_device_type( "ccd camera" ) )
	{
		print( FATAL, "Function can only be used when the module for a "
			   "CCD camera is loaded.\n" );
		THROW( EXCEPTION );
	}

	/* Get the width (in pixels) of the chip of the camera */

	if ( ! func_exists( "ccd_camera_pixel_area" ) )
	{
		print( FATAL, "CCD camera has no function for determining the "
			   "size of the chip.\n" );
		THROW( EXCEPTION );
	}

	cv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );

	if ( cv->type != INT_ARR ||
		 cv->val.lpnt[ 0 ] <= 0 || cv->val.lpnt[ 1 ] <= 0 )
	{
		print( FATAL, "Function of CCD for determining the size of the chip "
			   "does not return a useful value.\n" );
		THROW( EXCEPTION );
	}

	num_pixels = cv->val.lpnt[ 0 ];
	vars_pop( cv );

	/* Get the pixel size of the camera */

	if ( ! func_exists( "ccd_camera_pixel_size" ) )
	{
		print( FATAL, "CCD camera has no function for determining the "
			   "pixel size.\n" );
		THROW( EXCEPTION );
	}

	cv = func_call( func_get( "ccd_camera_pixel_size", &acc ) );

	if ( cv->type != FLOAT_VAR || cv->val.dval <= 0.0 )
	{
		print( FATAL, "Function of CCD for determining the pixel size "
			   "does not return a useful value.\n" );
		THROW( EXCEPTION );
	}

	pixel_width = cv->val.dval;
	vars_pop( cv );

	/* If no calibration file has been read or there is no (complete)
	   calibration for the current grating we just return values for
	   a pixel count scale */

	if ( ! spectrapro_300i.use_calib )
	{
		cv = vars_push( FLOAT_ARR, NULL, 2 );
		cv->val.dpnt[ 0 ] = - 0.5 * ( double ) ( num_pixels - 1 );
		cv->val.dpnt[ 1 ] = 1;
		return cv;
	}

	if ( v != NULL )
	{
		gn = get_strict_long( v, "grating number" ) - 1;
		if ( gn < 0 || gn >= MAX_GRATINGS )
		{
			print( FATAL, "Invalid grating #%ld.\n", gn + 1 );
			THROW( EXCEPTION );
		}
	}
	else
		gn = spectrapro_300i.current_gn;

	if ( ! spectrapro_300i.grating[ gn ].is_calib )
	{
		print( SEVERE, "No (complete) calibration for grating #%ld "
			   "found.\n", gn + 1 );
		cv = vars_push( FLOAT_ARR, NULL, 2 );
		cv->val.dpnt[ 0 ] = - 0.5 * ( double ) ( num_pixels - 1 );
		cv->val.dpnt[ 1 ] = 1;
		return cv;
	}

	if ( ( v = vars_pop( v ) ) )
	{
		wl = get_double( v, "center wavelength" );
		if ( wl < 0.0 )
		{
			print( FATAL, "Invalid negative center wavelength.\n" );
			THROW( EXCEPTION );
		}
	}
	else
		wl = spectrapro_300i.wavelength;

	too_many_arguments( v );

	wl_low = spectrapro_300i_conv( gn, wl, num_pixels, pixel_width, 1 );
	wl_hi = spectrapro_300i_conv( gn, wl, num_pixels, pixel_width,
								  num_pixels );

	cv = vars_push( FLOAT_ARR, NULL, 2 );
	cv->val.dpnt[ 1 ] = ( wl_hi - wl_low ) / ( num_pixels - 1 );
	cv->val.dpnt[ 0 ] = wl - 0.5 * cv->val.dpnt[ 1 ] * ( num_pixels - 1 );

	return cv;
}


/*----------------------------------------------------------------------*
 * Function returns an array of two wavenumber values that are suitable
 * for use as axis description parameters (start of axis and increment)
 * required by by the change_scale() function (if the camera uses
 * binning the second element may have to be multiplied by the
 * x-binning width). Please note: since the axis is not really
 * completely linear, the axis that gets displyed when using these
 * values is not 100% correct!
 *----------------------------------------------------------------------*/

Var_T *monochromator_wavenumber_axis( Var_T * v )
{
	Var_T *cv;
	Var_T *fv;
	double wn;
	int acc;
	long num_pixels;


	if ( v != NULL )
	{
		wn = get_double( v, "center wavenumber" );
		if ( wn <= 0.0 )
		{
			print( FATAL, "Invalid zero or negative center wavenumber\n" );
			THROW( EXCEPTION );
		}
		too_many_arguments( v );
		v = vars_push( FLOAT_VAR, 0.01 / wn );
	}

	cv = monochromator_wavelength_axis( v );

	fv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );
	num_pixels = fv->val.lpnt[ 0 ];
	vars_pop( fv );

	cv->val.dpnt[ 1 ] =
				(   0.01 / (   cv->val.dpnt[ 0 ]
							 + 0.5 * ( num_pixels - 1 ) * cv->val.dpnt[ 1 ] )
				  - 0.01 / (   cv->val.dpnt[ 0 ]
							 - 0.5 * ( num_pixels - 1 ) * cv->val.dpnt[ 1 ] ) )
				/ ( num_pixels - 1 );

	cv->val.dpnt[ 0 ] = 0.01 / cv->val.dpnt[ 0 ];

	return cv;
}


/*------------------------------------------------------------------------*
 * Function allows to calculate the exact wavelength for a pixel position
 * on the CCD cameras chip. It takes up to three arguments. The first
 * argument is either a single value or an array of pixel positions for
 * which the corresponding wavelength is to be calculated. The second,
 * optional argument is the grating used and the third, optional argument
 * is the center wavelength. If the center wavelength or both optional
 * arguments are not specified the currently set values are used.
 *------------------------------------------------------------------------*/

Var_T *monochromator_calc_wavelength( Var_T *v )
{
	long gn;
	double cwl;
	Var_T *cv;
	long num_pixels;
	double pixel_width;
	ssize_t i;
	int acc;


	if ( v == NULL )
	{
		print( FATAL, "Missing argument(s).\n" );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR |
				   INT_REF | FLOAT_REF );

	if ( v->type & ( INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF ) &&
		 v->len == 0 )
	{
		print( FATAL, "Array of pixel positions has no defined size.\n ");
		THROW( EXCEPTION );
	}

	if ( v->next != NULL )
	{
		gn = get_strict_long( v, "grating number" ) - 1;
		if ( gn < 0 || gn >= MAX_GRATINGS )
		{
			print( FATAL, "Invalid grating #%ld.\n", gn + 1 );
			THROW( EXCEPTION );
		}
	}
	else
		gn = spectrapro_300i.current_gn;
		
	if ( ! spectrapro_300i.grating[ gn ].is_calib )
	{
		print( SEVERE, "No (complete) calibration for current grating #%ld "
			   "found.\n", gn + 1 );
		switch( v->type )
		{
			case INT_VAR :
				return vars_push( FLOAT_VAR, ( double ) v->val.lval );

			case FLOAT_VAR :
				return vars_push( FLOAT_VAR, v->val.lval );

			case INT_ARR :
				cv = vars_push( FLOAT_ARR, NULL, v->len );
				for ( i = 0; i < v->len; i++ )
					cv->val.dpnt[ i ] = ( double ) v->val.lpnt[ i ];
				return cv;

			case FLOAT_ARR :
				return vars_push( FLOAT_ARR, v->val.dpnt, v->len );

			case INT_REF : case FLOAT_REF :
				return vars_push( FLOAT_REF, v );

			default :
				break;
		}
	}

	if ( v->next != NULL && v->next->next != NULL )
	{  
		cwl = get_double( v->next->next, "center wavelength" );
		if ( cwl < 0.0 )
		{
			print( FATAL, "Invalid negative center wavelength.\n" );
			THROW( EXCEPTION );
		}

		too_many_arguments( v );
	}
	else
		cwl = spectrapro_300i.wavelength;

	/* Get the width (in pixels) of the chip of the camera */

	if ( ! func_exists( "ccd_camera_pixel_area" ) )
	{
		print( FATAL, "CCD camera has no function for determining the "
			   "size of the chip.\n" );
		THROW( EXCEPTION );
	}

	cv = func_call( func_get( "ccd_camera_pixel_area", &acc ) );

	if ( cv->type != INT_ARR ||
		 cv->val.lpnt[ 0 ] <= 0 || cv->val.lpnt[ 1 ] <= 0 )
	{
		print( FATAL, "Function of CCD for determining the size of the chip "
			   "does not return a useful value.\n" );
		THROW( EXCEPTION );
	}

	num_pixels = cv->val.lpnt[ 0 ];
	vars_pop( cv );

	/* Get the pixel size of the camera */

	if ( ! func_exists( "ccd_camera_pixel_size" ) )
	{
		print( FATAL, "CCD camera has no function for determining the "
			   "pixel size.\n" );
		THROW( EXCEPTION );
	}

	cv = func_call( func_get( "ccd_camera_pixel_size", &acc ) );

	if ( cv->type != FLOAT_VAR || cv->val.dval <= 0.0 )
	{
		print( FATAL, "Function of CCD for determining the pixel size "
			   "does not return a useful value.\n" );
		THROW( EXCEPTION );
	}

	pixel_width = cv->val.dval;
	vars_pop( cv );

	if ( v->type & ( INT_VAR | FLOAT_VAR ) )
		return vars_push( FLOAT_VAR,
						  spectrapro_300i_conv( gn, cwl, num_pixels,
												pixel_width, VALUE( v ) ) );

	if ( v->type & ( INT_ARR | FLOAT_ARR ) )
		cv = vars_push( FLOAT_ARR, NULL, v->len );
	else
		cv = vars_push( FLOAT_REF, v );

	spectrapro_300i_arr_conv( gn, cwl, num_pixels, pixel_width, v, cv );

	return cv;
}


/*---------------------------------------------------------------*
 * Function converts the indices (relative to the left hand edge
 * of the CCD chip, where the index starts of with 1)  stored in
 * array 'src' to the wavelength at that positions, writing them
 * into array 'dest'.
 *---------------------------------------------------------------*/

static void spectrapro_300i_arr_conv( long gn, double cwl, long num_pixels,
									  double pixel_width, Var_T *src,
									  Var_T *dest )
{
	ssize_t i;
	double px;


	fsc2_assert( src->type & ( INT_ARR | FLOAT_ARR | INT_REF | FLOAT_REF ) );
	fsc2_assert( dest->type & ( FLOAT_ARR | FLOAT_REF ) );

	if ( dest->type == FLOAT_ARR )
	{
		fsc2_assert( src->type & ( INT_ARR | FLOAT_ARR ) );

		for ( i = 0; i < src->len; i++ )
		{
			if ( src->type == INT_ARR )
				px = ( double ) src->val.lpnt[ i ];
			else
				px = src->val.dpnt[ i ];
			dest->val.dpnt[ i ] = spectrapro_300i_conv( gn, cwl, num_pixels,
														pixel_width, px );
		}
		return;
	}

	fsc2_assert( src->type & ( INT_REF | FLOAT_REF ) );

	for ( i = 0; i < src->len; i++ )
		spectrapro_300i_arr_conv( gn, cwl, num_pixels, pixel_width,
								  src, dest );
}


/*--------------------------------------------------------------------*
 * Function takes an index (relative to the left edge of the CCD chip
 * and starting there with 1) into the wavelength at that position.
 *--------------------------------------------------------------------*/

static double spectrapro_300i_conv( long gn, double cwl, long num_pixels,
									double pixel_width, double px )
{
	double inclusion_angle_2;
	double focal_length;
	double detector_angle;
	double grating_angle;
	double theta;
	double x;


	if ( px < 1.0 )
	{
		print( SEVERE, "Pixel position too small, replacing by 1.\n" );
		px = 1.0;
	}

	if ( px > num_pixels )
	{
		print( SEVERE, "Pixel position too large, replacing by %ld.\n",
			   num_pixels );
		px = ( double ) num_pixels;
	}

	inclusion_angle_2 = 0.5 * spectrapro_300i.grating[ gn ].inclusion_angle;
	focal_length = spectrapro_300i.grating[ gn ].focal_length;
	detector_angle = spectrapro_300i.grating[ gn ].detector_angle;

	grating_angle = asin( 0.5 * cwl * spectrapro_300i.grating[ gn ].grooves
						  / cos( inclusion_angle_2 ) );

	x = pixel_width * ( px - 0.5 * ( double ) ( num_pixels + 1 ) );
	theta = atan(   x * cos( detector_angle )
				  / ( focal_length + x * sin( detector_angle ) ) );
	return (   sin( grating_angle - inclusion_angle_2 )
			 + sin( grating_angle + inclusion_angle_2 + theta ) )
		   / spectrapro_300i.grating[ gn ].grooves;
}


/*------------------------------------------------------------------*
 * Function allows to set a new calibration for one of the gratings
 * manually, i.e. instead of reading them from a file. It expects
 * five arguments:
 * 1. grating number (grating must be installed)
 * 2. init offset value (zero position of grating, within [-1,1])
 * 3. init adjust value (rotation speed of grating, within [-1,1])
 * 4. inclusion angle (in degree)
 * 5. focal length (in meters)
 * 6. detector angle (in degree)
 *------------------------------------------------------------------*/

Var_T *monochromator_set_calibration( Var_T *v )
{
	long grating;
	long gn;
	double inclusion_angle;
	double focal_length;
	double detector_angle;
	int i;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	grating = get_strict_long( v, "grating number" );

	if ( grating < 1 || grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	gn = grating - 1;

	if ( ! spectrapro_300i.grating[ gn ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		spectrapro_300i.grating[ gn ].used_in_test = SET;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	if ( v->type == STR_VAR )
	{
		if ( strcasecmp( v->val.sptr, "DELETE" ) )
		{
			print( FATAL, "Invalid second argument.\n" );
			THROW( EXCEPTION );
		}
		
		too_many_arguments( v );

		spectrapro_300i.grating[ gn ].is_calib = UNSET;

		spectrapro_300i.use_calib = UNSET;
		for ( i = 0; i < MAX_GRATINGS; i++ )
			if ( spectrapro_300i.grating[ gn ].is_calib )
				spectrapro_300i.use_calib = SET;

		return vars_push( INT_VAR, 1 );
	}

	inclusion_angle = get_double( v, "inclusion angle" );
	if ( inclusion_angle <= 0.0 )
	{
		print( FATAL, "Invalid zero or negative inclusion angle.\n" );
		THROW( EXCEPTION );
	}
	inclusion_angle *= atan( 1.0 ) / 45.0;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	focal_length = get_double( v, "focal length" );
	if ( focal_length <= 0.0 )
	{
		print( FATAL, "Invalid focal_length.\n" );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}
	
	detector_angle = get_double( v, "detector angle" );
	detector_angle *= atan( 1.0 ) / 45.0;

	too_many_arguments( v );

	spectrapro_300i.grating[ gn	].inclusion_angle = inclusion_angle;
	spectrapro_300i.grating[ gn	].focal_length    = focal_length;
	spectrapro_300i.grating[ gn	].detector_angle  = detector_angle;
	spectrapro_300i.grating[ gn	].is_calib        = SET;
	spectrapro_300i.use_calib = SET;

	return vars_push( INT_VAR, 1 );
}


/*------------------------------------------------------------------------*
 * The function either returns or sets the value of the zero angle offset
 * for one of the gratings. The actual value is in the interval [-1,1],
 * representing the range of of deviations that can be set (there is no
 * obvious relation between this value and the offset angle).
 * This function should only be used during the calibration of the
 * monochromator.
 *------------------------------------------------------------------------*/

Var_T *monochromator_zero_offset( Var_T *v )
{
	long grating;
	long gn;
	double offset;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	grating = get_strict_long( v, "grating number" );

	if ( grating < 1 || grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	gn = grating - 1;

	if ( ! spectrapro_300i.grating[ gn ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		spectrapro_300i.grating[ gn ].used_in_test = SET;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			spectrapro_300i.grating[ gn ].init_offset =
				( spectrapro_300i_get_offset( gn )
				  - ( gn % 3 ) * INIT_OFFSET )
				* ( double ) spectrapro_300i.grating[ gn ].grooves
				/ INIT_OFFSET_RANGE;
		return vars_push( FLOAT_VAR,
						  spectrapro_300i.grating[ gn ].init_offset );
	}

	offset = get_double( v, "zero offset value" );

	if ( fabs( offset ) > 1.0 )
	{
		print( FATAL, "Zero offset value is too large, valid range is "
			   "[-1,+1].\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_offset( gn,
									lrnd( ( offset * INIT_OFFSET_RANGE ) /
										  spectrapro_300i.grating[ gn ].grooves
										  + ( gn % 3 ) * INIT_OFFSET ) );

	spectrapro_300i.grating[ gn ].init_offset = offset;

	return vars_push( FLOAT_VAR, offset );
}


/*-------------------------------------------------------------------*
 * The function either returns or sets the value that determines the
 * wavelength the monochromator moves to when it gets send a new
 * wavelength. There is a default value, but to be able to do a
 * calibration this default value is adjustable in a certain range.
 * Using this function this deviation can be determined or set. The
 * deviation is a value in the interval [-1,1] (0 stands for the
 * default value).
 * This function should only be used during the calibration of the
 * monochromator.
 *-------------------------------------------------------------------*/

Var_T *monochromator_grating_adjust( Var_T *v )
{
	long grating;
	long gn;
	double gadjust;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	grating = get_strict_long( v, "grating number" );

	gn = grating - 1;

	if ( grating < 0 || grating >= MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	if ( ! spectrapro_300i.grating[ gn ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		spectrapro_300i.grating[ gn ].used_in_test = SET;

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
			spectrapro_300i.grating[ gn ].init_adjust =
				( double ) ( spectrapro_300i_get_adjust( gn ) - INIT_ADJUST )
				/ INIT_ADJUST_RANGE;

		return vars_push( FLOAT_VAR,
						  spectrapro_300i.grating[ gn ].init_adjust );
	}

	gadjust = get_double( v, "grating adjust value" );

	if ( fabs( gadjust ) > 1.0 )
	{
		print( FATAL, "Grating adjust value of %2.f is too large, valid "
			   "range is [-1,1].\n", gadjust );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_adjust( gn, lrnd( gadjust * INIT_ADJUST_RANGE
											  + INIT_ADJUST ) );

	spectrapro_300i.grating[ gn ].init_adjust = gadjust;

	return vars_push( FLOAT_VAR, gadjust );
}


/*----------------------------------------------------------------------*
 * Function for calculating the calibration values of a grating of the
 * monochromator. It takes the results of at least four measurements
 * of pixel offsets for known lines from the center of the detector (at
 * possibly different center wavelengths) and tries to to find the
 * values for the inclusion angle, focal length and detector angle that
 * reproduce these experimental values with the lowest deviation by
 * doing a simplex minimization. Unfortunately, when trying to minimize
 * all three parameters data at once with only slightly noisy one runs
 * into the simplex normally ends up with completely unrealistic values.
 * Thus we have to minimze the three parameters individually, keeping
 * both other fixed and starting with the inclusion angle, followed by
 * the focal length and ending with the detector angle.
 * Input parameters:
 *  1. number of the grating that has been used
 *  2. array of wavelengths of the measured lines
 *  3. array of center wavelengths for the measured value
 *  4. array of diffraction order for the measured values
 *  5. array of measured pixel offsets (if necessary already corrected
 *     for center wavelegth offsets)
 * The following arguments are optional:
 *  6. end of fit criterion value
 *  7. start value for fit of inclusion angle
 *  8. start value for fit of focal length
 *  9. start value for fit of detector angle
 * 10. start deviation for inclusion angle
 * 11. start deviation for focal length
 * 12. start deviation for detector angle
 *----------------------------------------------------------------------*/

Var_T *monochromator_calibrate( Var_T *v )
{
	Calib_Params_T c;
	Var_T *cv;
	int acc;
	long grating;
	long gn;
	double *x = NULL, *dx;
	double epsilon;
	double val;
	ssize_t i;


	CLOBBER_PROTECT( x );
	CLOBBER_PROTECT( v );

	/* Check if there's a CCD camera module loaded */

	if ( ! exists_device_type( "ccd camera" ) )
	{
		print( FATAL, "Function can only be used when the module for a "
			   "CCD camera is loaded.\n" );
		THROW( EXCEPTION );
	}

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	c.wavelengths = c.center_wavelengths = c.n_exp = NULL;
	c.m = NULL;

	/* Get the grating number */

	grating = get_strict_long( v, "grating number" );

	if ( grating < 1 || grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	gn = grating - 1;

	if ( ! spectrapro_300i.grating[ gn ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == TEST )
		spectrapro_300i.grating[ gn ].used_in_test = SET;

	v = vars_pop( v );

	/* Get the array of wavelengths of the lines */

	if ( ! ( v->type & ( INT_ARR | FLOAT_ARR ) ) || v->len < 4 )
	{
		print( FATAL, "Second argument isn't an array with at least "
			   "4 wavelengths.\n" );
		THROW( EXCEPTION );
	}

	TRY
	{
		c.num_values = v->len;
		c.wavelengths = DOUBLE_P T_malloc( c.num_values
										   * sizeof *c.wavelengths );
		if ( v->type == INT_ARR )
			for ( i = 0; i < v->len; i++ )
				c.wavelengths[ i ] = ( double ) v->val.lpnt[ i ];
		else
			memcpy( c.wavelengths, v->val.dpnt,
					c.num_values * sizeof *c.wavelengths );

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			print( FATAL, "Missing arguments.\n" );
			THROW( EXCEPTION );
		}

		/* Get the array of center wavelengths */

		if ( ! ( v->type & ( INT_ARR | FLOAT_ARR ) ) || v->len < 4 )
		{
			print( FATAL, "Third argument isn't an array with at least "
				   "4 center wavelengths.\n" );
			THROW( EXCEPTION );
		}

		if ( v->len != ( ssize_t ) c.num_values )
		{
			print( FATAL, "Number of wavelengths and center wavelengths "
				   "differ.\n" );
			THROW( EXCEPTION );
		}

		c.center_wavelengths = DOUBLE_P T_malloc( c.num_values
											  * sizeof *c.center_wavelengths );

		if ( v->type == INT_ARR )
			for ( i = 0; i < v->len; i++ )
				c.center_wavelengths[ i ] = ( double ) v->val.lpnt[ i ];
		else
			memcpy( c.center_wavelengths, v->val.dpnt,
					c.num_values * sizeof *c.center_wavelengths );

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			print( FATAL, "Missing arguments.\n" );
			THROW( EXCEPTION );
		}

		/* Get the array of diffraction orders */

		if ( ! ( v->type & ( INT_ARR | FLOAT_ARR ) ) || v->len < 4 )
		{
			print( FATAL, "Fourth argument isn't an array with at least "
				   "4 diffraction orders.\n" );
			THROW( EXCEPTION );
		}

		if ( v->len != ( ssize_t ) c.num_values )
		{
			print( FATAL, "Number of wavelengths and diffraction orders "
				   "differ.\n" );
			THROW( EXCEPTION );
		}

		c.m = LONG_P T_malloc( c.num_values * sizeof *c.m );

		if ( v->type == INT_ARR )
			memcpy( c.m, v->val.lpnt, c.num_values * sizeof *c.m );
		else
			for ( i = 0; i < v->len; i++ )
				c.m[ i ] = lrnd( v->val.dpnt[ i ] );

		if ( ( v = vars_pop( v ) ) == NULL )
		{
			print( FATAL, "Missing arguments.\n" );
			THROW( EXCEPTION );
		}

		/* Get the array of position offsets (in pixel) */

		if ( ! ( v->type & ( INT_ARR | FLOAT_ARR ) ) || v->len < 4 )
		{
			print( FATAL, "Fifth argument isn't an array with at least "
				   "4 line position offsets.\n" );
			THROW( EXCEPTION );
		}

		if ( v->len != ( ssize_t ) c.num_values )
		{
			print( FATAL, "Number of wavelengths and position offsets "
				   "differ.\n" );
			THROW( EXCEPTION );
		}

		c.n_exp = DOUBLE_P T_malloc( c.num_values * sizeof *c.n_exp );

		if ( v->type == INT_ARR )
			for ( i = 0; i < v->len; i++ )
				c.n_exp[ i ] = ( double ) v->val.lpnt[ i ];
		else
			memcpy( c.n_exp, v->val.dpnt, c.num_values * sizeof *c.n_exp );

		v = vars_pop( v );

		/* Get the grating constant */

		c.d = 1.0 / spectrapro_300i.grating[ gn ].grooves;
	
		/* Get the pixel size of the camera */

		if ( ! func_exists( "ccd_camera_pixel_size" ) )
		{
			print( FATAL, "CCD camera has no function for determining the "
				   "pixel size.\n" );
			THROW( EXCEPTION );
		}

		cv = func_call( func_get( "ccd_camera_pixel_size", &acc ) );
		if ( cv->type != FLOAT_VAR || cv->val.dval <= 0.0 )
		{
			print( FATAL, "Function of CCD for determining the pixel size "
				   "does not return a useful value.\n" );
			THROW( EXCEPTION );
		}

		c.pixel_width = cv->val.dval;
		vars_pop( cv );

		/* Get two arrays, one for the parameters we want to minimize and one
		   for the start deviations. Than either initialize them with default
		   values or with input arguments. Make sure that the values are
		   reasonable and none of the deviations are exactly 0. */

		x = DOUBLE_P T_malloc( 6 * sizeof *x );
		dx = x + 3;

		x[ 0 ] = DEFAULT_INCLUSION_ANGLE;
		x[ 1 ] = SPECTRAPRO_300I_FOCAL_LENGTH;
		x[ 2 ] = DEFAULT_DETECTOR_ANGLE;

		dx[ 0 ] = 0.1 * x[ 0 ];
		dx[ 1 ] = 0.025 * x[ 1 ];
		dx[ 2 ] = DEFAULT_DETECTOR_ANGLE_DELTA;

		epsilon = DEFAULT_EPSILON;

		if ( v != NULL )
		{
			epsilon = get_double( v, "minimization limit" );
			if ( epsilon == 0.0 )
				epsilon = 1.0e-12;
		}

		if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
		{
			x[ 0 ] = get_double( v, "inclusion angle" );
			if ( x[ 0 ] == 0.0 )
				x[ 0 ] = 10.0 * atan( 1.0 ) / 9.0;
		}

		if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
		{
			x[ 1 ] = get_double( v, "focal length" );
			if ( x[ 1 ] == 0.0 )
				x[ 1 ] = SPECTRAPRO_300I_FOCAL_LENGTH;
		}

		if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
			x[ 2 ] = get_double( v, "detector angle" );

		if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
		{
			dx[ 0 ] = get_double( v, "deviation for inclusion angle" );
			if ( dx[ 0 ] == 0.0 )
				dx[ 0 ] = 0.01 * x[ 0 ];
		}

		if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
		{
			dx[ 1 ] = get_double( v, "deviation for focal length" );
			if ( dx[ 1 ] == 0.0 )
				dx[ 1 ] = 0.01 * x[ 1 ];
		}

		if ( v != NULL && ( v = vars_pop( v ) ) != NULL )
		{
			dx[ 2 ] = get_double( v, "deviation for detector angle" );
			if ( dx[ 2 ] == 0.0 )
				dx[ 2 ] = dx[ 2 ] = atan( 1.0 ) / 900.0;
		}

		too_many_arguments( v );

		if ( FSC2_MODE == EXPERIMENT )
		{
			c.opt = 0;
			c.focal_length   = x[ 1 ];
			c.detector_angle = x[ 2 ];
			val = fsc2_simplex( 1, x, dx, ( void * ) &c, spectrapro_300i_min,
								epsilon );

			c.inclusion_angle = x[ 0 ];

			c.opt = 1;
			x[ 0 ] = c.focal_length;
			dx[ 0 ] = dx[ 1 ];
			val = fsc2_simplex( 1, x, dx, ( void * ) &c, spectrapro_300i_min,
								epsilon );
			c.focal_length = x[ 0 ];

			c.opt = 2;
			x[ 0 ] = x[ 2 ];
			dx[ 0 ] = dx[ 2 ];
			val = fsc2_simplex( 1, x, dx, ( void * ) &c, spectrapro_300i_min,
								epsilon );
			x[ 2 ] = x[ 0 ];
			x[ 0 ] = c.inclusion_angle;
			x[ 1 ] = c.focal_length;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( c.wavelengths != NULL )
			T_free( c.wavelengths );
		if ( c.center_wavelengths != NULL )
			T_free( c.center_wavelengths );
		if ( c.m != NULL )
			T_free( c.m );
		if ( c.n_exp != NULL )
			T_free( c.n_exp );
		if ( x != NULL )
			T_free( x );
		RETHROW( );
	}

	x[ 0 ] *= 45.0 / atan( 1.0 );
	x[ 2 ] *= 45.0 / atan( 1.0 );
	cv = vars_push( FLOAT_ARR, x, 3 );

	T_free( c.wavelengths );
	T_free( c.center_wavelengths );
	T_free( c.m );
	T_free( c.n_exp );
	T_free( x );

	return cv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
