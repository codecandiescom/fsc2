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


#if ! defined SERIAL_HEADER
#define SERIAL_HEADER


int fsc2_serial_open( const char *dev_name, int flags );
ssize_t int fsc2_serial_write( int fd, const void *buf, size_t count );
ssize_t fsc2_serial_read( int fd, void *buf, size_t count );
int fsc2_serial_close( int fd );
int fsc2_tcgetattr( int fd, struct termios *termios_p );
int fsc2_cfsetospeed( struct termios *termios_p, speed_t speed );
int fsc2_cfsetispeed( struct termios *termios_p, speed_t speed );
int fsc2_tcflush( int fd, int queue_selector );
int fsc2_tcsetattr( int fd, int optional_actions, struct termios *termios_p );
int fsc2_tcflush( int fd, int queue_selector );


#endif   /* ! SERIAL_HEADER */
