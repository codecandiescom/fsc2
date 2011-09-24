/*
 *  Copyright (C) 2002-2011  Jens Thoms Toerring
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


#define NI6601_MAX_BOARDS       4

#define NI6601_TIME_RESOLUTION  5.0e-8   /* 50 ns or 20 MHz */


typedef enum {
	NI6601_COUNTER_0 = 0,
	NI6601_COUNTER_1,
	NI6601_COUNTER_2,
	NI6601_COUNTER_3
} NI6601_COUNTER_NUM;


typedef enum {
	NI6601_NONE = 0,
	NI6601_LOW,
	NI6601_DEFAULT_SOURCE,
	NI6601_DEFAULT_GATE,
	NI6601_GATE_0,
	NI6601_GATE_1,
	NI6601_GATE_2,
	NI6601_GATE_3,
	NI6601_NEXT_SOURCE,
	NI6601_RTSI_0,
	NI6601_RTSI_1,
	NI6601_RTSI_2,
	NI6601_RTSI_3,
	NI6601_NEXT_OUT,
	NI6601_SOURCE_0,
	NI6601_SOURCE_1,
	NI6601_SOURCE_2,
	NI6601_SOURCE_3,
	NI6601_NEXT_GATE,
	NI6601_NEXT_TC,
	NI6601_TIMEBASE_1,
	NI6601_TIMEBASE_2,
	NI6601_TIMEBASE_3
} NI6601_INPUT;


typedef enum {
	NI6601_NORMAL = 0,
	NI6601_INVERTED
} NI6601_POLARITY;


typedef struct {
	NI6601_COUNTER_NUM counter;
} NI6601_DISARM;


typedef struct {
	unsigned char value;
	unsigned char mask;
} NI6601_DIO_VALUE;


typedef struct {
	NI6601_COUNTER_NUM counter;
	int wait_for_end;
	unsigned long count;
	int do_poll;
} NI6601_COUNTER_VAL;


typedef struct {
	NI6601_COUNTER_NUM counter;
	unsigned long low_ticks;
	unsigned long high_ticks;
	NI6601_INPUT gate;
	NI6601_INPUT source;
	int continuous;
	int disable_output;
	NI6601_POLARITY output_polarity;
} NI6601_PULSES;


typedef struct {
	NI6601_COUNTER_NUM counter;
	NI6601_POLARITY source_polarity;
	NI6601_INPUT gate;
	NI6601_INPUT source;
	int disable_output;
} NI6601_COUNTER;


typedef struct {
	NI6601_COUNTER_NUM counter;
	NI6601_POLARITY source_polarity;
	NI6601_INPUT gate;
	NI6601_INPUT source;
	int disable_output;
	unsigned long num_points;
	int continuous;
} NI6601_BUF_COUNTER;


typedef struct {
	unsigned long count;
} NI6601_BUF_AVAIL;

typedef struct {
	NI6601_COUNTER_NUM counter;
	int state;
} NI6601_IS_ARMED;


/* Definition the numbers of all the different ioctl() commands - the
   character 'j' as the 'magic' type seems to be not in use for command
   numbers above 0x3F (see linux/Documentation/ioctl-number.txt). All
   commands need write access to the structure passed to ioctl() because
   status and error states are returned via this structure. */

#define NI6601_MAGIC_IOC      'j'

#define NI6601_IOC_DIO_IN   _IOWR( NI6601_MAGIC_IOC, 0x80, NI6601_DIO_VALUE )
#define NI6601_IOC_DIO_OUT  _IOW ( NI6601_MAGIC_IOC, 0x81, NI6601_DIO_VALUE )
#define NI6601_IOC_COUNT    _IOWR( NI6601_MAGIC_IOC, 0x82, NI6601_COUNTER_VAL )
#define NI6601_IOC_PULSER   _IOW ( NI6601_MAGIC_IOC, 0x83, NI6601_PULSES )
#define NI6601_IOC_COUNTER  _IOW ( NI6601_MAGIC_IOC, 0x84, NI6601_COUNTER )
#define NI6601_IOC_START_BUF_COUNTER  \
			    _IOW ( NI6601_MAGIC_IOC, 0x85, NI6601_COUNTER )
#define NI6601_IOC_STOP_BUF_COUNTER  \
			    _IO  ( NI6601_MAGIC_IOC, 0x86 )
