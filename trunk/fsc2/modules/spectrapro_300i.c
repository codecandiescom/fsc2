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

#define SPECTRAPRO_300I_MAIN


#include "spectrapro_300i.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

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
		spectrapro_300i.grating[ i ].init_gadjust = 0.0;
	}

	spectrapro_300i.turret = 0;
	spectrapro_300i.current_grating = 0;
	spectrapro_300i.wavelength = 5.0e-7;
	spectrapro_300i.use_calib = 0;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int spectrapro_300i_exp_hook( void )
{
	if ( ! spectrapro_300i.is_needed )
		return 1;

	spectrapro_300i_open( );

	spectrapro_300i.use_calib = 0;

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

int spectrapro_300i_end_of_exp_hook( void )
{
	if ( ! spectrapro_300i.is_needed || ! spectrapro_300i.is_open )
		return 1;

	spectrapro_300i_close( );

	return 1;
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

void spectrapro_300i_exit_hook( void )
{
	spectrapro_300i.is_needed = UNSET;
	if ( spectrapro_300i.is_open )
		spectrapro_300i_close( );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*-----------------------------------------------------*/
/* Function for setting or quering the current grating */
/*-----------------------------------------------------*/

Var *monochromator_grating( Var *v )
{
	long grating;


	if ( v == 0 )
		return vars_push( INT_VAR, spectrapro_300i.current_grating + 1 );

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
			   spectrapro_300i.current_grating + 1 );
		return vars_push( INT_VAR, spectrapro_300i.current_grating + 1 );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
		{
			print( SEVERE,  "No grating #%ld is installed, keeping grating "
				   "#%ld\n", grating, spectrapro_300i.current_grating + 1 );
			return vars_push( INT_VAR, spectrapro_300i.current_grating + 1 );
		}

		if ( grating - spectrapro_300i.turret * 3 < 1 ||
			 grating - spectrapro_300i.turret * 3 > 3 )
		{
			print( FATAL, "Can't switch to grating #ld while turret #ld is "
				   "in use.\n", grating, spectrapro_300i.turret + 1 );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_grating( grating );

	spectrapro_300i.current_grating = grating - 1;

	return vars_push( INT_VAR, grating );
}


/*----------------------------------------------------------------------*/
/* Function for setting or quering the current turret. If the turret is */
/* changed also the grating is set to the one at the same position on   */
/* the new turret (if this is installed) or to one of the gratings      */
/* installed on the new turret. If there is no grating installed for    */
/* the new turret the function will return an error.                    */
/*----------------------------------------------------------------------*/

Var *monochromator_turret( Var *v )
{
	long turret;
	long new_grat;
	long i;


	if ( v == 0 )
		return vars_push( INT_VAR, spectrapro_300i.turret + 1 );

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
			   spectrapro_300i.turret + 1 );
		return vars_push( INT_VAR, spectrapro_300i.turret + 1 );
	}

	/* During the test run we don't have to do anything */

	if ( FSC2_MODE != EXPERIMENT )
	{
		spectrapro_300i.turret = turret - 1;		
		return vars_push( INT_VAR, turret );
	}

	/* Same if the new turret is identical to the current turret */

	if ( turret - 1 == spectrapro_300i.turret )
		return vars_push( INT_VAR, turret );

	/* Figure out the new grating to use */

	new_grat = spectrapro_300i.current_grating +
			   3 * ( turret - 1 - spectrapro_300i.turret );

	if ( ! spectrapro_300i.grating[ new_grat ].is_installed )
	{
		for ( i = 3 * ( turret - 1 ); i < 3 * turret; i++ )
			if ( spectrapro_300i.grating[ i ].is_installed )
				break;

		if ( i == 3 * turret )
		{
			print( FATAL, "Can't switch to turret #%ld, no grating installed "
				   "for this turret.\n" );
			THROW( EXCEPTION );
		}

		new_grat = i;
	}

	print( NO_ERROR, "Switching to grating #%ld.\n", new_grat + 1 );

	spectrapro_300i_set_turret( turret );
	spectrapro_300i.turret = turret - 1;

	spectrapro_300i_set_turret( new_grat + 1 );
	spectrapro_300i.current_grating = new_grat;	

	return vars_push( INT_VAR, turret );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_wavelength( Var *v )
{
	const char *reply;
	double wl;
	char *buf;


	CLOBBER_PROTECT( buf );
	CLOBBER_PROTECT( wl );

	if ( v == NULL )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			reply = spectrapro_300i_talk( "?NM", 100, 1 );
			spectrapro_300i.wavelength = T_atod( reply ) * 1.0e-9;
			T_free( ( void * ) reply );
		}

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
	{
		buf = get_string( "%.3f GOTO", 1.0e9 * wl );

		TRY
		{
			spectrapro_300i_send( buf );
			T_free( buf );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( buf );
			RETHROW( );
		}
	}

	spectrapro_300i.wavelength = wl;

	return vars_push( FLOAT_VAR, spectrapro_300i.wavelength );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_install_grating( Var *v )
{
	long grating;


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

	grating = get_strict_long( v->next, "grating position" );

	if ( grating < 1 && grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating position, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE == EXPERIMENT )
	{
		if ( ! strcmp( v->val.sptr, "UNINSTALL" ) )
			spectrapro_300i_uninstall_grating( grating );
		else
			spectrapro_300i_install_grating( v->val.sptr, grating );
	}

	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_groove_density( Var *v )
{
	long grating;


	grating = get_strict_long( v, "grating number" );

	if ( grating < 1 && grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}
	
	if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	return vars_push( FLOAT_VAR,
				   ( double ) spectrapro_300i.grating[ grating - 1 ].grooves );
}


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

Var *monochromator_load_calibration( Var * v )
{
	char *calib_file;
	FILE *cfp;


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
			if ( ( cfp = spectrapro_300i_open_calib( calib_file ) ) == NULL )
			{
				print( FATAL, "Default calibration file '%s' not found.\n",
					   calib_file );
				THROW( EXCEPTION );
			}
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
		CATCH( EXCEPTION )
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
	CATCH( EXCEPTION )
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


/*-------------------------------------------------------------------*/
/* Function returns an array of two values that are suitable for use */
/* as axis description parameters required by by the change_scale()  */
/* function (if the camera uses binning the second element may have  */
/* to be multiplied by the x-binning width). Please note: since the  */
/* axis is not really completely linear, the axis that gets displyed */
/* when using these values is not 1000% correct!                     */
/*-------------------------------------------------------------------*/

Var *monochromator_wavelength_axis( Var * v )
{
	Var *cv;
	double pixel_width;
	long num_pixels;
	size_t g;
	double offset;
	double inclusion_angle_2;
	double focal_length;
	double detector_angle;
	double grating_angle;
	double wl;
	double wl_low;
	double wl_hi;
	double x;
	double theta;
	int acc;


	UNUSED_ARGUMENT( v );

	/* Check that we can talk with the camera */

	if ( ! exists_device( "rs_spec10" ) )
	{
		print( FATAL, "Function can only be used when the module for the "
			   "Roper Scientific Spec-10 CCD camera is loaded.\n" );
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

	cv = vars_push( FLOAT_ARR, NULL, 2 );

	/* If no calibration file has been read or there is no (complete)
	   calibration for the current grating we just return values for
	   a pixel count scale */

	if ( ! spectrapro_300i.use_calib )
	{
		cv->val.dpnt[ 0 ] = - 0.5 * ( double ) ( num_pixels - 1 );
		cv->val.dpnt[ 1 ] = 1;
		return cv;
	}

	g = spectrapro_300i.current_grating;

	if ( ! spectrapro_300i.grating[ g ].is_calib )
	{
		print( SEVERE, "No (complete) calibration for current grating #%ld "
			   "found.\n", g + 1 );
		cv->val.dpnt[ 0 ] = - 0.5 * ( double ) ( num_pixels - 1 );
		cv->val.dpnt[ 1 ] = 1;
		return cv;
	}

	wl = spectrapro_300i.wavelength;
	offset = spectrapro_300i.grating[ g ].n0;
	inclusion_angle_2 = 0.5 * spectrapro_300i.grating[ g ].inclusion_angle;
	focal_length = spectrapro_300i.grating[ g ].focal_length;
	detector_angle = spectrapro_300i.grating[ g ].detector_angle;

	grating_angle = asin( 0.5 * wl * spectrapro_300i.grating[ g ].grooves
						  / cos( inclusion_angle_2 ) );

	x = - pixel_width * ( 0.5 * ( double ) ( num_pixels - 1 ) + offset );
	theta = atan(   x * cos( detector_angle )
				  / ( focal_length + x * sin( detector_angle ) ) );
	wl_low = (   sin( grating_angle - inclusion_angle_2 )
			   + sin( grating_angle + inclusion_angle_2 + theta ) )
		     / spectrapro_300i.grating[ g ].grooves;

	x = pixel_width * ( 0.5 * ( double ) ( num_pixels - 1 ) - offset );
	theta = atan(   x * cos( detector_angle )
				  / ( focal_length + x * sin( detector_angle ) ) );
	wl_hi = (   sin( grating_angle - inclusion_angle_2 )
			  + sin( grating_angle + inclusion_angle_2 + theta ) )
		    / spectrapro_300i.grating[ g ].grooves;

	cv->val.dpnt[ 1 ] = ( wl_hi - wl_low ) / num_pixels;
	cv->val.dpnt[ 0 ] = wl - 0.5 * cv->val.dpnt[ 1 ] * ( num_pixels - 1 );

	return cv;
}


/*------------------------------------------------------------------*/
/* Function allows to set a new calibration for one of the gratings */
/* manually, i.e. instead of reading them from a file. It expects   */
/* five arguments:                                                  */
/* 1. grating number (grating must be installed)                    */
/* 2. pixel offset                                                  */
/* 3. inclusion angle                                               */
/* 4. focal length                                                  */
/* 5. detector angle                                                */
/*------------------------------------------------------------------*/

Var *monochromator_set_calibration( Var *v )
{
	long grating;
	double offset;
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
	if ( grating < 1 && grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	if ( v->type == STR_VAR )
	{
		if ( strcasecmp( v->val.sptr, "OFF" ) )
		{
			print( FATAL, "Invalid second argument.\n" );
			THROW( EXCEPTION );
		}
		
		too_many_arguments( v );

		spectrapro_300i.grating[ grating - 1 ].is_calib = UNSET;

		spectrapro_300i.use_calib = UNSET;
		for ( i = 0; i < MAX_GRATINGS; i++ )
			if ( spectrapro_300i.grating[ grating - 1 ].is_calib )
				spectrapro_300i.use_calib = SET;

		return vars_push( INT_VAR, 1 );
	}

	offset = get_double( v, "pixel offset" );

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	inclusion_angle = get_double( v, "inclusion angle" );
	if ( inclusion_angle <= 0.0 )
	{
		print( FATAL, "Invalid inclusion angle.\n" );
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

	spectrapro_300i.grating[ grating - 1 ].n0 = offset;
	spectrapro_300i.grating[ grating - 1 ].inclusion_angle = inclusion_angle;
	spectrapro_300i.grating[ grating - 1 ].focal_length = focal_length;
	spectrapro_300i.grating[ grating - 1 ].detector_angle = detector_angle;
	spectrapro_300i.grating[ grating - 1 ].is_calib = SET;
	spectrapro_300i.use_calib = SET;

	return vars_push( INT_VAR, 1 );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *monochromator_init_offset( Var *v )
{
	long grating;
	double offset;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	grating = get_strict_long( v, "grating number" );
	if ( grating < 1 && grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( FLOAT_VAR,
						  spectrapro_300i.grating[ grating - 1 ].init_offset );
		return vars_push( FLOAT_VAR,
						  spectrapro_300i_get_offset( grating )
						  / INIT_OFFSET_RANGE );
	}

	offset = get_double( v, "grating offset value" );

	if ( abs( lrnd( offset * INIT_OFFSET_RANGE /
					spectrapro_300i.grating[ grating ].grooves ) ) >
		 INIT_OFFSET_RANGE / spectrapro_300i.grating[ grating ].grooves )
	{
		print( FATAL, "Grating offset value is too large, valid range is "
			   "[-1,+1].\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_set_offset( grating,
								lrnd( offset * INIT_OFFSET_RANGE /
									 spectrapro_300i.grating[ grating ].grooves
									 + ( grating % 3 ) * INIT_OFFSET ) );

	spectrapro_300i.grating[ grating - 1 ].init_offset = offset;

	return vars_push( FLOAT_VAR, offset );
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

Var *monochromator_init_adjust( Var *v )
{
	long grating;
	double gadjust;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments.\n" );
		THROW( EXCEPTION );
	}

	grating = get_strict_long( v, "grating number" );
	if ( grating < 1 && grating > MAX_GRATINGS )
	{
		print( FATAL, "Invalid grating number, must be in range between "
			   "1 and %d.\n", MAX_GRATINGS );
		THROW( EXCEPTION );
	}

	if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

	if ( ( v = vars_pop( v ) ) == NULL )
	{
		if ( FSC2_MODE == TEST )
			return vars_push( FLOAT_VAR,
						 spectrapro_300i.grating[ grating - 1 ].init_gadjust );
		return vars_push( FLOAT_VAR,
						  spectrapro_300i_get_gadjust( grating )
						  / INIT_ADJUST_RANGE );
	}

	gadjust = get_double( v, "grating adjust value" );

	if ( abs( lrnd( gadjust * INIT_ADJUST_RANGE ) ) > INIT_ADJUST_RANGE )
	{
		print( FATAL, "Grating adjust value is too large, valid range is "
			   "[-1,1].\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == EXPERIMENT )
		spectrapro_300i_init_gadjust( grating,
									  lrnd( gadjust * INIT_ADJUST_RANGE
											+ INIT_ADJUST ) );

	spectrapro_300i.grating[ grating - 1 ].init_gadjust = gadjust;

	return vars_push( FLOAT_VAR, gadjust );
}


/*----------------------------------------------------------------------*/
/* Function for calculating the calibration values of a grating of the  */
/* monochromator. It takes the results of at least four measurements    */
/* of pixel offsets for known lines from the center of the detector (at */
/* possibly different center wavelengths) and thries to to find the     */
/* values for the inclusion angle, focal length and detector angle that */
/* reproduce these experimental values with the lowest deviation by     */
/* doing a simplex minimization. Unfortunately, when trying to minimize */
/* all three parameters data at once with only slightly noisy one runs  */
/* into the simplex normally ends up with completely unrealistic values.*/
/* Thus we have to minimze the three parameters individually, keeping   */
/* both other fixed and starting with the inclusion angle, followed by  */
/* the focal length and ending with the detector angle.                 */
/* Input parameters:                                                    */
/*  1. number of the grating that has been used                         */
/*  2. array of wavelengths of the measured lines                       */
/*  3. array of center wavelengths for the measured value               */
/*  4. array of diffraction order for the measured values               */
/*  5. array of measured pixel offsets (if necessary already corrected  */
/*     for center wavelegth offsets)                                    */
/* The following arguments are optional:                                */
/*  6. end of fit criterion value                                       */
/*  7. start value for fit of inclusion angle                           */
/*  8. start value for fit of focal length                              */
/*  9. start value for fit of detector angle                            */
/* 10. start deviation for inclusion angle                              */
/* 11. start deviation for focal length                                 */
/* 12. start deviation for detector angle                               */
/*----------------------------------------------------------------------*/

Var *monochromator_calibration( Var *v )
{
	CALIB_PARAMS c;
	Var *cv;
	int acc;
	long grating;
	double *x = NULL, *dx;
	double epsilon;
	double val;
	ssize_t i;


	CLOBBER_PROTECT( x );
	CLOBBER_PROTECT( v );

	if ( ! exists_device( "rs_spec10" ) )
	{
		print( FATAL, "Function can only be used when the module for the "
			   "Roper Scientific Spec-10 CCD camera is loaded.\n" );
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

	if ( ! spectrapro_300i.grating[ grating - 1 ].is_installed )
	{
		print( FATAL, "Grating #%ld isn't installed.\n", grating );
		THROW( EXCEPTION );
	}

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

		c.d = 1.0 / spectrapro_300i.grating[ grating - 1 ].grooves;
	
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

