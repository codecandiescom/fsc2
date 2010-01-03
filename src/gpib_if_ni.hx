/* -*-C-*-
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


/*------------------------------------------------------------------------*
 * Contains all declarations possibly needed by routines using the GPIB
 * functions. Each of the functions returns either SUCCESS or FAILURE, in
 * the latter case in 'gpib_error_msg' a short text is stored explaining
 * the cause of the error.
 *------------------------------------------------------------------------*/


#if ! defined GPIB_IF_NI_HEADER
#define GPIB_IF_NI_HEADER

#if defined GPIB_LIBRARY_NI
#include <ni488.h>
#else
#include <sys/ugpib.h>
#endif

#include <dirent.h>

#define GPIB_NI


#define GPIB_MAX_DEV      30
#define GPIB_NAME_MAX     14
#define GPIB_MAX_INIT_DEV 14

/* End-of-string (EOS) modes */

#define EOT         0x01            /* Send END with last byte    */
#define IS_MASTER   ( 1 << 1 )


#define ADDRESS( pad, sad ) \
        ( ( ( pad ) & 0xff ) | ( ( ( sad ) & 0xff ) << 8 ) )

typedef struct GPIB_Dev GPIB_Dev_T;

struct GPIB_Dev {
    int is_online;
    int number;
    char name[ GPIB_NAME_MAX + 1 ];
    int pad;
    int sad;
    int eos;
    int eosmode;
    int timo;
    int flags;
};


#if ! defined IBERR
#define IBERR ERR
#endif


#if ! defined GPIB_CONF_FILE
#define GPIB_CONF_FILE  "/etc/gpib.conf"
#endif


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
 * definition of log levels allowed in calls of gpib_init()
 *----------------------------------------------------------*/

#define  LL_NONE  0    /* log nothing */
#define  LL_ERR   1    /* log errors only */
#define  LL_CE    2    /* log function calls and function exits */
#define  LL_ALL   3    /* log calls with parameters and function exits */


/*-------------------------------*
 * Definitions of utility macros
 *-------------------------------*/

#define GPIB_IS_TIMEOUT    ( ( ibsta & TIMO ) ? 1 : 0 )


#endif /* ! GPIB_IF_NI_HEADER */

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
