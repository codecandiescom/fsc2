/*
 *  $Id$
 * 
 *  Copyright (C) 2003-2006 Jens Thoms Toerring
 * 
 *  This library should simplify accessing the WITIO-48 DIO board by Wasco
 *  by avoiding to be forced to make ioctl() calls and use higher level
 *  functions instead.
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  To contact the author send email to:  jt@toerring.de
 */


#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "witio_48.h"


#define WITIO_48_DEVICE_NAME "witio_48"

struct {
    int fd;                        /* file descriptor for board */
	WITIO_48_MODE mode[ 2 ];       /* current I/O modes of the DIOs */
} dev_info = { -1, { WITIO_48_MODE_3x8, WITIO_48_MODE_3x8 } };


static int check_dio( WITIO_48_DIO dio );
static int check_mode( WITIO_48_MODE mode );
static int check_channel( WITIO_48_DIO dio, WITIO_48_CHANNEL channel );
static int check_value( WITIO_48_DIO dio, WITIO_48_CHANNEL channel,
						unsigned long value );
static int check_board( void );


static int witio_48_errno = 0;

const char *witio_48_errlist[ ] = {
	"Success",                                        /* WITIO_48_OK      */
	"Invalid channel number",						  /* WITIO_48_ERR_ICA */
	"Invalid channel for current I/O mode",			  /* WITIO_48_ERR_ICM */
	"Invalid DIO number",							  /* WITIO_48_ERR_IVD */
	"Invalid mode",									  /* WITIO_48_ERR_IMD */
	"Invalid output value",							  /* WITIO_48_ERR_IDV */
	"Board is busy",								  /* WITIO_48_ERR_BNO */
	"Board not open",								  /* WITIO_48_ERR_BBS */
	"No driver loaded for board",					  /* WITIO_48_ERR_NDV */
	"No permissions to open device file",			  /* WITIO_48_ERR_ACS */
	"Device file does not exist",					  /* WITIO_48_ERR_DFM */
	"Unspecified error when opening device file",	  /* WITIO_48_ERR_DFP */
	"Internal driver or library error"				  /* WITIO_48_ERR_INT */
};

const int witio_48_nerr =
			( int ) ( sizeof witio_48_errlist / sizeof witio_48_errlist[ 0 ] );



/*-----------------------------------------------------------------*
 * Function for closing the device file associated with the board.
 * Note: no function for opening the file is required, whenever a
 * function for accessing the board is called the board is opened
 * automatically.
 *-----------------------------------------------------------------*/

int witio_48_close( void )
{
    if ( dev_info.fd < 0 )
        return witio_48_errno = WITIO_48_ERR_BNO;

	while ( close( dev_info.fd ) == -1 && errno == EINTR )
		/* empty */ ;

    dev_info.fd = -1;

    return witio_48_errno = WITIO_48_OK;
}


/*--------------------------------------------------------------------*
 * Function for setting the I/O mode of one of the DIOs of the board.
 *--------------------------------------------------------------------*/

int witio_48_set_mode( WITIO_48_DIO dio, WITIO_48_MODE mode )
{
	int ret;
	WITIO_48_DIO_MODE dio_mode = { dio, mode };


    if ( ( ret = check_board( ) )     < 0 ||
		 ( ret = check_dio( dio ) )   < 0 ||
		 ( ret = check_mode( mode ) ) < 0 )
        return ret;

    if ( ioctl( dev_info.fd, WITIO_48_IOC_SET_MODE, &dio_mode ) < 0 )
        return witio_48_errno = WITIO_48_ERR_INT;

	dev_info.mode[ dio ] = mode;

    return witio_48_errno = WITIO_48_OK;
}


/*--------------------------------------------------------------------------*
 * Function returns the current I/O mode of the specified DIO on the board.
 *--------------------------------------------------------------------------*/

int witio_48_get_mode( WITIO_48_DIO dio, WITIO_48_MODE *mode )
{
	int ret;
	WITIO_48_DIO_MODE dio_mode = { dio, 0 };


    if ( ( ret = check_board( ) )   < 0 ||
		 ( ret = check_dio( dio ) ) < 0 )
        return ret;

    if ( ioctl( dev_info.fd, WITIO_48_IOC_GET_MODE, &dio_mode ) < 0 )
        return witio_48_errno = WITIO_48_ERR_INT;

	if ( mode != NULL )
		*mode = dev_info.mode[ dio ] = dio_mode.mode;

    return witio_48_errno = WITIO_48_OK;
}


