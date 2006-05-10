/*
 *  $Id$
 */

/* Device driver for Meilhaus me1000 board.
 * ========================================
 *
 *   Copyright (C) 2001 Meilhaus Electronic GmbH (support@meilhaus.de)
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
 */

/*
 * Source File : me6x00_drv.h
 * Destination : me6x00.o
 * Author      : GG (Guenter Gebhardt)
 *
 * File History: Version   Date       Editor   Action
 *---------------------------------------------------------------------
 *               1.00.00   02.01.21   GG       first release
 *               1.01.00   02.10.17   JTT      clean up and bug fixes
 *---------------------------------------------------------------------
 *
 * Description:
 *
 *   Header file for Meilhaus ME-6000 and ME-61000 driver module.
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


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __KERNEL__

/* Define the proper debug mode */

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
#define PDEBUG( fmt, args... ) printk( KERN_DEBUG "ME6X00: " fmt, ##args )
#else 
#define PDEBUG( fmt, args... ) /* no debugging, do nothing */
#endif

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

#define ME6X00_CTRLREG_DAC00  0x00  // R/W 
#define ME6X00_STATREG_DAC00  0x04  // R/_
#define ME6X00_FIFOREG_DAC00  0x08  // _/W
#define ME6X00_SVALREG_DAC00  0x0C  // R/W
#define ME6X00_TIMEREG_DAC00  0x10  // _/W

#define ME6X00_CTRLREG_DAC01  0x18  // R/W 
#define ME6X00_STATREG_DAC01  0x1C  // R/_
#define ME6X00_FIFOREG_DAC01  0x20  // _/W
#define ME6X00_SVALREG_DAC01  0x24  // R/W
#define ME6X00_TIMEREG_DAC01  0x28  // _/W

#define ME6X00_CTRLREG_DAC02  0x30  // R/W 
#define ME6X00_STATREG_DAC02  0x34  // R/_
#define ME6X00_FIFOREG_DAC02  0x38  // _/W
#define ME6X00_SVALREG_DAC02  0x3C  // R/W
#define ME6X00_TIMEREG_DAC02  0x40  // _/W

#define ME6X00_CTRLREG_DAC03  0x48  // R/W 
#define ME6X00_STATREG_DAC03  0x4C  // R/_
#define ME6X00_FIFOREG_DAC03  0x50  // _/W
#define ME6X00_SVALREG_DAC03  0x54  // R/W
#define ME6X00_TIMEREG_DAC03  0x58  // _/W

#define ME6X00_IRQSREG_DACXX  0x60  // R/_ 
#define ME6X00_IRQRREG_DAC00  0x64  // R/_
#define ME6X00_IRQRREG_DAC01  0x68  // R/_
#define ME6X00_IRQRREG_DAC02  0x6C  // R/_
#define ME6X00_IRQRREG_DAC03  0x70  // R/_

#define ME6X00_SVALREG_DAC04  0x74  // _/W
#define ME6X00_SVALREG_DAC05  0x78  // _/W
#define ME6X00_SVALREG_DAC06  0x7C  // _/W
#define ME6X00_SVALREG_DAC07  0x80  // _/W
#define ME6X00_SVALREG_DAC08  0x84  // _/W
#define ME6X00_SVALREG_DAC09  0x88  // _/W
#define ME6X00_SVALREG_DAC10  0x8C  // _/W
#define ME6X00_SVALREG_DAC11  0x90  // _/W
#define ME6X00_SVALREG_DAC12  0x94  // _/W
#define ME6X00_SVALREG_DAC13  0x98  // _/W
#define ME6X00_SVALREG_DAC14  0x9C  // _/W
#define ME6X00_SVALREG_DAC15  0xA0  // _/W

#define ME6X00_STATREG_DACXX  0xA4  // R/_


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

#endif


/* Maximum count of ME-6x00 devices */

#define ME6X00_MAX_BOARDS 4


/* Fifo size in values of short */

#define ME6X00_FIFO_SIZE 0x2000


/* Minimum Timer Divisor */

#define ME6X00_MIN_TICKS 66


typedef enum {
	ME6X00_SINGLE,
	ME6X00_WRAPAROUND,
	ME6X00_CONTINUOUS
} me6x00_mode_et;


typedef enum {
	ME6X00_START,
	ME6X00_STOP
} me6x00_stasto_et;


typedef enum {
	ME6X00_ENABLE,
	ME6X00_DISABLE
} me6x00_endis_et;


