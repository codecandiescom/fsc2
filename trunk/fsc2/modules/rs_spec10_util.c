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


#include "rs_spec10.h"
#include <dirent.h>


#define C2K_OFFSET   273.16         /* offset between Celsius and Kelvin */


/*------------------------------------------------------------------*/
/* Function for reading in the last stored state of the CCD camera. */
/*------------------------------------------------------------------*/

bool rs_spec10_read_state( void )
{
	char *fn;
	const char *dn;
	FILE *fp;
	int c;
	int i = 0;
	bool in_comment = UNSET;
	bool found_end = UNSET;
	long val;
	int line = 1;


	dn = fsc2_config_dir( );
	fn = get_string( "%s%s%s", dn, slash( dn ), RS_SPEC10_STATE_FILE );

	if ( ( fp = fsc2_fopen( fn, "r" ) ) == NULL )
	{
		print( FATAL, "Can't open state file '%s'.\n", fn );
		T_free( fn );
		THROW( EXCEPTION );
	}

	do
	{
		while ( ( c = fsc2_fgetc( fp ) ) != EOF && c != '\n' && isspace( c ) )
			/* empty */ ;

		switch ( c )
		{
			case EOF :
				found_end = SET;
				break;

			case '#' :
				in_comment = SET;
				break;

			case '\n' :
				line++;
				in_comment = UNSET;
				break;

			default :
				if ( in_comment )
					break;

				if ( ! isdigit( c ) )
				{
					print( FATAL, "Invalid line %d in state file '%s'.\n",
						   line, fn );
					T_free( fn );
					fsc2_fclose( fp );
					THROW( EXCEPTION );
				}

				fsc2_fseek( fp, -1, SEEK_CUR );

				if ( i > 7 || fsc2_fscanf( fp, "%ld", &val ) != 1 )
				{
					print( FATAL, "Invalid line %d in state file '%s'.\n",
						   line, fn );
					T_free( fn );
					fsc2_fclose( fp );
					THROW( EXCEPTION );
				}

				switch ( i )
				{
					case 0 :
						if ( val >= CCD_PIXEL_WIDTH )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.roi_is_set )
							rs_spec10->ccd.roi[ X ] = val;
						break;

					case 1 :
						if ( val >= CCD_PIXEL_HEIGHT )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.roi_is_set )
							rs_spec10->ccd.roi[ Y ] = val;
						break;

					case 2 :
						if ( val < rs_spec10->ccd.roi[ X ] ||
							 val >= CCD_PIXEL_WIDTH )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.roi_is_set )
							rs_spec10->ccd.roi[ X + 2 ] = val;
						break;

					case 3 :
						if ( val < rs_spec10->ccd.roi[ Y ] ||
							 val >= CCD_PIXEL_HEIGHT )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.roi_is_set )
							rs_spec10->ccd.roi[ Y + 2 ] = val;
						break;

					case 4 :
						if ( val < 1 ||
							 ( rs_spec10->ccd.roi[ X + 2 ]
							   - rs_spec10->ccd.roi[ X ] + 1 ) % val != 0 )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.bin_is_set )
							rs_spec10->ccd.bin[ X ] = val;
						break;

					case 5 :
						if ( val < 1 ||
							 ( rs_spec10->ccd.roi[ Y + 2 ]
							   - rs_spec10->ccd.roi[ Y ] + 1 ) % val != 0 )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.bin_is_set )
							rs_spec10->ccd.bin[ Y ] = val;
						break;

					case 6 :
						if ( val != HARDWARE_BINNING &&
							 val != SOFTWARE_BINNING )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						if ( ! rs_spec10->ccd.bin_mode_is_set )
							rs_spec10->ccd.bin_mode = val;
						break;

					case 7 :
						if ( val < CCD_MIN_CLEAR_CYCLES ||
							 val > CCD_MAX_CLEAR_CYCLES )
						{
							print( FATAL, "Invalid line %d in state file "
								   "'%s'.\n", line, fn );
							T_free( fn );
							fsc2_fclose( fp );
							THROW( EXCEPTION );
						}
						rs_spec10->ccd.clear_cycles = val;
						break;
				}
				i++;
				break;
		}

	} while ( ! found_end );

	fsc2_fclose( fp );
	T_free( fn );

	return OK;
}