#define NI6601_IOC_GET_BUF_AVAIL  \
			    _IOR ( NI6601_MAGIC_IOC, 0x88, NI6601_BUF_AVAIL )
#define NI6601_IOC_IS_BUSY  _IOWR( NI6601_MAGIC_IOC, 0x87, NI6601_IS_ARMED )
#define NI6601_IOC_DISARM   _IOW ( NI6601_MAGIC_IOC, 0x88, NI6601_DISARM )

#define NI6601_MIN_NR       _IOC_NR( NI6601_IOC_DIO_IN )
#define NI6601_MAX_NR       _IOC_NR( NI6601_IOC_DISARM )


/* All of the rest needs only to be seen by the module itself but not by
   user-land applications */


#if defined __KERNEL__


#include <linux/version.h>
#include <linux/autoconf.h>

#if ! defined ( CONFIG_PCI )
#error "PCI support in kernel is missing."
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif

#if defined( MODVERSIONS )
#include <linux/modversions.h>
#endif

#include <linux/module.h>

#if defined( CONFIG_SMP ) && ! defined( __SMP__ )
#define __SMP__
#endif

#else

#include <linux/module.h>

#endif


#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/stddef.h>             /* for definition of NULL              */
#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 26 )
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#include <linux/sched.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#else
#define __user
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 9 )
#define iowrite8( a, v )   writeb( a, v )
#define iowrite16( a, v )  writew( a, v )
#define iowrite32( a, v )  writel( a, v )
#define ioread8( a )       readb( a )
#define ioread16( a )      readw( a )
#define ioread32( a )      readl( a )
#endif


/* The following two macros are removed since kernel 2.6.37, but we use them */

#ifndef init_MUTEX
# define init_MUTEX( sem ) sema_init( sem, 1 )
#endif
#ifndef init_MUTEX_LOCKED
# define init_MUTEX_LOCKED( sem ) sema_init( sem, 0 )
#endif

#include "autoconf.h"


#define NI6601_NAME "ni6601"

#define PCI_VENDOR_NATINST         0x1093
#define PCI_DEVICE_NATINST_6601    0x2C60


/* Converts output pin names to PFI numbers */

