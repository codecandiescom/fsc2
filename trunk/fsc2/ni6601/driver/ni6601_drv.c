/*
  $Id$
 
  Driver for National Instruments 6601 boards

  Copyright (C) 2002-2003 Jens Thoms Toerring

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "autoconf.h"
#include "ni6601_drv.h"



EXPORT_NO_SYMBOLS;

static int ni6601_init_board( struct pci_dev *dev, Board *board );
static int ni6601_open( struct inode *inode_p, struct file *file_p );
static int ni6601_release( struct inode *inode_p, struct file *file_p );
static int ni6601_ioctl( struct inode *inode_p, struct file *file_p,
			 unsigned int cmd, unsigned long arg );
static int ni6601_dio_in( Board *board, NI6601_DIO_VALUE *arg );
static int ni6601_dio_out( Board *board, NI6601_DIO_VALUE *arg );
static int ni6601_disarm( Board *board, NI6601_DISARM *arg );
static int ni6601_read_count( Board *board, NI6601_COUNTER_VAL *arg );
static int ni6601_start_pulses( Board *board, NI6601_PULSES *arg );
static int ni6601_start_counting( Board *board, NI6601_COUNTER *arg );
static int ni6601_is_busy( Board *board, NI6601_IS_ARMED *arg );
static void ni6601_irq_handler( int irq, void *data, struct pt_regs *dummy );


static Board boards[ NI6601_MAX_BOARDS ];
static int major = NI6601_MAJOR;
static int board_count = 0;


struct file_operations ni6601_file_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 2, 0 )
	NULL,               /* lseek              */
	NULL,               /* read               */
	NULL,               /* write              */
	NULL,               /* readdir            */
	NULL,               /* select             */
	ni6601_ioctl,       /* ioctl              */
	NULL,               /* mmap               */
	ni6601_open,        /* open               */
	ni6601_release,     /* release            */
	NULL,               /* fsync              */
	NULL,               /* fasync             */
	NULL,               /* check_media_change */
	NULL                /* revalidate         */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 2, 0 ) && \
      LINUX_VERSION_CODE <  KERNEL_VERSION( 2, 4, 0 )
	NULL,               /* llseek             */
	NULL,               /* read               */
	NULL,               /* write              */
	NULL,               /* readdir            */
	NULL,               /* poll               */
	ni6601_ioctl,       /* ioctl              */
	NULL,               /* mmap               */
	ni6601_open,        /* open               */
	NULL,               /* flush              */
	ni6601_release,     /* release            */
	NULL,               /* fsync              */
	NULL,               /* fasync             */
	NULL,               /* check_media_change */
	NULL,               /* revalidate         */
	NULL                /* lock               */
#elif LINUX_VERSION_CODE > KERNEL_VERSION( 2, 4, 0 )
	owner:		    THIS_MODULE,
	ioctl:              ni6601_ioctl,
	open:               ni6601_open,
	release:            ni6601_release,
#endif
};


/*--------------------------------------------------------*/
/* Fuinction that gets executed when the module is loaded */
/*--------------------------------------------------------*/

