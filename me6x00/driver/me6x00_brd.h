/* Device driver for Meilhaus me6x00 boards
 * ========================================
 *
 *   This file is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   Responsible for this file:
 *   Jens Thoms Toerring <jt@toerring.de>
 */

/*
 * Description:
 *
 *   Header file for inclusion by the device driver only
 *
 */

/*
 * REGISTER DESCRIPTION
 * --------------------
 *
 * ME6X00_CTRLREG_DAC00-03:
 *   Bit 0 : Mode 1 | 00 = single, 01 = wrap around, 02 = continuous
 *   Bit 1 : Mode 2 |
 *   Bit 2 : Stop(1) Converting (only for wrap around and continuous)
 *   Bit 3 : Reset(0) FIFO
 *   Bit 4 : Enable(1) extern trigger
 *   Bit 5 : raising(0) or falling(1) edge of extern trigger
 *   Bit 6 : Disable(0) or enable(1) irq
 *   Bit 7-15 : unused
 *
 * ME6X00_STATREG_DAC00-03:
 *   Bit 0 : Status for single mode, if 0, last value was written to DAC
 *   Bit 1 : Status FF 0 if FIFO full
 *   Bit 2 : Status HF 0 if FIFO half full
 *   Bit 3 : Status EF 0 if FIFO empty
 *   Bit 4-15 : unused
 *
 * ME6X00_FIFOREG_DAC00:
 *   Bit 0-15 used in order to write a value to the FIFO
 *
 * ME6X00_FIFOREG_DAC01:
 *   Bit 16-31 used in order to write a value to the FIFO
 *
 * ME6X00_FIFOREG_DAC02:
 *   Bit 0-15 used in order to write a value to the FIFO
 *
 * ME6X00_FIFOREG_DAC03:
 *   Bit 16-31 used in order to write a value to the FIFO
 *
 * ME6X00_SVALREG_DAC00-03:
 *   Bit 0-15 used in order to write a value to the DAC in Single Mode
 * 
 * ME6X00_TIMEREG_DAC00-03:
 *   Specifies the number of Ticks(30.30ns) per value in the FIFO
 *   0 means one Tick per Value
 *   
 * ME6X00_IRQSREG_DACXX:
 *   State of the irqs of DAC00-03, 1 for interrupt occured
 *
 * ME6X00_IRQRREG_DAC00-03:
 *   Reading from this registers resets the corresponding irq
 *
 * ME6X00_SVALREG_DAC04-15:
 *   Bit 0-15 used in order to write a value to the DAC
 *
 * ME6X00_STATREG_DACXX:
 *   If the corresponding bit is 0, last value was written to DAC
 *   Bit 0 corresponds to DAC04
 */


#if ! defined ME6X00_BRD_HEADER
#define ME6X00_BRD_HEADER


#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 33 )
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif


#if ! defined ( CONFIG_PCI )
#error "PCI support in kernel is missing."
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/module.h>

#if defined( CONFIG_SMP ) && ! defined( __SMP__ )
#define __SMP__
#endif

#else

#include <linux/module.h>

#endif


/*
 * Needed for the registration of I/O and MEMORY regions.
 * (request_region, ...)
 */

#include <linux/ioport.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <asm/io.h>
#include <asm/system.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 2, 0 )
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/spinlock.h>
#else
#include <linux/malloc.h>

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#else
#define __user
#endif

/* Compatibility file for kernels from 2.0 up to 2.4 */

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
#include "sysdep.h"
#endif


/* The following two macros are removed since kernel 2.6.37, but we use them */

#ifndef init_MUTEX
# define init_MUTEX( sem ) sema_init( sem, 1 )
#endif
#ifndef init_MUTEX_LOCKED
# define init_MUTEX_LOCKED( sem ) sema_init( sem, 0 )
#endif


#include "autoconf.h"
#include "me6x00_drv.h"


#define PCI_VENDOR_ID_MEILHAUS 0x1402

