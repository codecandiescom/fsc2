/*
 *  $Id$
 * 
 *  Driver for National Instruments 6601 boards
 * 
 *  Copyright (C) 2002-2004 Jens Thoms Toerring
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
 */


#include "autoconf.h"
#include "ni6601_drv.h"


#define mite_CHOR( ch )             ( board->mite + 0x500 + 0x100 * ( ch ) )
#define mite_CHCR( ch )             ( board->mite + 0x504 + 0x100 * ( ch ) )
#define mite_TCR( ch )              ( board->mite + 0x508 + 0x100 * ( ch ) )
#define mite_CHSR( ch )             ( board->mite + 0x53C + 0x100 * ( ch ) )
#define mite_MCR( ch )              ( board->mite + 0x50C + 0x100 * ( ch ) )
#define mite_MAR( ch )              ( board->mite + 0x510 + 0x100 * ( ch ) )
#define mite_DCR( ch )              ( board->mite + 0x514 + 0x100 * ( ch ) )
#define mite_DAR( ch )              ( board->mite + 0x518 + 0x100 * ( ch ) )
#define mite_LKCR( ch )             ( board->mite + 0x51C + 0x100 * ( ch ) )
#define mite_LKAR( ch )             ( board->mite + 0x520 + 0x100 * ( ch ) )
#define mite_LLKAR( ch )            ( board->mite + 0x524 + 0x100 * ( ch ) )
#define mite_BAR( ch )	            ( board->mite + 0x528 + 0x100 * ( ch ) )
#define mite_BCR( ch )	            ( board->mite + 0x52C + 0x100 * ( ch ) )
#define mite_SAR( ch )	            ( board->mite + 0x530 + 0x100 * ( ch ) )
#define mite_WSCR( ch )	            ( board->mite + 0x534 + 0x100 * ( ch ) )
#define mite_WSER( ch )	            ( board->mite + 0x538 + 0x100 * ( ch ) )
#define mite_FCR( ch )              ( board->mite + 0x540 + 0x100 * ( ch ) )



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
static void ni6601_irq_enable( Board *board, int counter );
static void ni6601_irq_disable( Board *board, int counter );
static void ni6601_irq_handler( int irq, void *data, struct pt_regs *dummy );
static void mite_dump( Board *board, int channel );


static Board boards[ NI6601_MAX_BOARDS ];
static int major = NI6601_MAJOR;
static int board_count = 0;


struct file_operations ni6601_file_ops = {
	owner:		    THIS_MODULE,
	ioctl:              ni6601_ioctl,
	open:               ni6601_open,
	release:            ni6601_release,
};



/*-------------------------------------------------------*
 * Function that gets executed when the module is loaded
 *-------------------------------------------------------*/

