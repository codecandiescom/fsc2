/*
 *  $Id$
 * 
 *  Library for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2006 Jens Thoms Toerring
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 * 
 *  To contact the author send email to:  jt@toerring.de
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


NI_DAQ_DEV ni_daq_dev[ NI_DAQ_MAX_BOARDS ];

int ni_daq_errno = 0;

static const char *ni_daq_errlist[ ] = {
    "Success",                                       /* NI_DAQ_OK      */
    "No such board",                                 /* NI_DAQ_ERR_NSB */
    "Counter is busy",                               /* NI_DAQ_ERR_CBS */
    "Invalid argument",                              /* NI_DAQ_ERR_IVA */
    "Can't wait for continuous counter to stop",     /* NI_DAQ_ERR_WFC */
    "Board is busy",                                 /* NI_DAQ_ERR_BBS */
    "Source argument invalid",                       /* NI_DAQ_ERR_IVS */
    "Board not open",                                /* NI_DAQ_ERR_BNO */
    "No driver loaded for board",                    /* NI_DAQ_ERR_NDV */
    "Neighboring counter is busy",                   /* NI_DAQ_ERR_NCB */
    "Interrupted by signal",                         /* NI_DAQ_ERR_ITR */
    "No permissions to open device file",            /* NI_DAQ_ERR_ACS */
    "Device file does not exist",                    /* NI_DAQ_ERR_DFM */
    "Unspecified error when opening device file",    /* NI_DAQ_ERR_DFP */
    "Internal driver or library error",              /* NI_DAQ_ERR_INT */
	"Board is missing AO capabilities",              /* NI_DAQ_ERR_NAO */
	"Missing channel setup",                         /* NI_DAQ_ERR_NSS */
	"Missing AI acquisition setup",                  /* NI_DAQ_ERR_NAS */
	"Not enough memory",                             /* NI_DAQ_ERR_NEM */
	"Timing impossible to realize",                  /* NI_DAQ_ERR_NPT */
	"Interrupted by signal",                         /* NI_DAQ_ERR_SIG */
	"External reference for AO not available",       /* NI_DAQ_ERR_NER */
	"Only bipolar AO available",                     /* NI_DAQ_ERR_UAO */
	"No analog trigger available",                   /* NI_DAQ_ERR_NAT */
	"Pulser to be started not initialized"           /* NI_DAQ_ERR_PNI */
};

const int ni_daq_nerr =
                ( int ) ( sizeof ni_daq_errlist / sizeof ni_daq_errlist[ 0 ] );


static int ni_daq_first_call_handler( void );


/*------------------------------------------------------------------*
 * Function for initializing the board - it expects the name of the
 * device file associated with the board and a flag (only  O_NONBLOCK
 * or O_NODELAY make sense) as its arguments. It then tries to open
 * the device file, applies some default settings and asks the driver
 * for the boards capabilities. On success a non-negative integer
 * handle for the board is returned, on failure a negative number
 * indicating the reason of the failure.
 *------------------------------------------------------------------*/

