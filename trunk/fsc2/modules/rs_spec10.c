/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


#include "rs_spec10.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

struct RS_SPEC10 *rs_spec10 = NULL,
				  rs_spec10_prep,
				  rs_spec10_test,
				  rs_spec10_exp;


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

	/* Read in the file where the state of the CCD camera got stored */

	rs_spec10->ccd.roi_is_set = UNSET;
	rs_spec10->ccd.bin_is_set = UNSET;
	rs_spec10->ccd.bin_mode_is_set = UNSET;

	rs_spec10_read_state( );

	rs_spec10->ccd.max_size[ X ] = CCD_PIXEL_WIDTH;
	rs_spec10->ccd.max_size[ Y ] = CCD_PIXEL_HEIGHT;

	/* Try to initialize the library */

	if ( ! pl_pvcam_init( ) )
		rs_spec10_error_handling( );		

	rs_spec10->lib_is_init = SET;

	return 1;
}


/*-------------------------------------------*/
/* Start of test hook function of the module */
/*-------------------------------------------*/

int rs_spec10_test_hook( void )
{
	rs_spec10_test = rs_spec10_prep;
	rs_spec10 = &rs_spec10_test;

	rs_spec10->ccd.exp_time = 100000;
	rs_spec10->ccd.exp_res = 1.0e-6;
	rs_spec10->ccd.clear_cycles = CCD_DEFAULT_CLEAR_CYCLES;

	rs_spec10->ccd.test_min_exp_time = HUGE_VAL;

	return 1;
}


/*-------------------------------------------------*/
/* Start of experiment hook function of the module */
/*-------------------------------------------------*/

int rs_spec10_exp_hook( void )
{
	rs_spec10_exp = rs_spec10_prep;
	rs_spec10 = &rs_spec10_exp;

	/* Read in the file where the state of the CCD camera got stored,
	   it might have been changed during a previous experiment */

	rs_spec10_read_state( );

	rs_spec10_init_camera( );
	return 1;
}


/*--------------------------------------------------------------*/
/* Function gets called just before the child process doing the */
/* experiment exits.                                            */
/*--------------------------------------------------------------*/

void rs_spec10_child_exit_hook( void )
{
	rs_spec10_store_state( );
}


/*-----------------------------------------------*/
/* End of experiment hook function of the module */
/*-----------------------------------------------*/

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
	if ( rs_spec10 == NULL )
		return;

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


/*-----------------------------------------*/
/* Function returns the name of the device */
/*-----------------------------------------*/

Var *ccd_camera_name( Var *v )
{
	UNUSED_ARGUMENT( v );
	return vars_push( STR_VAR, DEVICE_NAME );
}


/*----------------------------------------------------------------*/
/* Function for setting the region of interest (ROI) for the next */
/* image or spectrum to be fetched. The function expects an array */
/* with 4 elements with the coordinates of the lower left hand    */
/* and the upper right hand corner. If one of the elements is 0   */
/* the corresponding ROI value remains unchanged.                 */
/*----------------------------------------------------------------*/

