/*
 *  $Id$
 * 
 *  Kernel (2.4 series) module for Wasco WITIO-48 DIO ISA card
 * 
 *  Copyright (C) 2003-2007 Jens Thoms Toerring
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


#ifdef __cplusplus
extern "C" {
#endif


#include <linux/ioctl.h>


typedef enum {
	WITIO_48_MODE_3x8 = 0,
	WITIO_48_MODE_2x12,
	WITIO_48_MODE_1x24,
	WITIO_48_MODE_16_8
} WITIO_48_MODE;


typedef enum {
	WITIO_48_DIO_1 = 0,
	WITIO_48_DIO_2
} WITIO_48_DIO;


typedef enum {
	WITIO_48_CHANNEL_0 = 0,
	WITIO_48_CHANNEL_1,
	WITIO_48_CHANNEL_2
} WITIO_48_CHANNEL;


typedef struct {
	WITIO_48_DIO  dio;
	WITIO_48_MODE mode;
} WITIO_48_DIO_MODE;


typedef struct {
	WITIO_48_DIO     dio;
	WITIO_48_CHANNEL channel;
	unsigned long    value;
} WITIO_48_DATA;


#define WITIO_48_MAGIC_IOC    'j'

#define WITIO_48_IOC_SET_MODE _IOW ( WITIO_48_MAGIC_IOC, 0x90, \
				     WITIO_48_DIO_MODE * )
#define WITIO_48_IOC_GET_MODE _IOWR( WITIO_48_MAGIC_IOC, 0x90, \
				     WITIO_48_DIO_MODE * )
#define WITIO_48_IOC_DIO_OUT _IOW ( WITIO_48_MAGIC_IOC, 0x91, WITIO_48_DATA * )
#define WITIO_48_IOC_DIO_IN  _IOWR( WITIO_48_MAGIC_IOC, 0x92, WITIO_48_DATA * )


#ifdef __cplusplus
} /* extern "C" */
#endif


/*
 * Local variables:
 * c-basic-offset: 8
 * c-indent-level: 8
 * c-brace-imaginary-offset: 0
 * c-brace-offset: 0
 * c-argdecl-indent: 4
 * c-label-ofset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: t
 * tab-width: 8
 * End:
 */
