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


#include "ni_daq_board.h"
#include "board.h"


static u8 eeprom_read( Board *board, u16 addr );
static void caldac_write( Board *board, u8 ser_dac, CALDAC_TYPES type,
			  u8 addr, u16 val );
static void caldac_send( Board *board, u8 ser_dac, u16 data, int num_bits );
static void calib_1( Board *board );
static void calib_2( Board *board );
static void calib_3( Board *board );
static void calib_4( Board *board );
static void calib_5( Board *board );
static void calib_6( Board *board );


/*------------------------------------------------*/
/* Function for setting the analog trigger levels */
/*------------------------------------------------*/

void pci_set_trigger_levels( Board *board, u16 high, u16 low )
{
	caldac_write( board, 0, board->type->atrig_caldac,
		      board->type->atrig_low_ch, low );
	caldac_write( board, 0, board->type->atrig_caldac,
		      board->type->atrig_high_ch, high );
}


/*------------------------------------------------*/
/*  Function for reading out data from the EEPROM */
/*------------------------------------------------*/

static u8 eeprom_read( Board *board, u16 addr )
{
	u16 bit;
	u8 bits = 0;
	u16 bitstring;


	/* Please check with the timing diagram, figure 5-1, of the calibration
	   chapter in the PCI E Series Register Programming Manual to
	   understand why the bitstring is set up in this way. */

	bitstring = 0x300 | ( ( addr & 0x100 ) << 3 ) | ( addr & 0xff );

	/* Send the address we want to read from to the EEPROM */

	writeb( EEPromCS, board->regs->Serial_Command );
	udelay( 1 );

	for ( bit = 0x8000; bit; bit >>= 1 )
	{
		bits = EEPromCS | ( ( bit & bitstring ) ? SerData : 0 );
		writeb( bits, board->regs->Serial_Command );
		udelay( 1 );
		bits |= SerClk;
		writeb( bits, board->regs->Serial_Command );
		udelay( 1 );
	}

	/* Now read in what the EEPROM returns */

	bitstring = 0;

	for ( bit = 0x80; bit; bit >>= 1 )
	{
		writeb( EEPromCS, board->regs->Serial_Command );
		udelay( 1 );
		writeb( EEPromCS | SerClk, board->regs->Serial_Command );
		udelay( 1 );
		bitstring |=
			  ( readb( board->regs->Status ) & PROMOUT ) ? bit : 0;
	}

	writeb( 0, board->regs->Serial_Command );
	writeb( 0, board->regs->Serial_Command );

	return bitstring;
}


/*------------------------------------------------------------------*/
/* Function for writing data to (one of) the CalDAC(s) of the board */
/*------------------------------------------------------------------*/

