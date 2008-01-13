/*
  $Id$
 
  Driver for National Instruments PCI E Series DAQ boards

  Copyright (C) 2003-2008 Jens Thoms Toerring

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

  To contact the author send email to:  jt@toerring.de
*/


#include "ni_daq_board.h"
#include "board.h"



#if defined NI_DAQ_DEBUG
static char *DAQ_reg_names[ ] = { "Window_Address",
				  "Window_Data_Write",
				  "Interrupt_A_Ack",
				  "Interrupt_B_Ack",
				  "AI_Command_2",
				  "AO_Command_2",
				  "G0_Command",
				  "G1_Command",
				  "AI_Command_1",
				  "AO_Command_1",
				  "DIO_Output",
				  "DIO_Control",
				  "AI_Mode_1",
				  "AI_Mode_2",
				  "AI_SI_Load_A",
				  "invalid addr. 15",
				  "AI_SI_Load_B",
				  "invalid addr. 17",
				  "AI_SC_Load_A",
				  "invalid addr. 19",
				  "AI_SC_Load_B",
				  "invalid addr. 21",
				  "invald addr. 22",
				  "AI_SI2_Load_A",
				  "invalid addr. 22",
				  "AI_SI2_Load_B",
				  "G0_Mode",
				  "G1_Mode",
				  "G0_Load_A",
				  "invalid addr. 29",
				  "G0_Load_B",
				  "invalid addr. 31",
				  "G1_Load_A",
				  "invalid addr. 33",
				  "G1_Load_B",
				  "invalid addr. 35",
				  "G0_Input_Select",
				  "G1_Input_Select",
				  "AO_Mode_1",
				  "AO_Mode_2",
				  "AO_UI_Load_A",
				  "invalid addr. 41",
				  "AO_UI_Load_B",
				  "invalid addr. 43",
				  "AO_BC_Load_A",
				  "invalid addr. 45",
				  "AO_BC_Load_B",
				  "invalid addr. 47",
				  "AO_UC_Load_A",
				  "invalid addr. 49",
				  "AO_UC_Load_B",
				  "invalid addr. 51",
				  "invalid addr. 52",
				  "AO_UI2_Load_A",
				  "invalid addr. 54",
				  "AO_UI2_Load_B",
				  "Clock_and_FOUT",
				  "IO_Bidirection_Pin",
				  "RTSI_Trig_Direction",
				  "Interrupt_Control",
				  "AI_Output_Control",
				  "Analog_Trigger_Etc",
				  "AI_START_STOP_Select",
				  "AI_Trigger_Select",
				  "AI_DIV_Load_A",
				  "invalid addr. 65",
				  "AO_START_Select",
				  "AO_Trigger_Select",
				  "G0_Autoincrement",
				  "G1_Autoincrement",
				  "AO_Mode_3",
				  "Generic_Control",
				  "Joint_Reset",
				  "Interrupt_A_Enable",
				  "Second_Irq_A_Enable",
				  "Interrupt_B_Enable",
				  "Second_Irq_B_Enable",
				  "AI_Personal",
				  "AO_Personal",
				  "RTSI_Trig_A_Output",
				  "RTSI_Trig_B_Output",
				  "RTSI_Board",
				  "Write_Strobe_0",
				  "Write_Strobe_1",
				  "Write_Strobe_2",
				  "Write_Strobe_3",
				  "AO_Output_Control",
				  "AI_Mode_3" };
#endif


/*------------------------------------------------------------*
 * Initialization of the data structure used in protection of
 * parts of code that must not be interrupted.
 *------------------------------------------------------------*/

inline void pci_init_critical_section_handling( Board * board )
{
	board->critical_section.count = 0;
	spin_lock_init( &board->critical_section.spinlock );
}


/*---------------------------------------------------------------------*
 * Function to protect parts of code that must not be interrupted.
 * The inverse function must be called as many times as this
 * function has been to give up the spinlock and re-enable interrupts.
 *---------------------------------------------.-----------------------*/

void pci_start_critical_section( Board * board )
{
	if ( board->critical_section.count++ == 0 )
		spin_lock_irqsave( &board->critical_section.spinlock,
				   board->critical_section.flags );
}


/*----------------------------------------------------------*
 * Function to be called at the end of a parts of code that
 * must not be interrupted.
 *----------------------------------------------------------*/

void pci_end_critical_section( Board * board )
{
#if defined NI_DAQ_DEBUG
	if ( board->critical_section.count == 0 ) {
		PDEBUG( "Unbalanced start/end critical section\n" );
		return;
	}
#endif

	if ( --board->critical_section.count == 0 )
		spin_unlock_irqrestore( &board->critical_section.spinlock,
					board->critical_section.flags );
}


/*------------------------------------------------------------------*
 * Function for writing to one of the 16-bit wide DAQ-STC registers
 * Interrupts must be disabled between setting the window address
 * and writing to the window data register.
 *------------------------------------------------------------------*/

