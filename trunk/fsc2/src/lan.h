/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


#if ! defined LAN_HEADER
#define LAN_HEADER

#include <netinet/in.h>         /* needed for struct in_addr */


typedef struct LAN_List LAN_List_T;

struct LAN_List {
    const char *   name;
    int            fd;
    struct in_addr address;
    int            port;
    long           us_read_timeout;
    long           us_write_timeout;
    bool           so_timeo_avail;
    LAN_List_T *   next;
    LAN_List_T *   prev;
};


int fsc2_lan_open( const char * /* dev_name */,
                   const char * /* address */,
                   int          /* port */,
                   long         /* us_timeout */,
                   bool         /* quit_on_signal */ );

int fsc2_lan_close( int /* handle */ );

ssize_t fsc2_lan_write( int          /* handle */,
                        const char * /* buffer */,
                        long         /* length */,
                        long         /* us_timeout */,
                        bool         /* quit_on_signal */ );

ssize_t fsc2_lan_writev( int                  /* handle */,
                         const struct iovec * /* data */,
                         int                  /* count */,
                         long                 /* us_timeout */,
                         bool                 /* quit_on_signal */ );

ssize_t fsc2_lan_read( int    /* handle */,
                       char * /* buffer */,
                       long   /* length */,
                       long   /* us_timeout */,
                       bool   /* quit_on_signal */ );

ssize_t fsc2_lan_readv( int            /* handle */,
                        struct iovec * /* data */,
                        int            /* count */,
                        long           /* us_timeout */,
                        bool           /* quit_on_signal */ );

void fsc2_lan_log_message( const char * /* fmt */,
                           ... );

void fsc2_lan_cleanup( void );

void fsc2_lan_exp_init( const char * /* log_file_name */,
                        int          /* log_level */ );


#endif   /* ! LAN_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 * tab-width: 4
 * indent-tabs-mode: nil
 */

