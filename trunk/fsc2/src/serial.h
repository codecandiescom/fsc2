/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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

/*
  PLEASE NOTE: This file should only be included when access to a serial
               port is needed. Unfortunately, some of the definitions in
               <termios.h> conflict with definitions in the files created
               by flex (actually just one definition, ECHO), so including
			   this file everywhere results in several compiler warnings.
			   While these do not indicate a serious problem it's simply
			   ugly and might lead to confusion.
*/


#if ! defined SERIAL_HEADER
#define SERIAL_HEADER

#include <termios.h>


void fsc2_request_serial_port( int sn, const char *devname );
void fsc2_serial_init( void );
void fsc2_serial_clean_up( void );
int fsc2_serial_open( int sn, int flags );
ssize_t fsc2_serial_write( int fd, const void *buf, size_t count );
ssize_t fsc2_serial_read( int fd, void *buf, size_t count );
int fsc2_serial_close( int fd );
int fsc2_tcgetattr( int fd, struct termios *termios_p );
int fsc2_tcsetattr( int fd, int optional_actions, struct termios *termios_p );
int fsc2_tcsendbreak ( int fd, int duration );
int fsc2_tcdrain( int fd );
int fsc2_tcflush( int fd, int queue_selector );
int fsc2_tcflow( int fd, int action );
int fsc2_cfmakeraw( struct termios *termios_p );
speed_t fsc2_cfgetospeed( struct termios *termios_p );
int fsc2_cfsetospeed( struct termios *termios_p, speed_t speed );
speed_t fsc2_cfgetispeed( struct termios *termios_p );
int fsc2_cfsetispeed( struct termios *termios_p, speed_t speed );
pid_t fsc2_tcgetpgrp( int fd );
int fsc2_tcsetpgrp( int fd, pid_t pgrpid );


#endif   /* ! SERIAL_HEADER */
