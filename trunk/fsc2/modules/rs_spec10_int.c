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


#include "rs_spec10.h"
#include <sys/mman.h>


static void rs_spec10_ccd_init( void );
static void rs_spec10_temperature_init( void );


/*-----------------------------------------*/
/*-----------------------------------------*/

void rs_spec10_init_camera( void )
{
	int16 total_cams;
	char cam_name[ CAM_NAME_LEN ];
	int16 i;


	/* Try to figure out how many cameras are attached to the system */

	if ( ! pl_cam_get_total( &total_cams ) )
		rs_spec10_error_handling( );

	/* Loop over the cameras until we find the one we're looking for */

	for ( i = 0; i < total_cams; i++ )
	{
		pl_cam_get_name( i, cam_name );
		if ( ! strcmp( cam_name, rs_spec10->dev_file ) )
			break;
	}

	/* Check if we found it, otherwise throw an exception */

	if ( i == total_cams )
	{
		print( FATAL, "No camera device file '/dev/%s' found.\n",
			   rs_spec10->dev_file );
		THROW( EXCEPTION );
	}

	/* Try to get a handle for the camera */

	if ( ! pl_cam_open( ( char * ) rs_spec10->dev_file, &rs_spec10->handle,
						OPEN_EXCLUSIVE ) )
		rs_spec10_error_handling( );

	rs_spec10->is_open = SET;

	/* Do a simple checks */

	if ( ! pl_cam_check( rs_spec10->handle ) )
		rs_spec10_error_handling( );

	rs_spec10_ccd_init( );
	rs_spec10_temperature_init( );
}


/*-----------------------------------------*/
/*-----------------------------------------*/

static void rs_spec10_ccd_init( void )
{
	uns32 set_uns32;
	flt64 ret_flt64;

	uns16 acc;
	uns16 num_pix;
	uns32 clear_mode;


	if ( ! rs_spec10_param_access( PARAM_SER_SIZE, &acc ) ||
		 ( acc != ACC_READ_ONLY && acc != ACC_READ_WRITE ) ||
		 ! rs_spec10_param_access( PARAM_PAR_SIZE, &acc ) ||
		 ( acc != ACC_READ_ONLY && acc != ACC_READ_WRITE ) )
	{
		print( FATAL, "Can't determine number of pixels of CCD\n" );
		THROW( EXCEPTION );
	}

	/* Get the the number of pixels in x- and y-direction */

	if ( ! pl_get_param( rs_spec10->handle, PARAM_SER_SIZE, ATTR_DEFAULT,
						 ( void_ptr ) &num_pix ) )
		rs_spec10_error_handling( );
	rs_spec10->ccd.max_size[ X ] = num_pix;

	if ( ! pl_get_param( rs_spec10->handle, PARAM_PAR_SIZE, ATTR_DEFAULT,
						 ( void_ptr ) &num_pix ) )
		rs_spec10_error_handling( );
	rs_spec10->ccd.max_size[ Y ] = num_pix;

	/* Make sure the sizes are identical to what the configuration file
	   claims, we did rely on the values during the test run */

	if ( rs_spec10->ccd.max_size[ X ] != CCD_PIXEL_WIDTH ||
		 rs_spec10->ccd.max_size[ Y ] != CCD_PIXEL_HEIGHT )
	{
		print( FATAL, "Configuration file for camera has invalid CCD "
			   "size, real size is %ldx%ld.\n",
			   ( long ) rs_spec10->ccd.max_size[ X ],
			   ( long ) rs_spec10->ccd.max_size[ Y ] );
		THROW( EXCEPTION );
	}

	/* If the clear mode can be set use CLEAR_PRE_EXPOSURE mode if (since we
	   don't have a shutter and time between exposures is probably rather long
	   this seems to be the most reasonable mode) */

	if ( rs_spec10_param_access( PARAM_CLEAR_MODE, &acc ) &&
		 ( acc == ACC_READ_WRITE || acc == ACC_WRITE_ONLY ) )
	{
		clear_mode = CLEAR_PRE_EXPOSURE;
		if ( ! pl_set_param( rs_spec10->handle, PARAM_CLEAR_MODE, 
							 ( void_ptr ) &clear_mode ) )
			rs_spec10_error_handling( );
	}

	/* Set the time resolution for the exposure time to 1 us. The exposure
	   time argument of pl_exp_setup_seq() must thus be in units of 1 us. */

	if ( ! rs_spec10_param_access( PARAM_EXP_RES, &acc ) )
	{
		print( FATAL, "Can't determine exposure time resolution.\n" );
		THROW( EXCEPTION );
	}

	if ( acc == ACC_READ_WRITE || acc == ACC_WRITE_ONLY )
	{
		set_uns32 = EXP_RES_ONE_MICROSEC;
		if ( ! pl_set_param( rs_spec10->handle, PARAM_EXP_RES,
							 ( void_ptr ) &set_uns32 ) )
			rs_spec10_error_handling( );
	}
	else if ( acc == ACC_READ_ONLY &&
			  ! pl_get_param( rs_spec10->handle, PARAM_EXP_RES, ATTR_DEFAULT,
							  ( void_ptr ) &set_uns32 ) )
		rs_spec10_error_handling( );

//	  if ( ! pl_get_param( rs_spec10->handle, PARAM_EXP_MIN_TIME, ATTR_ACCESS,
//						   ( void_ptr ) &ret_uns16 ) )
//		  rs_spec10_error_handling( );
//
//	  if ( ret_uns16 == ACC_READ_WRITE || ret_uns16 == ACC_READ_ONLY )
//	  {
//		  if ( ! pl_get_param( rs_spec10->handle, PARAM_EXP_MIN_TIME,
//							   ATTR_DEFAULT, ( void_ptr ) &ret_flt64 ) )
//			  rs_spec10_error_handling( );
//
//		  fprintf( stderr, "EXP_MIN_TIME = %f\n", ( double ) ret_flt64 );
//	  }


	/* We might also need to set the shutter mode to OPEN_NO_CHANGE,
	   set the number of clears (with PARAM_CLEAR_CYCLES).
	   What about gain settings ?
	   We probably also should look how long the speed table is...
	   But all of this can only be done when we have the camera. */
}


