/*
 *  Driver for National Instruments PCI E Series DAQ boards
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


void pci_mite_init( Board * board );
void pci_mite_close( Board *board );

#if defined NI_DAQ_DEBUG
void pci_mite_dump( Board *board, int channel );
#endif


/* MITE DMA control registers for channel 0 start at offset 0x500 from the
   MITE base address, and the others channels follow in steps of 0x100 */

/* Channel Operation Registers */

#define mite_CHOR( ch )             ( board->mite + 0x500 + 0x100 * ( ch ) )

#define CHOR_DMARESET               ( 1 << 31 )
#define CHOR_SET_SEND_TC            ( 1 << 11 )
#define CHOR_CLR_SEND_TC            ( 1 << 10 )
#define CHOR_SET_LPAUSE             ( 1 <<  9 )
#define CHOR_CLR_LPAUSE             ( 1 <<  8 )
#define CHOR_CLRDONE                ( 1 <<  7 )
#define CHOR_CLRRB                  ( 1 <<  6 )
#define CHOR_CLRLC                  ( 1 <<  5 )
#define CHOR_FRESET                 ( 1 <<  4 )
#define CHOR_ABORT                  ( 1 <<  3 )
#define CHOR_STOP                   ( 1 <<  2 )
#define CHOR_CONT                   ( 1 <<  1 )
#define CHOR_START                  ( 1 <<  0 )
#define CHOR_PON                    ( CHOR_CLR_SEND_TC | CHOR_CLR_LPAUSE )


/* Channel Control Registers */

#define mite_CHCR( ch )             ( board->mite + 0x504 + 0x100 * ( ch ) )

#define CHCR_SET_DMA_IE             ( 1 << 31 )
#define CHCR_CLR_DMA_IE             ( 1 << 30 )
#define CHCR_SET_LINKP_IE           ( 1 << 29 )
#define CHCR_CLR_LINKP_IE           ( 1 << 28 )
#define CHCR_SET_SAR_IE             ( 1 << 27 )
#define CHCR_CLR_SAR_IE             ( 1 << 26 )
#define CHCR_SET_DONE_IE            ( 1 << 25 )
#define CHCR_CLR_DONE_IE            ( 1 << 24 )
#define CHCR_SET_MRDY_IE            ( 1 << 23 )
#define CHCR_CLR_MRDY_IE            ( 1 << 22 )
#define CHCR_SET_DRDY_IE            ( 1 << 21 )
#define CHCR_CLR_DRDY_IE            ( 1 << 20 )
#define CHCR_SET_LC_IE              ( 1 << 19 )
#define CHCR_CLR_LC_IE              ( 1 << 18 )
#define CHCR_SET_CONT_RB_IE         ( 1 << 17 )
#define CHCR_CLR_CONT_RB_IE         ( 1 << 16 )
#define CHCR_FIFODIS                ( 1 << 15 )
#define CHCR_FIFO_ON                ( 0 << 15 )
#define CHCR_BURSTEN                ( 1 << 14 )
#define CHCR_NO_BURSTEN             ( 0 << 14 )
#define CHCR_NFTP16                 ( 5 << 11 )
#define CHCR_NFTP8                  ( 4 << 11 )
#define CHCR_NFTP4                  ( 3 << 11 )
#define CHCR_NFTP2                  ( 2 << 11 )
#define CHCR_NFTP1                  ( 1 << 11 )
#define CHCR_NFTP0                  ( 0 << 11 )
#define CHCR_NETP8                  ( 4 << 11 ) /* ? */
#define CHCR_NETP4                  ( 3 << 11 ) /* ? */
#define CHCR_NETP2                  ( 2 << 11 ) /* ? */
#define CHCR_NETP1                  ( 1 << 11 ) /* ? */
#define CHCR_NETP0                  ( 0 << 11 ) /* ? */
#define CHCR_CHEND1                 ( 1 <<  5 )
#define CHCR_CHEND0                 ( 1 <<  4 )
#define CHCR_LINKLONG               ( 5 <<  0 )
#define CHCR_LINKSHORT              ( 4 <<  0 )
#define CHCR_DEV_TO_MEM             ( 1 <<  3 )
#define CHCR_MEM_TO_DEV             ( 0 <<  3 )
#define CHCR_RINGBUFF               ( 1 <<  1 )
#define CHCR_CONTINUE               ( 1 <<  0 )
#define CHCR_NORMAL                 ( 0 <<  0 )