/*-------------------------------------------------------------------------*
 * Function for outputting a value at one of the DIOs. Which channels and
 * which range of values are allowed depend on the current I/O mode of the
 * specified DIO:
 *
 * |--------------------|--------------------|-------------------------|
 * |        mode        |     channels       |       ranges            |
 * |--------------------|--------------------|-------------------------|
 * | WITIO_48_MODE_3x8  | WITIO_48_CHANNEL_0 | 0 - 255 (0xFF)		   |
 * |                    | WITIO_48_CHANNEL_1 | 0 - 255 (0xFF)		   |
 * |                    | WITIO_48_CHANNEL_2 | 0 - 255 (0xFF)		   |
 * |--------------------|--------------------|-------------------------|
 * | WITIO_48_MODE_2x12 | WITIO_48_CHANNEL_0 | 0 - 4095 (0xFFF)		   |
 * |                    | WITIO_48_CHANNEL_1 | 0 - 4095 (0xFFF)		   |
 * |--------------------|--------------------|-------------------------|
 * | WITIO_48_MODE_1x24 | WITIO_48_CHANNEL_0 | 0 - 16777215 (0xFFFFFF) |
 * |--------------------|--------------------|-------------------------|
 * | WITIO_48_MODE_16_8 | WITIO_48_CHANNEL_0 | 0 - 65536 (0xFFFF)	   |
 * |                    | WITIO_48_CHANNEL_1 | 0 - 255 (0xFF)		   |
 * |--------------------|--------------------|-------------------------|
 *
 * At which pins of the output connectors the bits of the values apear
 * also depends on the I/O mode and then differ for the different modes:
 *
 * |--------------------|--------------------|---------------------------|
 * |        mode        |     channels       |  pins ( <- MSB   LSB-> )  |
 * |--------------------|--------------------|---------------------------|
 * | WITIO_48_MODE_3x8  | WITIO_48_CHANNEL_0 | A7 - A0                   |
 * |                    | WITIO_48_CHANNEL_1 | B7 - B0                   |
 * |                    | WITIO_48_CHANNEL_2 | C7 - C0                   |
 * |--------------------|--------------------|---------------------------|
 * | WITIO_48_MODE_2x12 | WITIO_48_CHANNEL_0 | C7 - C4, A7 - A0          |
 * |                    | WITIO_48_CHANNEL_1 | C3 - C0, B7 - B0          |
 * |--------------------|--------------------|---------------------------|
 * | WITIO_48_MODE_1x24 | WITIO_48_CHANNEL_0 | C7 - C0, B7 - B0, A7 - A0 |
 * |--------------------|--------------------|---------------------------|
 * | WITIO_48_MODE_16_8 | WITIO_48_CHANNEL_0 | B7 - B0, A7 - A0          |
 * |                    | WITIO_48_CHANNEL_1 | C7 - C0                   |
 * |--------------------|--------------------|---------------------------|
 *
 *-------------------------------------------------------------------------*/

int witio_48_dio_out( WITIO_48_DIO dio, WITIO_48_CHANNEL channel,
					  unsigned long value )
{
	int ret;
	WITIO_48_DATA data = { dio, channel, value };


    if ( ( ret = check_board( ) )                     < 0 ||
		 ( ret = check_dio( dio ) )                   < 0 ||
		 ( ret = check_channel( dio, channel ) )      < 0 ||
		 ( ret = check_value( dio, channel, value ) ) < 0 )
        return ret;

    if ( ioctl( dev_info.fd, WITIO_48_IOC_DIO_OUT, &data ) < 0 )
        return witio_48_errno = WITIO_48_ERR_INT;

    return witio_48_errno = WITIO_48_OK;
}


/*-------------------------------------------------------------*
 * Function for reading in a value from one of the DIOs. Which
 * channels can be used for a certain I/O mode and the range
 * of values to be expected as well as the way the pins of the
 * input connectors correspond to the bits of the returned
 * value are identical to the entries in the tables of the
 * description of the function witio_48_dio_out() (see above).
 *-------------------------------------------------------------*/

int witio_48_dio_in( WITIO_48_DIO dio, WITIO_48_CHANNEL channel,
					 unsigned long *value )
{
	int ret;
	WITIO_48_DATA data = { dio, channel, 0 };


    if ( ( ret = check_board( ) )                < 0 ||
		 ( ret = check_dio( dio ) )              < 0 ||
		 ( ret = check_channel( dio, channel ) ) < 0 )
		return ret;

    if ( ioctl( dev_info.fd, WITIO_48_IOC_DIO_IN, &data ) < 0 )
        return witio_48_errno = WITIO_48_ERR_INT;

	if ( value != NULL )
		*value = data.value;

    return witio_48_errno = WITIO_48_OK;
}


/*------------------------------------------------------------*
 * Internally used function to test a user supplied DIO value
 *------------------------------------------------------------*/

static int check_dio( WITIO_48_DIO dio )
{
	if ( dio > WITIO_48_DIO_2 )
		return witio_48_errno = WITIO_48_ERR_IVD;

	return WITIO_48_OK;
}


/*-------------------------------------------------------------*
 * Internally used function to test a user supplied mode value
 *-------------------------------------------------------------*/

static int check_mode( WITIO_48_MODE mode )
{
	if ( mode > WITIO_48_MODE_16_8 )
		return witio_48_errno = WITIO_48_ERR_IMD;

	return witio_48_errno = WITIO_48_OK;
}


/*----------------------------------------------------------------*
 * Internally used function to test a user supplied channel value
 *----------------------------------------------------------------*/

