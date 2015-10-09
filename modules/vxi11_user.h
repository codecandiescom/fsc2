/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
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
 *
 *
 *  The C++ source code of the implementations written by Steve D. Sharples
 *  from Nottingham University (steve.sharples@nottingham.ac.uk), see
 *
 *  http://optics.eee.nottingham.ac.uk/vxi11/
 *
 *  was of great help in writing this implementation and is gratefully
 *  acknowleged.
 *
 *  For more information about the VXI-11 protocol see the "VMEbus Extensions
 *  for Instrumentation, TCP/IP Instroment Protocol Specifications" that can
 *  be downloaded from
 *
 *  http://www.vxibus.org/files/VXI_Specs/VXI-11.zip
 */


#pragma once
#if ! defined VX11_USER_HEADER
#define VX11_USER_HEADER

#include "fsc2.h"
#include <rpc/rpc.h>
#include "vxi11.h"


#define VXI11_SUCCESS            0
#define VXI11_FAILURE           -1
#define VXI11_TIMEOUT            1


#define VXI11_READ         0
#define VXI11_WRITE        1


#define	VXI11_DEFAULT_TIMEOUT	5000	/* in ms */


#define VXI11_WAITLOCK_FLAG     ( 1 << 0 )
#define VXI11_END_FLAG          ( 1 << 3 )
#define VCI11_TERMCHRSET_FLAG   ( 1 << 7 )


#define VXI11_REQCNT_REASON     ( 1 << 0 )  /* Transmission ended because as
                                               many bytes as requested have
                                               beed received */
#define VXI11_CHR_REASON        ( 1 << 1 )  /* Transmission ended because the
                                               'termchar' was received */
#define VXI11_END_REASON        ( 1 << 2 )  /* Transmission ended because there
                                               are no more bytes to receive */


#define VXI11_TIMEOUT_ERROR     15


int vxi11_open( const char * /* dev_name   */,
                const char * /* address      */,
                const char * /* vxi11_name   */,
                bool         /* lock_device  */,
                bool         /* create_async */,
                long         /* us_timeout   */  );

int vxi11_close( void );

int vxi11_set_timeout( int  /* dir        */,
                       long /* us_timeout */  );

int vxi11_read_stb( unsigned char * stb );

int vxi11_lock_out( bool /* lock_state */ );

int vxi11_device_clear( void );

int vxi11_device_trigger( void );

int vxi11_write( const char * /* buffer      */,
                 size_t     * /* length      */,
                 bool         /* allow_abort */  );

int vxi11_read( char   * /* buffer      */,
                size_t * /* length      */,
                bool     /* allow_abort */  );


#endif /* ! defined VX11_USER_HEADER */


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
