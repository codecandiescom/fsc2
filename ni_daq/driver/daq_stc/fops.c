/*
 *  Driver for National Instruments DAQ boards based on a DAQ-STC
 * 
 *  Copyright (C) 2003-2011 Jens Thoms Toerring
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


#include "ni_daq_board.h"


static int ni_daq_open( struct inode * /* inode_p */,
			struct file *  /* file_p  */);

static int ni_daq_release( struct inode * /* inode_p */,
			   struct file *  /* file_p  */ );

static unsigned int ni_daq_poll( struct file *              /* file_p */,
				 struct poll_table_struct * /* pt     */ );

static ssize_t ni_daq_read( struct file * /* file_p */,
			    char __user * /* buff   */,
			    size_t        /* count  */,
			    loff_t *      /* offp   */ );

static ssize_t ni_daq_write( struct file       * /* file_p */,
			     const char __user *  /* buff   */,
			     size_t               /* count  */,
			     loff_t *             /* offp   */ );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static int ni_daq_ioctl( struct inode * /* inode_p */,
			 struct file *  /* file_p  */,
			 unsigned int   /* cmd     */,
			 unsigned long  /* arg     */ );
#else
static long ni_daq_ioctl( struct file *  /* file_p */,
			  unsigned int   /* cmd    */,
			  unsigned long  /* arg    */ );
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
struct file_operations ni_daq_file_ops = {
	owner:		    THIS_MODULE,
	ioctl:              ni_daq_ioctl,
	open:               ni_daq_open,
	poll:               ni_daq_poll,
	read:               ni_daq_read,
	write:              ni_daq_write,
	release:            ni_daq_release,
};
#else
struct file_operations ni_daq_file_ops = {
	.owner =	    THIS_MODULE,
#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
	.ioctl =            ni_daq_ioctl,
#else
	.unlocked_ioctl =   ni_daq_ioctl,
#endif
	.open =             ni_daq_open,
	.poll =             ni_daq_poll,
	.read =             ni_daq_read,
	.write =            ni_daq_write,
	.release =          ni_daq_release,
};
#endif


/*---------------------------------------------------------------------*
 * Function gets executed when the device file for a board gets opened
 *---------------------------------------------------------------------*/

static int ni_daq_open( struct inode * inode_p,
			struct file *  file_p )
{
	int minor;
	Board *board;


	minor = MINOR( inode_p->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( file_p->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->open_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->open_mutex ) )
		return -ERESTARTSYS;

	if ( board->in_use ) {
		up( &board->open_mutex );
		PDEBUG( "Board already in use by another process/thread\n" );
		return -EBUSY;
	}


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
	MOD_INC_USE_COUNT;
#endif

	board->in_use = 1;
	up( &board->open_mutex );

	return 0;
}


/*---------------------------------------------------------------------*
 * Function gets executed when the device file for a board gets closed
 *---------------------------------------------------------------------*/

static int ni_daq_release( struct inode * inode_p,
			   struct file *  file_p )
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

	if ( down_interruptible( &board->open_mutex ) )
		return -ERESTARTSYS;

	if ( ! board->in_use ) {
		up( &board->open_mutex );
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	/* Reset the board before closing it */

	board->func->board_reset_all( board );
		

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
	MOD_DEC_USE_COUNT;
#endif

	board->in_use = 0;
	up( &board->open_mutex );

	return 0;
}


/*--------------------------------------------------------------------*
 * Function for dealing with poll() and select() calls for the driver
 *--------------------------------------------------------------------*/

static unsigned int ni_daq_poll( struct file *              file_p,
				 struct poll_table_struct * pt )
{
	int minor;
	Board *board;
	unsigned int mask = 0;


