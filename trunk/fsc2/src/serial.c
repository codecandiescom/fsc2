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


#include "fsc2.h"
#include "serial.h"


#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
static struct {
	bool in_use;
	const char* devname;
	char* dev_file;
} Serial_Port[ NUM_SERIAL_PORTS ];
#endif



/*-------------------------------------------------------------------*/
/* This function must be called by device modules that need a serial */
/* port. Here it is checked if the requested serial port is still    */
/* available and if the user has access permissions to the serial    */
/* ports device file. If one of these conditions isn't satisfied the */
/* function throws an exception.                                     */
/* -> 1. Serial port number - must be smaller than compiled in       */
/*       constant NUM_SERIAL_PORTS                                   */
/*    2. Name of the device the serial port is requested for         */
/*-------------------------------------------------------------------*/

void fsc2_request_serial_port( int sn, const char *devname )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0

	/* Do some sanity checks on the serial port number */

	if ( sn >= NUM_SERIAL_PORTS || sn < 0 )
	{
		if ( NUM_SERIAL_PORTS > 1 )
			eprint( FATAL, UNSET, "%s: Serial port number %d out of valid "
					"range (0-%d).\n", sn, NUM_SERIAL_PORTS - 1, devname );
		else
			eprint( FATAL, UNSET, "%s: Serial port number %d out of valid "
					"range (0 is allowed only).\n", sn, devname );
		THROW( EXCEPTION );
	}

	/* Check that serial port isn't already in use by another device */

	if ( Serial_Port[ sn ].in_use )
	{
		eprint( FATAL, UNSET, "%s: Requested serial port %d (i.e. /dev/ttyS%d "
				"or COM%d) is already used by device %s.\n", devname, sn, sn,
				sn + 1, Serial_Port[ sn ].devname );
		THROW( EXCEPTION );
	}

	Serial_Port[ sn ].in_use = SET;
	Serial_Port[ sn ].devname = devname;
	Serial_Port[ sn ].dev_file = NULL;

	/* Assemble name of the device file */

	Serial_Port[ sn ].dev_file = get_init_string( "/dev/ttyS%d", sn );

	/* Finally test if device file exists and we have read and write
	   permissions */

	if ( access( Serial_Port[ sn ].dev_file, W_OK | R_OK ) < 0 )
	{
		if ( errno == ENOENT )
			eprint( FATAL, UNSET, "%s: Device file %s for serial port %d does "
					"not exist.\n", devname, sn, Serial_Port[ sn ].dev_file );
		else
			eprint( FATAL, UNSET, "%s: No permission to access serial "
					"port %d.\n", devname, sn );
		THROW( EXCEPTION );
	}
#else
	sn = sn;

	eprintf( FATAL, UNSET, "%s: Device needs serial port but fsc2 was "
			 "not compiled with support for serial port access.\n", devname );
	THROW( EXCEPTION );
#endif
}


/*----------------------------------------------------------------------*/
/* This function is called only ones at the start of fsc2 to initialise */
/* the structure used in granting access to the serial ports.           */
/*----------------------------------------------------------------------*/

void fsc2_serial_init( void )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
	{
		Serial_Port[ i ].dev_file = NULL;
		Serial_Port[ i ].devname = NULL;
		Serial_Port[ i ].in_use = UNSET;
	}
#endif
}


/*-----------------------------------------------------------------------*/
/* This function is called after the end of an experiment (or when a new */
/* EDL file is loaded) to reset the structure used in granting access to */
/* the serial ports.                                                     */
/*-----------------------------------------------------------------------*/

void fsc2_serial_cleanup( void )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int i;


	for ( i = 0; i < NUM_SERIAL_PORTS; i++ )
	{
		if ( Serial_Port[ i ].in_use )
			Serial_Port[ i ].dev_file = T_free( Serial_Port[ i ].dev_file );
		Serial_Port[ i ].devname = NULL;
		Serial_Port[ i ].in_use = UNSET;
	}
#endif
}


/*--------------------------------------------------------------------*/
/* This function should be called by device modules that need to open */
/* a serial port device file. Instead of the device file name as in   */
/* the open() function this routine expects the number of the serial  */
/* port and the name of the device to make it possible to check if    */
/* the device has requested this port. The third parameter is, as in  */
/* the open() function, the flags used for opening the device file.   */
/*--------------------------------------------------------------------*/

int fsc2_serial_open( int sn, const char *devname, int flags )
{
#if defined( NUM_SERIAL_PORTS ) && NUM_SERIAL_PORTS > 0
	int fd;


	/* Check if serial port has been requested */

	if ( ! Serial_Port[ sn ].in_use ||
		 strcmp( Serial_Port[ sn ].devname, devname ) )
	{
		errno = EACCES;
		return -1;
	}

	fd = open( Serial_Port[ sn ].dev_file, flags );

	return fd;
#else
	sn = sn;
	devname = devname;
	flags = flags;


	errno = EACCES;
	return -1;
#endif
}
