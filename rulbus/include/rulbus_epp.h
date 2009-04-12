/*
 *  $Id$
 * 
 *  In clude file for the driver for the Rulbus (Rijksuniversiteit Leiden
 *  BUS) EPP interface
 *
 *  Copyright (C) 2003-2009 Jens Thoms Toerring
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
 *  To contact the author send email to jt@toerring.de
 */


#if ! defined RULBUS_EPP_HEADER
#define RULBUS_EPP_HEADER


#ifdef __cplusplus
extern "C" {
#endif


#include <linux/ioctl.h>


typedef struct {
	unsigned char rack;
	unsigned char offset;
	unsigned char byte;
	unsigned char *data;
	size_t len;
} RULBUS_EPP_IOCTL_ARGS;


/* Definition of the numbers of ioctl() commands - the character 'j' as
   the 'magic' type seems to be not in use for command numbers above 0x3F
   (see linux/Documentation/ioctl-number.txt). */

#define RULBUS_EPP_MAGIC_IOC    'j'

#define RULBUS_EPP_IOC_READ          \
		    _IOWR( RULBUS_EPP_MAGIC_IOC, 0xA0, RULBUS_EPP_IOCTL_ARGS )
#define RULBUS_EPP_IOC_WRITE         \
		    _IOWR( RULBUS_EPP_MAGIC_IOC, 0xA1, RULBUS_EPP_IOCTL_ARGS )
#define RULBUS_EPP_IOC_READ_RANGE    \
		    _IOWR( RULBUS_EPP_MAGIC_IOC, 0xA2, RULBUS_EPP_IOCTL_ARGS )
#define RULBUS_EPP_IOC_WRITE_RANGE   \
		    _IOWR( RULBUS_EPP_MAGIC_IOC, 0xA3, RULBUS_EPP_IOCTL_ARGS )



#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* ! RULBUS_EPP_HEADER */


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