Var *ccd_camera_roi( Var *v )
{
	int i;
	long vroi[ 4 ];
	long temp;


	for ( i = 0; i < 4; i++ )
		vroi[ i ] = ( long ) rs_spec10->ccd.roi[ i ] + 1;

	if ( v == NULL )
		return vars_push( INT_ARR, vroi, 4 );

	vars_check( v, INT_ARR | FLOAT_ARR | STR_VAR );

	if ( v->type == STR_VAR )
	{
		if ( strcasecmp( v->val.sptr, "ALL" ) )
		{
			print( FATAL, "Invalid argument string '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}

		vroi[ X ] = vroi[ Y ] = 1;
		vroi[ X + 2 ] = ( long ) rs_spec10->ccd.max_size[ X ];
		vroi[ Y + 2 ] = ( long ) rs_spec10->ccd.max_size[ Y ];
	}
	else
	{
		if ( v->type == FLOAT_ARR )
			print( WARN, "Float values used for region of interest.\n" );

		if ( v->len > 4 )
			print( WARN, "Argument array has more than the required 4 "
				   "elements, discarding superfluous ones.\n" );

		for ( i = 0; i < 4 && i < v->len; i++ )
			if ( v->type == INT_ARR && v->val.lpnt[ i ] != 0 )
				vroi[ i ] = v->val.lpnt[ i ];
			else if ( v->type == FLOAT_ARR && v->val.dpnt[ i ] != 0.0 )
				vroi[ i ] = lrnd( v->val.dpnt[ i ] );

		/* For values of 0 we use the old ROI value */

		for ( i = 0; i < 4; i++ )
			if ( vroi[ i ] == 0 )
				vroi[ i ] = rs_spec10->ccd.roi[ i ] + 1;

		/* Check that the arguments are reasonable */

		for ( i = 0; i < 2; i++ )
		{
			if ( vroi[ i ] < 0 )
			{
				print( FATAL, "Negative LL%c value.\n", 'X' + i );
				THROW( EXCEPTION );
			}

			if ( vroi[ i ] > ( long ) rs_spec10->ccd.max_size[ i ] )
			{
				print( FATAL, "LL%c larger than CCD %c-size.\n",
					   'X' + i, 'X' + i );
				THROW( EXCEPTION );
			}

			if ( vroi[ i + 2 ] < 0 )
			{
				print( FATAL, "Negative UR%c value.\n", 'X' + i );
				THROW( EXCEPTION );
			}

			if ( vroi[ i + 2 ] > ( long ) rs_spec10->ccd.max_size[ i ] )
			{
				print( FATAL, "UR%c larger than CCD %c-size.\n",
					   'X' + i, 'X' + i );
				THROW( EXCEPTION );
			}

			if ( vroi[ i ] > vroi[ i + 2 ] )
			{
				print( FATAL, "LL%c larger than UR%c value.\n",
					   'X' + i, 'X' + i );
				THROW( EXCEPTION );
			}
		}
	}

	too_many_arguments( v );

	for ( i = 0; i < 4; i++ )
		rs_spec10->ccd.roi[ i ] = ( uns16 ) vroi[ i ] - 1;
	rs_spec10->ccd.roi_is_set = SET;

	return vars_push( INT_ARR, vroi, 4 );
}


/*-----------------------------------------------------------------------*/
/* Function for setting the binning used with the next image or spectrum */
/* to be returned by the camera. The function expects to get an array of */
/* 2 elements, the vertical and horizontal binning factors. A value of 0 */
/* indicates that the current binning value should remain unchanged.     */
/* Optionally, the function can get passed a second argument, a number   */
/* or string for the type of binning (i.e. in hardware or software) to   */
/* be used.                                                              */
/*-----------------------------------------------------------------------*/

Var *ccd_camera_binning( Var *v )
{
	int i;
	long vbin[ 2 ];


	for ( i = 0; i < 2; i++ )
		vbin[ i ] = ( long ) rs_spec10->ccd.bin[ i ];

	if ( v == NULL )
		return vars_push( INT_ARR, vbin, 2 );

	vars_check( v, INT_ARR | FLOAT_ARR | STR_VAR );

	if ( v->type == STR_VAR )
	{
		if ( strcasecmp( v->val.sptr, "NONE" ) )
		{
			print( FATAL, "Invalid argument string '%s'.\n", v->val.sptr );
			THROW( EXCEPTION );
		}

		vbin[ 0 ] = vbin[ 2 ] = 1;
	}
	else
	{
		if ( v->type == FLOAT_ARR )
			print( WARN, "Float values used as binning factors.\n" );

		if ( v->len > 2 )
			print( WARN, "Argument array has more than the required 2 "
				   "elements, discarding superfluous ones.\n" );

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
	}

	if ( ( v = vars_pop( v ) ) != NULL )
		vars_pop( ccd_camera_binning_method( v ) );

	for ( i = 0; i < 2; i++ )
		rs_spec10->ccd.bin[ i ] = ( uns16 ) vbin[ i ];
	rs_spec10->ccd.bin_is_set = SET;

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

	rs_spec10->ccd.bin_mode_is_set = UNSET;

	return vars_push( INT_VAR, rs_spec10->ccd.bin_mode ? 1L : 0L );
}


/*----------------------------------------------------*/
/* Function for query or setting if the exposure time */
/*----------------------------------------------------*/

Var *ccd_camera_exposure_time( Var *v )
{
	double et;


	if ( v == NULL )
		return vars_push( FLOAT_VAR,
						  rs_spec10->ccd.exp_time * rs_spec10->ccd.exp_res );

	et = get_double( v, "exposure time" );

	rs_spec10->ccd.exp_time = ( uns32 ) lrnd( et / rs_spec10->ccd.exp_res );

	if ( rs_spec10->ccd.exp_time < 1 )
	{
		print( FATAL, "Invalid exposure time of %s.\n",
			   rs_spec10_ptime( et ) );
		THROW( EXCEPTION );
	}

	/* Accepting a maximum exposure time of "only" one hour is a bit arbitrary,
	   but it's hard to imagine that anyone will ever need such a long exposure
	   time... ("640 kB will be enough for everyone" ;-) */

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


/*-----------------------------------------------------*/
/* Function to query or set the number of clear cycles */
/* to be done before an exposure.                      */
/*-----------------------------------------------------*/

Var *ccd_camera_clear_cycles( Var *v )
{
	long count;


	if ( v == 0 )
		return vars_push( INT_VAR, ( long ) rs_spec10->ccd.clear_cycles );

	count = get_strict_long( v, "number of clear cycles" );

	if ( count < CCD_MIN_CLEAR_CYCLES ||
		 count > CCD_MAX_CLEAR_CYCLES )
	{
		print( FATAL, "Invalid number of clear clear cycles, valid range is "
			   "%d to %d.\n", CCD_MIN_CLEAR_CYCLES, CCD_MAX_CLEAR_CYCLES );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE == TEST )
		rs_spec10->ccd.clear_cycles = ( uns16 ) count;
	else
		rs_spec10_clear_cycles( ( uns16 ) count );

	return vars_push( INT_VAR, count );
}


/*-----------------------------------------------*/
/* Function for getting an image from the camera */
/*-----------------------------------------------*/

Var *ccd_camera_get_image( Var *v )
{
	uns16 *frame = NULL;
	long width, height;
	unsigned long max_val;
	Var *nv = NULL;
	long i, j, k, l, row_index;
	uns16 *cf;
	long *dest;
	uns16 bin[ 2 ];
	uns16 urc[ 2 ];
	uns32 size;


	UNUSED_ARGUMENT( v );

	CLOBBER_PROTECT( frame );
	CLOBBER_PROTECT( width );
	CLOBBER_PROTECT( height );
	CLOBBER_PROTECT( nv );
	CLOBBER_PROTECT( bin[ X ] );
	CLOBBER_PROTECT( bin[ Y ] );
	CLOBBER_PROTECT( urc[ X ] );
	CLOBBER_PROTECT( urc[ Y ] );

	/* Store the original binning size and the position of the upper right
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

	if ( rs_spec10->ccd.roi[ X + 2 ] != urc[ X ] ||
		 rs_spec10->ccd.roi[ Y + 2 ] != urc[ Y ] )
		print( SEVERE, "Upper right hand corner of ROI had to changed from "
			   "(%u, %u) to (%u, %u) to fit the binning factors.\n",
			   urc[ X ] + 1, urc[ Y ] + 1, rs_spec10->ccd.roi[ X + 2 ] + 1,
			   rs_spec10->ccd.roi[ Y + 2 ] + 1 );

	/* Now get the picture */

	TRY
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			frame = rs_spec10_get_pic( &size );

			/* There is a bug in the PVCAM library: For some hardware binning
			   sizes the library needs more bytes than are required to hold
			   all points. In these cases the first element(s) of the returned
			   array contains bogus values and the real data start only at
			   some later elements. */

			if ( rs_spec10->ccd.bin_mode == HARDWARE_BINNING )
				cf = frame + size / sizeof *frame - width * height;
			else
				cf = frame;
		}

		else
		{
			max_val = ~ ( uns16 ) 0;

			size = ( uns32 ) ( width * height *sizeof *frame );
			cf = frame = UNS16_P T_malloc( size );
			for ( i = 0; i < width * height; i++ )
				frame[ i ] = random( ) % max_val;
		}

		nv = vars_push_matrix( INT_REF, 2, height, width );

		/* During the test run or for hardware binning or without binning (i.e.
		   if both binning sizes are 1) we can leave most of the work to the
		   camera, otherwise we have to do the binning ourselves. Take care,
		   here we also have to turn the image upside-down or mirror it if
		   required. */

		if ( FSC2_MODE == TEST ||
			 rs_spec10->ccd.bin_mode == HARDWARE_BINNING ||
			 ( rs_spec10->ccd.bin[ X ] == 1 && rs_spec10->ccd.bin[ Y ] == 1 ) )
		{
			for ( i = 0; i < height; i++ )
			{
				if ( RS_SPEC10_UPSIDE_DOWN == 1 )
					row_index = height - 1 - i;
				else
					row_index = i;

				if ( RS_SPEC10_MIRROR == 1 )
				{
					dest = nv->val.vptr[ row_index ]->val.lpnt + width - 1;
					for ( j = 0; j < width; j++ )
						*dest-- = ( long ) *cf++;
				}
				else
				{
					dest = nv->val.vptr[ row_index ]->val.lpnt;
					for ( j = 0; j < width; j++ )
						*dest++ = ( long ) *cf++;
				}
			}
		}
		else
		{
			for ( i = 0; i < height; i++ )
			{
				if ( RS_SPEC10_UPSIDE_DOWN == 1 )
					row_index = height - 1 - i;
				else
					row_index = i;

				for ( j = 0; j < rs_spec10->ccd.bin[ Y ]; j++ )
				{
					if ( RS_SPEC10_MIRROR == 1 )
					{
						dest = nv->val.vptr[ row_index ]->val.lpnt + width - 1;
						for ( k = 0; k < width; k++, dest-- )
							for ( l = 0; l < rs_spec10->ccd.bin[ X ]; l++ )
								*dest += ( long ) *cf++;
					}
					else
					{
						dest = nv->val.vptr[ row_index ]->val.lpnt;
						for ( k = 0; k < width; k++, dest++ )
							for ( l = 0; l < rs_spec10->ccd.bin[ X ]; l++ )
								*dest += ( long ) *cf++;
					}
				}
			}
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
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


/*-------------------------------------------------*/
/* Function for getting a spectrum from the camera */
/*-------------------------------------------------*/

Var *ccd_camera_get_spectrum( Var *v )
{
	uns16 *frame = NULL;
	long width;
	unsigned long max_val;
	Var *nv = NULL;
	long i, j, k;
	uns16 *cf;
	long *dest;
	uns16 bin[ 2 ];
	uns16 urc[ 2 ];
	uns32 size;


	UNUSED_ARGUMENT( v );

	CLOBBER_PROTECT( frame );
	CLOBBER_PROTECT( width );
	CLOBBER_PROTECT( nv );
	CLOBBER_PROTECT( bin[ X ] );
	CLOBBER_PROTECT( bin[ Y ] );
	CLOBBER_PROTECT( urc[ X ] );
	CLOBBER_PROTECT( urc[ Y ] );

	/* Store the original binning sizes and the position of the upper right
	   hand corner, they might become re-adjusted and need to be reset at the
	   end */

	bin[ X ] = rs_spec10->ccd.bin[ X ];
	bin[ Y ] = rs_spec10->ccd.bin[ Y ];
	urc[ X ] = rs_spec10->ccd.roi[ X + 2 ];
	urc[ Y ] = rs_spec10->ccd.roi[ Y + 2 ];

	/* Calculate how many points the image will have after binning */

	width  = ( urc[ X ] - rs_spec10->ccd.roi[ X ] + 1 ) / bin[ X ];

	/* If the binning area is larger than the ROI reduce the binning sizes
	   to fit the the ROI sizes (the binning sizes are reset to their original
	   values before returning from the function) */

	if ( width == 0 )
	{
		rs_spec10->ccd.bin[ X ] =
					 rs_spec10->ccd.roi[ X + 2 ] - rs_spec10->ccd.roi[ X ] + 1;
		width = 1;
	}

	if ( rs_spec10->ccd.bin[ X ] != bin[ X ] )
		print( SEVERE, "Vertical binning parameter had to be changed from %ld "
			   "to %ld because binning width was larger than ROI.\n",
			   bin[ X ], rs_spec10->ccd.bin[ X ] );

	/* Reduce the horizontal ROI area to what we really need (in case the ROI
	   sizes aren't integer multiples of the binning sizes) by moving the 
	   right hand limit nearer to the left side in order to speed up fetching
	   the picture a bit (before the function returns the ROI is set back to
	   it's original size) */

	rs_spec10->ccd.roi[ X + 2 ] =
				 rs_spec10->ccd.roi[ X ] + width * rs_spec10->ccd.bin[ X ] - 1;

	rs_spec10->ccd.bin[ Y ] =
					 rs_spec10->ccd.roi[ Y + 2 ] - rs_spec10->ccd.roi[ Y ] + 1;

	/* Now try to get the picture */

	TRY
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			frame = rs_spec10_get_pic( &size );

			/* There is a bug in the PVCAM library: For some hardware binning
			   sizes the library needs more bytes than are required to hold
			   all points. In these cases the first element(s) of the returned
			   array contains bogus values and the real data start only at
			   some later elements. */

			if ( rs_spec10->ccd.bin_mode == HARDWARE_BINNING )
				cf = frame + size / sizeof *frame - width;
			else
				cf = frame;
		}
		else
		{
			max_val = ~ ( uns16 ) 0;

			size = ( uns32 ) ( width * sizeof *frame );
			cf = frame = UNS16_P T_malloc( size );
			for ( i = 0; i < width; i++ )
				frame[ i ] = random( ) * max_val;
		}

		nv = vars_push( INT_ARR, NULL, width );

		/* During the test run or for hardware binning we can leave most of
		   the work to the camera, otherwise we have to do add up the rows
		   of the image and do the vertical binning ourselves */

		if ( FSC2_MODE == TEST ||
			 rs_spec10->ccd.bin_mode == HARDWARE_BINNING )
			for ( dest = nv->val.lpnt, j = 0; j < width; j++ )
				*dest++ = ( long ) *cf++;
		else
		{
			for ( i = 0; i < rs_spec10->ccd.bin[ Y ]; i++ )
				for ( dest = nv->val.lpnt, j = 0; j < width; j++, dest++ )
					for ( k = 0; k < rs_spec10->ccd.bin[ X ]; k++ )
						*dest += ( long ) *cf++;
		}

		TRY_SUCCESS;
	}
	OTHERWISE
	{
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

	if ( FSC2_MODE == EXPERIMENT )
		rs_spec10_set_temperature( temp );

	rs_spec10->temp.setpoint = temp;
	rs_spec10->temp.is_setpoint = SET;

	return vars_push( FLOAT_VAR, temp );
}


/*--------------------------------------------------*/
/* Function returns the size of a pixel (in meters) */
/*--------------------------------------------------*/

Var *ccd_camera_pixel_size( Var *v )
{
	UNUSED_ARGUMENT( v );

	return vars_push( FLOAT_VAR, RS_SPEC10_PIXEL_SIZE );
}


/*----------------------------------------------*/
/* Function returns an array with the width and */
/* height of the CCD chip as its elements       */
/*----------------------------------------------*/

Var *ccd_camera_pixel_area( Var *v )
{
	Var *cv;


	UNUSED_ARGUMENT( v );

	cv = vars_push( INT_ARR, NULL, 2 );
	cv->val.lpnt[ 0 ] = CCD_PIXEL_WIDTH;
	cv->val.lpnt[ 1 ] = CCD_PIXEL_HEIGHT;

	return cv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
