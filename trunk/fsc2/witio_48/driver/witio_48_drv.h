/*
  $Id$
 
  Kernel (2.4 series) module for Wasco WITIO-48 DIO ISA card

  Copyright (C) 2003 Jens Thoms Toerring

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
*/



#include <linux/ioctl.h>


typedef enum {
	WITIO_48_MODE_3x8 = 0,
	WITIO_48_MODE_2x12,
	WITIO_48_MODE_1x24,
	WITIO_48_MODE_16_8
} WITIO_48_MODES;


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
	WITIO_48_DIO   dio;
	WITIO_48_MODES mode;
} WITIO_48_MODE;


typedef struct {
	WITIO_48_DIO     dio;
	WITIO_48_CHANNEL channel;
	unsigned long    value;
} WITIO_48_DATA;


#define WITIO_48_MAGIC_IOC    'j'

#define WITIO_48_IOC_MODE    _IOW ( WITIO_48_MAGIC_IOC, 0x90, WITIO_48_MODE * )
#define WITIO_48_IOC_DIO_OUT _IOW ( WITIO_48_MAGIC_IOC, 0x91, WITIO_48_DATA * )
#define WITIO_48_IOC_DIO_IN  _IOWR( WITIO_48_MAGIC_IOC, 0x92, WITIO_48_DATA * )

#define WITIO_48_MIN_NR      _IOC_NR( WITIO_48_IOC_MODE )
#define WITIO_48_MAX_NR      _IOC_NR( WITIO_48_IOC_DIO_IN )


/* All of the rest needs only to be seen by the module itself but not by
   user-land applications */

#if defined __KERNEL__

#include <linux/autoconf.h>

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif


/* Including modversions.h must come before including module.h !
   On some systems you get very strange compiler time errors otherwise
   (as I learned the hard way...) */

#if defined( MODVERSIONS )
#include <linux/modversions.h>
#endif

#define __NO_VERSION__
#include <linux/module.h>

#include <linux/version.h>

#if ! defined( KERNEL_VERSION )
#define KERNEL_VERSION( a, b, c ) ( ( ( a ) << 16 ) | ( ( b ) << 8 ) | ( c ) )
#endif

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define PORT unsigned int


#define A 0
#define B 1
#define C 2


#define WRITE_ALL   0
#define READ_LOWER  1
#define READ_UPPER  2
#define READ_ALL    3


typedef struct Board Board;
typedef struct Registers Registers;
typedef struct State State;

struct Registers {
	unsigned char *port[ 2 ][ 3 ];
	unsigned char *control[ 2 ];
	unsigned char *counter[ 3 ];
};


struct State {
	WITIO_48_MODES mode;
	unsigned char state[ 3 ];
	unsigned char out_value[ 3 ];
};


struct Board {
	int major;
	unsigned char *base;
	int in_use;
	uid_t owner;
	spinlock_t spinlock;
	Registers regs;
	State states[ 2 ];
};


#if defined WITIO_48_DEBUG
#define PDEBUG( fmt, args... ) \
	{ printk( KERN_DEBUG NI6601_NAME ": " fmt, ## args ); }
#else
#define PDEBUG( ftm, args... )
#endif


#endif    /* __KERNEL__ */

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
