/*
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


#pragma once
#if ! defined LAN_HEADER
#define LAN_HEADER

#include <netinet/in.h>         /* needed for struct in_addr */


/* Definition of log levels. Since they already may have been
   defined in the GPIB or serial port module only define them
   only if they aren't already known */

#if ! defined LL_NONE
#define  LL_NONE  0    /* log nothing */
#endif
#if ! defined LL_ERR
#define  LL_ERR   1    /* log errors only */
#endif
#if ! defined LL_CE
#define  LL_CE    2    /* log function calls and function exits */
#endif
#if ! defined LL_ALL
#define  LL_ALL   3    /* log calls with parameters and function exits */
#endif


typedef struct LAN_List LAN_List_T;

struct LAN_List {
    const char     * name;
    int              fd;
    struct in_addr   address;
    int              port;
    long             us_read_timeout;
    long             us_write_timeout;
    bool             so_timeo_avail;
	FILE           * log_fp;
    LAN_List_T     * next;
    LAN_List_T     * prev;
};


int fsc2_lan_open( const char * /* dev_name       */,
                   const char * /* address        */,
                   int          /* port           */,
                   long         /* us_timeout     */,
                   bool         /* quit_on_signal */  );

int fsc2_lan_close( int /* handle */ );

ssize_t fsc2_lan_write( int          /* handle         */,
                        const char * /* buffer         */,
                        long         /* length         */,
                        long         /* us_timeout     */,
                        bool         /* quit_on_signal */  );

ssize_t fsc2_lan_writev( int                  /* handle         */,
                         const struct iovec * /* data           */,
                         int                  /* count          */,
                         long                 /* us_timeout     */,
                         bool                 /* quit_on_signal */  );

ssize_t fsc2_lan_read( int    /* handle         */,
                       char * /* buffer         */,
                       long   /* length         */,
                       long   /* us_timeout     */,
                       bool   /* quit_on_signal */  );

ssize_t fsc2_lan_readv( int            /* handle         */,
                        struct iovec * /* data           */,
                        int            /* count          */,
                        long           /* us_timeout     */,
                        bool           /* quit_on_signal */  );

FILE * fsc2_lan_open_log( const char * /* dev_name */ );

FILE * fsc2_lan_close_log( const char * /* dev_name */,
						   FILE       * /* fp */ );

void fsc2_lan_log_message( FILE       * /* fp  */,
						   const char * /* fmt */,
						   ...                        );

void fsc2_lan_log_function_start( FILE       * /* handle   */,
								  const char * /* function */,
								  const char * /* dev_name */ );

void fsc2_lan_log_function_end( FILE *       /* fp       */,
								const char * /* function */,
								const char * /* dev_name */ );

void fsc2_lan_log_data( FILE       * /* fp     */,
						long         /* length */,
						const char * /* buffer */ );

int fsc2_lan_log_level( void );

void fsc2_lan_cleanup( void );


#endif   /* ! LAN_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 * tab-width: 4
 * indent-tabs-mode: nil
 */