static int __init ni6601_init( void )
{
	struct pci_dev *dev = NULL;


	while ( board_count < NI6601_MAX_BOARDS &&
		NULL != ( dev = pci_find_device( PCI_VENDOR_NATINST,
						 PCI_DEVICE_NATINST_6601,
						 dev ) ) ) {
		if ( ni6601_init_board( dev, boards + board_count ) ) {
			printk( KERN_ERR NI6601_NAME ": Failed to initialize "
				"board %d\n", board_count );
			ni6601_release_resources( boards, board_count );
			return -EIO;
		}

		board_count++;
	}

	if ( board_count == 0 ) {
		printk( KERN_WARNING NI6601_NAME ": No boards found.\n" );
		return -ENODEV;
	}

	if ( board_count == NI6601_MAX_BOARDS )
		printk( KERN_WARNING NI6601_NAME ": Possibly more than %d "
			"boards!\n", NI6601_MAX_BOARDS );

#ifdef CONFIG_DEVFS_FS
	if ( ( major = devfs_register_chrdev( major, NI6601_NAME,
					      &ni6601_file_ops ) ) < 0 ) {
#else
	if ( ( major = register_chrdev( major, NI6601_NAME,
					&ni6601_file_ops ) ) < 0 ) {
#endif
		printk( KERN_ERR NI6601_NAME ": Can't get assigned major "
			"device\n" );
		ni6601_release_resources( boards, board_count );
		return major;
	}

	printk( KERN_INFO NI6601_NAME ": Module successfully installed\n"
		NI6601_NAME ": Major device number = %d\n"
		NI6601_NAME ": Initialized %d board%s\n",
		major, board_count, board_count > 1 ? "(s)" : "" );

	return 0;
}


/*----------------------------------*/
/* Initialization of a single board */
/*----------------------------------*/

static int __init ni6601_init_board( struct pci_dev *dev, Board *board )
{
        if ( pci_enable_device( dev ) )
		return -EIO;

	pci_set_master( dev );

	board->dev = dev;

	if ( pci_request_regions( dev, NI6601_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for board %d\n",
			board - boards );
		return -1;
	}

	board->mite = board->addr = NULL;

	/* Remap the MITE memory region as well as the memory region at
	   which we're going to access the TIO */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	if ( ( board->mite = ioremap( board->mite_phys, board->mite_len ) )
	     == NULL ) {
		PDEBUG( "Can't remap MITE memory range for board %d\n",
			board - boards );
		goto init_failure;
	}

	board->addr_phys = pci_resource_start( dev, 1 );
	board->addr_len  = pci_resource_len( dev, 1 );
	if ( ( board->addr = ioremap( board->addr_phys, board->addr_len ) )
	     == NULL ) {
		PDEBUG( "Can't remap TIO memory range for board %d\n",
			board - boards );
		goto init_failure;
	}

	/* Enable access to the 2nd memory region where the TIO is located */

	writel( ( board->addr_phys & 0xFFFFFF00L ) | 0x80,
		board->mite + 0xC0 );

	/* Request the interrupt used by the board */

	board->irq = dev->irq;
	if ( request_irq( dev->irq, ni6601_irq_handler, SA_SHIRQ,
			  NI6601_NAME, board ) ) {
		PDEBUG( "Can't obtain IRQ %d\n", dev->irq );
		dev->irq = 0;
		goto init_failure;
	}

	spin_lock_init( &board->spinlock );

	ni6601_register_setup( board );
	ni6601_dio_init( board );

	board->in_use = 0;

	return 0;

 init_failure:
	ni6601_release_resources( boards, board_count );
	return -1;
}


/*-----------------------------------------------------*/
/* Function gets executed when the module gets removed */
/*-----------------------------------------------------*/

static void __exit ni6601_cleanup( void )
{
	ni6601_release_resources( boards, board_count );

#ifdef CONFIG_DEVFS_FS
	if ( devfs_unregister_chrdev( major, NI6601_NAME ) < 0 )
#else
	if ( unregister_chrdev( major, NI6601_NAME ) < 0 )
#endif
		printk( KERN_ERR NI6601_NAME ": Unable to unregister module "
			"with major = %d\n", major );
	else
		printk( KERN_INFO NI6601_NAME ": Module successfully "
			"removed\n" );
}


/*---------------------------------------------------------------------*/
/* Function gets exectuted when the device file for a board get opened */
/*---------------------------------------------------------------------*/

static int ni6601_open( struct inode *inode_p, struct file *file_p )
{
	int minor;
	Board *board;


	file_p = file_p;

	if ( ( minor = MINOR( inode_p->i_rdev ) ) >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	spin_lock( &board->spinlock );

	if ( board->in_use &&
	     board->owner != current->uid &&
	     board->owner != current->euid &&
	     ! capable( CAP_DAC_OVERRIDE ) ) {
		PDEBUG( "Board already in use by another user\n" );
		spin_unlock( &board->spinlock );
		return -EBUSY;
	}

	if ( ! board->in_use ) {
		board->in_use = 1;
		board->owner = current->uid;
	}

	MOD_INC_USE_COUNT;

	spin_unlock( &board->spinlock );

	return 0;
}


/*---------------------------------------------------------------------*/
/* Function gets executed when the device file for a board gets closed */
/*---------------------------------------------------------------------*/

static int ni6601_release( struct inode *inode_p, struct file * file_p )
{
	int minor;
	Board *board;
	int i;


	file_p = file_p;

	minor = MINOR( inode_p->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	/* Reset and disarm all counters */

	writew( G1_RESET | G0_RESET, board->regs.joint_reset[ 0 ] );
	writew( G1_RESET | G0_RESET, board->regs.joint_reset[ 2 ] );

	writew( 0, board->regs.dio_control );

	for ( i = 0; i < 10; i++ )
		writel( 0, board->regs.io_config[ i ] );
		
	board->in_use = 0;

	MOD_DEC_USE_COUNT;

	return 0;
}


/*--------------------------------------------------------------*/
/* Function for handling of ioctl() calls for one of the boards */
/*--------------------------------------------------------------*/

static int ni6601_ioctl( struct inode *inode_p, struct file *file_p,
			 unsigned int cmd, unsigned long arg )
{
	int minor;


	file_p = file_p;

	minor = MINOR( inode_p->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	if ( _IOC_TYPE( cmd ) != NI6601_MAGIC_IOC || 
	     _IOC_NR( cmd ) < NI6601_MIN_NR ||
	     _IOC_NR( cmd ) > NI6601_MAX_NR ) {
		PDEBUG( "Invalid ioctl() call %d\n", cmd );
		return -EINVAL;
	}

	switch ( cmd ) {
		case NI6601_IOC_DIO_IN :
			return ni6601_dio_in( boards + minor,
					      ( NI6601_DIO_VALUE * ) arg );

		case NI6601_IOC_DIO_OUT :
			return ni6601_dio_out( boards + minor,
					       ( NI6601_DIO_VALUE * ) arg );

		case NI6601_IOC_COUNT :
			return ni6601_read_count( boards + minor,
						( NI6601_COUNTER_VAL * ) arg );

		case NI6601_IOC_PULSER :
			return ni6601_start_pulses( boards + minor,
						    ( NI6601_PULSES * ) arg );

		case NI6601_IOC_COUNTER :
			return ni6601_start_counting( boards + minor,
						    ( NI6601_COUNTER * ) arg );

		case NI6601_IOC_IS_BUSY :
			return ni6601_is_busy( boards + minor,
					       ( NI6601_IS_ARMED * ) arg );

		case NI6601_IOC_DISARM :
			return ni6601_disarm( boards + minor,
					      ( NI6601_DISARM * ) arg );
	}			

	return 0;
}


/*-----------------------------------------------------------*/
/* Function reads an 8-bit value from the DIO pins (PFI 0-7) */
/* but only from pins for which bits are set in the mask     */
/* (this allows having some of the bits used for input while */
/* the others are used for output at the same time).         */
/*-----------------------------------------------------------*/

static int ni6601_dio_in( Board *board, NI6601_DIO_VALUE *arg )
{
	NI6601_DIO_VALUE dio;


	if ( copy_from_user( &dio, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	/* If necessary switch selected pins to input mode */

	if ( ( u8 ) ( ( board->dio_mask & 0xFF ) ^ dio.mask ) != 0xFF ) {
		board->dio_mask &= ~ ( u16 ) dio.mask;
		writew( board->dio_mask, board->regs.dio_control );
	}

	/* Read in data value from DIO parallel input register */

	dio.value = ( unsigned char ) 
		    ( readw( board->regs.dio_parallel_in ) & dio.mask );

	if ( copy_to_user( arg, &dio, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return 0;
}


/*-----------------------------------------------------------*/
/* Function outputs an 8-bit value at the DIO pins (PFI 0-7) */
/* but only at the pins for which bits are set in the mask   */
/* (this allows having some of the bits used for output      */
/* while the others are used for input at the same time).    */
/*-----------------------------------------------------------*/

static int ni6601_dio_out( Board *board, NI6601_DIO_VALUE *arg )
{
	NI6601_DIO_VALUE dio;


	if ( copy_from_user( &dio, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	/* Write data value to DIO parallel output register */

	writew( ( u16 ) dio.value, board->regs.dio_output );

	/* Now if necessary switch selected pins to output mode */

	if ( ( board->dio_mask & 0xFF ) != dio.mask ) {
		board->dio_mask |= dio.mask;
		writew( board->dio_mask, board->regs.dio_control );
	}

	return 0;
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static int ni6601_disarm( Board *board, NI6601_DISARM *arg )
{
	NI6601_DISARM d;


	if ( copy_from_user( &d, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	if ( d.counter < NI6601_COUNTER_0 || d.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", d.counter );
		return -EINVAL;
	}

	writew( d.counter & 1 ? G1_RESET : G0_RESET,
		board->regs.joint_reset[ d.counter ] );

	return 0;
}


/*---------------------------------------------------------------------------*/
/* Function for reading the value of a counter (from the SW save register).  */
/* When called with the 'wait_on_end' member of the NI6601_COUNTER_VAL       */
/* structure passed to the function being set it's assumed that the counter  */
/* is doing a gated measurement with the neighbouring counter as the source  */
/* of the (single) gate pulse. The function will then wait for the neigh-    */
/* bouring counter to stop counting before disarming the counter and reading */
/* the counter value.                                                        */
/*---------------------------------------------------------------------------*/

static int ni6601_read_count( Board *board, NI6601_COUNTER_VAL *arg )
{
	NI6601_COUNTER_VAL cs;
	u32 next_val;


	if ( copy_from_user( &cs, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	if ( cs.counter < NI6601_COUNTER_0 || cs.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", cs.counter );
		return -EINVAL;
	}

	/* If required wait for counting to stop (by checking if the
	   neighbouring counter creating the gate pulse is still counting)
	   and then reset both counters */

	if ( cs.wait_for_end ) {
		wait_queue_head_t waitqueue;

		init_waitqueue_head( &waitqueue );
		while ( readw( board->regs.joint_status[ cs.counter ] ) &
			( cs.counter & 1 ? G0_COUNTING : G1_COUNTING ) ) {
			interruptible_sleep_on_timeout( &waitqueue, 1 );
			if ( signal_pending( current ) ) {
				PDEBUG( "Aborted by signal\n" );
				return -EINTR;
			}
		}

		writew( G1_RESET | G0_RESET,
			board->regs.joint_reset[ cs.counter ] );
	}

	/* Read the SW save register */

	cs.count = readl( board->regs.sw_save[ cs.counter ] );

	/* If the counter is armed repeatedly read the SW save register until
	   two readings are identical */

	if ( readw( board->regs.joint_status[ cs.counter ] ) &
	     ( cs.counter & 1 ? G1_ARMED : G0_ARMED ) )
		while ( cs.count != ( next_val =
				 readl( board->regs.sw_save[ cs.counter ] ) ) )
			cs.count = next_val;

	if ( copy_to_user( arg, &cs, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return 0;
}


/*---------------------------------------------------*/
/* Function to start outputting pulses froma counter */
/*---------------------------------------------------*/

static int ni6601_start_pulses( Board *board, NI6601_PULSES *arg )
{
	NI6601_PULSES p;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = 0;
	int use_gate;
	u16 cmd_bits = ALWAYS_DOWN | ARM;


	if ( copy_from_user( &p, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	if ( p.counter < NI6601_COUNTER_0 || p.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", p.counter );
		return -EINVAL;
	}

	if ( ( use_gate = ni6601_input_gate( p.gate, &gate_bits ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", p.gate );
		return -EINVAL;
	}

	if ( ni6601_input_source( p.source, &source_bits ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", p.source );
		return -EINVAL;
	}

	if ( p.low_ticks == 0 || p.high_ticks == 0 ) {
		PDEBUG( "Invalid low or high ticks\n" );
		return -EINVAL;
	}

	/* Set the input select register */

	source_bits |= gate_bits;
	if ( p.output_polarity != NI6601_NORMAL )
		source_bits |= INVERTED_OUTPUT_POLARITY;

	writew( source_bits, board->regs.input_select[ p.counter ] );

	/* Load counter with 1 so we're able to start the first pulse as
	   fast as possible, i.e. 100 ns after arming the counter */

	writel( 1, board->regs.load_a[ p.counter ] );
	writew( LOAD, board->regs.command[ p.counter ] );

	/* Use normal counting and no second gating */

	writew( 0, board->regs.counting_mode[ p.counter ] );
	writew( 0, board->regs.second_gate[ p.counter ] );

	if ( ! p.disable_output )
		ni6601_enable_out( board, NI6601_OUT( p.counter ) );
	else
		ni6601_disable_out( board, NI6601_OUT( p.counter ) );

	/* Set the count for the duration of the high and the low voltage
	   phase of the pulse */

	writel( p.high_ticks, board->regs.load_a[ p.counter ] );
	writel( p.low_ticks,  board->regs.load_b[ p.counter ] );

	/* Assemble value to be written into the mode register */

	mode_bits = RELOAD_SOURCE_SWITCHING | LOADING_ON_TC |
		    OUTPUT_MODE_TOGGLE_ON_TC | LOAD_SOURCE_SELECT_A;

	if ( ! p.continuous )
		mode_bits |= STOP_MODE_ON_GATE_OR_2ND_TC;

	if ( use_gate ) {
		mode_bits |= GATING_MODE_LEVEL;
		cmd_bits  |= SYNCHRONIZE_GATE;
	}

	writew( mode_bits, board->regs.mode[ p.counter ] );

	/* Finally start outputting the pulses */

	writew( cmd_bits, board->regs.command[ p.counter ] );

	return 0;
}


/*---------------------------------------*/
/* Function for starting event counting. */
/*---------------------------------------*/

static int ni6601_start_counting( Board *board, NI6601_COUNTER *arg )
{
	NI6601_COUNTER c;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = OUTPUT_MODE_TC;
	int use_gate;
	u16 cmd_bits = ALWAYS_UP | LOAD | ARM;


	if ( copy_from_user( &c, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	if ( c.counter < NI6601_COUNTER_0 || c.counter > NI6601_COUNTER_3 ) {
		PDEBUG( "Invalid counter number %d\n", c.counter );
		return -EINVAL;
	}

	if ( ( use_gate = ni6601_input_gate( c.gate, &gate_bits ) ) < 0 ) {
		PDEBUG( "Invalid gate setting %d\n", c.gate );
		return -EINVAL;
	}

	if ( ni6601_input_source( c.source, &source_bits ) <= 0 ) {
		PDEBUG( "Invalid source setting %d\n", c.source );
		return -EINVAL;
	}

	/* Set input source and gate as well as source input polarity */

	source_bits |= gate_bits;
	if ( c.source_polarity != NI6601_NORMAL )
		source_bits |= INVERTED_SOURCE_POLARITY;

	writew( source_bits, board->regs.input_select[ c.counter ] );

	if ( ! c.disable_output )
		ni6601_enable_out( board, NI6601_OUT( c.counter ) );
	else
		ni6601_disable_out( board, NI6601_OUT( c.counter ) );

	/* Use normal counting and no second gating */

	writew( 0, board->regs.counting_mode[ c.counter ] );
	writew( 0, board->regs.second_gate[ c.counter ] );

	if ( use_gate ) {
		mode_bits |= GATING_MODE_LEVEL;
		cmd_bits  |= SYNCHRONIZE_GATE;
	}

	writew( mode_bits, board->regs.mode[ c.counter ] );

	/* Load counter from A, set counting direction and arm */

	writel( 0, board->regs.load_a[ c.counter ] );
	writew( cmd_bits, board->regs.command[ c.counter ] );

	return 0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static int ni6601_is_busy( Board *board, NI6601_IS_ARMED *arg )
{
	NI6601_IS_ARMED a;


	if ( copy_from_user( &a, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	/* Test if the counter is armed */

	a.state =  readw( board->regs.joint_status[ a.counter ] ) &
		   ( a.counter & 1 ? G1_ARMED : G0_ARMED );
	
	if ( copy_from_user( &a, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	return 0;
}


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

static void ni6601_irq_handler( int irq, void *data, struct pt_regs *dummy )
{
	Board *board = ( Board * ) data;


	if ( irq != board->irq ) {
		PDEBUG( "Invalid interrupt num: %d\n", irq );
		return;
	}

	PDEBUG( "Got interrupt\n" );
}



module_init( ni6601_init );
module_exit( ni6601_cleanup );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 2, 0 )
MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
MODULE_AUTHOR( "Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>" );
MODULE_DESCRIPTION( "National Instruments 6601 GBCT board driver" );
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 4, 10 )
MODULE_LICENSE( "GPL" );
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