/*-----------------------------------------*/
/*-----------------------------------------*/

static void rs_spec10_temperature_init( void )
{
	uns16 acc;
	int16 temp;
	long min_temp, max_temp;


	/* Determine if reading and setting a setpoint is possible */

	if ( ! rs_spec10_param_access( PARAM_TEMP_SETPOINT, &acc ) )
	{
		print( FATAL, "Can't set a temperature setpoint.\n" );
		THROW( EXCEPTION );
	}

	rs_spec10->temp.acc_setpoint = acc;

	/* Get maximum and minimum temperature that can be set and check that the
	   values in the configuration file are correct, we did rely on them in
	   the PREAPARATIONS section and during the test run */

	if ( rs_spec10->temp.acc_setpoint == ACC_READ_ONLY ||
		 rs_spec10->temp.acc_setpoint == ACC_READ_WRITE )
	{
		if ( ! pl_get_param( rs_spec10->handle, PARAM_TEMP_SETPOINT, ATTR_MAX,
							 ( void_ptr ) &temp ) )
			rs_spec10_error_handling( );
		max_temp = temp;

		if ( ! pl_get_param( rs_spec10->handle, PARAM_TEMP_SETPOINT, ATTR_MIN,
							 ( void_ptr ) &temp ) )
			rs_spec10_error_handling( );
		min_temp = ( long )temp;

		if ( max_temp != lrnd( CCD_MAX_TEMPERATURE * 100.0 ) ||
			 min_temp != lrnd( CCD_MIN_TEMPERATURE * 100.0 ) )
		{
			print( FATAL, "Configuration file for camera has invalid CCD "
				   "temperature range, valid range is %.2f K (%.2fC) to "
				   "%.2f K (%.2f C).\n", CCD_MIN_TEMPERATURE,
				   rs_spec10_k2c( CCD_MIN_TEMPERATURE ), CCD_MAX_TEMPERATURE,
				   rs_spec10_k2c( CCD_MAX_TEMPERATURE ) );
			THROW( EXCEPTION );
		}
	}


	if ( rs_spec10->temp.is_setpoint &&
		 rs_spec10->temp.acc_setpoint != ACC_READ_WRITE &&
		 rs_spec10->temp.acc_setpoint != ACC_WRITE_ONLY )
	{
		print( FATAL, "During the PREPARATIONS section or the test run a "
			   "temperature setpoint was set, but camera doesn't allow to "
			   "do so.\n" );
		THROW( EXCEPTION );
	}

	/* Set a target temperature if this was requested in the PREPARATIONS
	   section */

	if ( rs_spec10->temp.is_setpoint )
		rs_spec10_set_temperature( rs_spec10->temp.setpoint );

	/* Check if we can read the current temperature and if yes get it */

	if ( ! rs_spec10_param_access( PARAM_TEMP, &acc ) ||
		 ! ( acc == ACC_READ_ONLY || acc == ACC_READ_WRITE ) )
	{
		print( FATAL, "Can't determine the current temperature.\n" );
		THROW( EXCEPTION );
	}

	rs_spec10_get_temperature( );
}


/*-----------------------------------------*/
/*-----------------------------------------*/