#define NI6601_OUT( num )      ( 36 - ( num ) * 4 )
#define NI6601_UD_AUX( num )   ( 37 - ( num ) * 4 )
#define NI6601_GATE( num )     ( 38 - ( num ) * 4 )
#define NI6601_SOURCE( num )   ( 39 - ( num ) * 4 )


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 9 )
typedef struct {
	unsigned char *irq_ack[ 4 ];                 /* 16-bit write-only   */
	unsigned char *status[ 4 ];                  /* 16-bit read-only    */
	unsigned char *joint_status[ 4 ];            /* 16-bit read-only    */
	unsigned char *command[ 4 ];                 /* 16-bit write-only   */
	unsigned char *hw_save[ 4 ];                 /* 32-bit read-only    */
	unsigned char *sw_save[ 4 ];                 /* 32-bit read-only    */
	unsigned char *mode[ 4 ];                    /* 16-bit write-only   */
	unsigned char *joint_status_1[ 4 ];          /* 16-bit read-only    */
	unsigned char *autoincrement[ 4 ];           /* 16-bit write-only   */
	unsigned char *load_a[ 4 ];                  /* 32-bit write-only   */
	unsigned char *joint_status_2[ 4 ];          /* 16-bit read-only    */
	unsigned char *load_b[ 4 ];                  /* 32-bit write-only   */
	unsigned char *input_select[ 4 ];            /* 16-bit write-only   */
	unsigned char *joint_reset[ 4 ];             /* 16-bit write-only   */
	unsigned char *irq_enable[ 4 ];              /* 16-bit write-only   */
	unsigned char *counting_mode[ 4 ];           /* 16-bit write-only   */
	unsigned char *second_gate[ 4 ];             /* 16-bit write-only   */
	unsigned char *dma_config[ 4 ];              /* 16-bit write-only   */
	unsigned char *dma_status[ 4 ];              /* 16-bit read-only    */

	unsigned char *dio_parallel_in;              /* 16-bit read-only    */
	unsigned char *dio_output;                   /* 16-bit write-only   */
	unsigned char *dio_control;                  /* 16-bit write-only   */
	unsigned char *dio_serial_input;             /* 16-bit read-only    */

	unsigned char *io_config[ 10 ];              /* 16-bit read & write */

	unsigned char *reset_control;                /* 32-bit write-only   */
	unsigned char *global_irq_control;           /* 32-bit write-only   */
	unsigned char *global_irq_status;            /* 32-bit read-only    */
	unsigned char *dma_configuration;            /* 32-bit write-only   */
	unsigned char *clock_config;                 /* 32-bit write-only   */
	unsigned char *chip_signature;               /* 32-bit read-only    */
} Registers;
#else
typedef struct {
	void __iomem *irq_ack[ 4 ];                 /* 16-bit write-only   */
	void __iomem *status[ 4 ];                  /* 16-bit read-only    */
	void __iomem *joint_status[ 4 ];            /* 16-bit read-only    */
	void __iomem *command[ 4 ];                 /* 16-bit write-only   */
	void __iomem *hw_save[ 4 ];                 /* 32-bit read-only    */
	void __iomem *sw_save[ 4 ];                 /* 32-bit read-only    */
	void __iomem *mode[ 4 ];                    /* 16-bit write-only   */
	void __iomem *joint_status_1[ 4 ];          /* 16-bit read-only    */
	void __iomem *autoincrement[ 4 ];           /* 16-bit write-only   */
	void __iomem *load_a[ 4 ];                  /* 32-bit write-only   */
	void __iomem *joint_status_2[ 4 ];          /* 16-bit read-only    */
	void __iomem *load_b[ 4 ];                  /* 32-bit write-only   */
	void __iomem *input_select[ 4 ];            /* 16-bit write-only   */
	void __iomem *joint_reset[ 4 ];             /* 16-bit write-only   */
	void __iomem *irq_enable[ 4 ];              /* 16-bit write-only   */
	void __iomem *counting_mode[ 4 ];           /* 16-bit write-only   */
	void __iomem *second_gate[ 4 ];             /* 16-bit write-only   */
	void __iomem *dma_config[ 4 ];              /* 16-bit write-only   */
	void __iomem *dma_status[ 4 ];              /* 16-bit read-only    */

	void __iomem *dio_parallel_in;              /* 16-bit read-only    */
	void __iomem *dio_output;                   /* 16-bit write-only   */
	void __iomem *dio_control;                  /* 16-bit write-only   */
	void __iomem *dio_serial_input;             /* 16-bit read-only    */

	void __iomem *io_config[ 10 ];              /* 16-bit read & write */

	void __iomem *reset_control;                /* 32-bit write-only   */
	void __iomem *global_irq_control;           /* 32-bit write-only   */
	void __iomem *global_irq_status;            /* 32-bit read-only    */
	void __iomem *dma_configuration;            /* 32-bit write-only   */
	void __iomem *clock_config;                 /* 32-bit write-only   */
	void __iomem *chip_signature;               /* 32-bit read-only    */
} Registers;
#endif

typedef struct {
	struct pci_dev *dev;

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 9 )
	char *mite;
	char *addr;
#else
	void __iomem *mite;
	void __iomem *addr;
#endif

	unsigned long mite_phys;
	unsigned long mite_len;
	unsigned long addr_phys;
	unsigned long addr_len;

	int is_init;
	int in_use;
	int is_enabled;
	int has_region;
	struct semaphore open_mutex;
	struct semaphore ioctl_mutex;

	unsigned int irq;
	int expect_tc_irq;
	int tc_irq_enabled[ 4 ];
	int tc_irq_raised[ 4 ];
	int dma_irq_enabled;

	wait_queue_head_t tc_waitqueue;

	u16 dio_mask;

	Registers regs;

	int buf_counter;             /* counter used for buffered counting */
	int buf_continuous;
	int buf_overflow;
	int too_fast;
	int is_just_started;
	int buf_waiting;
	u32 *buf;
	u32 *low_ptr;
	u32 *high_ptr;
	u32 *buf_top;

	wait_queue_head_t dma_waitqueue;
} Board;


/* Functions from fops.c */

int ni6601_open( struct inode * /* inode_p */,
		 struct file *  /* filep   */ );

int ni6601_release( struct inode * /* inode_p */,
		    struct file *  /* filep   */ );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
int ni6601_ioctl( struct inode * /* inode_p */,
		  struct file *  /* filep   */,
		  unsigned int   /* cmd     */,
		  unsigned long  /* arg     */ );
