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


#include "spectrapro_300i.h"


static FILE *spectrapro_300i_find_calib( char *name );
static FILE *spectrapro_300i_open_calib( char *name );
static double calibration_min( size_t n, double *x, void *par );
static double simplex( size_t n, double *x, double *dx, void *par,
					   double func( size_t n, double *x, void *par ),
					   double epsilon );
static int is_minimum( int n, double *y, double epsilon );



typedef struct CALIB_PARAMS CALIB_PARAMS;

struct CALIB_PARAMS {
	double d;                    /* grating grooves per millimeter */
	double pixel_width;          /* width of a pixel of the CCD */
	size_t num_values;           /* number of values */
	double *n_exp;               /* array of measured values */
	double *wavelengths;         /* array of wavelengths */
	double *center_wavelengths;  /* array of center wavelengths */
	long *m;                     /* array of diffraction orders */
	int opt;
	double inclusion_angle;
	double focal_length;
	double detector_angle;
};


#define DEFAULT_INCLUSION_ANGLE  ( 35.0 * atan( 1.0 ) / 45.0 ) /* 35 degree */
#define DEFAULT_DETECTOR_ANGLE   0.0
#define DEFAULT_DETECTOR_ANGLE_DELTA ( atan( 1.0 ) / 90.0 )    /* 0.5 degree */
#define DEFAULT_EPSILON          1.0e-9



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

		if ( ( cfp = spectrapro_300i_open_calib( calib_file ) ) == NULL )
		{
			print( FATAL, "Default calibration file '%s' not found.\n",
				   calib_file );
			T_free( calib_file );
			THROW( EXCEPTION );
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


/*-----------------------------------------------------------------------*/
/*-----------------------------------------------------------------------*/

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

	printf( "%g %g %g\n", grating_angle, wl_low, wl_hi );

	cv->val.dpnt[ 0 ] = wl_low;
	cv->val.dpnt[ 1 ] = ( wl_hi - wl_low ) / num_pixels;

	return cv;
}


/*-----------------------------------------------------------------------*/
/* If the function succeeds it returns a file pointer to the calibration */
/* file. Otherwise an exception is thrown.                               */
/*-----------------------------------------------------------------------*/

static FILE *spectrapro_300i_find_calib( char *name )
{
	FILE *cfp;
	char *new_name = NULL;


	/* Expand a leading tilde to the users home directory */

	if ( name[ 0 ] == '~' )
		new_name = get_string( "%s%s%s", getenv( "HOME" ),
							   name[ 1 ] != '/' ? "/" : "", name + 1 );

	/* Now try to open the file and return the file pointer */

	if ( ( cfp = spectrapro_300i_open_calib( new_name != NULL ?
											 new_name : name ) ) != NULL )
	{
		if ( new_name != NULL )
			T_free( new_name );
		return cfp;
	}

	/* If the file name contains a slash we give up after freeing memory */

	if ( strchr( name, '/' ) != NULL )
	{
		print( FATAL, "Calibration file '%s' not found.\n", new_name );
		THROW( EXCEPTION );
	}

	/* Last chance: The calibration file is in the library directory... */

	new_name = get_string( "%s%s%s", libdir, slash( libdir ), name );

	if ( ( cfp = spectrapro_300i_open_calib( new_name ) ) == NULL )
	{
		print( FATAL, "Calibration file '%s' not found in either the current "
			   "directory or in '%s'.\n", strip_path( new_name ), libdir );
		T_free( new_name );
		THROW( EXCEPTION );
	}

	T_free( new_name );

	return cfp;
}


/*------------------------------------------------------------------*/
/* Tries to open the file with the given name and returns the file  */
/* pointer, returns NULL if file does not exist returns, or throws  */
/* an exception if the file can't be read (either because of prob-  */
/* lems with the permissions or other, unknown reasons). If opening */
/* the file fails with an exception memory used for its name is     */
/* deallocated.                                                     */
/*------------------------------------------------------------------*/

