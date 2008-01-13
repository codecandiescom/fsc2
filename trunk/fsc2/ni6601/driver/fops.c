/*
 *  $Id$
 * 
 *  Driver for National Instruments 6601 boards
 * 
 *  Copyright (C) 2002-2008 Jens Thoms Toerring
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


#include "ni6601_drv.h"


extern Board boards[ NI6601_MAX_BOARDS ];    /* defined in ni6601_drv.c */
extern int board_count;                      /* defined in ni6601_drv.c */


/*----------------------------------------------------------------------*
 * Function gets exectuted when the device file for a board gets opened
 *----------------------------------------------------------------------*/

int ni6601_open( struct inode * inode_p,
		 struct file *  filep )
{
	int minor;
	Board *board;
	int i;


	minor = MINOR( inode_p->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	/* All the follwing code can be only be accessed from a single thread
	   at a time. For callers that want to open the device not prepared
	   to wait we need to work with down_trylock(). For others we call
	   down_interruptible() in order to be able to react to signals (the
	   return value ERESTARTSYS is a "magic" value saying that it's ok to
	   restart the system call automagically after the signal has been
	   handled and gets converted to EINTR if that's not possible).
	   Please note that the mutex is also used during the close() call. */

	if ( filep->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->open_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->open_mutex ) )
		return -ERESTARTSYS;

	if ( board->in_use ) {
		up( &board->open_mutex );
		PDEBUG( "Board already in use by another process/thread\n" );
		return -EBUSY;
	}

	/* Globally enable interrupts (without this they get raised internally
	   in the timer chip only and don't make it through to the CPU) */

	iowrite32( GLOBAL_IRQ_ENABLE, board->regs.global_irq_control );

	/* Clean out the autoincrement registers */

	for ( i = 0; i < 4; i++ )
		iowrite16( 0, board->regs.autoincrement[ i ] );

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

int ni6601_release( struct inode * inode_p,
		    struct file * filep )
{
	int minor;
	Board *board;
	int i;


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

	/* Disable interrupts, reset and disarm all counters and shutdown
	   DMA if necessary, etc. */

	iowrite16( G1_RESET | G0_RESET, board->regs.joint_reset[ 0 ] );
	iowrite16( G1_RESET | G0_RESET, board->regs.joint_reset[ 2 ] );
	iowrite32( 0, board->regs.global_irq_control );

	for ( i = NI6601_COUNTER_0; i <= NI6601_COUNTER_3; i++ )
	{
		ni6601_tc_irq_disable( board, i );
		ni6601_dma_irq_disable( board, i );
	}

	if ( board->buf ) {
		vfree( board->buf );
		board->buf = NULL;
		board->buf_counter = -1;
	}

	iowrite16( 0, board->regs.dio_control );

	for ( i = 0; i < 10; i++ )
		iowrite32( 0, board->regs.io_config[ i ] );
		
	iowrite32( SOFT_RESET, board->regs.reset_control );

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 0 )
	MOD_DEC_USE_COUNT;
#endif

	board->in_use = 0;
	up( &board->open_mutex );

	return 0;
}


/*--------------------------------------------------------------*
 * Function for handling of ioctl() calls for one of the boards
 *--------------------------------------------------------------*/

#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 11 )
int ni6601_ioctl( struct inode * inode_p,
		  struct file *  filep,
		  unsigned int   cmd,
		  unsigned long  arg )