#else
long ni6601_ioctl( struct file *  /* filep */,
		   unsigned int   /* cmd   */,
		   unsigned long  /* arg   */ );
#endif

unsigned int ni6601_poll( struct file *              /* filep */,
			  struct poll_table_struct * /* pt    */);

ssize_t ni6601_read( struct file * /* filep */,
		     char __user * /* buff  */,
		     size_t        /* count */,
		     loff_t *      /* offp  */ );


/* Functions from ioctl.c */

int ni6601_dio_in( Board *            /* board */,
		   NI6601_DIO_VALUE * /* arg   */ );

int ni6601_dio_out( Board *            /* board */,
		    NI6601_DIO_VALUE * /* arg   */ );

int ni6601_disarm( Board *         /* board */,
		   NI6601_DISARM * /* arg   */ );

int ni6601_read_count( Board *              /* board */,
		       NI6601_COUNTER_VAL * /* arg   */ );

int ni6601_start_pulses( Board *         /* board */,
			 NI6601_PULSES * /* arg   */);

int ni6601_start_counting( Board *          /* board */,
			   NI6601_COUNTER * /* arg   */);

int ni6601_start_buf_counting( Board *              /* board */,
			       NI6601_BUF_COUNTER * /* arg   */ );

int ni6601_get_buf_avail( Board *            /* board */,
			  NI6601_BUF_AVAIL * /* arg   */ );

int ni6601_stop_buf_counting( Board * /* board */ );

int ni6601_is_busy( Board *           /* board */,
		    NI6601_IS_ARMED * /* arg   */ );

void ni6601_tc_irq_enable( Board * /* board   */,
			   int     /* counter */ );

void ni6601_tc_irq_disable( Board * /* board   */,
			    int     /* counter */ );

void ni6601_dma_irq_enable( Board * /* board   */,
			    int     /* counter */ );

void ni6601_dma_irq_disable( Board * /* board   */,
			     int     /* counter */ );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 19 )
irqreturn_t ni6601_irq_handler( int              /* irq   */,
				void *           /* data  */ );
#elif LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
irqreturn_t ni6601_irq_handler( int              /* irq   */,
				void *           /* data  */,
				struct pt_regs * /* dummy */ );
#else
void ni6601_irq_handler( int              /* irq   */,
			 void *           /* data  */,
			 struct pt_regs * /* dummy */ );
#endif


/* Functions from utils.c */

void ni6601_release_resources( Board * /* board */ );

void ni6601_register_setup( Board * /* board */ );

void ni6601_dio_init( Board * /* board */ );

void ni6601_enable_out( Board * /* board */,
			int     /* pfi   */ );

void ni6601_disable_out( Board * /* board */,
			 int     /* pfi   */ );

int ni6601_input_gate( int   /* gate */,
		       u16 * /* bits */ );

int ni6601_input_source( int   /* source */,
			 u16 * /* bits   */ );


/* Clock Config Register */

#define COUNTER_SWAP                        ( 1 << 21 )

/* Gi Command Register */

#define DISARM_COPY                         ( 1 << 15 )
#define SAVE_TRACE_COPY                     ( 1 << 14 )
#define ARM_COPY                            ( 1 << 13 )
#define BANK_SWITCH_ENABLE                  ( 1 << 12 )
#define BANK_SWITCH_MODE                    ( 1 << 11 )
#define BANK_SWITCH_START                   ( 1 << 10 )
#define LITTLE_BIG_ENDIAN                   ( 1 <<  9 )
#define SYNCHRONIZE_GATE                    ( 1 <<  8 )
#define WRITE_SWITCH                        ( 1 <<  7 )
#define ALWAYS_DOWN                         ( 0 <<  5 )
#define ALWAYS_UP                           ( 1 <<  5 )
#define HW_UP_DOWN                          ( 2 <<  5 )
#define HW_DOWN_UP                          ( 3 <<  5 )
#define DISARM                              ( 1 <<  4 )
#define LOAD                                ( 1 <<  2 )
#define SAVE_TRACE                          ( 1 <<  1 )
#define ARM                                 ( 1 <<  0 )

/* Counting Mode Register */

