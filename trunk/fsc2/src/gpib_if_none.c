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


#define __GPIB_IF__


#include "fsc2.h"
#include "gpib_if.h"


int gpib_init( const char *log_file_name, int log_level )
{
	log_file_name = log_file_name;
	log_level = log_level;
	strcpy( gpib_error_msg, "No GPIB support compiled into fsc2." );
	return FAILURE;
}


int gpib_shutdown( void )
{
    return SUCCESS;
}


int gpib_init_device( const char *device_name, int *dev )
{
	device_name = device_name;
	dev = dev;
	return FAILURE;
}


int gpib_timeout( int device, int period )
{
	device = device;
	period = period;
	return FAILURE;
}


int gpib_clear_device( int device )
{
	device = device;
	return FAILURE;
}


int gpib_local( int device )
{
	device = device;
	return FAILURE;
}


int gpib_trigger( int device )
{
	device = device;
	return FAILURE;
}


int gpib_write( int device, const char *buffer, long length )
{
	device = device;
	buffer = buffer;
	length = length;
	return FAILURE;
}


int gpib_read( int device, char *buffer, long *length )
{
	device = device;
	buffer = buffer;
	length = length;
	return FAILURE;
}
