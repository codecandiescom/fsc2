/*
  $Id$
 
  Driver for National Instruments PCI E Series DAQ boards

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

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


#if ! defined __KERNEL__
#error "This file is for inclusion by modules only."
#endif


#if ! defined NI_DAQ_BOARD_HEADER
#define NI_DAQ_BOARD_HEADER

#include <linux/config.h>

#if defined( CONFIG_MODVERSIONS ) && ! defined( MODVERSIONS )
#define MODVERSIONS
#endif

#if defined( MODVERSIONS )
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/version.h>

#if ! defined( KERNEL_VERSION )
#define KERNEL_VERSION( a, b, c ) ( ( ( a ) << 16 ) | ( ( b ) << 8 ) | ( c ) )
#endif

#if defined( CONFIG_SMP ) && ! defined( __SMP__ )
#define __SMP__
#endif


#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/stddef.h>             /* for definition of NULL              */
#include <asm/semaphore.h>            /* for definition of struct semaphore  */
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/delay.h>

#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/poll.h>

#if defined ( CONFIG_PROC_FS )
#include <linux/proc_fs.h>
#endif


/* Please note: All of the following must *not* depend on the special
                hardware, i.e. it must work for both PCI-E-series as
	        well as AT-MIO-series boards! */


#include "autoconf.h"
#include "ni_daq_drv.h"
#include "daq_stc.h"


#define NUM_MITE_DMA_CHANNELS      4
#define BOARD_SERIES_NAME          "ni_daq"


typedef struct Board_Properties Board_Properties;
typedef struct Register_Addresses Register_Addresses;
typedef struct Board Board;
typedef struct AI_Subsystem AI_Subsystem;
typedef struct AO_Subsystem AO_Subsystem;
typedef struct GPCT_Subsystem GPCT_Subsystem;
typedef struct MSC_Subsystem MSC_Subsystem;
typedef struct IRQ_Handling IRQ_Handling;
typedef struct Critical_Section Critical_Section;
typedef struct Board_Functions Board_Functions;


extern int board_count;
extern Board boards[ ];


typedef enum {
	caldac_none = 0,
	mb88341,
	dac8800,
	dac8043,
	ad8522,
} CALDAC_TYPES;


struct Board_Properties {
	unsigned short          id;
	char                    *name;

	unsigned int            ai_num_channels;
	unsigned int            ai_num_bits;
	unsigned int            ai_fifo_depth;
	unsigned int            ai_always_dither : 1;
	NI_DAQ_AI_GAIN_TYPES    ai_gains[ 8 ];
	unsigned int            ai_speed;

	unsigned int            ao_num_channels;
	unsigned int            ao_num_bits;
   	unsigned int            ao_fifo_depth;
	unsigned int            ao_unipolar : 1;
	int                     ao_has_ext_ref;

	unsigned int            has_analog_trig : 1;
	CALDAC_TYPES            atrig_caldac;
	u8	                atrig_low_ch;
	u8	                atrig_high_ch;
	unsigned int            atrig_bits;

	unsigned int            num_mite_channels;

	CALDAC_TYPES            caldac[ 3 ];
};


struct Board_Functions {
	void ( *board_reset_all )( Board * );
#if ! defined NI_DAQ_DEBUG
	void ( *stc_writew )( Board *, u16, u16 );
	void ( *stc_writel )( Board *, u16, u32 );
#else
	void ( *stc_writew )( const char *, Board *, u16, u16 );
	void ( *stc_writel )( const char *, Board *, u16, u32 );
#endif
	u16 ( *stc_readw )( Board *, u16 );
	u32 ( *stc_readl )( Board *, u16 );
	void ( *start_critical_section )( Board * );
	void ( *end_critical_section )( Board * );

	void ( *clear_configuration_memory )( Board * );
	void ( *clear_ADC_FIFO )( Board * );
	void ( *clear_DAC_FIFO )( Board * );
	int ( *configuration_high )( Board *, unsigned int, NI_DAQ_AI_TYPE );
	int ( *configuration_low )( Board *, unsigned int, NI_DAQ_STATE,
				    NI_DAQ_STATE, NI_DAQ_BU_POLARITY,
				    NI_DAQ_AI_GAIN_TYPES );
	int ( * ao_configuration )( Board *, unsigned int, NI_DAQ_STATE,
				    NI_DAQ_STATE, NI_DAQ_STATE,
				    NI_DAQ_BU_POLARITY );
	int ( *dac_direct_data )( Board *, unsigned int, int );
	int ( *dma_setup )( Board *, NI_DAQ_SUBSYSTEM );
	int ( *dma_buf_setup )( Board *, NI_DAQ_SUBSYSTEM, size_t, int );
	int ( *dma_buf_get )( Board *, NI_DAQ_SUBSYSTEM, void *,
			      size_t *, int );
	size_t ( *dma_get_available )( Board *, NI_DAQ_SUBSYSTEM );
	void ( *dma_buf_release )( Board *, NI_DAQ_SUBSYSTEM );
	int ( *dma_shutdown )( Board *, NI_DAQ_SUBSYSTEM );
	void ( *set_trigger_levels )( Board *, u16 th, u16 tl );
};


