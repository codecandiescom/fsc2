/*
  $Id$
 
  Driver for National Instruments PCI E Series boards

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


#include "ni_daq_board.h"


static int ni_daq_open( struct inode *inode_p, struct file *file_p );
static int ni_daq_release( struct inode *inode_p, struct file *file_p );
static unsigned int ni_daq_poll( struct file *filep,
				 struct poll_table_struct * pt );
static ssize_t ni_daq_read( struct file *filep, char *buff, size_t count,
			    loff_t *offp );
static ssize_t ni_daq_write( struct file *filep, const char *buff,
			     size_t count, loff_t *offp );
static int ni_daq_ioctl( struct inode *inode_p, struct file *file_p,
			 unsigned int cmd, unsigned long arg );


struct file_operations ni_daq_file_ops = {
	owner:		    THIS_MODULE,
	ioctl:              ni_daq_ioctl,
	open:               ni_daq_open,
	poll:               ni_daq_poll,
	read:               ni_daq_read,
	write:              ni_daq_write,
	release:            ni_daq_release,
};


/*----------------------------------------------------------------------*/
/* Function gets exectuted when the device file for a board gets opened */
/*----------------------------------------------------------------------*/

static int ni_daq_open( struct inode *inode_p, struct file *file_p )
{
	int minor;
	Board *board;


	file_p = file_p;

	if ( ( minor = MINOR( inode_p->i_rdev ) ) >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	spin_lock( &board->open_spinlock );

	if ( board->in_use > 0 &&
	     board->owner != current->uid &&
	     board->owner != current->euid &&
	     ! capable( CAP_DAC_OVERRIDE ) ) {
		PDEBUG( "Board already in use by another user\n" );
		spin_unlock( &board->open_spinlock );
		return -EBUSY;
	}

	if ( board->in_use == 0 )
		board->owner = current->uid;

	board->in_use++;

	MOD_INC_USE_COUNT;

	spin_unlock( &board->open_spinlock );

	return 0;
}


/*---------------------------------------------------------------------*/
/* Function gets executed when the device file for a board gets closed */
/*---------------------------------------------------------------------*/

static int ni_daq_release( struct inode *inode_p, struct file *file_p )
{
	int minor;
	Board *board;


	file_p = file_p;

	minor = MINOR( inode_p->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( board->in_use == 0 ) {
		PDEBUG( "Board %d not open\n", minor );
		return -ENODEV;
	}

	if ( board->owner != current->uid &&
	     board->owner != current->euid &&
	     ! capable( CAP_DAC_OVERRIDE ) ) {
		PDEBUG( "No permission to release board %d\n", minor );
		return -EPERM;
	}

	/* Reset the board before closing it */

	board->func->board_reset_all( board );
		
	board->in_use--;

	MOD_DEC_USE_COUNT;

	return 0;
}


/*--------------------------------------------------------------------*/
/* Function for dealing with poll() and select() calls for the driver */ 
/*--------------------------------------------------------------------*/

static unsigned int ni_daq_poll( struct file *filep,
				 struct poll_table_struct * pt )
{
	int minor;
	Board *board;
	unsigned int mask = 0;


	minor = MINOR( filep->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( board->mite_chain[ NI_DAQ_AI_SUBSYSTEM ] ) {

		/* As long as the acquisition hasn't ended (in which case
		   the SC TC interrupt would have been raised) and there are
		   no new data in the DMA buffer enable the STOP
		   interrupt (indicating that new data are available) and
		   let poll_wait() do whatever it needs to do. Afterwards
		   disable STOP interrupts, otherwise we would get flooded
		   with interrupts... */

		if ( board->func->dma_get_available( board,
						   NI_DAQ_AI_SUBSYSTEM ) )
			mask |= POLLIN | POLLRDNORM;
		else if ( ! board->irq_hand[ IRQ_AI_SC_TC ].raised ) {
			daq_irq_enable( board, IRQ_AI_STOP, AI_irq_handler );
			poll_wait( filep, &board->AI.waitqueue, pt );
			daq_irq_disable( board, IRQ_AI_STOP );
			if ( board->func->dma_get_available( board,
							NI_DAQ_AI_SUBSYSTEM ) )
				mask |= POLLIN | POLLRDNORM;
		}
	}
	else
		mask |= POLLHUP;
	
	if ( board->mite_chain[ NI_DAQ_AO_SUBSYSTEM ] &&
	     board->func->dma_get_available( board, NI_DAQ_AO_SUBSYSTEM ) )
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}


/*----------------------------------------------------------------------*/
/* Function for reading from the device file associated with the board. */
/* Reading the device file only works for data from the AI subsystem.   */
/* 1. if no acquisition set up has been done, return EOF                */
/* 2. if there's an acquisition set up but the acquisition hasn't been  */
/*    started, trying to read from the file automatically starts it     */
/* 3. if the file wasn't opened in non-blocking mode wait until data    */
/*    are available                                                     */
/* 4. return as many data as are available (or as many as requested)    */
/* 5. if the acquisition is finished and all data to be expected are    */
/*    getting passed on to the user a reset of the AI subsystem is      */
/*    done, i.e. DMA is automatically disabled, buffers are deallocated */
/*    and trigger input lines are released back into the pool           */
/*----------------------------------------------------------------------*/

static ssize_t ni_daq_read( struct file *filep, char *buff, size_t count,
			    loff_t *offp )
{
	int minor;
	Board *board;
	int ret;


	minor = MINOR( filep->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	/* If there can't be any data return a value indicating end of file */

	if ( ! board->AI.is_acq_setup ) {
		PDEBUG( "Missing acquisition setup\n" );
		return -EINVAL;
	}

	/* If an acquisition has been set up but hasn't been started yet
	   trying to read data triggers the start of the acquisition */

	if ( ! board->mite_chain[ NI_DAQ_AI_SUBSYSTEM ] &&
	     ( ret = AI_start_acq( board ) ) < 0 )
		return ret;

	/* If the device file was opened in blocking mode make sure there are
	   data: if no data are available immediately enable the STOP
	   interrupt (which gets raised when a scan is finished) and then
	   wait for it (or for the SC TC interrupt, which is raised at the
	   end of the acquisition). */

	if ( ! ( filep->f_flags & O_NONBLOCK ) &&
	     ! board->func->dma_get_available( board,
					       NI_DAQ_AI_SUBSYSTEM ) ) {
		daq_irq_enable( board, IRQ_AI_STOP, AI_irq_handler );

		if ( wait_event_interruptible( board->AI.waitqueue,
			   board->irq_hand[ IRQ_AI_SC_TC ].raised ||
			   board->irq_hand[ IRQ_AI_STOP ].raised ) ) {
			daq_irq_disable( board, IRQ_AI_STOP );
			return -EINTR;
		}

		daq_irq_disable( board, IRQ_AI_STOP );
	}

	/* Now get as many new data as possible */

        ret = board->func->dma_buf_get( board, NI_DAQ_AI_SUBSYSTEM,
					buff, &count, 0 );

	/* If there are no new data and the device file was opened in non-
	   blocking mode return -EAGAIN */

	if ( ret == 0 && count == 0 && filep->f_flags & O_NONBLOCK )
		return -EAGAIN;

	/* If all points to be expected from the acquisition have been fetched
	   or if there was an unrecoverable error disable AI SC_TC interrupt
	   and release all AI trigger inputs as well as switching off DMA and
	   releasing the associated buffers. */

	if ( ret != 0 && board->AI.is_running ) {
		board->AI.is_running = 0;
		daq_irq_disable( board, IRQ_AI_SC_TC );
		MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
			       NI_DAQ_PFI_UNUSED );
	}

	if ( ret < 0 )
	{
		board->func->dma_shutdown( board, NI_DAQ_AI_SUBSYSTEM );
		return ret;
	}

	*offp += count;

	PDEBUG( "Returning count = %ld\n", ( long ) count );

	return count;
}

/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static ssize_t ni_daq_write( struct file *filep, const char *buff,
			     size_t count, loff_t *offp )
{
	return 0;
}


/*--------------------------------------------------------------*/
/* Function for handling of ioctl() calls for one of the boards */
/*--------------------------------------------------------------*/

static int ni_daq_ioctl( struct inode *inode_p, struct file *file_p,
			 unsigned int cmd, unsigned long arg )
{
	int minor;


	file_p = file_p;

	minor = MINOR( inode_p->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	if ( _IOC_TYPE( cmd ) != NI_DAQ_MAGIC_IOC || 
	     _IOC_NR( cmd ) < NI_DAQ_MIN_NR ||
	     _IOC_NR( cmd ) > NI_DAQ_MAX_NR ) {
		PDEBUG( "Invalid ioctl() call %d\n", cmd );
		return -EINVAL;
	}

	switch ( cmd ) {
		case NI_DAQ_IOC_MSC :
			return MSC_ioctl_handler( boards + minor,
						  ( NI_DAQ_MSC_ARG * ) arg );

		case NI_DAQ_IOC_AI :
			return AI_ioctl_handler( boards + minor,
						 ( NI_DAQ_AI_ARG * ) arg );

		case NI_DAQ_IOC_AO :
			return AO_ioctl_handler( boards + minor,
						 ( NI_DAQ_AO_ARG * ) arg );

		case NI_DAQ_IOC_GPCT :
			return GPCT_ioctl_handler( boards + minor,
						   ( NI_DAQ_GPCT_ARG * ) arg );

		case NI_DAQ_IOC_DIO :
			return DIO_ioctl_handler( boards + minor,
						  ( NI_DAQ_DIO_ARG * ) arg );

	}

	return -EINVAL;
}


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