#else
long ni6601_ioctl( struct file *  filep,
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

	minor = MINOR( filep->f_dentry->d_inode->i_rdev );

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

	board = boards + minor;

	if ( filep->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->ioctl_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->ioctl_mutex ) )
		return -ERESTARTSYS;

	if ( ! board->in_use ) {
		up( &board->ioctl_mutex );
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	switch ( cmd ) {
		case NI6601_IOC_DIO_IN :
			ret = ni6601_dio_in( board,
					      ( NI6601_DIO_VALUE * ) arg );
			break;

		case NI6601_IOC_DIO_OUT :
			ret = ni6601_dio_out( board,
					      ( NI6601_DIO_VALUE * ) arg );
			break;

		case NI6601_IOC_COUNT :
			ret = ni6601_read_count( board,
						( NI6601_COUNTER_VAL * ) arg );
			break;

		case NI6601_IOC_PULSER :
			ret = ni6601_start_pulses( board,
						   ( NI6601_PULSES * ) arg );
			break;

		case NI6601_IOC_COUNTER :
			ret = ni6601_start_counting( board,
						    ( NI6601_COUNTER * ) arg );
			break;

		case NI6601_IOC_START_BUF_COUNTER :
			ret = ni6601_start_buf_counting( board,
					        ( NI6601_BUF_COUNTER * ) arg );
			break;

		case NI6601_IOC_STOP_BUF_COUNTER :
			ret = ni6601_stop_buf_counting( board );
			break;

		case NI6601_IOC_GET_BUF_AVAIL :
			ret = ni6601_get_buf_avail( board,
						  ( NI6601_BUF_AVAIL * ) arg );
			break;

		case NI6601_IOC_IS_BUSY :
			ret = ni6601_is_busy( board,
					      ( NI6601_IS_ARMED * ) arg );
			break;

		case NI6601_IOC_DISARM :
			ret = ni6601_disarm( board, ( NI6601_DISARM * ) arg );
			break;

		default :
			PDEBUG( "Invalid ioctl() call %d\n", cmd );
			ret = -EINVAL;
	}			

	up( &board->ioctl_mutex );
	return ret;
}


/*--------------------------------------------------------------------*
 * Function for dealing with poll() and select() calls for the driver
 *--------------------------------------------------------------------*/

unsigned int ni6601_poll( struct file *              filep,
			  struct poll_table_struct * pt )
{
	int minor;
	Board *board;
	unsigned int mask = POLLIN | POLLRDNORM;
	int ret = 0;


	minor = MINOR( filep->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( filep->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->ioctl_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->ioctl_mutex ) )
		return -ERESTARTSYS;