/* ME6000 standard version */

#define PCI_DEVICE_ID_MEILHAUS_ME6004   0x6004  
#define PCI_DEVICE_ID_MEILHAUS_ME6008   0x6008  
#define PCI_DEVICE_ID_MEILHAUS_ME600F   0x600F

/* ME6000 isolated version */

#define PCI_DEVICE_ID_MEILHAUS_ME6014   0x6014  
#define PCI_DEVICE_ID_MEILHAUS_ME6018   0x6018  
#define PCI_DEVICE_ID_MEILHAUS_ME601F   0x601F

/* ME6000 isle version */

#define PCI_DEVICE_ID_MEILHAUS_ME6034   0x6034  
#define PCI_DEVICE_ID_MEILHAUS_ME6038   0x6038  
#define PCI_DEVICE_ID_MEILHAUS_ME603F   0x603F

/* ME6100 standard version */

#define PCI_DEVICE_ID_MEILHAUS_ME6104   0x6104  
#define PCI_DEVICE_ID_MEILHAUS_ME6108   0x6108  
#define PCI_DEVICE_ID_MEILHAUS_ME610F   0x610F

/* ME6100 isolated version */

#define PCI_DEVICE_ID_MEILHAUS_ME6114   0x6114  
#define PCI_DEVICE_ID_MEILHAUS_ME6118   0x6118  
#define PCI_DEVICE_ID_MEILHAUS_ME611F   0x611F

/* ME6100 isle version */

#define PCI_DEVICE_ID_MEILHAUS_ME6134   0x6134  
#define PCI_DEVICE_ID_MEILHAUS_ME6138   0x6138  
#define PCI_DEVICE_ID_MEILHAUS_ME613F   0x613F

  
/* Device name, for entries in /proc/.. */

#define ME6X00_NAME "me6x00"


/* Buffer size in bytes (must be a power of 2) */

#define ME6X00_BUFFER_SIZE 0x10000


/* Buffer count in values of short */

#define ME6X00_BUFFER_COUNT ( ME6X00_BUFFER_SIZE / 2 )


/* ME6X00 register offsets */

#define ME6X00_CTRLREG_DAC00  0x00  /* R/W */
#define ME6X00_STATREG_DAC00  0x04  /* R/_ */
#define ME6X00_FIFOREG_DAC00  0x08  /* _/W */
#define ME6X00_SVALREG_DAC00  0x0C  /* R/W */
#define ME6X00_TIMEREG_DAC00  0x10  /* _/W */

#define ME6X00_CTRLREG_DAC01  0x18  /* R/W */
#define ME6X00_STATREG_DAC01  0x1C  /* R/_ */
#define ME6X00_FIFOREG_DAC01  0x20  /* _/W */
#define ME6X00_SVALREG_DAC01  0x24  /* R/W */
#define ME6X00_TIMEREG_DAC01  0x28  /* _/W */

#define ME6X00_CTRLREG_DAC02  0x30  /* R/W */
#define ME6X00_STATREG_DAC02  0x34  /* R/_ */
#define ME6X00_FIFOREG_DAC02  0x38  /* _/W */
#define ME6X00_SVALREG_DAC02  0x3C  /* R/W */
#define ME6X00_TIMEREG_DAC02  0x40  /* _/W */

#define ME6X00_CTRLREG_DAC03  0x48  /* R/W */
#define ME6X00_STATREG_DAC03  0x4C  /* R/_ */
#define ME6X00_FIFOREG_DAC03  0x50  /* _/W */
#define ME6X00_SVALREG_DAC03  0x54  /* R/W */
#define ME6X00_TIMEREG_DAC03  0x58  /* _/W */

#define ME6X00_IRQSREG_DACXX  0x60  /* R/_ */
#define ME6X00_IRQRREG_DAC00  0x64  /* R/_ */
#define ME6X00_IRQRREG_DAC01  0x68  /* R/_ */
#define ME6X00_IRQRREG_DAC02  0x6C  /* R/_ */
#define ME6X00_IRQRREG_DAC03  0x70  /* R/_ */

