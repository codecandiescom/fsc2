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


#define RS_SPEC10_MAIN

#include "rs_spec10.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;



/*----------------------------------------------------------------*/
/* This function is called directly after all modules are loaded. */
/* Its main function is to initialize all global variables that   */
/* are needed in the module.                                      */
/*----------------------------------------------------------------*/

int rs_spec10_init_hook( void )
{
	rs_spec10 = &rs_spec10_prep;

	rs_spec10->dev_file = DEVICE_FILE;
	rs_spec10->is_open = UNSET;
	rs_spec10->lib_is_init = UNSET;

	rs_spec10->temp.is_setpoint = UNSET;

	rs_spec10->ccd.roi[ X ]     = 0;
	rs_spec10->ccd.roi[ Y ]     = 0;
	rs_spec10->ccd.roi[ X + 2 ] = CCD_PIXEL_WIDTH - 1;
	rs_spec10->ccd.roi[ Y + 2 ] = CCD_PIXEL_HEIGHT - 1;

	rs_spec10->ccd.max_size[ X ] = CCD_PIXEL_WIDTH;
	rs_spec10->ccd.max_size[ Y ] = CCD_PIXEL_HEIGHT;

	rs_spec10->ccd.bin[ X ] = 1;
	rs_spec10->ccd.bin[ Y ] = 1;

	rs_spec10->ccd.exp_time =
						   lrnd( CCD_EXPOSURE_TIME / CCD_EXPOSURE_RESOLUTION );

	rs_spec10->ccd.bin_mode = HARDWARE_BINNING;  

	/* Try to initialize the library */

	if ( ! pl_pvcam_init( ) )
		rs_spec10_error_handling( );		

	rs_spec10->lib_is_init = SET;

	return 1;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

int rs_spec10_test_hook( void )
{
	rs_spec10_test = rs_spec10_prep;
	rs_spec10 = &rs_spec10_test;

	rs_spec10->ccd.exp_time = 100000;
	rs_spec10->ccd.exp_res = 1.0e-6;

	rs_spec10->ccd.test_min_exp_time = HUGE_VAL;

	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int rs_spec10_exp_hook( void )
{
	rs_spec10_exp = rs_spec10_prep;
	rs_spec10 = &rs_spec10_exp;

	rs_spec10_init_camera( );
	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int rs_spec10_end_of_exp_hook( void )
{
	if ( rs_spec10->is_open )
	{
		pl_cam_close( rs_spec10->handle );
		rs_spec10->is_open = UNSET;
	}

	return 1;
}


/*-----------------------------------------*/
/* Called before device driver is unloaded */
/*-----------------------------------------*/

void rs_spec10_exit_hook( void )
{
	if ( rs_spec10->is_open )
	{
		pl_cam_close( rs_spec10->handle );
		rs_spec10->is_open = UNSET;
	}

	/* Close library, release all system resources that pl_pvcam_init()
	   acquired. */

	if ( rs_spec10->lib_is_init )
	{
		pl_pvcam_uninit( );
		rs_spec10->lib_is_init = UNSET;
	}
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

Var *ccd_camera_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------*/
/* Function for setting the region of interest (ROI) */
/* for the next picture or spectrum to be fetched.   */
/* The function expects to get passed an array with  */
/* 4 elements with the coordinates of the lower left */
/* hand and the upper right hand corner. If one of   */
/* the elements is 0 the corresponfing ROI value     */
/* remains unchanged.                                */
/*---------------------------------------------------*/

Var *ccd_camera_roi( Var *v )
{
	int i;
	long vroi[ 4 ];
	long temp;


	for ( i = 0; i < 4; i++ )
		vroi[ i ] = ( long ) rs_spec10->ccd.roi[ i ] + 1;

	if ( v == NULL )
		return vars_push( INT_ARR, vroi, 4 );

	vars_check( v, INT_ARR | FLOAT_ARR );

	if ( v->type == FLOAT_ARR )
		print( WARN, "Float values used for region of interest.\n" );

	if ( v->len > 4 )
		print( WARN, "Argument array has more than the required 4 elements, "
			   "discarding superfluous ones.\n" );

	for ( i = 0; i < 4 && i < v->len; i++ )
		if ( v->type == INT_ARR && v->val.lpnt[ i ] != 0 )
			vroi[ i ] = v->val.lpnt[ i ];
		else if ( v->type == FLOAT_ARR && v->val.dpnt[ i ] != 0.0 )
			vroi[ i ] = lrnd( v->val.dpnt[ i ] );

	/* For values of 0 we use the old ROI value */

	for ( i = 0; i < 4; i++ )
		if ( vroi[ i ] == 0 )
			vroi[ i ] = rs_spec10->ccd.roi[ i ] + 1;

	for ( i = 0; i < 2; i++ )
		if ( vroi[ i ] > vroi[ i + 2 ] )
		{
			print( WARN, "LL%c larger than UR%c value, exchanging.\n",
				   'X' + i, 'X' + i );
			temp = vroi[ i ];
			vroi[ i ] = vroi[ i + 2 ];
			vroi[ i + 2 ] = temp;
		}

	for ( i = 0; i < 2; i++ )
	{
		if ( vroi[ i ] < 0 )
		{
			print( FATAL, "LL%c value smaller than 1.\n", 'X' + i );
			THROW( EXCEPTION );
		}

		if ( vroi[ i + 2 ] > ( long ) rs_spec10->ccd.max_size[ i ] )
		{
			print( FATAL, "UR%c larger than CCD %c-size.\n",
				   'X' + i, 'X' + i );
			THROW( EXCEPTION );
		}
	}

	too_many_arguments( v );

	for ( i = 0; i < 4; i++ )
		rs_spec10->ccd.roi[ i ] = ( uns16 ) vroi[ i ] - 1;

	return vars_push( INT_ARR, vroi, 4 );
}


/*----------------------------------------------------------------*/
/* Function for setting the binning used with the next picture or */
/* spectrum to be returned by the camera. The function expects to */
/* get an array with 2 elements for the vertical and horizontal   */
/* binning. A value of 0 indicates that the current binning value */
/* should remain unchanged. Optionally, the function can get      */
/* passed a second argument, a number or string for the type of   */
/* binning (i.e. in hardware or software) to be used.             */
/*----------------------------------------------------------------*/

Var *ccd_camera_binning( Var *v )
{
	int i;
	long vbin[ 2 ];


	for ( i = 0; i < 2; i++ )
		vbin[ i ] = ( long ) rs_spec10->ccd.bin[ i ];

	if ( v == NULL )
		return vars_push( INT_ARR, vbin, 2 );

	vars_check( v, INT_ARR | FLOAT_ARR );

	if ( v->type == FLOAT_ARR )
		print( WARN, "Float values used for binning.\n" );

	if ( v->len > 2 )
		print( WARN, "Argument array has more than the required 2 elements, "
			   "discarding superfluous ones.\n" );

	for ( i = 0; i < 2 && i < v->len; i++ )
		if ( v->type == INT_ARR && v->val.lpnt[ i ] != 0 )
			vbin[ i ] = v->val.lpnt[ i ];
		else if ( v->type == FLOAT_ARR && v->val.dpnt[ i ] != 0.0 )
			vbin[ i ] = lrnd( v->val.dpnt[ i ] );

	for ( i = 0; i < 2; i++ )
		if ( vbin[ i ] == 0 )
			vbin[ i ] = rs_spec10->ccd.bin[ i ];

	for ( i = 0; i < 2; i++ )
	{
		if ( vbin[ i ] < 1 )
		{
			print( FATAL, "%c binning value smaller than 1.\n", 'X' + i );
			THROW( EXCEPTION );
		}

		if ( vbin[ i ] > ( long ) rs_spec10->ccd.max_size[ i ] + 1 )
		{
			print( FATAL, "%c binning value larger than CCD %c-size.\n",
				   'X' + i, 'X' + i );
			THROW( EXCEPTION );
		}
	}

	if ( ( v = vars_pop( v ) ) != NULL )
		vars_pop( ccd_camera_binning_method( v ) );

	too_many_arguments( v );

	for ( i = 0; i < 2; i++ )
		rs_spec10->ccd.bin[ i ] = ( uns16 ) vbin[ i ];

	return vars_push( INT_ARR, vbin, 2 );
}


/*-----------------------------------------------------------------------*/
/* Function for setting the binning method (i.e. either via hardware or  */
/* software) for the next pictures fetched from the camera. The function */
/* expects either a number (where 0 stands for hardware binning while    */
/* any other number for software binning) or a string, either "SOFT" or  */
/* "SOFTWARE" or "HARD" or "HARDWARE".                                   */
/*-----------------------------------------------------------------------*/

Var *ccd_camera_binning_method( Var *v )
{
	if ( v == NULL )
		return vars_push( INT_VAR, rs_spec10->ccd.bin_mode ? 1L : 0L );

	vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	switch ( v->type )
	{
		case INT_VAR :
			if ( v->val.lval == 0 )
				rs_spec10->ccd.bin_mode = HARDWARE_BINNING;
			else
				rs_spec10->ccd.bin_mode = SOFTWARE_BINNING;
			break;

		case FLOAT_VAR :
			if ( lrnd( v->val.dval ) == 0 )
				rs_spec10->ccd.bin_mode = HARDWARE_BINNING;
			else
				rs_spec10->ccd.bin_mode = SOFTWARE_BINNING;
			break;

		case STR_VAR :
			if ( ! strcasecmp( v->val.sptr, "soft" ) ||
				 ! strcasecmp( v->val.sptr, "software" ) )
				rs_spec10->ccd.bin_mode = SOFTWARE_BINNING;
			else if ( ! strcasecmp( v->val.sptr, "hard" ) ||
					  ! strcasecmp( v->val.sptr, "hardware" ) )
				rs_spec10->ccd.bin_mode = HARDWARE_BINNING;
			else
			{
				print( FATAL, "Unrecognized string '%s' used for binning "
					   "method.\n", v->val.sptr );
				THROW( EXCEPTION );
			}
			break;
	}

	too_many_arguments( v );

	return vars_push( INT_VAR, rs_spec10->ccd.bin_mode ? 1L : 0L );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

Var *ccd_camera_exposure_time( Var *v )
{
	double et;


	if ( v == NULL )
		return vars_push( FLOAT_VAR,
						  rs_spec10->ccd.exp_time * rs_spec10->ccd.exp_res );

	et = get_double( v, "exposure time" );

	rs_spec10->ccd.exp_time = ( uns32 ) lrnd( et / rs_spec10->ccd.exp_res );

	if ( rs_spec10->ccd.exp_time <= 0 )
	{
		print( FATAL, "Invalid exposure time of %s.\n",
			   rs_spec10_ptime( et ) );
		THROW( EXCEPTION );
	}

	/* Requiring a maximum exposure time of one hour is a bit arbitrary,
	   but it's hard to imagine that anyone will ever need such a long
	   exposure time... ("640 kB will be enough for everyone" ;-) */

	if ( et > 3600.0 )
	{
		print( FATAL, "Exposure time of %.1f s is too long.\n", et );
		THROW( EXCEPTION );
	}

	if ( fabs( et - rs_spec10->ccd.exp_time * rs_spec10->ccd.exp_res ) / et
		 > 0.01 )
		print( SEVERE, "Exposure time had to be adjusted to %s.\n",
			   rs_spec10_ptime(   rs_spec10->ccd.exp_time
							    * rs_spec10->ccd.exp_res ) );

	too_many_arguments( v );

	if ( FSC2_MODE == TEST && et < rs_spec10->ccd.test_min_exp_time )
		rs_spec10->ccd.test_min_exp_time = et;

	return vars_push( FLOAT_VAR,
					  rs_spec10->ccd.exp_time * rs_spec10->ccd.exp_res );
}


/*-----------------------------------------------*/
/*-----------------------------------------------*/

Var *ccd_camera_get_picture( Var *v )
{
	uns16 *frame = NULL;
	long width, height;
	long w;
	unsigned long max_val;
	Var *nv = NULL;
	long i, j, k;
	long *bin_arr = NULL;
	long *bap;
	long *cl;
	uns16 *cf;
	long *dest;
	double divs;
	uns16 bin[ 2 ];
	uns16 urc[ 2 ];
	uns32 size;


	UNUSED_ARGUMENT( v );

	CLOBBER_PROTECT( frame );
	CLOBBER_PROTECT( width );
	CLOBBER_PROTECT( height );
	CLOBBER_PROTECT( nv );
	CLOBBER_PROTECT( bin_arr );
	CLOBBER_PROTECT( bin[ X ] );
	CLOBBER_PROTECT( bin[ Y ] );
	CLOBBER_PROTECT( urc[ X ] );
	CLOBBER_PROTECT( urc[ Y ] );

	/* Store the oringinal binning size and the position of the upper right
	   hand corner, they might become adjusted and need to be reset at the
	   end */

	bin[ X ] = rs_spec10->ccd.bin[ X ];
	bin[ Y ] = rs_spec10->ccd.bin[ Y ];
	urc[ X ] = rs_spec10->ccd.roi[ X + 2 ];
	urc[ Y ] = rs_spec10->ccd.roi[ Y + 2 ];

	/* Calculate how many points the picture will have after binning */

	width  = ( urc[ X ] - rs_spec10->ccd.roi[ X ] + 1 ) / bin[ X ];
	height = ( urc[ Y ] - rs_spec10->ccd.roi[ Y ] + 1 ) / bin[ Y ];

	/* If the binning area is larger than the ROI reduce the binning sizes
	   to fit the the ROI sizes (the binning sizes are reset to their original
	   values before returning from the function) */

	if ( width == 0 )
	{
		rs_spec10->ccd.bin[ X ] = urc[ X ] - rs_spec10->ccd.roi[ X ] + 1;
		width = 1;
	}

	if ( height == 0 )
	{
		rs_spec10->ccd.bin[ Y ] = urc[ Y ] - rs_spec10->ccd.roi[ Y ] + 1;
		height = 1;
	}

	if ( rs_spec10->ccd.bin[ X ] != bin[ X ] ||
		 rs_spec10->ccd.bin[ Y ] != bin[ Y ] )
		print( SEVERE, "Binning parameters had to be changed from (%ld, %ld) "
			   "to (%ld, %ld) because binning area was larger than ROI.\n",
			   bin[ X ], bin[ Y ], rs_spec10->ccd.bin[ X ],
			   rs_spec10->ccd.bin[ Y ] );

	/* Reduce the ROI to the area we really need (in case the ROI sizes aren't
	   integer multiples of the binning sizes) by moving the upper right hand
	   corner nearer to the lower left hand corner in order to speed up
	   fetching the picture a bit (before the function returns the ROI is set
	   back to it's original size) */

	rs_spec10->ccd.roi[ X + 2 ] =
				 rs_spec10->ccd.roi[ X ] + width * rs_spec10->ccd.bin[ X ] - 1;
	rs_spec10->ccd.roi[ Y + 2 ] =
				rs_spec10->ccd.roi[ Y ] + height * rs_spec10->ccd.bin[ Y ] - 1;

	/* Now try to get the picture */

	TRY
	{
		if ( FSC2_MODE == EXPERIMENT )
			frame = rs_spec10_get_pic( &size );
		else
		{
			max_val = ~ ( uns16 ) 0 + 1;

			size = ( uns32 ) ( width * sizeof *frame );
			frame = UNS16_P T_malloc( size );
			for ( i = 0; i < width * height; i++ )
				frame[ i ] = random( ) % max_val;
		}

		nv = vars_push_matrix( INT_REF, 2, height, width );

		/* There is a bug (or undocumented feature :-) in the PVCAM library:
		   For some parallel hardware binning sizes the library needs two more
		   bytes than would be required to hold all points (when stored as
		   2-byte values). In these cases the very first element of the
		   returned array contains a bogus value and the real data seem to
		   start only at the second element. This hack tries to avoid the
		   problem.*/

		cf = frame + size / sizeof *frame - width;

		/* During the test run or for hardware binning or without binning (i.e.
		   if both binning sizes are 1) we can leave most of the work to the
		   camera, otherwise we have to do the binning ourselves */

		if ( FSC2_MODE == TEST ||
			 rs_spec10->ccd.bin_mode == HARDWARE_BINNING ||
			 ( rs_spec10->ccd.bin[ X ] == 1 && rs_spec10->ccd.bin[ Y ] == 1 ) )
			for ( i = 0; i < height; i++ )
			{
				cl = nv->val.vptr[ i ]->val.lpnt;
				for ( j = 0; j < width; j++ )
					*cl++ = ( long ) *cf++;
			}
		else
		{
			w = width * rs_spec10->ccd.bin[ X ];
			divs = 1.0 / ( rs_spec10->ccd.bin[ X ] * rs_spec10->ccd.bin[ Y ] );

			bin_arr = LONG_P T_malloc( w * sizeof *bin_arr );

			for ( i = 0; i < height; i++ )
			{
				memset( bin_arr, 0, w * sizeof *bin_arr );

				/* Add up all values for the current piece in y-direction */

				for ( bap = bin_arr, j = 0; j < w; j++, bap++ )
					for ( k = 0; k < rs_spec10->ccd.bin[ Y ]; k++, cf++ )
						*bap += ( long ) *cf;

				/* Now do the binning in x-direction for all pieces - since
				   with hardware binning the sum of all pixel counts is
				   divided by the number of pixels in the binning area we also
				   have to do this division for the total result of each of
				   the pieces. */

				for ( bap = bin_arr, dest = nv->val.vptr[ i ]->val.lpnt,
						  j = 0; j < width; j++, dest++ )
				{
					for ( k = 0; k < rs_spec10->ccd.bin[ X ]; k++, bap++ )
						*dest += *bap;
					*dest = lrnd( *dest * divs );
				}
			}

			bin_arr = LONG_P T_free( bin_arr );
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( bin_arr != NULL )
			T_free( bin_arr );
		if ( frame != NULL )
			T_free( frame );
		if ( nv != NULL )
			vars_pop( nv );
		rs_spec10->ccd.bin[ X ] = bin[ X ];
		rs_spec10->ccd.bin[ Y ] = bin[ Y ];
		rs_spec10->ccd.roi[ X + 2 ] = urc[ X ];
		rs_spec10->ccd.roi[ Y + 2 ] = urc[ Y ];
		RETHROW( );
	}

	T_free( frame );

	rs_spec10->ccd.bin[ X ] = bin[ X ];
	rs_spec10->ccd.bin[ Y ] = bin[ Y ];
	rs_spec10->ccd.roi[ X + 2 ] = urc[ X ];
	rs_spec10->ccd.roi[ Y + 2 ] = urc[ Y ];

	return nv;
}


/*-----------------------------------------------*/
/*-----------------------------------------------*/

Var *ccd_camera_get_spectrum( Var *v )
{
	uns16 *frame = NULL;
	long width, height;
	long w;
	unsigned long max_val;
	Var *nv = NULL;
	long i, j, k;
	long *bin_arr = NULL;
	long *bap;
	long *cl;
	uns16 *cf;
	long *dest;
	double divs;
	uns16 bin[ 2 ];
	uns16 urc[ 2 ];
	uns32 size;


	UNUSED_ARGUMENT( v );

	CLOBBER_PROTECT( frame );
	CLOBBER_PROTECT( width );
	CLOBBER_PROTECT( height );
	CLOBBER_PROTECT( nv );
	CLOBBER_PROTECT( bin_arr );
	CLOBBER_PROTECT( bin[ X ] );
	CLOBBER_PROTECT( bin[ Y ] );
	CLOBBER_PROTECT( urc[ X ] );
	CLOBBER_PROTECT( urc[ Y ] );

	/* Store the oringinal binning size and the position of the upper right
	   hand corner, they might become adjusted and need to be reset at the
	   end */

	bin[ X ] = rs_spec10->ccd.bin[ X ];
	bin[ Y ] = rs_spec10->ccd.bin[ Y ];
	urc[ X ] = rs_spec10->ccd.roi[ X + 2 ];
	urc[ Y ] = rs_spec10->ccd.roi[ Y + 2 ];

	/* Calculate how many points the picture will have after binning */

	width  = ( urc[ X ] - rs_spec10->ccd.roi[ X ] + 1 ) / bin[ X ];

	/* If the binning area is larger than the ROI reduce the binning sizes
	   to fit the the ROI sizes (the binning sizes are reset to their original
	   values before returning from the function) */

	if ( width == 0 )
	{
		rs_spec10->ccd.bin[ X ] = urc[ X ] - rs_spec10->ccd.roi[ X ] + 1;
		width = 1;
	}

	rs_spec10->ccd.bin[ Y ] = urc[ Y ] - rs_spec10->ccd.roi[ Y ] + 1;

	if ( rs_spec10->ccd.bin[ X ] != bin[ X ] )
		print( SEVERE, "Binning parameter had to be changed from %ld "
			   "to %ld because binning width was larger than ROI.\n",
			   bin[ X ], rs_spec10->ccd.bin[ X ] );

	/* Reduce the ROI to the area we really need (in case the ROI sizes aren't
	   integer multiples of the binning sizes) by moving the upper right hand
	   corner nearer to the lower left hand corner in order to speed up
	   fetching the picture a bit (before the function returns the ROI is set
	   back to it's original size) */

	rs_spec10->ccd.roi[ X + 2 ] =
				 rs_spec10->ccd.roi[ X ] + width * rs_spec10->ccd.bin[ X ] - 1;
	rs_spec10->ccd.roi[ Y + 2 ] =
						 rs_spec10->ccd.roi[ Y ] + rs_spec10->ccd.bin[ Y ] - 1;

	/* Now try to get the picture */

	TRY
	{
		if ( FSC2_MODE == EXPERIMENT )
			frame = rs_spec10_get_pic( &size );
		else
		{
			max_val = ~ ( uns16 ) 0 + 1;

			size = ( uns32 ) ( width * sizeof *frame );
			frame = UNS16_P T_malloc( size );
			for ( i = 0; i < width; i++ )
				frame[ i ] = random( ) % max_val;
		}

		nv = vars_push( INT_ARR, NULL, width );

		/* There is a bug (or undocumented feature :-) in the PVCAM library:
		   For some parallel hardware binning sizes the library needs two more
		   bytes than would be required to hold all points (when stored as
		   2-byte values). In these cases the very first element of the
		   returned array contains a bogus value and the real data seem to
		   start only at the second element. This hack tries to avoid the
		   problem.*/

		cf = frame + size / sizeof *frame - width;

		/* During the test run or for hardware binning or without binning (i.e.
		   if both binning sizes are 1) we can leave most of the work to the
		   camera, otherwise we have to do the binning ourselves */

		if ( FSC2_MODE == TEST ||
			 rs_spec10->ccd.bin_mode == HARDWARE_BINNING ||
			 ( rs_spec10->ccd.bin[ X ] == 1 && rs_spec10->ccd.bin[ Y ] == 1 ) )

		{
			cl = nv->val.lpnt;
			for ( j = 0; j < width; j++ )
				*cl++ = ( long ) *cf++;
		}
		else
		{
			w = width * rs_spec10->ccd.bin[ X ];
			divs = 1.0 / ( rs_spec10->ccd.bin[ X ] );

			bin_arr = LONG_P T_malloc( w * sizeof *bin_arr );

			memset( bin_arr, 0, w * sizeof *bin_arr );

			/* Add up all values for the current piece in y-direction */

			for ( bap = bin_arr, j = 0; j < w; j++, bap++ )
				for ( k = 0; k < rs_spec10->ccd.bin[ Y ]; k++, cf++ )
					*bap += ( long ) *cf;

			/* Now do the binning in x-direction for all pieces - since
			   with hardware binning the sum of all pixel counts is
			   divided by the number of pixels in the binning area we also
			   have to do this division for the total result of each of
			   the pieces. */

			for ( bap = bin_arr, dest = nv->val.lpnt,
					  j = 0; j < width; j++, dest++ )
			{
				for ( k = 0; k < rs_spec10->ccd.bin[ X ]; k++, bap++ )
					*dest += *bap;
				*dest = lrnd( *dest * divs );
			}

			bin_arr = LONG_P T_free( bin_arr );
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
		if ( bin_arr != NULL )
			T_free( bin_arr );
		if ( frame != NULL )
			T_free( frame );
		if ( nv != NULL )
			vars_pop( nv );
		rs_spec10->ccd.bin[ X ] = bin[ X ];
		rs_spec10->ccd.bin[ Y ] = bin[ Y ];
		rs_spec10->ccd.roi[ X + 2 ] = urc[ X ];
		rs_spec10->ccd.roi[ Y + 2 ] = urc[ Y ];
		RETHROW( );
	}

	T_free( frame );

	rs_spec10->ccd.bin[ X ] = bin[ X ];
	rs_spec10->ccd.bin[ Y ] = bin[ Y ];
	rs_spec10->ccd.roi[ X + 2 ] = urc[ X ];
	rs_spec10->ccd.roi[ Y + 2 ] = urc[ Y ];

	return nv;
}


/*------------------------------------------------------*/
/* Function for determining the current CCD temperature */
/* or to set a target temperature (setpoint).           */
/*------------------------------------------------------*/

Var *ccd_camera_temperature( Var *v )
{
	double temp;


	if ( v == NULL )
		switch ( FSC2_MODE )
		{
			case PREPARATION :
				no_query_possible( );

			case TEST :
				if ( rs_spec10->temp.is_setpoint )
					return vars_push( FLOAT_VAR, rs_spec10->temp.setpoint );
				else
					return vars_push( FLOAT_VAR, RS_SPEC10_TEST_TEMP );

			case EXPERIMENT :
				return vars_push( FLOAT_VAR, rs_spec10_get_temperature( ) );
		}

	/* Make sure camera allows setting a setpoint (in case the call of the
	   function has been well hidden within e.g. an IF construct, so it
	   wasn't run already during the test run) */

	if ( FSC2_MODE == EXPERIMENT &&
		 rs_spec10->temp.acc_setpoint != ACC_READ_WRITE &&
		 rs_spec10->temp.acc_setpoint != ACC_WRITE_ONLY )
	{
		print( SEVERE, "Camera does not allow setting a temperature "
			   "setpoint.\n" );
		return vars_push( FLOAT_VAR, 0.0 );
	}

	temp = get_double( v, "camera temperature" );

	if ( lrnd( rs_spec10_k2c( temp ) * 100.0 ) >
										  lrnd( CCD_MAX_TEMPERATURE * 100.0 ) )
	{
		print( FSC2_MODE == EXPERIMENT ? SEVERE : FATAL, "Requested setpoint "
			   "temperature of %.2f K (%.2f C) is too high, valid range is "
			   "%.2f K (%.2f C) to %.2f K (%.2f C).\n", temp,
			   rs_spec10_k2c( temp ), CCD_MIN_TEMPERATURE,
			   rs_spec10_k2c( CCD_MIN_TEMPERATURE ), CCD_MAX_TEMPERATURE,
			   rs_spec10_k2c( CCD_MAX_TEMPERATURE ) );
		if ( FSC2_MODE != EXPERIMENT )
			THROW( EXCEPTION );
	}

	if ( lrnd( rs_spec10_k2c( temp ) * 100.0 ) <
										  lrnd( CCD_MIN_TEMPERATURE * 100.0 ) )
	{
		print( FSC2_MODE == EXPERIMENT ? SEVERE : FATAL, "Requested setpoint "
			   "temperature of %.2f K (%.2f C) is too low, valid range is "
			   "%.2f K (%.2f C) to %.2f K (%.2f C).\n", temp,
			   rs_spec10_k2c( temp ), CCD_MIN_TEMPERATURE,
			   rs_spec10_k2c( CCD_MIN_TEMPERATURE ), CCD_MAX_TEMPERATURE,
			   rs_spec10_k2c( CCD_MAX_TEMPERATURE ) );
		if ( FSC2_MODE != EXPERIMENT )
			THROW( EXCEPTION );
	}

	too_many_arguments( v );

	rs_spec10->temp.setpoint = temp;
	rs_spec10->temp.is_setpoint = SET;

	return vars_push( FLOAT_VAR, temp );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