#define CHCRPON                     ( CHCR_CLR_DMA_IE  | CHCR_CLR_LINKP_IE | \
				      CHCR_CLR_SAR_IE  | CHCR_CLR_DONE_IE  | \
				      CHCR_CLR_MRDY_IE | CHCR_CLR_DRDY_IE  | \
				      CHCR_CLR_LC_IE   | CHCR_CLR_CONT_RB_IE )


/* Transfer Count Registers */

#define mite_TCR( ch )              ( board->mite + 0x508 + 0x100 * ( ch ) )

/* CR bits */

#define CR_RL64                     ( 7 << 21 )
#define CR_RL32                     ( 6 << 21 )
#define CR_RL16                     ( 5 << 21 )
#define CR_RL8                      ( 4 << 21 )
#define CR_RL4                      ( 3 << 21 )
#define CR_RL2                      ( 2 << 21 )
#define CR_RL1                      ( 1 << 21 )
#define CR_RL0                      ( 0 << 21 )
#define CR_RD8192                   ( 3 << 19 )
#define CR_RD512                    ( 2 << 19 )
#define CR_RD32                     ( 1 << 19 )
#define CR_RD0                      ( 0 << 19 )
#define CR_REQS( x )                ( ( x + 4 ) << 16 )
#define CR_REQSDRQ3                 ( 7 << 16 )
#define CR_REQSDRQ2                 ( 6 << 16 )
#define CR_REQSDRQ1                 ( 5 << 16 )
#define CR_REQSDRQ0                 ( 4 << 16 )
#define CR_ASEQx( x )               ((x)<<10)
#define CR_ASEQx0                   CR_ASEQx(0)
#define CR_ASEQDONT                 CR_ASEQx0
#define CR_ASEQxP1                  CR_ASEQx(1)
#define CR_ASEQUP                   CR_ASEQxP1
#define CR_ASEQxP2                  CR_ASEQx(2)
#define CR_ASEQDOWN                 CR_ASEQxP2
#define CR_ASEQxP4                  CR_ASEQx(3)
#define CR_ASEQxP8                  CR_ASEQx(4)
#define CR_ASEQxP16                 CR_ASEQx(5)
#define CR_ASEQxP32                 CR_ASEQx(6)
#define CR_ASEQxP64                 CR_ASEQx(7)
#define CR_ASEQxM1                  CR_ASEQx(9)
#define CR_ASEQxM2                  CR_ASEQx(10)
#define CR_ASEQxM4                  CR_ASEQx(11)
#define CR_ASEQxM8                  CR_ASEQx(12)
#define CR_ASEQxM16                 CR_ASEQx(13)
#define CR_ASEQxM32                 CR_ASEQx(14)
#define CR_ASEQxM64                 CR_ASEQx(15)
#define CR_PSIZEWORD                ( 3 <<  8 )
#define CR_PSIZEHALF                ( 2 <<  8 )
#define CR_PSIZEBYTE                ( 1 <<  8 )
#define CR_PORTCPU                  ( 0 <<  6 )
#define CR_PORTIO                   ( 1 <<  6 )
#define CR_PORTVXI                  ( 2 <<  6 )
#define CR_PORTMXI                  ( 3 <<  6 )
#define CR_AMDEVICE                 ( 1 <<  0 )


/* Channel Status Registers */

#define mite_CHSR( ch )             ( board->mite + 0x53C + 0x100 * ( ch ) )