/*------------------------------------------------------------*/
/* Function for writing the state of the CCD camera to a file */
/*------------------------------------------------------------*/

bool rs_spec10_store_state( void )
{
	char *fn;
	const char *dn;
	uns16 bin[ 2 ];
	uns16 urc[ 2 ];
	long width, height;
	FILE *fp;


	dn = fsc2_config_dir( );
	fn = get_string( "%s%s%s", dn, slash( dn ), RS_SPEC10_STATE_FILE );

	if ( ( fp = fsc2_fopen( fn, "w" ) ) == NULL )
	{
		print( SEVERE, "Can't store state data in '%s'.\n", fn );
		return FAIL;
	}

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

	/* Reduce the ROI to the area we really need (in case the ROI sizes aren't
	   integer multiples of the binning sizes) by moving the upper right hand
	   corner nearer to the lower left hand corner in order to speed up
	   fetching the picture a bit */

	urc[ X ] = rs_spec10->ccd.roi[ X ] + width * rs_spec10->ccd.bin[ X ] - 1;
	urc[ Y ] = rs_spec10->ccd.roi[ Y ] + height * rs_spec10->ccd.bin[ Y ] - 1;

	fsc2_fprintf( fp, "# --- Do *not* edit this file, it gets created "
				  "automatically ---\n\n"
				  "# In this file state information for the rs_spec10 "
				  "driver is stored:\n"
				  "# 1. ROI (LLX, LLY, URX, URY)\n"
				  "# 2. Binning factors\n"
				  "# 3. Binning mode (hardware = 0, software = 1)\n"
				  "# 4. Number of pre-exposure clear cycles\n\n"
				  "%u %u %u %u\n%u %u\n%u\n%u\n",
				  rs_spec10->ccd.roi[ X ], rs_spec10->ccd.roi[ Y ],
				  urc[ X ], urc[ Y ], bin[ X ], bin[ Y ],
				  rs_spec10->ccd.bin_mode, rs_spec10->ccd.clear_cycles );

	fsc2_fclose( fp );
	return OK;
}


/*---------------------------------------------------------------------*/
/* Function for converting a temperature from Kelvin to degree Celsius */
/*---------------------------------------------------------------------*/

double rs_spec10_k2c( double tk )
{
	return tk - C2K_OFFSET;
}


/*---------------------------------------------------------------------*/
/* Function for converting a temperature from degree Celsius to Kelvin */
/*---------------------------------------------------------------------*/

double rs_spec10_c2k( double tc )
{
	return tc + C2K_OFFSET;
}


/*-------------------------------------------------*/
/* Function for converting a temperature in Kelvin */
/* to a value that can be send to the device.      */
/*-------------------------------------------------*/

int16 rs_spec10_k2ic( double tk )
{
	return ( int16 ) lrnd( 100.0 * ( tk - C2K_OFFSET ) );
}


/*-------------------------------------------------------*/
/* Function for converting a temperature from the value  */
/* returned from the device into a temperature in Kelvin */
/*-------------------------------------------------------*/

double rs_spec10_ic2k( int16 tci )
{
	return 0.01 * tci + C2K_OFFSET;
}


/*----------------------------------------------------------*/
/* Function for testing if a certain attribute is available */
/* and if it can be only read, only written or both. It     */
/* returns FAIL if the attribute isn't available, otherwise */
/* OK and the 'acc' pointer is set to either ACC_READ_ONLY, */
/* ACC_WRITE_ONLY or ACC_READ_WRITE.                        */
/*----------------------------------------------------------*/

