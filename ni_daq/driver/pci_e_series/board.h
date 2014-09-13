/*
 *  Driver for National Instruments PCI E Series DAQ boards
 * 
 *  Copyright (C) 2003-2014 Jens Thoms Toerring
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


#if ! defined PCI_E_SERIES_BOARD_H
#define PCI_E_SERIES_BOARD_H


#include "io.h"
#include "mite.h"


#if ! defined ( CONFIG_PCI )
#error "PCI support in kernel is missing."
#endif


#define PCI_VENDOR_ID_NATINST          0x1093


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 9 )
#define iowrite8( v, a )   writeb( v, a )
#define iowrite16( v, a )  writew( v, a )
#define iowrite32( v, a )  writel( v, a )
#define ioread8(  a )      readb( a )
#define ioread16( a )      readw( a )
#define ioread32( a )      readl( a )
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION( 2, 6, 9 )
struct Register_Addresses {
	/* Misc Register Group */

	u8  *Serial_Command;
	u8  *Misc_Command;
	u8  *Status;

	/* Analog Input Register Group */

	u16 *ADC_FIFO_Data_Register;
	u16 *Configuration_Memory_Low;
	u16 *Configuration_Memory_High;

	/* Analog Output Register Group */

	u16 *AO_Configuration;
	u16 *DAC_FIFO_Data;
	u16 *DAC0_Direct_data;
	u16 *DAC1_Direct_data;

	/* DMA Control Register Group */

	u8  *AI_AO_Select;
	u8  *G0_G1_Select;

	/* DAQ_STC Register Group */

	u16 *Window_Address;
	u16 *Window_Data;
	u16 *Interrupt_A_Acknowledge;
	u16 *Interrupt_B_Acknowledge;
	u16 *AI_Command_2;
	u16 *AO_Command_2;
	u16 *Gi_Command[ 2 ];
	u16 *AI_Status_1;
	u16 *AO_Status_1;
	u16 *G_status;
	u16 *AI_Status_2;
	u16 *AO_Status_2;
	u16 *DIO_Parallel_Input;
};
#else
struct Register_Addresses {
	/* Misc Register Group */

	void __iomem  *Serial_Command;             /*  8 bit */
	void __iomem  *Misc_Command;               /*  8 bit */
	void __iomem  *Status;                     /*  8 bit */

	/* Analog Input Register Group */

	void __iomem *ADC_FIFO_Data_Register;      /* 16 bit */
	void __iomem *Configuration_Memory_Low;    /* 16 bit */
	void __iomem *Configuration_Memory_High;   /* 16 bit */

	/* Analog Output Register Group */

	void __iomem *AO_Configuration;            /* 16 bit */
	void __iomem *DAC_FIFO_Data;               /* 16 bit */
	void __iomem *DAC0_Direct_data;            /* 16 bit */
	void __iomem *DAC1_Direct_data;            /* 16 bit */

	/* DMA Control Register Group */

	void __iomem  *AI_AO_Select;               /*  8 bit */
	void __iomem  *G0_G1_Select;               /*  8 bit */

	/* DAQ_STC Register Group */

	void __iomem *Window_Address;              /* 16 bit */
	void __iomem *Window_Data;		   /* 16 bit */
	void __iomem *Interrupt_A_Acknowledge;	   /* 16 bit */
	void __iomem *Interrupt_B_Acknowledge;	   /* 16 bit */
	void __iomem *AI_Command_2;		   /* 16 bit */
	void __iomem *AO_Command_2;		   /* 16 bit */
	void __iomem *Gi_Command[ 2 ];		   /* 16 bit */
	void __iomem *AI_Status_1;		   /* 16 bit */
	void __iomem *AO_Status_1;		   /* 16 bit */
	void __iomem *G_status;			   /* 16 bit */
	void __iomem *AI_Status_2;		   /* 16 bit */
	void __iomem *AO_Status_2;		   /* 16 bit */
	void __iomem *DIO_Parallel_Input;          /* 16 bit */ 
};
#endif


/* Serial_Command */

#define SerDacLd2                (    1 <<  5 )
#define SerDacLd1                (    1 <<  4 )
#define SerDacLd0                (    1 <<  3 )
#define EEPromCS                 (    1 <<  2 )
#define SerData                  (    1 <<  1 )
#define SerClk                   (    1 <<  0 )

#define SerDacLd_Shift           3


/* Misc_Command */

#define Int_Ext_Trig             (    1 <<  7 )


/* Status */

#define PROMOUT                  (    1 <<  0 )


/* Configuration_Memory_Low */

#define LastChan                 (    1 << 15 )
#define GenTrig                  (    1 << 12 )
#define DitherEn                 (    1 <<  9 )
#define Unip_Bip                 (    1 <<  8 )
#define Gain_Field               (    7 <<  0 )


#define Gain_0_5                 0
#define Gain_1                   1
#define Gain_2                   2
#define Gain_5                   3
#define Gain_10                  4
#define Gain_20                  5
#define Gain_50                  6
#define Gain_100                 7


