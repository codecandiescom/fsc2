/*
  $Id$
 
  Library for National Instruments DAQ boards based on a DAQ-STC

  Copyright (C) 2003-2004 Jens Thoms Toerring

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "ni_daq_lib.h"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

int ni_daq_dio_write( int board, unsigned char value, unsigned char mask )
{
	NI_DAQ_DIO_ARG dio;
	int ret;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	dio.cmd = NI_DAQ_DIO_OUTPUT;
    dio.value = value;
    dio.mask = mask;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_DIO, &dio ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

    return ni_daq_errno = NI_DAQ_OK;
}


/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/

int ni_daq_dio_read( int board, unsigned char *value, unsigned char mask )
{
	NI_DAQ_DIO_ARG dio;
	int ret;


	if ( ( ret = ni_daq_basic_check( board ) ) < 0 )
		return ret;

	if ( value == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	dio.cmd = NI_DAQ_DIO_INPUT;
    dio.mask = mask;

    if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_DIO, &dio ) < 0 )
		return ni_daq_errno = NI_DAQ_ERR_INT;

    *value = dio.value;

    return ni_daq_errno = NI_DAQ_OK;
}
