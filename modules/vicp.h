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
#if ! defined VICP_HEADER
#define VICP_HEADER


#include "fsc2_module.h"


#define SUCCESS            0
#define SUCCESS_BUT_MORE   1
#define FAILURE           -1


void vicp_open( const char    * /* dev_name       */,
                const char    * /* address        */,
                volatile long   /* us_timeout     */,
                bool            /* quit_on_signal */  );

void vicp_close( void );

void vicp_lock_out( bool /* lock_state */ );

void vicp_set_timeout( int  /* dir        */,
                       long /* us_timeout */ );

int vicp_write( const char * /* buffer         */,
                ssize_t    * /* length         */,
                bool         /* with_eoi       */,
                bool         /* quit_on_signal */  );

int vicp_read( char    * /* buffer         */,
               ssize_t * /* length         */,
               bool    * /* with_eoi       */,
               bool      /* quit_on_signal */  );

void vicp_device_clear( void );


#endif /* ! VICP_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