struct AI_Subsystem {
	unsigned int num_channels;
	unsigned int num_data_per_scan;
	unsigned long num_scans;
	int is_acq_setup;
	int is_running;

	NI_DAQ_INPUT START_source;
	NI_DAQ_INPUT START1_source;
	NI_DAQ_INPUT CONVERT_source;
	NI_DAQ_INPUT SI_source;

	unsigned int timebase1;

	wait_queue_head_t waitqueue;
};


struct AO_Subsystem {
	unsigned int is_channel_setup[ 2 ];
	NI_DAQ_BU_POLARITY polarity;
	unsigned int timebase1;
	wait_queue_head_t waitqueue;
};


struct GPCT_Subsystem {
	wait_queue_head_t waitqueue;
	unsigned int timebase1;
};


struct MSC_Subsystem {
	NI_DAQ_CLOCK_TYPE clock;
	NI_DAQ_CLOCK_SPEED_VALUE speed;
	NI_DAQ_STATE output_state;
	unsigned int divider;
};


struct IRQ_Handling {
	int enabled;                   /* set when interrupt is enabled */
	void ( * handler )( Board *board ); /* handler for the interrupt - set
					       to NULL if IRQ is not enabled */
	u16 enable_bit_pattern;        /* bit to set to enable interrupt */
	u16 status_bit_pattern;        /* status bit pattern to check for */
	u16 ack_bit_pattern;           /* bit pattern set in acknowledgment */
	unsigned int raised;           /* incremented in interrupt handler */
};


struct Critical_Section {
	unsigned long count;
	spinlock_t spinlock;
	unsigned long flags;
};


typedef struct {
	char          *buf;       /* virtual address of buffer */
	size_t        size;       /* size of buffer */
	unsigned long order;
	size_t        transfered; /* size of buffer that's already transfered
				     to the user or to the buffer */
	int    is_mapped;         /* set while buffer is PCI-mapped */
	struct {                  /* this structure gets read in by the MITE */
		u32 tcr;          /* size of buffer */
		u32 mar;          /* bus address of buffer */
		u32 lkar;         /* bus address of next link */
		u32 dar;          /* ? */
	} lc;
} MITE_DMA_CHAIN;


struct Board {
	struct pci_dev *dev;        /* PCI device structure of board */

	u8 *mite;                   /* virtual address of MITE */
	unsigned long mite_phys;    /* physical address of MITE */
	unsigned long mite_len;     /* length of MITE address space */

	u8 *daq_stc;                /* virtual address of DAQ-STC registers */
	unsigned long daq_stc_phys; /* physical address of DAQ-STC registers */
	unsigned long daq_stc_len;  /* length of DAQ-STC address space */

	unsigned int irq;           /* Interrupt the board is using */

	int in_use;                 /* set when board is opened */
	uid_t owner;                /* current owner of the board */

	/* Spinlock used when opening the board to keep multiple users
	   opening the board at the same time */

	spinlock_t open_spinlock;

	/* Pointer to structure describing the properties of the board */

	Board_Properties *type;

	/* Pointer to structure with hardware specific functions for board */

	Board_Functions *func;

	/* Structure with addresses of the registers directly accessible
	   via the PCI bus */

	Register_Addresses *regs;

	/* Structure for a private copy of the write-only registers of
	   the DAQ-STC */

	STC_Write_Registers stc;

	/* Structures for storing the state of the subsystems of the board */

	AI_Subsystem AI;
	AO_Subsystem AO;
	GPCT_Subsystem GPCT0;
	GPCT_Subsystem GPCT1;
	MSC_Subsystem MSC;

	/* Current speed of the slow clock as seen by the subsystems when
	   used as a source of input */

	unsigned int timebase2;

	/* Structure that's going to be used in handling of the 22 possible
	   DAQ-STC interrupts */

	int irq_group_A_enabled;
	int irq_group_B_enabled;
	IRQ_Handling irq_hand[ 22 ];

	/* Structure used in protecting critical regions of code from
	   interrupts */

	Critical_Section critical_section;

	/* Chains of DMA buffers used with the mite for AI and AO */

	int mite_irq_enabled[ NUM_MITE_DMA_CHANNELS ];
	MITE_DMA_CHAIN *mite_chain[ NUM_MITE_DMA_CHANNELS ];
};


/* Now we have the declaration of all the important structures and can
   include header files that rely on their existence */

#include "AI.h"
#include "AO.h"
#include "GPCT.h"
#include "DIO.h"
#include "MSC.h"


#if defined NI_DAQ_DEBUG
#define PDEBUG( fmt, args... )                                \
	do {                                                  \
        	printk( KERN_DEBUG " "BOARD_SERIES_NAME ": "  \
			__FUNCTION__ "(): " fmt, ## args );   \
	} while( 0 )
#else
#define PDEBUG( fmt, args... )
#endif


#endif /* NI_DAQ_BOARD_HEADER */


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