uns16 *rs_spec10_get_pic( void )
{
	rgn_type region;
    uns32 size;
	uns16 *frame;
	int16 status;
	uns32 dummy;
	char cur_dir[ PATH_MAX ];


	getcwd( cur_dir, PATH_MAX );
	chdir( "/usr/local/lib" );

	region.s1   = rs_spec10->ccd.roi[ 0 ];
	region.p1   = rs_spec10->ccd.roi[ 1 ];
	region.s2   = rs_spec10->ccd.roi[ 2 ];
	region.p2   = rs_spec10->ccd.roi[ 3 ];

	if ( rs_spec10->ccd.bin_mode == HARDWARE_BINNING )
	{
		region.sbin = rs_spec10->ccd.bin[ 0 ];
		region.pbin = rs_spec10->ccd.bin[ 1 ];
	}
	else
	{
		region.sbin = 1;
		region.pbin = 1;
	}

	if ( ! pl_exp_init_seq( ) )
		rs_spec10_error_handling( );

	if ( ! pl_exp_setup_seq( rs_spec10->handle, 1, 1, &region, TIMED_MODE,
							 rs_spec10->ccd.exp_time, &size ) )
	{
		pl_exp_uninit_seq( );
		rs_spec10_error_handling( );
	}

#ifndef NDEBUG
	if ( size == 0 )
	{
		print( FATAL, "Internal error detected at %s:%d.\n",
			   __FILE__, __LINE__ );
		pl_exp_abort( rs_spec10->handle, CCS_HALT );
		pl_exp_uninit_seq( );
		THROW( EXCEPTION );
	}
#endif

	/* Now we need memory for storing the new picture. The manual requires
	   this memory to be protected against swapping, thus we must mlock()
	   it. */

	TRY
	{
		frame = T_malloc( ( size_t ) size );
//		  if ( mlock( frame, size ) != 0 )
//		  {
//            T_free( frame );
//			  print( FATAL, "Failure to obtain properly protected memory.\n" );
//			  THROW( EXCEPTION );
//		  }
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		pl_exp_abort( rs_spec10->handle, CCS_HALT );
		pl_exp_uninit_seq( );
		RETHROW( );
	}

	if ( ! pl_exp_start_seq( rs_spec10->handle, frame ) )
	{
		chdir( cur_dir );
		pl_exp_abort( rs_spec10->handle, CCS_HALT );
		pl_exp_uninit_seq( );
//		munlock( frame, size );
		T_free( frame );
		rs_spec10_error_handling( );
	}

	/* Loop until data are available */

	TRY
	{
		do
		{
			stop_on_user_request( );
			if ( ! pl_exp_check_status( rs_spec10->handle, &status, &dummy ) )
				rs_spec10_error_handling( );
		} while ( status != READOUT_COMPLETE );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		pl_exp_abort( rs_spec10->handle, CCS_HALT );
		pl_exp_uninit_seq( );
//		munlock( frame, size );
		T_free( frame );
		chdir( cur_dir );
		RETHROW( );
	}

    pl_exp_finish_seq( rs_spec10->handle, frame, 0 );
	pl_exp_uninit_seq();

//	munlock( frame, size );

	chdir( cur_dir );
	return frame;
}


/*-----------------------------------------------*/
/* Returns the current temperature of the camera */
/*-----------------------------------------------*/

double rs_spec10_get_temperature( void )
{
	int16 itemp;


	/* Get the current temperature */

	if ( ! pl_get_param( rs_spec10->handle, PARAM_TEMP, ATTR_CURRENT,
						 ( void_ptr ) &itemp ) )
		rs_spec10_error_handling( );

	/* Return current temperature, converted to Kelvin */

	return rs_spec10_ic2k( itemp );
}


/*------------------------------------------*/
/* Sets a target temperature for the camera */
/*------------------------------------------*/

double rs_spec10_set_temperature( double temp )
{
	int16 itemp;


	itemp = rs_spec10_k2ic( temp );

	/* A bit of paranoia... */

	fsc2_assert( FSC2_MODE == EXPERIMENT &&
				 itemp <= lrnd( CCD_MAX_TEMPERATURE * 100.0 ) &&
				 itemp >= lrnd( CCD_MIN_TEMPERATURE * 100.0 ) );

	if ( ! pl_set_param( rs_spec10->handle, PARAM_TEMP_SETPOINT, 
						 &itemp ) )
		rs_spec10_error_handling( );

	/* Return the new setpoint */

	return rs_spec10_ic2k( itemp );
}


/*-----------------------------------------*/
/*-----------------------------------------*/

void rs_spec10_error_handling( void )
{
	char pcam_err_msg[ ERROR_MSG_LEN ];


	pl_error_message( pl_error_code( ), pcam_err_msg );
	print( FATAL, "%s\n", pcam_err_msg );

	if ( rs_spec10->is_open )
	{
		pl_cam_close( rs_spec10->handle );
		rs_spec10->is_open = UNSET;
	}

	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
