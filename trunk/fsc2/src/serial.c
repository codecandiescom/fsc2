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


/* The purpose of all function defined in this file is to allow access to
   the serial port with the necessary permissions. The minimum requirement
   in the installation of fsc2 is that fsc2 belongs to a group that has
   read and write permissions to the serial port. Usually fsc2 runs with
   the permissions of the user that started the program. Thus, when accessing
   the serial port, it is necessary to raise the permissions by setting
   the UID and GID to the one of fsc2 before running a function dealing
   with the serial port and to switch back to the normal UID and GID
   afterwards. So, here are wrapper functions for opening, reading, writing
   and closing the device files as well as for all functions for controlling
   the serial port as declared in <termios.h>.
*/
   


#include "fsc2.h"
#include "serial.h"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_serial_open( const char *dev_name, int flags )
{
	int fd;

	raise_permissions( );
	fd = open( dev_name, flags );
	lower_permissions( );

	return fd;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

ssize_t fsc2_serial_write( int fd, const void *buf, size_t count )
{
	ssize_t ret_count;


	raise_permissions( );
	ret_count = write( fd, buf, count );
	lower_permissions( );

	return ret_count;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

ssize_t fsc2_serial_read( int fd, void *buf, size_t count )
{
	ssize_t ret_count;


	raise_permissions( );
	ret_count = read( fd, buf, count );
	lower_permissions( );

	return ret_count;
}
	

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_serial_close( int fd )
{
	int ret;


	raise_permissions( );
	ret = close( fd );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcgetattr( int fd, struct termios *termios_p )
{
	int ret;


	raise_permissions( );
	ret = tcgetattr( fd, termios_p );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcsetattr( int fd, int optional_actions, struct termios *termios_p )
{
	int ret;


	raise_permissions( );
	ret = tcsetattr( fd, optional_actions, termios_p );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcsendbreak( int fd, int duration )
{
	int ret;


	raise_permissions( );
	ret = tcsendbreak( fd, duration );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcdrain( int fd )
{
	int ret;


	raise_permissions( );
	ret = tcdrain( fd );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcflush( int fd, int queue_selector )
{
	int ret;


	raise_permissions( );
	ret = tcflush( fd, queue_selector );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcflow( int fd, int action )
{
	int ret;


	raise_permissions( );
	ret = tcflow( fd, action );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_cfmakeraw( struct termios *termios_p )
{
	int ret;


	raise_permissions( );
	ret = cfmakeraw( termios_p );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

speed_t fsc2_cfgetospeed( struct termios *termios_p )
{
	speed_t ret;


	raise_permissions( );
	ret = cfgetospeed( termios_p );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_cfsetospeed( struct termios *termios_p, speed_t speed )
{
	int ret;


	raise_permissions( );
	ret = cfsetospeed( termios_p, speed );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

speed_t fsc2_cfgetispeed( struct termios *termios_p )
{
	speed_t ret;


	raise_permissions( );
	ret = cfgetispeed( termios_p );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_cfsetispeed( struct termios *termios_p, speed_t speed )
{
	int ret;


	raise_permissions( );
	ret = cfsetispeed( termios_p, speed );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

pid_t fsc2_tcgetpgrp( int fd )
{
	pid_t ret;


	raise_permissions( );
	ret = tcgetpgrp( fd );
	lower_permissions( );

	return ret;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int fsc2_tcsetpgrp( int fd, pid_t pgrpid )
{
	int ret;


	raise_permissions( );
	ret = tcsetpgrp( fd, pgrpid );
	lower_permissions( );

	return ret;
}