static FILE *spectrapro_300i_open_calib( char *name )
{
	FILE *cfp;


	if ( access( name, R_OK ) == -1 )
	{
		if ( errno == ENOENT )       /* file not found */
			return NULL;

		print( FATAL, "No read permission for calibration file '%s'.\n",
			   name );
		T_free( name );
		THROW( EXCEPTION );
	}

	if ( ( cfp = fopen( name, "r" ) ) == NULL )
	{
		print( FATAL, "Can't open calibration file '%s'.\n", name );
		T_free( name );
		THROW( EXCEPTION );
	}

	return cfp;
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
/*  5. array of measured pixel offsets                                  */
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

//		if ( FSC2_MODE == EXPERIMENT )
		{
			c.opt = 0;
			c.focal_length   = x[ 1 ];
			c.detector_angle = x[ 2 ];
			val = simplex( 1, x, dx, ( void * ) &c, calibration_min,
						   epsilon );

			c.inclusion_angle = x[ 0 ];

			c.opt = 1;
			x[ 0 ] = c.focal_length;
			dx[ 0 ] = dx[ 1 ];
			val = simplex( 1, x, dx, ( void * ) &c, calibration_min,
						   epsilon );
			c.focal_length = x[ 0 ];

			c.opt = 2;
			x[ 0 ] = x[ 2 ];
			dx[ 0 ] = dx[ 2 ];
			val = simplex( 1, x, dx, ( void * ) &c, calibration_min,
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

	cv = vars_push( FLOAT_ARR, x, 3 );

	T_free( c.wavelengths );
	T_free( c.center_wavelengths );
	T_free( c.m );
	T_free( c.n_exp );
	T_free( x );

	return cv;
}


/*-----------------------------------------------------------------------*/
/* Function for calculation of the sum of the squares of the differences */
/* between the measured offsets of the lines from he center (in pixels)  */
/* and the calculated offsets. The formulas used are (in TeX notation):  */
/* $n$: deviation in numbers of pixels                                   */
/* $x$: width of a single pixel                                          */
/* $m$: diffraction order                                                */
/* $\psi$: grating angle, rotation angle of the grating                  */
/* $\gamma$: inclusion angle, angle between incoming and outgoing beam   */
/* $f$: focal length of monochromator                                    */
/* $\delta$: detector angle, deviation of angle between center beam and  */ 
/* perpendicular on the CCD plane                                        */
/* $\theta$: incident angle of beam on detector                          */ 
/* $\lambda$: wavelength of measured line                                */
/* $\lambda_C$: wavelength at the center of the detector                 */ 
/* \begin{eqnarray*}                                                     */
/* \tan \theta & = & \frac{ nx \, \cos \delta}{f + nx \, \sin \delta} \\ */
/* \sin \psi & = & \frac{m \lambda_C}{2 d \, \cos \gamma / 2}         \\ */
/* \lambda & = & \frac{d}{m} \, \{ \sin ( \psi - \frac{\gamma}{2} )      */	
/*               + \sin ( \psi - \frac{\gamma}{2} + \theta ) \}          */
/* \end{eqnarray*}                                                       */
/*-----------------------------------------------------------------------*/

static double calibration_min( size_t n, double *x, void *par )
{
	double inclusion_angle_2;                 /* half of inclusion angle */
	double focal_length;
	double detector_angle;
	CALIB_PARAMS *c;
	double grating_angle;
	double sum = 0.0;
	double a, b;
	size_t i;


	UNUSED_ARGUMENT( n );

	c = ( CALIB_PARAMS * ) par;

	if ( c->opt == 0 )
	{
		inclusion_angle_2 = 0.5 * x[ 0 ];
		if ( inclusion_angle_2 <= 0.0 )
			return HUGE_VAL;

		focal_length = c->focal_length;
		detector_angle = c->detector_angle;
	}
	else if ( c->opt == 1 )
	{
		focal_length = x[ 0 ];
		if ( focal_length <= 0.0 )
			return HUGE_VAL;

		inclusion_angle_2 = 0.5 * c->inclusion_angle;
		detector_angle    = c->detector_angle;
	}
	else if ( c->opt == 2 )
	{
		detector_angle    = x[ 0 ];

		inclusion_angle_2 = 0.5 * c->inclusion_angle;
		focal_length      = c->focal_length;
	}
	else
		THROW( EXCEPTION );

	if ( focal_length <= 0.0 )
		return HUGE_VAL;

	/* For each measured value (that's the deviation of the line position
	   from the center, expressed in pixels) calculate the theoretical value
	   with the values for the inclusion angle, the focal length and the
	   detector angle we got from the simplex routine. Then sum up the squares
	   of the deviations from the measured values, which is what we want to
	   minimize. */

	for ( i = 0; i < c->num_values; i++ )
	{
		a = c->m[ i ] * c->center_wavelengths[ i ]
			/ ( 2.0 * c->d * cos( inclusion_angle_2 ) );

		if ( fabs( a ) > 1.0 )
		{
			sum = HUGE_VAL;
			break;
		}

		grating_angle = asin( a );

		a = c->m[ i ] * c->wavelengths[ i ] / c->d
			- sin( grating_angle - inclusion_angle_2 );

		if ( fabs( a ) > 1.0 )
		{
			sum = HUGE_VAL;
			break;
		}

		a = tan( asin( a ) - grating_angle - inclusion_angle_2 );

		b = cos( detector_angle ) - a * sin( detector_angle );

		if ( b == 0.0 )
		{
			sum = HUGE_VAL;
			break;
		}

		b = c->n_exp[ i ] - focal_length * a / ( b * c->pixel_width );

		sum += b * b;
	}

	return sum;
}


/*-----------------------------------------------------------------------*/
/* Function for determining the minimum of a function using the SIMPLEX  */
/* algorithm (see J.A. Nelder, R. Mead, Comp. J. 7, (1965), p. 308-313)  */
/* Input arguments:                                                      */
/* 1. number of parameters of function                                   */
/* 2. array of starting values for the fit                               */
/* 3. array of deviations of start values                                */
/* 4. void pointer that gets passed on to the function to be minimized   */
/* 5. address of function to be minimized - function has to accept the   */
/*    following arguments:                                               */
/*    a. number of parameters passed to function                         */
/*    b. pointer to (double array) of parameters                         */
/*    c. void pointer (can be used to transferfurther required data)     */
/* 6. size of criterion for end of minimization: when the ratio between  */
/*    the standard error of the function values at the corners of the    */
/*    simplex to the mean of this function values is smaller than the    */
/*    size minimization is stopped.                                      */
/* When the function returns the data in the array with the start values */
/* has been overwritten by the paramters that yield the mimimum value.   */
/*-----------------------------------------------------------------------*/

static double simplex( size_t n, double *x, double *dx, void *par,
					   double func( size_t n, double *x, void *par ),
					   double epsilon )
{
    double *p,              /* matrix of the (n + 1) simplex corners */
		   *y,              /* array of function values at the corners */
		   *p_centroid,     /* coordinates of center of simplex */
           *p_1st_try,      /* point after reflection */
           *p_2nd_try,      /* point after expansion/contraction */
           y_1st_try,       /* function value after reflection */
           y_2nd_try,       /* function value after expansion/contraction */
           y_lowest,        /* smallest function value */
           y_highest,       /* largest function value */
           y_2nd_highest,   /* second-largest function value */
           alpha = 1.0,     /* reflection factor*/
           beta = 0.5,      /* contraction factor*/
           gamm = 2.0;      /* expansion factor */
    size_t i, j;            /* counters */
    size_t highest,         /* index of corner with largest/smallest */
           lowest;          /* function value */


	CLOBBER_PROTECT( i );

    /* Get enough memory for the corners of the simplex, center points and
	   function values at the (n + 1) corners */

	p = T_malloc( ( n * ( n + 5 ) + 1 ) * sizeof *p );
	p_centroid = p + n * ( n + 1 );
    p_1st_try = p_centroid + n;
    p_2nd_try = p_1st_try + n;
    y = p_2nd_try + n;

    /* Set up the corners of the simplex and calculate the function values
	   at these positions */

    for ( i = 0; i < n + 1; i++ )
    {
        for ( j = 0; j < n; j++ )
            p[ i * n  + j ] = x[ j ];

        if ( i != n )
            p[ i * ( n + 1 ) ] += dx[ i ];

		TRY
		{
			y[ i ] = func( n, p + i * n, par );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			T_free( p );
			RETHROW( );
		}
    }

    /* Here starts the loop for finding the minimum. It will only stop when
	   the minimum criterion is satisfied. */

    do
    {
		/* Determine index and function value of the corner with the lowest,
		   the largest and the second largest function value */

        lowest = ( y[ 0 ] < y[ 1 ] ) ? ( highest = 1, 0 ) : ( highest = 0, 1 );

        y_lowest = y_2nd_highest = y[ lowest ];
        y_highest = y[ highest ];

        for ( i = 2; i < n + 1; i++ )
        {
            if ( y[ i ] < y_lowest )
                y_lowest = y[ lowest = i ];

            if ( y[ i ] > y_highest )
            {
                y_2nd_highest = y_highest;
                y_highest = y[ highest = i ];
            }
            else
                if ( y[ i ] > y_2nd_highest )
                    y_2nd_highest = y[ i ];
        }

		/* Calculate center of simplex (excluding the corner with the largest
		   function value) */

        for ( j = 0; j < n; j++ )
        {
            for ( p_centroid[ j ] = 0.0, i = 0; i < n + 1; i++ )
            {
                if ( i == highest )
                    continue;
                p_centroid[ j ] += p[ i * n + j ];
            }

            p_centroid[ j ] /=  n;
        }

		/* Do a reflection, i.e. mirror the point with the largest function
		   value at the center point and calculate the function value at
		   this new point */

        for ( j = 0; j < n; j++ )
            p_1st_try[ j ] = ( 1.0 + alpha ) * p_centroid[ j ]
							 - alpha * p[ highest * n + j ];

		y_1st_try = func( n, p_1st_try, par );

		/* If the function value at the new point is smaller than the largest
		   value, the new point replaces the corner with the largest function
		   value */
		   
        if ( y_1st_try < y_highest )
        {
            for ( j = 0; j < n; j++ )
                p[ highest * n + j ] = p_1st_try[ j ];

            y[ highest ] = y_1st_try;
        }

		/* If the function value at the new point even smaller than all other
		   function values try an expansion of the simplex */

        if ( y_1st_try < y_lowest )
        {
            for ( j = 0; j < n; j++ )
                p_2nd_try[ j ] = gamm * p_1st_try[ j ] +
								 ( 1.0 - gamm ) * p_centroid[ j ];

			y_2nd_try = func( n, p_2nd_try, par );

			/* If the expansion was successful, i.e. led to an even smaller
			   function value, replace the corner with the largest function
			   by this new improved new point */

            if ( y_2nd_try < y_1st_try )
            {
                for ( j = 0; j < n; j++ )
                    p[ highest * n + j ] = p_2nd_try[ j ];

                y[ highest ] = y_2nd_try;
            }

            continue;                     /* start next cycle */
        }

		/* If the new point is at least smaller than the previous maximum
		   value start a new cycle, otherwise try a contraction */

        if ( y_1st_try < y_2nd_highest )
            continue;                     /* start next cycle */

        for ( j = 0; j < n; j++ )
            p_2nd_try[ j ] = beta * p[ highest * n + j ]
							 + ( 1.0 - beta ) * p_centroid[ j ];

		y_2nd_try = func( n, p_2nd_try, par );

		/* If the contraction was successful, i.e. led to a value that is
		   smaller than the previous maximum value, the new point replaces
		   the largest value. Otherwise the whole simplex gets contracted
		   by shrinking all edges by a factor of 2 toward the point toward
		   the point with the smallest function value. */

        if ( y_2nd_try < y[ highest ] )
        {
            for ( j = 0; j < n; j++ )
                p[ highest * n +  j ] = p_2nd_try[ j ];

            y[ highest ] = y_2nd_try;
        }
        else
            for ( i = 0; i < n + 1; i++ )
            {
                if ( i == lowest )
                    continue;

                for ( j = 0; j < n; j++ )
                    p[ i * n + j ] =
								0.5 * ( p[ i * n + j ] + p[ lowest * n +  j ]);

				y[ i ] = func( n, p + n * i, par );
            }
    } while ( ! is_minimum( n + 1, y, epsilon ) );

	/* Pick the corner with the lowest function value and write its
	   coordinates over the array with the start values, deallocate
	   memory and return the minimum function value */

    y_lowest = y[ 0 ];

    for ( i = 1; i < n + 1; i++ )
        if ( y_lowest > y[ i ] )
            y_lowest = y[ lowest = i ];

    for ( j = 0; j < n; j++ )
        x[ j ] = p[ lowest * n + j ];

    T_free( p );

	return y_lowest;
}


/*-------------------------------------------------------------------*/
/* Tests for the simplex() function if the minimum has been reached. */
/* It checks if the ratio of the standard error of the function      */
/* values at the corners of the simplex and the mean of the function */
/* values is smaller than a constant 'epsilon'.                      */
/* Arguments:                                                        */
/* 1. number of corners of the simplex                               */
/* 2. array of function values at the corners                        */
/* 3. constant 'epsilon'                                             */
/* Function returns 1 when the minimum has been found, 0 otherwise.  */
/*-------------------------------------------------------------------*/

static int is_minimum( int n, double *y, double epsilon )
{
    int i;                      /* counter */
    double yq,                  /* sum of function values */
		   yq_2,                /* Sum of squares of function values */
           dev;                 /* standard error of fucntion values */


    for ( yq = 0.0, yq_2 = 0.0, i = 0; i < n; i++ )
    {
        yq += y[ i ];
        yq_2 += y[ i ] * y[ i ];
    }

    dev = sqrt( ( yq_2 - yq * yq / n ) /  ( n - 1 ) );

	return dev * n / fabs( yq ) < epsilon;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