#define ALTERNATE_SYNC                      ( 1 << 15 )
#define PRESCALE                            ( 1 << 14 )
#define INDEX_PHASE_AL_BL                   ( 0 <<  5 )
#define INDEX_PHASE_AL_BH                   ( 1 <<  5 )
#define INDEX_PHASE_AH_BL                   ( 2 <<  5 )
#define INDEX_PHASE_AH_BH                   ( 3 <<  5 )
#define INDEX_MODE                          ( 1 <<  4 )
#define NORMAL                                0
#define QUAD_X1                               1
#define QUAD_X2                               2
#define QUAD_X4                               3
#define TWO_PULSE                             4
#define SYNC_SOURCE                           6

/* Gi DMA Config Register */

#define DMA_INT                             ( 1 <<  2 )
#define DMA_WRITE                           ( 1 <<  1 )
#define DMA_ENABLE                          ( 1 <<  0 )

/* Gi DMA Status Register */

#define DRQ_STATUS                          ( 1 << 15 )
#define DRQ_ERROR                           ( 1 << 14 )
#define DRQ_READBACK                        ( 1 << 13 )

/* Gi Status Register */

#define Gi_INTERRUPT                        ( 1 << 15 )
#define Gi_TC_STATUS                        ( 1 <<  3 )


/* #### The bit(s) of the Gi Interrupt Acknowledge Register aren't   ####
   #### documented and the picked bits are just guesswork, derived   ####
   #### from the documentation of the DAQ-STC chip, where also many  ####
   #### other bits are at the same positions as on the TIO chip      #### */

/* Gi Interrupt Acknowledge Register */

#define Gi_TC_INTERRUPT_ACK                 ( 1 << 14 )

/* Gi Input Select Register */

#define INVERTED_SOURCE_POLARITY            ( 1 << 15 )
#define INVERTED_OUTPUT_POLARITY            ( 1 << 14 )
#define OR_GATE                             ( 1 << 13 )
#define GATE_SELECT_LOAD_SOURCE             ( 1 << 12 )

/* Gi Mode Register */

#define RELOAD_SOURCE_SWITCHING            ( 1 << 15 )
#define LOADING_ON_GATE                    ( 1 << 14 )
#define GATE_POLARITY                      ( 1 << 13 )
#define LOADING_ON_TC                      ( 1 << 12 )
#define COUNTING_ONCE_NONE                   0
#define COUNTING_ONCE_TC                   ( 1 << 10 )
#define COUNTING_ONCE_GATE                 ( 2 << 10 )
#define COUNTING_ONCE_TC_OR_GATE           ( 3 << 10 )
#define OUTPUT_MODE_TC                     ( 1 <<  8 )
#define OUTPUT_MODE_TOGGLE_ON_TC           ( 2 <<  8 )
#define OUTPUT_MODE_TOGGLE_ON_TC_OR_GATE   ( 3 <<  8 )
#define LOAD_SOURCE_SELECT_A                 0
#define LOAD_SOURCE_SELECT_B               ( 1 <<  7 )
#define STOP_MODE_ON_GATE                    0
#define STOP_MODE_ON_GATE_OR_1ST_TC        ( 1 <<  5 )
#define STOP_MODE_ON_GATE_OR_2ND_TC        ( 2 <<  5 )
#define TRIGGER_MODE_1ST_STARTS_2ND_STOPS    0
#define TRIGGER_MODE_1ST_STOPS_2ND_STARTS  ( 1 <<  3 )
#define TRIGGER_MODE_EDGE_STARTS_TC_STOPS  ( 2 <<  3 )
#define TRIGGER_MODE_NO_GATE               ( 3 <<  3 )
#define GATE_ON_BOTH_EDGES                 ( 1 <<  2 )
#define GATING_MODE_DISABLED                 0
#define GATING_MODE_LEVEL                    1
#define GATING_MODE_RISING_EDGE              2
#define GATING_MODE_FALLING_EDGE             3

/* G01/G23 Status Register */