int ni_daq_open( const char * name,
				 int          flag )
{
	int board;
	struct stat stat_buf;
	NI_DAQ_MSC_ARG m;


	if ( name == NULL )
		return ni_daq_errno = NI_DAQ_ERR_IVA;

	ni_daq_first_call_handler( );

	/* Figure out the board number by looking for the minor device number -
	   it seems to be the lower byte of the 'st_rdev' member of the stat
	   structure */

	if ( stat( name, &stat_buf ) < 0 )
		switch ( errno )
		{
			case ENOENT :  case ENOTDIR : case ENAMETOOLONG : case ELOOP :
				return ni_daq_errno = NI_DAQ_ERR_DFM;

			case EACCES :
				return ni_daq_errno = NI_DAQ_ERR_ACS;

			default :
				return ni_daq_errno = NI_DAQ_ERR_DFP;
		}

	board = stat_buf.st_rdev & 0xFF;

	if ( ( ni_daq_dev[ board ].fd = open( name, O_RDWR | flag ) ) < 0 )
		switch ( errno )
		{
			case ENOENT :  case ENOTDIR : case ENAMETOOLONG : case ELOOP :
				return ni_daq_errno = NI_DAQ_ERR_DFM;

			case ENODEV : case ENXIO :
				return ni_daq_errno = NI_DAQ_ERR_NDV;
            
			case EACCES :
				return ni_daq_errno = NI_DAQ_ERR_ACS;

			case EBUSY : case EAGAIN :
				return ni_daq_errno = NI_DAQ_ERR_BBS;

			case EINTR :
				return ni_daq_errno = NI_DAQ_ERR_ITR;

			default :
				return ni_daq_errno = NI_DAQ_ERR_DFP;
		}

	/* Set the FD_CLOEXEC bit for the device file - exec()'ed application
	   should have no interest at all in the board (and even don't know
	   that they have the file open) but could interfere seriously with
	   normal operation of the program that did open the device file. */

	fcntl( ni_daq_dev[ board ].fd, F_SETFD, FD_CLOEXEC );

	/* Find out about the board properties by asking the driver to fill in
	   the corresponding structure */

	m.cmd = NI_DAQ_MSC_BOARD_PROPERTIES;
	m.properties = &ni_daq_dev[ board ].props;

	if ( ioctl( ni_daq_dev[ board ].fd, NI_DAQ_IOC_MSC, &m ) < 0 )
	{
		ni_daq_close( board );
		return ni_daq_errno = NI_DAQ_ERR_DFP;
	}

	/* Since the board has just been opened do the initialization */

	if ( ni_daq_msc_init( board ) || ni_daq_ai_init( board ) ||
		 ni_daq_ao_init( board )  || ni_daq_gpct_init( board ) )
	{
		ni_daq_close( board );
		return ni_daq_errno = NI_DAQ_ERR_INT;
	}
	
	return board;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

int ni_daq_close( int board )
{
	int ret;


	if ( board < 0 || board > NI_DAQ_MAX_BOARDS )
        return ni_daq_errno = NI_DAQ_ERR_NSB;

	if ( ni_daq_dev[ board ].fd < 0 )
		return ni_daq_errno = NI_DAQ_ERR_BNO;

	while ( close( ni_daq_dev[ board ].fd ) && errno == EINTR )
		/* empty */ ;
	ni_daq_dev[ board ].fd = -1;

    return ni_daq_errno = NI_DAQ_OK;
}


/*---------------------------------------------------------------*
 * Prints out a string to stderr, consisting of a user supplied
 * string (the argument of the function), a colon, a blank and
 * a short descriptive text of the error encountered in the last
 * invocation of one of the ni_daq_xxx() functions, followed by
 * a new-line. If the argument is NULL or an empty string only
 * the error message is printed.
 *---------------------------------------------------------------*/

int ni_daq_perror( const char * s )
{
    if ( s != NULL && *s != '\0' )
        return fprintf( stderr, "%s: %s\n",
                        s, ni_daq_errlist[ - ni_daq_errno ] );

    return fprintf( stderr, "%s\n", ni_daq_errlist[ - ni_daq_errno ] );
}


/*---------------------------------------------------------------*
 * Returns a string with a short descriptive text of the error
 * encountered in the last invocation of one of the ni_daq_xxx()
 * functions.
 *---------------------------------------------------------------*/

const char *ni_daq_strerror( void )
{
    return ni_daq_errlist[ - ni_daq_errno ];
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

int ni_daq_basic_check( int board )
{
	if ( board < 0 || board > NI_DAQ_MAX_BOARDS )
        return ni_daq_errno = NI_DAQ_ERR_NSB;

	ni_daq_first_call_handler( );

	if ( ni_daq_dev[ board ].fd < 0 )
		return ni_daq_errno = NI_DAQ_ERR_BNO;

	return NI_DAQ_OK;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

static int ni_daq_first_call_handler( void )
{
	static int first_call = 1;

	if ( first_call )
	{
		int i;

		for ( i = 0; i < NI_DAQ_MAX_BOARDS; i++ )
			ni_daq_dev[ i ].fd = -1;

		first_call = 0;
	}

	return 0;
}
