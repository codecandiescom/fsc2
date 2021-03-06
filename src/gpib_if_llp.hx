/* -*-C-*-
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/*------------------------------------------------------------------------*
 * Contains all declarations possibly needed by routines using the GPIB
 * functions. Each of the functions returns either SUCCESS or FAILURE, in
 * the latter case in 'gpib_error_msg' a short text is stored explaining
 * the cause of the error.
 *------------------------------------------------------------------------*/


#pragma once
#if ! defined GPIB_IF_LLP_HEADER
#define GPIB_IF_LLP_HEADER


/* If ib.h can't be found set the variable GPIB_HEADER_FILE in the top level
   Makefile to the full path of the directory where this header file resides */

#include "gpibd.h"
#include <ib.h>

#include <dirent.h>


#if ! defined IBERR
#define IBERR ERR
#endif


#define DMA_SIZE 64512    /* compare this with entry in /etc/gpib.conf ! */

typedef struct GPIB_Dev GPIB_Dev_T;

struct GPIB_Dev {
    int number;               /* device number */
    char *name;               /* symbolic name of device */
    GPIB_Dev_T *next;         /* pointer to next GPIB_Dev structure */
    GPIB_Dev_T *prev;         /* pointer to previous GPIB_Dev structure */
};


int gpib_init( const char * /* log_file_name */,
               int          /* log_level     */ );
int gpib_shutdown( void );
int gpib_init_device( const char * /* device_name */,
                      int *        /* dev         */ );
int gpib_remove_device( int /* device */ );
int gpib_timeout( int /* device */,
                  int /* period */ );
int gpib_clear_device( int /* device */ );
int gpib_local( int /* device */ );
int gpib_local_lockout( int /* device */ );
int gpib_trigger( int /* device */ );
int gpib_wait( int   /* device */,
               int   /* mask   */,
               int * /* status */ );
int gpib_write( int          /* device */,
                const char * /* buffer */,
                long         /* length */ );
int gpib_read( int    /* device */,
               char * /* buffer */,
               long * /* length */ );
int gpib_serial_poll( int             /* device */,
                      unsigned char * /* stb    */ );
void gpib_log_message( const char * /* fmt */,
                       ... );


#define SUCCESS   0
#define FAILURE  -1


/*----------------------------------------------------------*
 * Definition of log levels allowed in calls of gpib_init()
 *----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */


#endif /* ! GPIB_IF_LLP_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