static int __init ni6601_init( void )
{
	struct pci_dev *dev = NULL;
	int res_major;


	/* Try to find all boards */

	while ( board_count < NI6601_MAX_BOARDS ) {
		dev = pci_find_device( PCI_VENDOR_NATINST,
				       PCI_DEVICE_NATINST_6601, dev );

		if ( dev == NULL )
			break;

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
		printk( KERN_WARNING NI6601_NAME ": There could be more than "
			"the %d supported boards!\n", NI6601_MAX_BOARDS );

#ifdef CONFIG_DEVFS_FS
	if ( ( res_major = devfs_register_chrdev( major, NI6601_NAME,
						  &ni6601_file_ops ) ) < 0 ) {
#else
	if ( ( res_major = register_chrdev( major, NI6601_NAME,
					    &ni6601_file_ops ) ) < 0 ) {
#endif
		printk( KERN_ERR NI6601_NAME ": Can't register as char "
			"device.\n" );
		ni6601_release_resources( boards, board_count );
		return major;
	}

	if ( major == 0 )
		major = res_major;

	printk( KERN_INFO NI6601_NAME ": Module successfully installed\n"
		NI6601_NAME ": Initialized %d board%s\n",
		board_count, board_count > 1 ? "(s)" : "" );

	return 0;
}


/*-----------------------------------------------------*
 * Function gets executed when the module gets removed
 *-----------------------------------------------------*/

static void __exit ni6601_cleanup( void )
{
	ni6601_release_resources( boards, board_count );

	/* Unregister the character device (but only when we aren't using
	   devfs exclussively as indicated by major being 0 */

#ifdef CONFIG_DEVFS_FS
	if ( major != 0 && devfs_unregister_chrdev( major, NI6601_NAME ) < 0 )
#else
	if ( unregister_chrdev( major, NI6601_NAME ) < 0 )
#endif
		printk( KERN_ERR NI6601_NAME ": Unable to unregister "
			"module.\n" );
	else
		printk( KERN_INFO NI6601_NAME ": Module successfully "
			"removed\n" );
}


/*----------------------------------*
 * Initialization of a single board
 *----------------------------------*/

static int __init ni6601_init_board( struct pci_dev *dev, Board *board )
{
	int i;


	board->dev = dev;
	board->irq = 0;

        if ( pci_enable_device( dev ) )
	{
		PDEBUG( "Failed to enable board %d\n", board - boards );
		goto init_failure;
	}

	pci_set_master( dev );

	if ( pci_request_regions( dev, NI6601_NAME ) ) {
		PDEBUG( "Failed to lock memory regions for board %d\n",
			board - boards );
		goto init_failure;
	}

	/* Remap the MITE memory region as well as the memory region at
	   which we're going to access the TIO */

	board->mite_phys = pci_resource_start( dev, 0 );
	board->mite_len  = pci_resource_len( dev, 0 );
	board->mite = ioremap( board->mite_phys, board->mite_len );
	if ( board->mite == NULL ) {
		PDEBUG( "Can't remap MITE memory range for board %d\n",
			board - boards );
		goto init_failure;
	}

	board->addr_phys = pci_resource_start( dev, 1 );
	board->addr_len  = pci_resource_len( dev, 1 );
	board->addr = ioremap( board->addr_phys, board->addr_len );
	if ( board->addr == NULL ) {
		PDEBUG( "Can't remap TIO memory range for board %d\n",
			board - boards );
		goto init_failure;
	}

	/* Enable access to the 2nd memory region where the TIO is located */

	writel( ( board->addr_phys & 0xFFFFFF00L ) | 0x80,
		board->mite + 0xC0 );

	/* Request the interrupt used by the board */

	if ( request_irq( dev->irq, ni6601_irq_handler, SA_SHIRQ,
			  NI6601_NAME, board ) ) {
		PDEBUG( "Can't obtain IRQ %d\n", dev->irq );
		dev->irq = 0;
		goto init_failure;
	}

	/* Initialize variables for interrupt handling */

	board->irq = dev->irq;
	for ( i = 0; i < 4; i++ ) {
		board->irq_enabled[ i ] = 0;
		board->TC_irq_raised[ i ] = 0;
	}

	init_waitqueue_head( &board->waitqueue );

	spin_lock_init( &board->spinlock );

	ni6601_register_setup( board );
	ni6601_dio_init( board );

	board->in_use = 0;

	return 0;

 init_failure:
	ni6601_release_resources( boards, board_count );
	return -1;
}


/*----------------------------------------------------------------------*
 * Function gets exectuted when the device file for a board gets opened
 *----------------------------------------------------------------------*/

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


/*---------------------------------------------------------------------*
 * Function gets executed when the device file for a board gets closed
 *---------------------------------------------------------------------*/

static int ni6601_release( struct inode *inode_p, struct file *file_p )
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


/*--------------------------------------------------------------*
 * Function for handling of ioctl() calls for one of the boards
 *--------------------------------------------------------------*/

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


/*-----------------------------------------------------------*
 * Function reads an 8-bit value from the DIO pins (PFI 0-7)
 * but only from pins for which bits are set in the mask
 * (this allows having some of the bits used for input while
 * the others are used for output at the same time).
 *-----------------------------------------------------------*/

static int ni6601_dio_in( Board *board, NI6601_DIO_VALUE *arg )
{
	NI6601_DIO_VALUE dio;


	if ( copy_from_user( &dio, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	/* If necessary switch selected pins to input mode */

	if ( ( ( u8 ) ( board->dio_mask & 0xFF ) ^ dio.mask ) != 0xFF ) {
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


/*-----------------------------------------------------------*
 * Function outputs an 8-bit value at the DIO pins (PFI 0-7)
 * but only at the pins for which bits are set in the mask
 * (this allows having some of the bits used for output
 * while the others are used for input at the same time).
 *-----------------------------------------------------------*/

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


/*------------------------------------------------------------------*
 * Function for stopping the counter of a board, i.e. disarming it.
 *------------------------------------------------------------------*/

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

	writew( DISARM, board->regs.command[ d.counter ] );

	return 0;
}


/*---------------------------------------------------------------------------*
 * Function for reading the value of a counter (from the SW save register).
 * When called with the 'wait_on_end' member of the NI6601_COUNTER_VAL
 * structure passed to the function being set it's assumed that the counter
 * is doing a gated measurement with the neighbouring counter as the source
 * of the (single) gate pulse. The function will then wait for the neigh-
 * bouring counter to stop counting before disarming the counter and reading
 * the counter value.
 *---------------------------------------------------------------------------*/

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

	/* If required wait for counting to stop (by waiting for the
	   neighboring counter creating the gate pulse to stop) and
	   then reset both counters. */

	if ( cs.wait_for_end ) {

		int oc = cs.counter + ( cs.counter & 1 ? -1 : 1 );

		ni6601_irq_enable( board, oc );

		if ( readw( board->regs.joint_status[ oc ] ) &
		     Gi_COUNTING( oc ) )
			wait_event_interruptible( board->waitqueue,
						  board->TC_irq_raised[ oc ] );
		ni6601_irq_disable( board, oc );

		writew( G1_RESET | G0_RESET,
			board->regs.joint_reset[ cs.counter ] );

		if ( signal_pending( current ) ) {
			PDEBUG( "Aborted by signal\n" );
			return -EINTR;
		}
	}

	/* Read the SW save register to get the current count value */

	cs.count = readl( board->regs.sw_save[ cs.counter ] );

	/* If the counter is armed, re-read the SW save register until two
	   readings are identical */

	if ( readw( board->regs.joint_status[ cs.counter ] ) &
	     Gi_ARMED( cs.counter ) )
		while ( cs.count != ( next_val =
				 readl( board->regs.sw_save[ cs.counter ] ) ) )
			cs.count = next_val;
	
	if ( copy_to_user( arg, &cs, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return 0;
}


/*----------------------------------------------------*
 * Function to start outputting pulses from a counter
 *----------------------------------------------------*/

static int ni6601_start_pulses( Board *board, NI6601_PULSES *arg )
{
	NI6601_PULSES p;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = 0;
	int use_gate;
	u16 cmd_bits = ALWAYS_DOWN;


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

	if ( p.low_ticks < 2 || p.high_ticks < 2 ) {
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

	writel( 1UL, board->regs.load_a[ p.counter ] );
	writew( LOAD, board->regs.command[ p.counter ] );

	/* Use normal counting and no second gating */

	writew( 0, board->regs.counting_mode[ p.counter ] );
	writew( 0, board->regs.second_gate[ p.counter ] );

	if ( ! p.disable_output )
		ni6601_enable_out( board, NI6601_OUT( p.counter ) );
	else
		ni6601_disable_out( board, NI6601_OUT( p.counter ) );

	/* Set the count for the duration of the high and the low voltage
	   phase of the pulse (after subtracting 1) */

	writel( p.high_ticks - 1, board->regs.load_a[ p.counter ] );
	writel( p.low_ticks - 1,  board->regs.load_b[ p.counter ] );

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
	writew( cmd_bits | ARM, board->regs.command[ p.counter ] );

	return 0;
}


/*---------------------------------------*
 * Function for starting event counting.
 *---------------------------------------*/

static int ni6601_start_counting( Board *board, NI6601_COUNTER *arg )
{
	NI6601_COUNTER c;
	u16 gate_bits;
	u16 source_bits;
	u16 mode_bits = OUTPUT_MODE_TC;
	int use_gate;
	u16 cmd_bits = ALWAYS_UP | LOAD;


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

	/* Load counter from A, set counting direction etc., then arm
	   (arming while loading does not seems to work for some of the
	   counters...) */

	writel( 0UL, board->regs.load_a[ c.counter ] );
	writew( cmd_bits, board->regs.command[ c.counter ] );
	writew( cmd_bits | ARM, board->regs.command[ c.counter ] );

	return 0;
}


/*---------------------------------------------------------------*
 * Function checks if a counter of a board is currently counting
 * (i.e. if it's armed).
 *---------------------------------------------------------------*/

static int ni6601_is_busy( Board *board, NI6601_IS_ARMED *arg )
{
	NI6601_IS_ARMED a;


	if ( copy_from_user( &a, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	/* Test if the counter is armed */

	a.state =  ( readw( board->regs.joint_status[ a.counter ] ) &
		     Gi_ARMED( a.counter ) ) ? 1 : 0;
	
	if ( copy_to_user( arg, &a, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return 0;
}


/*-----------------------------------------------------------*
 * Function to enable TC interrupts from one of the counters
 *-----------------------------------------------------------*/

static void ni6601_irq_enable( Board *board, int counter )
{
	board->TC_irq_raised[ counter ] = 0;
	if ( ! board->irq_enabled[ counter ] )
	{
		PDEBUG( "Enabling IRQ for counter %d\n", counter );
		writew( Gi_TC_INTERRUPT_ACK, board->regs.irq_ack[ counter ] );
		writew( Gi_TC_INTERRUPT_ENABLE( counter ),
			board->regs.irq_enable[ counter ] );
		board->irq_enabled[ counter ] = 1;
	}
}


/*------------------------------------------------------------*
 * Function to disable TC interrupts from one of the counters
 *------------------------------------------------------------*/

static void ni6601_irq_disable( Board *board, int counter )
{
	if ( board->irq_enabled[ counter ] )
	{
		PDEBUG( "Disabling IRQ for counter %d\n", counter );

		writew( 0, board->regs.irq_enable[ counter ] );
		writew( Gi_TC_INTERRUPT_ACK,
			board->regs.irq_ack[ counter ] );
		board->irq_enabled[ counter ] = 0;
	}
}


/*-------------------------------------------------------------------*
 * Interrupt handler for all boards, raising a flag on TC interrupts
 *-------------------------------------------------------------------*/

static void ni6601_irq_handler( int irq, void *data, struct pt_regs *dummy )
{
	Board *board = ( Board * ) data;
	u16 mask = Gi_INTERRUPT | Gi_TC_STATUS;
	int i;


	if ( irq != board->irq ) {
		PDEBUG( "Invalid interrupt num: %d\n", irq );
		return;
	}

	for ( i = 0; i < 4; i++ )
		if ( board->irq_enabled[ i ] &&
		     mask == ( readw( board->regs.status[ i ] ) & mask ) )
		{
			PDEBUG( "Interrupt for counter %d\n", i );
			writew( Gi_TC_INTERRUPT_ACK,
				board->regs.irq_ack[ i ] );
			board->TC_irq_raised[ i ]++;
			wake_up_interruptible( &board->waitqueue );
		}
}


static void mite_dump( Board *board, int channel )
{
	printk( KERN_INFO "mite_CHOR  = 0x%08X\n", readl( mite_CHOR(  channel ) ) );
	printk( KERN_INFO "mite_CHCR  = 0x%08X\n", readl( mite_CHCR(  channel ) ) );
	printk( KERN_INFO "mite_TCR   = 0x%08X\n", readl( mite_TCR(   channel ) ) );
	printk( KERN_INFO "mite_CHSR  = 0x%08X\n", readl( mite_CHSR(  channel ) ) );
	printk( KERN_INFO "mite_MCR   = 0x%08X\n", readl( mite_MCR(   channel ) ) );
	printk( KERN_INFO "mite_MAR   = 0x%08X\n", readl( mite_MAR(   channel ) ) );
	printk( KERN_INFO "mite_DCR   = 0x%08X\n", readl( mite_DCR(   channel ) ) );
	printk( KERN_INFO "mite_DAR   = 0x%08X\n", readl( mite_DAR(   channel ) ) );
	printk( KERN_INFO "mite_LKCR  = 0x%08X\n", readl( mite_LKCR(  channel ) ) );
	printk( KERN_INFO "mite_LKAR  = 0x%08X\n", readl( mite_LKAR(  channel ) ) );
	printk( KERN_INFO "mite_LLKAR = 0x%08X\n", readl( mite_LLKAR( channel ) ) );
	printk( KERN_INFO "mite_BAR   = 0x%08X\n", readl( mite_BAR(   channel ) ) );
	printk( KERN_INFO "mite_BCR   = 0x%08X\n", readl( mite_BCR(   channel ) ) );
	printk( KERN_INFO "mite_SAR   = 0x%08X\n", readl( mite_SAR(   channel ) ) );
	printk( KERN_INFO "mite_WSCR  = 0x%08X\n", readl( mite_WSCR(  channel ) ) );
	printk( KERN_INFO "mite_WSER  = 0x%08X\n", readl( mite_WSER(  channel ) ) );
	printk( KERN_INFO "mite_FCR   = 0x%08X\n", readl( mite_FCR(   channel ) ) );
}


EXPORT_NO_SYMBOLS;


module_init( ni6601_init );
module_exit( ni6601_cleanup );

MODULE_PARM( major, "i" );
MODULE_PARM_DESC( major, "Major device number to use" );
MODULE_AUTHOR( "Jens Thoms Toerring <Jens.Toerring@physik.fu-berlin.de>" );
MODULE_DESCRIPTION( "National Instruments 6601 GBCT board driver" );


/* MODULE_LICENSE should be defined since at least 2.4.10 */

#if defined MODULE_LICENSE
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