#define CHSR_INT                    ( 1 << 31 )
#define CHSR_LPAUSES                ( 1 << 29 )
#define CHSR_SARS                   ( 1 << 27 )
#define CHSR_DONE                   ( 1 << 25 )
#define CHSR_MRDY                   ( 1 << 23 )
#define CHSR_DRDY                   ( 1 << 21 )
#define CHSR_LINKC                  ( 1 << 19 )
#define CHSR_CONTS_RB               ( 1 << 17 )
#define CHSR_ERROR                  ( 1 << 15 )
#define CHSR_SABORT                 ( 1 << 14 )
#define CHSR_HABORT                 ( 1 << 13 )
#define CHSR_STOPS                  ( 1 << 12 )
#define CHSR_OPERR_mask             ( 3 << 10 )
#define CHSR_OPERR_FIFOERROR        ( 1 << 10 )
#define CHSR_OPERR_LINKERROR        ( 1 << 10 ) /* ??? */
#define CHSR_OPERR_NOERROR          ( 0 << 10 )
#define CHSR_XFERR                  ( 1 <<  9 )
#define CHSR_END                    ( 1 <<  8 )
#define CHSR_DRQ1                   ( 1 <<  7 )
#define CHSR_DRQ0                   ( 1 <<  6 )

#define CHSR_LxERR_mask             ( 3 <<  4 )

#define CHSR_LOERR                  ( 3 <<  4 )
#define CHSR_LRERR                  ( 2 <<  4 )
#define CHSR_LBERR                  ( 1 <<  4 )

#define CHSR_MxERR_mask             ( 3 <<  2 )

#define CHSR_MOERR                  ( 3 <<  2 )
#define CHSR_MRERR                  ( 2 <<  2 )
#define CHSR_MBERR                  ( 1 <<  2 )

#define CHSR_DxERR_mask             ( 3 <<  0 )

#define CHSR_DOERR                  ( 3 <<  0 )
#define CHSR_DRERR                  ( 2 <<  0 )
#define CHSR_DBERR                  ( 1 <<  0 )

/* Memory Control Registers */

#define mite_MCR( ch )              ( board->mite + 0x50C + 0x100 * ( ch ) )
#define MCRPON                      0

/* Memory Address Registers */

#define mite_MAR( ch )              ( board->mite + 0x510 + 0x100 * ( ch ) )

/* Device Control Registers */

#define mite_DCR( ch )              ( board->mite + 0x514 + 0x100 * ( ch ) )

#define DCR_NORMAL                  ( 1 << 29 )
#define DCRPON                      0

/* Device Address Registers */

#define mite_DAR( ch )              ( board->mite + 0x518 + 0x100 * ( ch ) )

/* Link Control Registers */

#define mite_LKCR( ch )             ( board->mite + 0x51C + 0x100 * ( ch ) )

/* Link Address Registers */

#define mite_LKAR( ch )             ( board->mite + 0x520 + 0x100 * ( ch ) )

#define mite_LLKAR( ch )            ( board->mite + 0x524 + 0x100 * ( ch ) )
#define mite_BAR( ch )		    ( board->mite + 0x528 + 0x100 * ( ch ) )
#define mite_BCR( ch )		    ( board->mite + 0x52C + 0x100 * ( ch ) )
#define mite_SAR( ch )		    ( board->mite + 0x530 + 0x100 * ( ch ) )
#define mite_WSCR( ch )		    ( board->mite + 0x534 + 0x100 * ( ch ) )
#define mite_WSER( ch )		    ( board->mite + 0x538 + 0x100 * ( ch ) )

/* FIFO Control Registers */

#define mite_FCR( ch )              ( board->mite + 0x540 + 0x100 * ( ch ) )

#define MITE_FIFO                   0x80
#define MITE_FIFOEND                0xff

#define MITE_AMRAM                  0x00
#define MITE_AMDEVICE               0x01
#define MITE_AMHOST_A32_SINGLE      0x09
#define MITE_AMHOST_A24_SINGLE      0x39
#define MITE_AMHOST_A16_SINGLE      0x29
#define MITE_AMHOST_A32_BLOCK       0x0b
#define MITE_AMHOST_A32D64_BLOCK    0x08
#define MITE_AMHOST_A24_BLOCK       0x3B


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