bool rs_spec10_param_access( uns32 param, uns16 *acc )
{
	boolean avail;

	if ( ! pl_get_param( rs_spec10->handle, param, ATTR_AVAIL,
						 ( void_ptr ) &avail ) )
		rs_spec10_error_handling( );

	if ( ! avail )
		return FAIL;

	if ( ! pl_get_param( rs_spec10->handle, param, ATTR_ACCESS,
						 ( void_ptr ) acc ) )
		rs_spec10_error_handling( );

	return OK;
}


/*-------------------------------------------------------------------*/
/* Function for writing a time value into a string in a pretty form. */
/* Please note that the buffer for that string is static, i.e. the   */
/* string is overwritten on each invokation of the function.         */
/*-------------------------------------------------------------------*/

const char *rs_spec10_ptime( double p_time )
{
	static char buffer[ 128 ];

	if ( fabs( p_time ) >= 1.0 )
		sprintf( buffer, "%g s", p_time );
	else if ( fabs( p_time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * p_time );
	else
		sprintf( buffer, "%g us", 1.e6 * p_time );

	return buffer;
}


/*--------------------------------------------------------------------*/
/* The following functions are part of a dirty hack to get around the */
/* problem that the PVCAM library does not set the close-on-exec flag */
/* for the device file. The first function builds a list of all open  */
/* file descriptor from the entries in the /proc/self/fd directory of */
/* the current process. Then the function for initialisation of the   */
/* board gets called, opening the device file. Afterwards, the second */
/* function is invoked that compares the fd's in the list with the    */
/* entries in the /proc/self/fd directory and sets the close-on-exec  */
/* flag for the new fd (i.e. the fd that wasn't already in the old    */
/* list). Of course this will only work on Linux but fails (hopefully */
/* benignly) on other systems.                                        */
/*--------------------------------------------------------------------*/

int *rs_spec10_get_fd_list( void )
{
	DIR *dir;
	struct dirent *de;
	int *fd_list = NULL;
	int num_fds = 0;
	char *dn;
	int fd;
	struct stat buf;


	CLOBBER_PROTECT( fd_list );
	CLOBBER_PROTECT( num_fds );

	if ( ( dir = opendir( "/proc/self/fd" ) ) == NULL )
		return NULL;

	while ( ( de = readdir( dir ) ) != NULL )
	{
		for ( dn = de->d_name; *dn && isdigit( *dn ); dn++ )
			/* empty */ ;

		if ( *dn != '\0' )
			continue;

		TRY
		{
			fd = T_atoi( de->d_name );
			if ( fstat( fd, &buf ) == -1 )
				THROW( EXCEPTION );

			if ( S_ISDIR( buf.st_mode ) )
				continue;

			fd_list = INT_P T_realloc( fd_list,
									   ( num_fds + 2 ) * sizeof *fd_list );
			fd_list[ num_fds++ ] = fd;
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			if ( fd_list != NULL )
				T_free( fd_list );
			closedir( dir );
			return NULL;
		}

		fd_list[ num_fds ] = -1;
	}

	closedir( dir );
	return fd_list;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void rs_spec10_close_on_exec_hack( int *fd_list )
{
	int *new_fd_list;
	int i, j;
	int flags;
	bool found;


	if ( fd_list == NULL )
		return;

	/* Get list of open file descriptors */

	if ( ( new_fd_list = rs_spec10_get_fd_list( ) ) == NULL )
	{
		T_free( fd_list );
		return;
	}

	/* Check for files that weren't in the old list and set the close-on-exec
	   flag for these files (there should be only one...) */

	for ( i = 0; new_fd_list[ i ] != -1; i++ )
	{
		for ( found = UNSET, j = 0; fd_list[ j ] != -1 && ! found; j++ )
			found = new_fd_list[ i ] == fd_list[ j ];

		if ( ! found &&
			 ( flags = fcntl( new_fd_list [ i ], F_GETFD, 0 ) ) != -1 )
			fcntl( new_fd_list [ i ], F_SETFD, flags | FD_CLOEXEC );
	}

	T_free( new_fd_list );
	T_free( fd_list );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