#define ME6X00_SVALREG_DAC04  0x74  /* _/W */
#define ME6X00_SVALREG_DAC05  0x78  /* _/W */
#define ME6X00_SVALREG_DAC06  0x7C  /* _/W */
#define ME6X00_SVALREG_DAC07  0x80  /* _/W */
#define ME6X00_SVALREG_DAC08  0x84  /* _/W */
#define ME6X00_SVALREG_DAC09  0x88  /* _/W */
#define ME6X00_SVALREG_DAC10  0x8C  /* _/W */
#define ME6X00_SVALREG_DAC11  0x90  /* _/W */
#define ME6X00_SVALREG_DAC12  0x94  /* _/W */
#define ME6X00_SVALREG_DAC13  0x98  /* _/W */
#define ME6X00_SVALREG_DAC14  0x9C  /* _/W */
#define ME6X00_SVALREG_DAC15  0xA0  /* _/W */

#define ME6X00_STATREG_DACXX  0xA4  /* R/_ */


#define FROM_IOCTL   0
#define FROM_INIT    1
#define FROM_CLOSE   2
#define FROM_CLEANUP 3


typedef struct {
	unsigned int ctrl;
	unsigned int stat;
	unsigned int fifo;
	unsigned int timer;
	unsigned int irqr;
} Registers;


/* Size of I/O spaces */

#define PLX_REG_SIZE 128
#define ME6X00_REG_SIZE 256
#define XILINX_REG_SIZE 256


/* Offset Interrupt Control/Status Register PLX */

#define PLX_INTCSR 0x4C


/* Offset Initialization Control Register PLX */

#define PLX_ICR 0x50

/* Test for ME6100 boards */

#define IS_ME6100( minor ) ( info_vec[ minor ].device_id & 0x100 )


typedef struct {
	unsigned short *buf;
	int head;
	int tail;
} me6x00_circ_buf_st;


/* Board specific information */

typedef struct {
  char name[ 7 ];
  struct pci_dev *dev;
  int is_init;                      /* Flag, set if board is initialized     */
  unsigned int plx_regbase;         /* PLX configuration space base address  */
  unsigned int me6x00_regbase;      /* Base address of the ME6X00            */
  unsigned int xilinx_regbase;      /* Base address for Xilinx control       */
  unsigned int plx_regbase_size;    /* Size of PLX space                     */
  unsigned int me6x00_regbase_size; /* Size of ME6X00 base address           */
  unsigned int xilinx_regbase_size; /* Size of Xilinx base address           */
  unsigned int serial_no;           /* Serial number of the board            */
  unsigned char hw_revision;        /* Hardware revision of the board        */
  unsigned short vendor_id;         /* Meilhaus vendor id (0x1402)           */
  unsigned short device_id;         /* Device ID                             */
  int pci_bus_no;                   /* PCI bus number                        */
  int pci_dev_no;                   /* PCI device number                     */
  int pci_func_no;                  /* PCI function number                   */
  char irq;                         /* IRQ assigned from the PCI BIOS        */
  int board_in_use;                 /* Indicates if board is already in use  */
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 4, 0 )
  struct semaphore use_lock;        /* Guards board_in_use                   */
  struct semaphore ioctl_mutex;     /* Guards ioctl() calls                  */
  spinlock_t irq_lock;
#endif
  unsigned int num_dacs;            /* number of DACs on board               */
  int keep_voltage_on_close[ 16 ];
  me6x00_circ_buf_st buf[ 4 ];
  wait_queue_head_t wait_queues[ 4 ];
} me6x00_info_st;


/* Prototypes of all functions */

static int me6x00_open( struct inode * /* inode_p */,
			struct file  * /* file_p  */ );