	minor = MINOR( file_p->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( ! board->in_use ) {
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	if ( file_p->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->use_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->use_mutex ) )
		return -ERESTARTSYS;

	if ( board->data_buffer[ NI_DAQ_AI_SUBSYSTEM ] != NULL ) {

		/* As long as the acquisition hasn't ended (in which case
		   the SC TC interrupt would have been raised) and there are
		   no new data in the data buffer enable the STOP interrupt
		   (indicating that new data are available) and let
		   poll_wait() do whatever it needs to do. When the STOP
		   interrupts finally arrives disable it again, otherwise
		   we would get flooded with these interrupts... */

		if ( board->func->data_get_available( board,
						      NI_DAQ_AI_SUBSYSTEM ) )
			mask |= POLLIN | POLLRDNORM;
		else if ( ! board->irq_hand[ IRQ_AI_SC_TC ].raised ) {
			daq_irq_enable( board, IRQ_AI_STOP, AI_irq_handler );
			poll_wait( file_p, &board->AI.waitqueue, pt );
			daq_irq_disable( board, IRQ_AI_STOP );

			if ( board->func->data_get_available( board,
							NI_DAQ_AI_SUBSYSTEM ) )
				mask |= POLLIN | POLLRDNORM;
		}
	}
	else
		mask |= POLLHUP;
	
	if (    board->data_buffer[ NI_DAQ_AO_SUBSYSTEM ] != NULL
	     && board->func->data_get_available( board, NI_DAQ_AO_SUBSYSTEM ) )
		mask |= POLLOUT | POLLWRNORM;

	up( &board->use_mutex );
	return mask;
}


/*-----------------------------------------------------------------------*
 * Function for reading from the device file associated with the board.
 * Reading the device file only works for data from the AI subsystem.
 * 1. If no valid acquisition set up has been done return -EINVAL
 * 2. If there's an acquisition set up but the acquisition hasn't been
 *    started, trying to read from the file automatically starts it
 * 3. If the file wasn't opened in non-blocking mode wait until data
 *    are available
 * 4. Return as many data as are available (or as many as requested)
 * 5. If the acquisition is finished and all data to be expected are
 *    getting passed on to the user a reset of the AI subsystem is
 *    done, i.e. tarnsfer of data from the DAQ to the data buffer
 *    (possibly via DMA) is automatically disabled, buffers are de-
 *    allocated and trigger input lines are released back into the pool
 *-----------------------------------------------------------------------*/

static ssize_t ni_daq_read( struct file * file_p,
			    char __user * buff,
			    size_t        count,
			    loff_t *      offp )
{
	int minor;
	Board *board;
	int ret;


	minor = MINOR( file_p->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( ! board->in_use ) {
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	if ( file_p->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->use_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->use_mutex ) )
		return -ERESTARTSYS;

	/* It's a fatal error if there can't be any data because no valid
	   acquisition setup has been done */

	if ( ! board->AI.is_acq_setup ) {
		up( &board->use_mutex );
		PDEBUG( "Missing acquisition setup\n" );
		return -EINVAL;
	}

	/* If an acquisition has been set up but hasn't been started yet
	   trying to read data triggers the start of the acquisition */

	if (    board->data_buffer[ NI_DAQ_AI_SUBSYSTEM ] == NULL
	     && ( ret = AI_start_acq( board ) ) < 0 ) {
		up( &board->use_mutex );
		return ret;
	}

	/* If the device file was opened in blocking mode make sure there are
	   data: if no data are available immediately enable the STOP
	   interrupt (which gets raised when a scan is finished) and then
	   wait for it (or for the SC TC interrupt, which is raised at the
	   end of the acquisition). */

	if (    ! ( file_p->f_flags & O_NONBLOCK )
	     && ! board->func->data_get_available( board,
						   NI_DAQ_AI_SUBSYSTEM ) ) {
		daq_irq_enable( board, IRQ_AI_STOP, AI_irq_handler );

		if ( wait_event_interruptible( board->AI.waitqueue,
			           board->irq_hand[ IRQ_AI_SC_TC ].raised
			        || board->irq_hand[ IRQ_AI_STOP ].raised ) ) {
			daq_irq_disable( board, IRQ_AI_STOP );
			up( &board->use_mutex );
			return -EINTR;
		}

		daq_irq_disable( board, IRQ_AI_STOP );
	}

	/* Now get as many new data as possible */

        ret = board->func->data_buf_get( board, NI_DAQ_AI_SUBSYSTEM,
					 ( void __user * ) buff, &count, 0 );

	/* If there are no new data and the device file was opened in non-
	   blocking mode return -EAGAIN */

	if ( ret == 0 && count == 0 && file_p->f_flags & O_NONBLOCK ) {
		up( &board->use_mutex );
		return -EAGAIN;
	}

	/* If all points to be expected from the acquisition have been fetched
	   or if there was an unrecoverable error disable AI SC_TC interrupt
	   and release all AI trigger inputs as well as switching off data
	   transfer from the DAQ to the bufers (possibly via DMA) and releasing
	   the associated buffers. */

	if ( ret != 0 && board->AI.is_running ) {
		board->AI.is_running = 0;
		daq_irq_disable( board, IRQ_AI_SC_TC );
		MSC_PFI_setup( board, NI_DAQ_AI_SUBSYSTEM, NI_DAQ_ALL,
			       NI_DAQ_PFI_UNUSED );
	}

	if ( ret < 0 ) {
		board->func->data_shutdown( board, NI_DAQ_AI_SUBSYSTEM );
		up( &board->use_mutex );
		return ret;
	}

	*offp += count;

	up( &board->use_mutex );

	PDEBUG( "Returning count = %ld\n", ( long ) count );
	return count;
}


/*--------------------------------------------------------*
 * Not implemented yet (to be used with the AO subsystem)
 *--------------------------------------------------------*/

static ssize_t ni_daq_write( struct file *        file_p,
			     const char __user *  buff,
			     size_t               count,
			     loff_t *             offp )
{
	return -ENOSYS;
}


/*--------------------------------------------------------------*
 * Function for handling of ioctl() calls for one of the boards
 *--------------------------------------------------------------*/

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
static int ni_daq_ioctl( struct inode * inode_p,
			 struct file *  file_p,
			 unsigned int   cmd,
			 unsigned long  arg )
#else
static long ni_daq_ioctl( struct file *  file_p,
			  unsigned int   cmd,
			  unsigned long  arg )
#endif
{
	int minor;
	Board *board;
	int ret;


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
	inode_p = inode_p;
#endif

	minor = MINOR( file_p->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( ! board->in_use ) {
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	if (    _IOC_TYPE( cmd ) != NI_DAQ_MAGIC_IOC
	     || _IOC_NR( cmd ) < NI_DAQ_MIN_NR
	     || _IOC_NR( cmd ) > NI_DAQ_MAX_NR ) {
		PDEBUG( "Invalid ioctl() call %d\n", cmd );
		return -EINVAL;
	}

	if ( file_p->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->use_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->use_mutex ) )
		return -ERESTARTSYS;

	switch ( cmd ) {
		case NI_DAQ_IOC_MSC :
			ret = MSC_ioctl_handler( board,
						 ( NI_DAQ_MSC_ARG * ) arg );
			break;

		case NI_DAQ_IOC_AI :
			ret = AI_ioctl_handler( board,
						( NI_DAQ_AI_ARG * ) arg );
			break;

		case NI_DAQ_IOC_AO :
			ret = AO_ioctl_handler( board,
						( NI_DAQ_AO_ARG * ) arg );
			break;

		case NI_DAQ_IOC_GPCT :
			ret = GPCT_ioctl_handler( board,
						  ( NI_DAQ_GPCT_ARG * ) arg );
			break;

		case NI_DAQ_IOC_DIO :
			ret = DIO_ioctl_handler( board,
						  ( NI_DAQ_DIO_ARG * ) arg );
			break;

		default :
			PDEBUG( "Invalid ioctl() call %d\n", cmd );
			ret = -EINVAL;
	}

	up( &board->use_mutex );
	return ret;
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