	if ( ! board->in_use ) {
		up( &board->ioctl_mutex );
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	/* If no acquisition is under way or a buffer overflow happened
	   (only possible in continuous mode) return immediately - the
	   device is "readable" */

	if ( board->buf_counter < 0 ||
	     board->buf_overflow    ||
	     board->too_fast ) {
		up( &board->ioctl_mutex );
		return mask | POLLERR;
	}

	/* If we already returned as many points as requested in non-continuous
	   mode return HUP */

	if ( ! board->buf_continuous &&
	     board->high_ptr == board->buf_top ) {
		up( &board->ioctl_mutex );
		return mask | POLLHUP;
	}

	/* If necessary wait for data to be acquired */

	if ( board->low_ptr == board->high_ptr ) {
		board->buf_waiting = 1;
		ret = wait_event_interruptible( board->dma_waitqueue,
					  board->low_ptr != board->high_ptr ||
					  board->buf_overflow ||
					  board->too_fast );
		board->buf_waiting = 0;
	}
	
	if ( board->buf_overflow || board->too_fast ) {
		up( &board->ioctl_mutex );
		return mask | POLLERR;
	}

	up( &board->ioctl_mutex );

	if ( ret != 0 )
		return 0;

	return mask;
}


/*----------------------------------------------------------------------*
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
 *    done, i.e. DMA is automatically disabled, buffers are deallocated
 *    and trigger input lines are released back into the pool
 *----------------------------------------------------------------------*/

ssize_t ni6601_read( struct file * filep,
		     char __user * buff,
		     size_t        count,
		     loff_t *      offp )
{
	int minor;
	Board *board;
	size_t transferable,
	       transfered = 0;
	int ret;
	u32 *high_ptr;


	minor = MINOR( filep->f_dentry->d_inode->i_rdev );

	if ( minor >= board_count ) {
		PDEBUG( "Board %d does not exist\n", minor );
		return -ENODEV;
	}

	board = boards + minor;

	if ( filep->f_flags & ( O_NONBLOCK | O_NDELAY ) ) {
		if ( down_trylock( &board->ioctl_mutex ) )
			return -EAGAIN;
	} else if ( down_interruptible( &board->ioctl_mutex ) )
		return -ERESTARTSYS;

	if ( ! board->in_use ) {
		up( &board->ioctl_mutex );
		PDEBUG( "Board %d not open\n", minor );
		return -EBADF;
	}

	/* Make sure the number of bytes to fetch is an integer multiple
	   of the size of a data point */

	count = ( count / sizeof *board->buf ) * sizeof *board->buf;

	if ( count == 0 ) {
		up( &board->ioctl_mutex );
		return -EINVAL;
	}

	/* If the acquisition buffer does not exist either no acquisition
	   was started or it's already finished */

	if ( board->buf_counter < 0 || board->buf == NULL ) {
		up( &board->ioctl_mutex );
		return 0;
	}

	/* In case the buffer wasn't large enough to store all new values
	   (which can only happen in continuous mode) stop everything and
	   return EOVERFLOW */

	if ( board->buf_overflow ) {
		board->buf_counter = -1;
		if ( board->buf ) {
			vfree( board->buf );
			board->buf = NULL;
		}
		up( &board->ioctl_mutex );
		return -EOVERFLOW;
	}

	/* It is also possible that data got acquired that fast that the
	   machine couldn't keep up with the amount of interrupts - in that
	   case return ESTRPIPE */
	
	if ( board->too_fast ) {
		board->buf_counter = -1;
		if ( board->buf ) {
			vfree( board->buf );
			board->buf = NULL;
		}
		up( &board->ioctl_mutex );
		return -ESTRPIPE;
	}

	/* If the device file was opened in blocking mode make sure there are
	   data, if none are available immediately wait for them. Otherwise
	   check if data are available and return EAGAIN if not. */

	if ( ! ( filep->f_flags & O_NONBLOCK ) ) {
		while ( board->low_ptr == board->high_ptr ) {
			board->buf_waiting = 1;
			ret = wait_event_interruptible( board->dma_waitqueue,
					   board->low_ptr != board->high_ptr ||
					   board->buf_overflow ||
					   board->too_fast );
			board->buf_waiting = 0;
			if ( ret != 0 ) {
				up( &board->ioctl_mutex );
				return ret;
			}
		}
	} else if ( board->low_ptr == board->high_ptr ) {
		up( &board->ioctl_mutex );
		return -EAGAIN;
	}

	/* From now on only use the current value of the high pointer - its
	   value may get changed from within the interrupt handler */

	high_ptr = board->high_ptr;

	/* Again check that no kind of overflow happened */

	if ( board->buf_overflow ) {
		board->buf_counter = -1;
		if ( board->buf ) {
			vfree( board->buf );
			board->buf = NULL;
		}
		up( &board->ioctl_mutex );
		return -EOVERFLOW;
	}

	if ( board->too_fast ) {
		board->buf_counter = -1;
		if ( board->buf ) {
			vfree( board->buf );
			board->buf = NULL;
		}
		up( &board->ioctl_mutex );
		return -ESTRPIPE;
	}

	if ( ! board->buf_continuous ) {
 		transferable = ( high_ptr - board->low_ptr )
			       * sizeof *board->buf;

		if ( transferable > count )
			transferable = count;
	
		if ( copy_to_user( buff, board->low_ptr, transferable ) ) {
			up( &board->ioctl_mutex );
			return -EFAULT;
		}
		board->low_ptr += transferable / sizeof *board->buf;
		count = transferable;

		/* If all data to be expected have been transfered delete the
		   buffer */

		if ( board->low_ptr == board->buf_top ) {
			board->buf_counter = -1;
			vfree( board->buf );
			board->buf = NULL;
		}
	} else {
		/* First get data from the top of the buffer */

		if ( board->low_ptr > high_ptr ) {
			transferable = sizeof *board->buf
			 	       * ( board->buf_top - board->low_ptr );

			if ( transferable > count )
				transferable = count;

			if ( copy_to_user( buff, board->low_ptr,
					   transferable ) ) {
				up( &board->ioctl_mutex );
				return -EFAULT;
			}
			transfered = transferable;

			board->low_ptr += transferable / sizeof *board->buf;
			if ( board->low_ptr == board->buf_top )
				board->low_ptr = board->buf;
		}

		/* And now get the ones from the bottom */

		if ( count - transfered > 0 && board->low_ptr < high_ptr ) {
			transferable = ( high_ptr - board->low_ptr )
				       * sizeof *board->buf;

			if ( transferable > count - transfered  )
				transferable = count - transfered;
	
			if ( copy_to_user( buff, board->low_ptr,
					   transferable ) ) {
				up( &board->ioctl_mutex );
				return -EFAULT;
			}
			count = transfered + transferable;

			board->low_ptr += transferable / sizeof *board->buf;
			if ( board->low_ptr == board->buf_top )
				board->low_ptr = board->buf;
		} else
			count = transfered;
	}

	*offp += count;

	up( &board->ioctl_mutex );

	return count;
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