static int me6x00_release( struct inode * /* inode_p */,
			   struct file  * /* file_p  */ );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static int me6x00_ioctl( struct inode * /* inode_p */,
			 struct file  * /* file_p  */,
			 unsigned int   /* service */,
			 unsigned long  /* arg     */ );
#else
static long me6x00_ioctl( struct file  * /* file_p  */,
			  unsigned int   /* service */,
			  unsigned long  /* arg     */ );
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 19 )
static irqreturn_t me6x00_isr( int              /* irq    */,
			       void *           /* dev_id */ );
#elif LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static irqreturn_t me6x00_isr( int              /* irq    */,
			       void *           /* dev_id */,
			       struct pt_regs * /* dummy  */ );
#else
static void me6x00_isr( int              /* irq    */,
			void *           /* dev_id */,
			struct pt_regs * /* dummy  */ );
#endif

static int me6x00_find_boards( void );


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static int me6x00_init_board( struct pci_dev *             /* dev */,
			      const struct pci_device_id * /* id  */ );
#else
static __init int me6x00_init_board( int              /* board_count */,
				     struct pci_dev * /* dev         */ );
#endif

static int me6x00_xilinx_download( me6x00_info_st * info );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
static void me6x00_release_board( struct pci_dev * /* dev */ );
#endif

static int me6x00_reset_board( int /* board */,
			       int /* from  */ );

static int me6x00_board_info( void __user * /* arg   */,
			      int           /* minor */ );

static int me6x00_board_keep_volts( void __user * /* arg   */,
				    int           /* minor */ );

static int me6x00_set_mode( void __user * /* arg   */,
			    int           /* minor */);

static int me6x00_start_stop_conv( void __user * /* arg   */,
				   int           /* minor */ );

static int me6x00_clear_enable_fifo( void __user * /* arg   */,
				     int           /* minor */ );

static int me6x00_endis_extrig( void __user * /* arg   */,
				int           /* minor */ );

static int me6x00_rifa_extrig( void __user * /* arg   */,
			       int           /* minor */ );

static int me6x00_set_timer( void __user * /* arg   */,
			     int           /* minor */ );

static int me6x00_write_single( void __user * /* arg   */,
				int           /* minor */ );

static int me6x00_write_continuous( void __user * /* arg   */,
				    int           /* minor */ );

static int me6x00_write_wraparound( me6x00_write_st * /* arg   */,
				    int               /* minor */ );

static int me6x00_buf_count( me6x00_circ_buf_st * /* buf */ );

static int me6x00_space_to_end( me6x00_circ_buf_st * /* buf */ );

static int me6x00_values_to_end( me6x00_circ_buf_st * /* buf */);


/* Debugging macros */

#ifdef ME6X00_CALL_DEBUG
#undef CALL_PDEBUG /* only to be sure */
#define CALL_PDEBUG( fmt, args... ) printk(KERN_DEBUG "ME6X00: " fmt, ##args )
#else 
# define CALL_PDEBUG( fmt, args... ) /* no debugging, do nothing */
#endif


#ifdef ME6X00_ISR_DEBUG
#undef ISR_PDEBUG /* only to be sure */
#define ISR_PDEBUG( fmt, args... ) printk( KERN_DEBUG "ME6X00: " fmt, ##args )
#else 
#define ISR_PDEBUG( fmt, args... ) /* no debugging, do nothing */
#endif

#ifdef ME6X00_PORT_DEBUG
#undef PORT_PDEBUG /* only to be sure */
#define PORT_PDEBUG( fmt, args... ) printk( KERN_DEBUG "ME6X00: " fmt, ##args )
#else 
#define PORT_PDEBUG( fmt, args... ) /* no debugging, do nothing */
#endif

#ifdef ME6X00_DEBUG
#undef PDEBUG // only to be sure
#define PDEBUG( fmt, args... ) \
printk( KERN_DEBUG "ME6X00: " fmt, ##args )
#else 
#define PDEBUG( fmt, args... ) /* no debugging, do nothing */
#endif

#endif  /* ! ME6X00_BRD_HEADER */


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