/* Configuration_Memory_High */

#define ChanType_Field           (    7 << 12 )
#define Bank_Field               (    2 <<  4 )
#define Chan_Field               ( 0x1F <<  0 )

#define ChanType_Shift           7
#define Bank_Shift               4

#define ChanType_Calibration     0
#define ChanType_Differential    1         /* differential */
#define ChanType_NRSE            2         /* non-referenced single-ended */
#define ChanType_RSE             3         /* referenced single-ended */
#define ChanType_Aux             5
#define ChanType_Ghost           7


/* AO_Configuration */

#define DACSel                   (    1 <<  8 )
#define GroundRef                (    1 <<  3 )
#define ExtRef                   (    1 <<  2 )
#define ReGlitch                 (    1 <<  1 )
#define BipDac                   (    1 <<  0 )


/* AI_AO_Select */

#define Output_D                 (    1 <<  7 )
#define Output_C                 (    1 <<  6 )
#define Output_B                 (    1 <<  5 )
#define Output_A                 (    1 <<  4 )
#define Input_D                  (    1 <<  3 )
#define Input_C                  (    1 <<  2 )
#define Input_B                  (    1 <<  1 )
#define Input_A                  (    1 <<  0 )


/* G0_G1_Select */

#define GPCT1_D                  (    1 <<  7 )
#define GPCT1_C                  (    1 <<  6 )
#define GPCT1_B                  (    1 <<  5 )
#define GPCT1_A                  (    1 <<  4 )
#define GPCT0_D                  (    1 <<  3 )
#define GPCT0_C                  (    1 <<  2 )
#define GPCT0_B                  (    1 <<  1 )
#define GPCT0_A                  (    1 <<  0 )


/* Functions from board.c - all must be globally visible and may not
   contain any reference to the underlying hardware */

void pci_board_register_setup( Board * /* board */ );

void pci_board_irq_handling_setup( Board * /* board */ );

void pci_board_reset_all( Board * /* board */ );

void pci_clear_configuration_memory( Board * /* board */ );

void pci_clear_ADC_FIFO( Board * /* board */ );

void pci_clear_DAC_FIFO( Board * /* board */ );

int pci_configuration_high( Board *        /* board        */,
			    unsigned int   /* channel      */, 
			    NI_DAQ_AI_TYPE /* channel_type */ );

int pci_configuration_low( Board *              /* board            */,
			   unsigned int         /* last_channel     */,
			   NI_DAQ_STATE         /* generate_trigger */,
			   NI_DAQ_STATE         /* dither_enable    */,
			   NI_DAQ_BU_POLARITY   /* polarity         */,
			   NI_DAQ_AI_GAIN_TYPES /* gain             */ );

int pci_ao_configuration( Board *            /* board        */,
			  unsigned int       /* channel      */,
			  NI_DAQ_STATE       /* ground_ref   */,
			  NI_DAQ_STATE       /* external_ref */,
			  NI_DAQ_STATE       /* reglitch     */,
			  NI_DAQ_BU_POLARITY /* polarity     */ );

int pci_dac_direct_data( Board *      /* board   */,
			 unsigned int /* channel */,
			 int          /* value   */ );

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 19 )
irqreturn_t pci_board_irq_handler( int              /* irq   */,
				   void *           /* data  */ );
#elif LINUX_VERSION_CODE >= KERNEL_VERSION( 2, 6, 0 )
irqreturn_t pci_board_irq_handler( int              /* irq   */,
				   void *           /* data  */,
				   struct pt_regs * /* dummy */ );
#else
void pci_board_irq_handler( int              /* irq   */,
			    void *           /* data  */,
			    struct pt_regs * /* dummy */ );
#endif


/* Functions from mite.c that must be globally visible (and thus don't
   contain direct references to the MITE, i.e. the underlying hardware) */

int pci_dma_setup( Board *          /* board      */,
		   NI_DAQ_SUBSYSTEM /* sub_system */ );

int pci_dma_buf_setup( Board *          /* board       */,
		       NI_DAQ_SUBSYSTEM /* sub_system  */,
		       size_t           /* data_points */,
		       int              /* continuous  */ );

int pci_dma_buf_get( Board *          /* board      */,
		     NI_DAQ_SUBSYSTEM /* sys        */,
		     void __user *    /* dest       */,
		     size_t *         /* size       */,
		     int              /* still_used */ );

size_t pci_dma_get_available( Board *          /* board */,
			      NI_DAQ_SUBSYSTEM /* sys   */ );

void pci_dma_buf_release( Board *          /* board */,
			  NI_DAQ_SUBSYSTEM /* sys   */ );

int pci_dma_shutdown( Board *          /* board */,
		      NI_DAQ_SUBSYSTEM /* sys   */ );


/* Functions from caldac.c that must be globally visible */

void pci_set_trigger_levels( Board * /* board */,
			     u16     /* high  */,
			     u16     /* low   */ );

void caldac_calibrate( Board * /* board */ );


#endif /* PCI_E_SERIES_BOARD_H */


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
