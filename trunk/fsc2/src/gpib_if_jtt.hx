/* -*-C-*-
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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

/*------------------------------------------------------------------------*/
/* Contains all declarations possibly needed by routines using the GPIB   */
/* functions. Each of the functions returns either SUCCESS or FAILURE, in */
/* the latter case in 'gpib_error_msg' a short text is stored explaining  */
/* the cause of the error.                                                */
/*------------------------------------------------------------------------*/


#include <gpib.h>

#define GPIB_JTT


#if defined ( __GPIB_IF__ )
	#define GPIB_VARIABLE
#else
	#define GPIB_VARIABLE extern
#endif


#define DMA_SIZE 64512    /* compare this with entry in /etc/gpib.conf ! */


typedef struct _gd_
{
	int number;           /* device number */
    char *name;           /* symbolic name of device */
	struct _gd_ *next;    /* pointer to next GPIB_DEV structure */
	struct _gd_ *prev;    /* pointer to previous GPIB_DEV structure */
} GPIB_DEV;


GPIB_VARIABLE int gpib_init( const char *log_file_name, int log_level );
GPIB_VARIABLE int gpib_shutdown( void );
GPIB_VARIABLE int gpib_init_device( const char *device_name, int *dev );
GPIB_VARIABLE int gpib_timeout( int device, int period );
GPIB_VARIABLE int gpib_clear_device( int device );
GPIB_VARIABLE int gpib_local( int device );
GPIB_VARIABLE int gpib_trigger( int device );
GPIB_VARIABLE int gpib_wait( int device, int mask, int *status );
GPIB_VARIABLE int gpib_write( int device, const char *buffer, long length );
GPIB_VARIABLE int gpib_read( int device, char *buffer, long *length );
GPIB_VARIABLE int gpib_serial_poll( int device, unsigned char *stb );
GPIB_VARIABLE void gpib_log_message( const char *fmt, ... );


GPIB_VARIABLE char gpib_error_msg[ 1024 ]; /* global for GPIB error messages */


#define SUCCESS   0
#define FAILURE  -1


#ifdef __cplusplus
#define GPIB_DEV_P ( GPIB_DEV * )
#else
#define GPIB_DEV_P
#endif


/*----------------------------------------------------------*/
/* definition of log levels allowed in calls of gpib_init() */
/*----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