static int check_channel( WITIO_48_DIO dio, WITIO_48_CHANNEL channel )
{
	if ( channel > WITIO_48_CHANNEL_2 )
		return witio_48_errno = WITIO_48_ERR_ICA;

	if ( ( ( dev_info.mode[ dio ] == WITIO_48_MODE_2x12 ||
			 dev_info.mode[ dio ] == WITIO_48_MODE_16_8 ) &&
		   channel > WITIO_48_CHANNEL_1 ) ||
		 ( dev_info.mode[ dio ] == WITIO_48_MODE_1x24 &&
		   channel > WITIO_48_CHANNEL_0 ) )
		return witio_48_errno = WITIO_48_ERR_ICM;

	return witio_48_errno = WITIO_48_OK;
}


/*---------------------------------------------------------------*
 * Internally used function to test a user supplied output value
 *---------------------------------------------------------------*/

static int check_value( WITIO_48_DIO dio, WITIO_48_CHANNEL channel,
						unsigned long value )
{
	if ( ( value >= 1UL << 24 )                                    ||
		 ( dev_info.mode[ dio ] == WITIO_48_MODE_16_8 &&
		   channel == WITIO_48_CHANNEL_1              &&
		   value >= 1UL << 16 )                                    ||
		 ( dev_info.mode[ dio ] == WITIO_48_MODE_2x12 &&
		   value >= 1UL << 12 )                                    ||
		 ( ( dev_info.mode[ dio ] == WITIO_48_MODE_3x8       ||
			 ( dev_info.mode[ dio ] == WITIO_48_MODE_16_8 &&
			   channel == WITIO_48_CHANNEL_2 ) )                &&
		   value >= 1UL << 8 ) )
		return witio_48_errno = WITIO_48_ERR_IDV;

	return witio_48_errno = WITIO_48_OK;
}


/*----------------------------------------------------------------*
 * Internally used function that tests if the device file for the
 * board is already open. If not the function tries to open the
 * device file.
 *----------------------------------------------------------------*/

static int check_board( void )
{
    const char *name = "/dev/" WITIO_48_DEVICE_NAME;
    struct stat buf;
	WITIO_48_DIO_MODE dio_mode = { 0, 0 };
	int i;


    if ( dev_info.fd >= 0 )
		return WITIO_48_OK;

	/* Check if the device file exists and can be accessed */

	if ( stat( name, &buf ) < 0 )
		switch ( errno )
		{
			case ENOENT :
				return witio_48_errno = WITIO_48_ERR_DFM;

			case EACCES :
				return witio_48_errno = WITIO_48_ERR_ACS;

			default :
				return witio_48_errno = WITIO_48_ERR_DFP;
		}

	/* Try to open the device file for the board */

	if ( ( dev_info.fd = open( name, O_RDWR ) ) < 0 )
		switch ( errno )
		{
			case ENODEV : case ENXIO :
				return witio_48_errno = WITIO_48_ERR_NDV;

			case EACCES :
				return witio_48_errno = WITIO_48_ERR_ACS;

			case EBUSY :
				return witio_48_errno = WITIO_48_ERR_BBS;

			default :
				return witio_48_errno = WITIO_48_ERR_DFP;
		}

	/* Set the FD_CLOEXEC bit for the device file, exec()'ed applications
	   should have no interest at all in the board (and even don't know
	   that they have the file open) but could interfere seriously with
	   normal operation of the program that did open the device file. */

	fcntl( dev_info.fd, F_SETFD, FD_CLOEXEC );

	/* Finally get the I/O mode for both DIOs */

	for ( i = 0; i < 2; i++ )
	{
		dio_mode.dio = i;

		if ( ioctl( dev_info.fd, WITIO_48_IOC_GET_MODE, &dio_mode ) < 0 )
		{
			while ( close( dev_info.fd ) == -1 && errno == EINTR )
				/* empty */ ;

			dev_info.fd = -1;
			return witio_48_errno = WITIO_48_ERR_INT;
		}

		dev_info.mode[ i ] = dio_mode.mode;
	}

	return witio_48_errno = WITIO_48_OK;
}


/*----------------------------------------------------------------*
 * Prints out a string to stderr, consisting of a user supplied
 * string (the argument of the function), a colon, a blank and
 * a short descriptive text of the error encountered in the last
 * invocation of one of the witio_48_xxx() functions, followed by
 * a new-line. If the argument is NULL or an empty string only
 * the error message is printed.
 *----------------------------------------------------------------*/

int witio_48_perror( const char *s )
{
	if ( s != NULL && *s != '\0' )
		return fprintf( stderr, "%s: %s\n",
						s, witio_48_errlist[ - witio_48_errno ] );

	return fprintf( stderr, "%s\n", witio_48_errlist[ - witio_48_errno ] );
}


/*-----------------------------------------------------------------*
 * Returns a string with a short descriptive text of the error
 * encountered in the last invocation of one of the witio_48_xxx()
 * functions.
 *-----------------------------------------------------------------*/

const char *witio_48_strerror( void )
{
	return witio_48_errlist[ - witio_48_errno ];
}