#if ! defined NI_DAQ_DEBUG
void pci_stc_writew( Board * board,
		     u16     offset,
		     u16     data )
#else
void pci_stc_writew( const char * fn,
		     Board *      board,
		     u16          offset,
		     u16          data )
#endif
{
#if defined NI_DAQ_DEBUG
	if ( offset <= STC_Window_Data_Write ||
	     offset > STC_AI_Mode_3 ) {
		PDEBUG( "%s(): 16-bit write to invalid DAQ-STC offset (%u)\n",
			fn, ( unsigned int ) offset );
		return;
	}

	PDEBUG( "%30s(): %20s: 0x%04x\n", fn, DAQ_reg_names[ offset ], data );
#endif

	/* If we're going to write to one of the DAQ-STC register that can
	   be directly written to use the faster direct write method,
	   otherwise do a windowed write operation. */

	if ( offset >= STC_Interrupt_A_Ack &&
	     offset <= STC_Gi_Command( 1 ) )
		iowrite16( data, board->regs->Window_Address + offset );
	else {
		pci_start_critical_section( board );
		iowrite16( offset, board->regs->Window_Address );
		iowrite16( data, board->regs->Window_Data );
		pci_end_critical_section( board );
	}

	/* Keep a private copy of what we've just written to the register */

	* ( &board->stc.Window_Address + offset ) = data;
}


/*------------------------------------------------------------------*
 * Function for writing to one of the 32-bit wide DAQ-STC registers
 * Interrupts must be disabled between setting the window address
 * and writing to the window data register.
 *------------------------------------------------------------------*/

#if defined NI_DAQ_DEBUG
void pci_stc_writel( const char * fn,
		     Board *      board,
		     u16          offset,
		     u32          data )
#else
void pci_stc_writel( Board * board,
		     u16     offset,
		     u32     data )
#endif
{
#if defined NI_DAQ_DEBUG
	if ( offset <= STC_Window_Data_Write ||
	     offset > STC_AO_Output_Control ) {
		PDEBUG( "%s(): 32-bit write to invalid DAQ-STC offset (%u)\n",
			fn, ( unsigned int ) offset );
		return;
	}

	PDEBUG( "%30s(): %20s: 0x%08x\n", fn, DAQ_reg_names[ offset ], data );
#endif

	pci_start_critical_section( board );
	iowrite16( offset, board->regs->Window_Address );
	iowrite16( ( u16 ) ( data >> 16 ), board->regs->Window_Data );
	iowrite16( offset + 1, board->regs->Window_Address );
	iowrite16( ( u16 ) ( data & 0xFFFF ), board->regs->Window_Data );
	pci_end_critical_section( board );

	/* Keep a private copy of what we've just written to the register */

	* ( u32 * ) ( &board->stc.Window_Address + offset ) = data;
}


/*--------------------------------------------------------------------*
 * Function for reading from one of the 16-bit wide DAQ-STC registers
 * Interrupts must be disabled between setting the window address and
 * reading from the window data register.
 *--------------------------------------------------------------------*/

u16 pci_stc_readw( Board * board,
		   u16     offset )
{
	u16 data;


#if defined NI_DAQ_DEBUG
	if ( offset <= STC_Window_Data_Read || offset > STC_AI_Mode_3 ) {
		PDEBUG( "16-bit read from invalid DAQ-STC offset (%u)\n",
			( unsigned int ) offset );
		return 0;
	}
#endif

	/* If we're going to read from one of the DAQ-STC register that can
	   be directly read from use the faster direct read method, otherwise
	   do a windowed read operation. */

	if ( offset >= STC_AI_Status_1 && offset <= STC_DIO_Parallel_Input )
		data = ioread16( board->regs->Window_Address + offset );
	else {
		pci_start_critical_section( board );
		iowrite16( offset, board->regs->Window_Address );
		data = ioread16( board->regs->Window_Data );
		pci_end_critical_section( board );
	}

	return data;
}


/*--------------------------------------------------------------------*
 * Function for reading from one of the 32-bit wide DAQ-STC registers
 * Interrupts must be disabled between setting the window address and
 * reading from the window data register.
 *--------------------------------------------------------------------*/

u32 pci_stc_readl( Board * board,
		   u16     offset )
{
	u32 data;


#if defined NI_DAQ_DEBUG
	if ( offset <= STC_Window_Data_Read || offset > STC_AI_Mode_3 ) {
		PDEBUG( "32-bit read from invalid DAQ-STC offset (%u)\n",
			( unsigned int ) offset );
		return 0;
	}
#endif

	pci_start_critical_section( board );
	iowrite16( offset, board->regs->Window_Address );
	data = ( u32 ) ioread16( board->regs->Window_Data ) << 16;
	iowrite16( offset + 1, board->regs->Window_Address );
	data |= ( u32 ) ( ioread16( board->regs->Window_Data ) & 0xFFFF );
	pci_end_critical_section( board );

	return data;
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