typedef enum {
	ME6X00_RISING,
	ME6X00_FALLING
} me6x00_rifa_et;


typedef enum {
	ME6X00_DONT_KEEP,
	ME6X00_KEEP
} me6x00_keep_et;

typedef enum { 
	ME6X00_DAC00,
	ME6X00_DAC01,
	ME6X00_DAC02,
	ME6X00_DAC03,
	ME6X00_DAC04,
	ME6X00_DAC05,
	ME6X00_DAC06,
	ME6X00_DAC07,
	ME6X00_DAC08,
	ME6X00_DAC09,
	ME6X00_DAC10,
	ME6X00_DAC11,
	ME6X00_DAC12,
	ME6X00_DAC13,
	ME6X00_DAC14,
	ME6X00_DAC15
} me6x00_dac_et;


typedef struct {
	me6x00_dac_et  dac;
	me6x00_mode_et mode;
} me6x00_mode_st;


typedef struct {
	me6x00_dac_et    dac;
	me6x00_stasto_et stasto;
} me6x00_stasto_st;


typedef struct {
	me6x00_dac_et   dac;
	me6x00_endis_et endis;
} me6x00_endis_st;


typedef struct {
	me6x00_dac_et   dac;
	me6x00_rifa_et  rifa;
} me6x00_rifa_st;


typedef struct {
	me6x00_dac_et   dac;
	unsigned int    divisor;
} me6x00_timer_st;


typedef struct {
	me6x00_dac_et   dac;
	unsigned short  value;
} me6x00_single_st;


typedef struct {
	me6x00_dac_et   dac;
	me6x00_keep_et  value;
} me6x00_keep_st;


typedef struct {
	unsigned int vendor_ID;
	unsigned int device_ID;
	unsigned int serial_no;
	unsigned int hw_revision;
	int bus_no;
	int dev_no;
	int func_no;
} me6x00_dev_info;


typedef struct {
	me6x00_dac_et   dac;    /* Selects the DAC                         */
	unsigned short  *buf;   /* Pointer to user data                    */
	int             count;  /* Count of short values                   */
	int             ret;    /* Returns number of values really written */
} me6x00_write_st;


#ifdef __KERNEL__

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
  spinlock_t use_lock;              /* Guards board_in_use                   */
  spinlock_t irq_lock;
#endif
  unsigned int num_dacs;            /* number of DACs on board               */
  int keep_voltage_on_close[ 16 ];
  me6x00_circ_buf_st buf[ 4 ];
  wait_queue_head_t wait_queues[ 4 ];
} me6x00_info_st;

#endif


/* IOCTL's */

#define ME6X00_MAGIC 'r'
#define ME6X00_SET_MODE           _IOW  ( ME6X00_MAGIC, 32, me6x00_mode_st   )
#define ME6X00_START_STOP_CONV    _IOW  ( ME6X00_MAGIC, 33, me6x00_stasto_st )
#define ME6X00_CLEAR_ENABLE_FIFO  _IOW  ( ME6X00_MAGIC, 34, me6x00_endis_st  )
#define ME6X00_ENDIS_EXTRIG       _IOW  ( ME6X00_MAGIC, 35, me6x00_endis_st  )
#define ME6X00_RIFA_EXTRIG        _IOW  ( ME6X00_MAGIC, 36, me6x00_rifa_st   )
#define ME6X00_SET_TIMER          _IOW  ( ME6X00_MAGIC, 37, me6x00_timer_st  )
#define ME6X00_WRITE_SINGLE       _IOW  ( ME6X00_MAGIC, 38, me6x00_single_st )
#define ME6X00_WRITE_CONTINUOUS   _IOWR ( ME6X00_MAGIC, 39, me6x00_write_st  )
#define ME6X00_WRITE_WRAPAROUND   _IOWR ( ME6X00_MAGIC, 40, me6x00_write_st  )
#define ME6X00_RESET_BOARD        _IO   ( ME6X00_MAGIC, 41                   )
#define ME6X00_BOARD_INFO         _IOR  ( ME6X00_MAGIC, 42, me6x00_dev_info  )
#define ME6X00_KEEP_VOLTAGE       _IOW  ( ME6X00_MAGIC, 43, me6x00_keep_st   )

#define ME6X00_IOCTL_MINNR        _IOC_NR( ME6X00_SET_MODE )
#define ME6X00_IOCTL_MAXNR        _IOC_NR( ME6X00_KEEP_VOLTAGE )


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