static void caldac_write( Board *board, u8 ser_dac, CALDAC_TYPES type,
			  u8 addr, u16 val )
{
	static unsigned char rev_nibble[ ] = { 0,  8,  4, 12,
					       2, 10,  6, 14,
					       1,  9,  5, 13,
					       3, 11,  7, 15 };

	switch ( type )
	{
		case mb88341:
			caldac_send( board, ser_dac,
				     ( rev_nibble[ addr & 0x0F ] << 8 ) |
				     ( val & 0xFF ), 12 );
			break;

		case dac8800 :
			caldac_send( board, ser_dac,
				     ( addr & 0x07 ) | ( val & 0xFF ), 11 );
			break;

		case dac8043 :
			caldac_send( board, ser_dac, val & 0xFFF, 12 );
			break;

		case ad8522 :
			caldac_send( board, ser_dac,
				     0x8000 | ( ( addr ? 1 : 2 ) << 13 ) |
				     ( val & 0xFFF ), 16 );
			break;

		case caldac_none :
			break;
	}
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

static void caldac_send( Board *board, u8 ser_dac, u16 data, int num_bits )
{
	u16 bit;
	u16 bitstring;


	for ( bit = 1 << ( num_bits - 1 ); bit; bit >>= 1 )
	{
		bitstring = ( ( bit & data ) ? SerData : 0 );
		writeb( bitstring, board->regs->Serial_Command );
		udelay( 1 );
		bitstring |= SerClk;
		writeb( bitstring, board->regs->Serial_Command );
		udelay( 1 );
	}

	bitstring = 1 << ( ser_dac + SerDacLd_Shift );
	writeb( bitstring, board->regs->Serial_Command );
	udelay( 1 );
	bitstring = 0;
	writeb( bitstring, board->regs->Serial_Command );
	udelay( 1 );
}


/*------------------------------------------------------------*/
/*------------------------------------------------------------*/

void caldac_calibrate( Board *board )
{
	struct calib_list {
		const char *board_name;
		void ( *calib_func )( Board *board );
	} bc[ ] = { { "pci-mio-16e-1", calib_1 },
		    { "pci-mio-16e-4", calib_1 },
		    { "pci-mio-16xe-10", calib_2 },
		    { "pci-mio-16xe-50", calib_3 },
		    { "pci-6023e", calib_4 },
		    { "pci-6024e", calib_5 },
		    { "pci-6025e", calib_5 },
		    { "pci-6031e", calib_2 },
		    { "pci-6032e", calib_2 },
		    { "pci-6033e", calib_2 },
		    { "pci-6052e", calib_6 },
		    { "pci-6071e", calib_1 } };
	
	size_t i;

	for ( i = 0; i < sizeof bc / sizeof bc[ 0 ]; i++ )
		if ( ! strcmp( board->type->name, bc[ i ].board_name ) )
		{
			bc[ i ].calib_func( board );
			return;
		}
}


/*---------------------------------------------------------------------*/
/* Function for setting the CalDAC of the PCI-MIO-16E-1, PCI-MIO-16E-4 */
/* and PCI-6071E to the values from the EEPROM                         */
/*---------------------------------------------------------------------*/

static void calib_1( Board *board )
{
#if 0
	u16 val;


	val = eeprom_read( board, 424 );
	caldac_write( board, 0, mb88341, 4,  val );
	val = eeprom_read( board, 423 );
	caldac_write( board, 0, mb88341, 1,  val );
	val = eeprom_read( board, 422 );
	caldac_write( board, 0, mb88341, 3,  val );
	caldac_write( board, 0, mb88341, 14, val );    /* why ? */
	val = eeprom_read( board, 421 );
	caldac_write( board, 0, mb88341, 2,  val );

	val = eeprom_read( board, 420 );
	caldac_write( board, 0, mb88341, 5,  val );
	val = eeprom_read( board, 419 );
	caldac_write( board, 0, mb88341, 7,  val );
	caldac_write( board, 0, mb88341, 13, val );    /* why ? */
	val = eeprom_read( board, 418 );
	caldac_write( board, 0, mb88341, 6,  val );
	val = eeprom_read( board, 417 );
	caldac_write( board, 0, mb88341, 8,  val );
	val = eeprom_read( board, 416 );
	caldac_write( board, 0, mb88341, 10, val );
	val = eeprom_read( board, 415 );
	caldac_write( board, 0, mb88341, 9,  val );
#endif
}


/*--------------------------------------------------------------------*/
/* Function for setting the CalDAC of the PCI-MIO-16XE-10, PCI-6031E, */
/* PCI-6032E and PCI-6033E to the values from the EEPROM              */
/*--------------------------------------------------------------------*/

static void calib_2( Board *board )
{
	u16 val;


	val = eeprom_read( board, 429 ) << 8;
	val |= eeprom_read( board, 428 );
	caldac_write( board, 1, dac8043, 0, val);

	val = eeprom_read( board, 427 );
	caldac_write( board, 0, dac8800, 2, val );
	val = eeprom_read( board, 426 );
	caldac_write( board, 0, dac8800, 3, val );
	val = eeprom_read( board, 425 );
	caldac_write( board, 0, dac8800, 0, val );
	val = eeprom_read( board, 424 );
	caldac_write( board, 0, dac8800, 1, val );

	val = eeprom_read( board, 417 );
	caldac_write( board, 0, dac8800, 6, val );
	val = eeprom_read( board, 416 );
	caldac_write( board, 0, dac8800, 4, val );
	val = eeprom_read( board, 415 );
	caldac_write( board, 0, dac8800, 7, val );
	val = eeprom_read( board, 414 );
	caldac_write( board, 0, dac8800, 5, val );
}


/*----------------------------------------------------------------------*/
/* Function for setting the CalDAC of the PCI-MIO-16XE-50 to the values */
/* from the EEPROM                                                      */
/*----------------------------------------------------------------------*/

static void calib_3( Board *board )
{
	u16 val;


	val = eeprom_read( board, 436 ) << 8;
	val |= eeprom_read( board, 435 );
	caldac_write( board, 1, dac8043, 0, val);

	val = eeprom_read( board, 434 );
	caldac_write( board, 0, dac8800, 2, val );
	val = eeprom_read( board, 433 );
	caldac_write( board, 0, dac8800, 0, val );
	val = eeprom_read( board, 432 );
	caldac_write( board, 0, dac8800, 1, val );

	val = eeprom_read( board, 426 );
	caldac_write( board, 0, dac8800, 6, val );
	val = eeprom_read( board, 425 );
	caldac_write( board, 0, dac8800, 4, val );
	val = eeprom_read( board, 424 );
	caldac_write( board, 0, dac8800, 7, val );
	val = eeprom_read( board, 423 );
	caldac_write( board, 0, dac8800, 5, val );
}


/*----------------------------------------------------------------*/
/* Function for setting the CalDAC of the PCI-6023E to the values */
/* from the EEPROM                                                */
/*----------------------------------------------------------------*/

static void calib_4( Board *board )
{
	u16 val;


	val = eeprom_read( board, 442 );
	caldac_write( board, 0, mb88341, 4,  val );
	val = eeprom_read( board, 441 );
	caldac_write( board, 0, mb88341, 11, val );
	val = eeprom_read( board, 440 );
	caldac_write( board, 0, mb88341, 1,  val );
	val = eeprom_read( board, 439 );
	caldac_write( board, 0, mb88341, 2,  val );
}


/*----------------------------------------------------------------*/
/* Function for setting the CalDAC of the PCI-6024E and PCI-6025E */
/* to the values from the EEPROM                                  */
/*----------------------------------------------------------------*/

static void calib_5( Board *board )
{
	u16 val;


	val = eeprom_read( board, 430 );
	caldac_write( board, 0, mb88341, 4,  val );
	val = eeprom_read( board, 429 );
	caldac_write( board, 0, mb88341, 11, val );
	val = eeprom_read( board, 428 );
	caldac_write( board, 0, mb88341, 1,  val );
	val = eeprom_read( board, 427 );
	caldac_write( board, 0, mb88341, 2,  val );

	val = eeprom_read( board, 426 );
	caldac_write( board, 0, mb88341, 5,  val );
	val = eeprom_read( board, 425 );
	caldac_write( board, 0, mb88341, 7,  val );
	val = eeprom_read( board, 424 );
	caldac_write( board, 0, mb88341, 6,  val );
	val = eeprom_read( board, 423 );
	caldac_write( board, 0, mb88341, 8,  val );
	val = eeprom_read( board, 422 );
	caldac_write( board, 0, mb88341, 10, val );
	val = eeprom_read( board, 421 );
	caldac_write( board, 0, mb88341, 9,  val );
}


/*----------------------------------------------------------------*/
/* Function for setting the CalDAC of the PCI-6052E to the values */
/* from the EEPROM                                                */
/*----------------------------------------------------------------*/

static void calib_6( Board *board )
{
	/* Tha manual claims that this board uses two MB88341 chips, but
	   this can't be correct because the MB88341 has only 12 DACs
	   and the manual also tells to set 15 DACs. */
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
