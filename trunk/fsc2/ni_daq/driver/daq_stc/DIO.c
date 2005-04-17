/*
 *  $Id$
 * 
 *  Driver for National Instruments PCI E Series DAQ boards
 * 
 *  Copyright (C) 2003-2005 Jens Thoms Toerring
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
 *  To contact the author send email to
 *  Jens.Toerring@physik.fu-berlin.de
 */


#include "ni_daq_board.h"


#if defined NI_DAQ_DEBUG
#define stc_writew( a, b, c ) stc_writew( __FUNCTION__, a, b, c )
#define stc_writel( a, b, c ) stc_writel( __FUNCTION__, a, b, c )
#endif


/*--------------------------------------------------------------*
 * Function resets the DIO subsystem back into a known state by
 * disabling serial output and switching all pins to input.
 *--------------------------------------------------------------*/

void DIO_reset_all( Board *board )
{
	board->stc.DIO_Control &= ~ ( DIO_HW_Serial_Enable |
				      DIO_Pins_Dir_Field );
	board->func->stc_writew( board, STC_DIO_Control,
				 board->stc.DIO_Control );
}


/*-------------------------------------------------*
 * Handler for ioctl() calls for the DIO subsystem
 *-------------------------------------------------*/

int DIO_ioctl_handler( Board *board, NI_DAQ_DIO_ARG *arg )
{
	NI_DAQ_DIO_ARG a;
	int ret;


	if ( copy_from_user( &a, arg, sizeof *arg ) ) {
		PDEBUG( "Can't read from user space\n" );
		return -EACCES;
	}

	switch ( a.cmd ) {
		case NI_DAQ_DIO_INPUT :
			ret = DIO_in( board, &a.value, a.mask );
			break;

		case NI_DAQ_DIO_OUTPUT :
			return DIO_out( board, a.value, a.mask );

		default :
			PDEBUG( "Invalid DIO command %d\n", a.cmd );
			return -EINVAL;
	}

	if ( ret == 0 && copy_to_user( arg, &a, sizeof *arg ) ) {
		PDEBUG( "Can't write to user space\n" );
		return -EACCES;
	}

	return ret;
}


/*---------------------------------------------------------------*
 * Function reads an 8-bit value from the DIO pins but only from
 * pins for which bits are set in the mask (this allows having
 * some of the pins used for input while the others are used for
 * output at the same time).
 *---------------------------------------------------------------*/

int DIO_in( Board *board, unsigned char *value, unsigned char mask )
{
	/* If necessary switch selected pins to input mode */

	if ( ( u8 ) ( ( board->stc.DIO_Control & 0xFF ) ^ mask ) != 0xFF ) {
		board->stc.DIO_Control &= ~ ( u16 ) mask;
		board->func->stc_writew( board, STC_DIO_Control,
					 board->stc.DIO_Control );
	}

	/* Read in data value from DIO parallel input register */

	*value = ( unsigned char ) board->func->stc_readw( board,
					       STC_DIO_Parallel_Input ) & mask;

	return 0;
}


/*------------------------------------------------------------------*
 * Function outputs an 8-bit value at the DIO pins but only at the
 * pins for which bits are set in the mask (this allows having some
 * of the pins being used for output while the others are used for
 * input at the same time).
 *------------------------------------------------------------------*/

int DIO_out( Board *board, unsigned char value, unsigned char mask )
{
	/* Write data value to DIO parallel output register */

	board->func->stc_writew( board, STC_DIO_Output, ( u16 ) value );

	/* Now if necessary switch selected pins to output mode */

	if ( ( board->stc.DIO_Control & 0xFF ) != mask ) {
		board->stc.DIO_Control |= mask;
		board->func->stc_writew( board, STC_DIO_Control,
					 board->stc.DIO_Control );
	}

	return 0;
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