#define G1_ARMED                           ( 1 <<  9 )
#define G3_ARMED                           ( 1 <<  9 )
#define G0_ARMED                           ( 1 <<  8 )
#define G2_ARMED                           ( 1 <<  8 )
#define G1_NEXT_LOAD_SOURCE_A                0
#define G3_NEXT_LOAD_SOURCE_A                0
#define G1_NEXT_LOAD_SOURCE_B              ( 1 <<  5 )
#define G3_NEXT_LOAD_SOURCE_B              ( 1 <<  5 )
#define G0_NEXT_LOAD_SOURCE_A                0
#define G2_NEXT_LOAD_SOURCE_A                0
#define G0_NEXT_LOAD_SOURCE_B              ( 1 <<  4 )
#define G2_NEXT_LOAD_SOURCE_B              ( 1 <<  4 )
#define G1_COUNTING                        ( 1 <<  3 )
#define G3_COUNTING                        ( 1 <<  3 )
#define G0_COUNTING                        ( 1 <<  2 )
#define G2_COUNTING                        ( 1 <<  2 )

#define Gi_ARMED( x )                      ( 1 << ( 8 + ( ( x ) & 1 ) ) )
#define Gi_NEXT_LOAD_SOURCE_A( x )         0
#define Gi_NEXT_LOAD_SOURCE_B( x )         ( 1 << ( 4 + ( ( x ) & 1 ) ) )
#define Gi_COUNTING( x )                   ( 1 << ( 2 + ( ( x ) & 1 ) ) )


/* G01/G23 Joint Reset Register */

#define G1_RESET                           ( 1 <<  3 )
#define G3_RESET                           ( 1 <<  3 )
#define G0_RESET                           ( 1 <<  2 )
#define G2_RESET                           ( 1 <<  2 )

#define Gi_Reset( x )                      ( 1 << ( 2 + ( ( x ) & 1 ) ) )


/* #### Also the bit(s) of the Gi Interrupt Enable Register aren't   ####
   #### documented and derived by pure guesswork from the documen-   ####
   #### tation of the DAQ-STC chip, where also many other bits are   ####
   #### at the same positions as on the TIO chip                     #### */

/* Gi Interrupt Enable Register */

#define G1_TC_INTERRUPT_ENABLE             ( 1 <<  9 )
#define G3_TC_INTERRUPT_ENABLE             ( 1 <<  9 )
#define G0_TC_INTERRUPT_ENABLE             ( 1 <<  6 )
#define G2_TC_INTERRUPT_ENABLE             ( 1 <<  6 )

#define Gi_TC_INTERRUPT_ENABLE( x )        ( 1 << ( 6 + 3 * ( ( x ) & 1 ) ) )


/* #### Neither this 32-bit register nor its bits are documented by  ####
   #### National Instruments and I only found out about it from the  ####
   #### newsgroup 'natinst.public.daq.counter-timer.general', where  ####
   #### someone told about that register (on Aug 25 2004)            #### */

/* Global Interrupt Control Register */

#define GLOBAL_IRQ_ENABLE                  ( 1 << 31 )


/* DMA Configuration Register */

#define DMA_RESET_3        		   ( 1 << 31 )
#define DMA_RESET_2        		   ( 1 << 23 )
#define DMA_RESET_1        		   ( 1 << 15 )
#define DMA_RESET_0        		   ( 1 <<  7 )
#define DMA_RESET_ALL      		   ( DMA_RESET_3 | DMA_RESET_2 |  \
                                             DMA_RESET_1 | DMA_RESET_0 )
#define GLOBAL_DMA_RESET( ch )             ( 1 << ( 8 * ( ch ) + 7 ) )
#define DMA_SELECT( ch, cnt )              ( ( 1 << ( cnt ) )             \
                                             << ( 8 * ( ch  ) ) )

/* Reset Control Register */

#define SOFT_RESET                         ( 1 << 0 )


/* Global Interrupt Status Register */

#define GLOBAL_INT                         ( 1 << 31 )
#define CASCADE_INT                        ( 1 << 29 )
#define COUNTER_3_INT                      ( 1 << 19 )
#define COUNTER_2_INT                      ( 1 << 18 )
#define COUNTER_1_INT                      ( 1 << 17 )
#define COUNTER_0_INT                      ( 1 << 16 )
#define COUNTER_INT( cnt )                 ( ( 1 << ( cnt ) ) << 16 )


#if defined NI6601_DEBUG
#define PDEBUG( fmt, args... )                                \
	do {                                                  \
        	printk( KERN_DEBUG " " NI6601_NAME " %s(): "  \
			fmt, __FUNCTION__ , ##args );         \
	} while( 0 )
#else
#define PDEBUG( ftm, args... )
#endif


#endif /* defined __KERNEL__ */


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
