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
#include <dirent.h>


#define C2K_OFFSET   273.16         /* offset between Celsius and Kelvin */


/*--------------------------------------------------*/
/*--------------------------------------------------*/

double rs_spec10_k2c( double tk )
{
	return tk - C2K_OFFSET;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

double rs_spec10_c2k( double tc )
{
	return tc + C2K_OFFSET;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

int16 rs_spec10_k2ic( double tk )
{
	return ( int16 ) lrnd( 100.0 * ( tk - C2K_OFFSET ) );
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

double rs_spec10_ic2k( int16 tci )
{
	return 0.01 * tci + C2K_OFFSET;
}


/*--------------------------------------------------*/
/*--------------------------------------------------*/

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


/*----------------------------------------------------*/
/*----------------------------------------------------*/

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

			fd_list = T_realloc( fd_list, ( num_fds + 2 ) * sizeof *fd_list );
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
