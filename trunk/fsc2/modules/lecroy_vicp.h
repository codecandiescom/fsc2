/* -*-C-*-
 *
 *  $Id$
 *
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


#include "fsc2_module.h"


#if ! defined LECROY_VICP_HEADER
#define LECROY_VICP_HEADER


#define SUCCESS            0
#define SUCCESS_BUT_MORE   1
#define FAILURE           -1


void lecroy_vicp_init( const char * /* dev_name       */,
                       const char * /* address        */,
                       long         /* us_timeout     */,
                       bool         /* quit_on_signal */ );

void lecroy_vicp_close( void );

void lecroy_vicp_lock_out( bool /* lock_state */ );

void lecroy_vicp_set_timeout( int  /* dir        */,
                              long /* us_timeout */ );

int lecroy_vicp_write( const char * /* buffer         */,
                       ssize_t    * /* length         */,
                       bool         /* with_eoi       */,
                       bool         /* quit_on_signal */ );

int lecroy_vicp_read( char *    /* buffer         */,
                      ssize_t * /* length         */,
                      bool    * /* with_eoi       */,
                      bool      /* quit_on_signal */ );

void lecroy_vicp_device_clear( void );


#endif /* ! LECROY_VICP_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
