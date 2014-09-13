/*
 *  Driver for National Instruments DAQ boards based on a DAQ-STC
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


/* Structure for storing copies of data written to write-only registers */


typedef struct STC_Write_Registers STC_Write_Registers;

struct STC_Write_Registers {
	u16 Window_Address;
	u16 Window_Data_Write	   __attribute__ ( ( packed ) );
	u16 Interrupt_A_Ack	   __attribute__ ( ( packed ) );
	u16 Interrupt_B_Ack	   __attribute__ ( ( packed ) );
	u16 AI_Command_2	   __attribute__ ( ( packed ) );
	u16 AO_Command_2	   __attribute__ ( ( packed ) );
	u16 Gi_Command[ 2 ]	   __attribute__ ( ( packed ) );
	u16 AI_Command_1	   __attribute__ ( ( packed ) );
	u16 AO_Command_1	   __attribute__ ( ( packed ) );
	u16 DIO_Output		   __attribute__ ( ( packed ) );
	u16 DIO_Control		   __attribute__ ( ( packed ) );
	u16 AI_Mode_1		   __attribute__ ( ( packed ) );
	u16 AI_Mode_2		   __attribute__ ( ( packed ) );
	u32 AI_SI_Load_A	   __attribute__ ( ( packed ) );
	u32 AI_SI_Load_B	   __attribute__ ( ( packed ) );
	u32 AI_SC_Load_A	   __attribute__ ( ( packed ) );
	u32 AI_SC_Load_B	   __attribute__ ( ( packed ) );
	u16 unused_22		   __attribute__ ( ( packed ) );
	u16 AI_SI2_Load_A	   __attribute__ ( ( packed ) );
	u16 unused_24		   __attribute__ ( ( packed ) );
	u16 AI_SI2_Load_B	   __attribute__ ( ( packed ) );
	u16 Gi_Mode[ 2 ]	   __attribute__ ( ( packed ) );
	u32 G0_Load_A		   __attribute__ ( ( packed ) );
	u32 G0_Load_B		   __attribute__ ( ( packed ) );
	u32 G1_Load_A		   __attribute__ ( ( packed ) );
	u32 G1_Load_B		   __attribute__ ( ( packed ) );
	u16 Gi_Input_Select[ 2 ]   __attribute__ ( ( packed ) );
	u16 AO_Mode_1		   __attribute__ ( ( packed ) );
	u16 AO_Mode_2		   __attribute__ ( ( packed ) );
	u32 AO_UI_Load_A	   __attribute__ ( ( packed ) );
	u32 AO_UI_Load_B	   __attribute__ ( ( packed ) );
	u32 AO_BC_Load_A	   __attribute__ ( ( packed ) );
	u32 AO_BC_Load_B	   __attribute__ ( ( packed ) );
	u32 AO_UC_Load_A	   __attribute__ ( ( packed ) );
	u32 AO_UC_Load_B	   __attribute__ ( ( packed ) );
	u16 unused_52		   __attribute__ ( ( packed ) );
	u16 AO_UI2_Load_A	   __attribute__ ( ( packed ) );
	u16 unused_54		   __attribute__ ( ( packed ) );
	u16 AO_UI2_Load_B	   __attribute__ ( ( packed ) );
	u16 Clock_and_FOUT	   __attribute__ ( ( packed ) );
	u16 IO_Bidirection_Pin	   __attribute__ ( ( packed ) );
	u16 RTSI_Trig_Direction    __attribute__ ( ( packed ) );
	u16 Interrupt_Control	   __attribute__ ( ( packed ) );
	u16 AI_Output_Control	   __attribute__ ( ( packed ) );
	u16 Analog_Trigger_Etc	   __attribute__ ( ( packed ) );
	u16 AI_START_STOP_Select   __attribute__ ( ( packed ) );
	u16 AI_Trigger_Select	   __attribute__ ( ( packed ) );
	u16 AI_DIV_Load_A	   __attribute__ ( ( packed ) );
	u16 unused_65		   __attribute__ ( ( packed ) );
	u16 AO_START_Select	   __attribute__ ( ( packed ) );
	u16 AO_Trigger_Select	   __attribute__ ( ( packed ) );
	u16 Gi_Autoincrement[ 2 ]  __attribute__ ( ( packed ) );
	u16 AO_Mode_3		   __attribute__ ( ( packed ) );
	u16 Generic_Control	   __attribute__ ( ( packed ) );
	u16 Joint_Reset		   __attribute__ ( ( packed ) );
	u16 Interrupt_A_Enable	   __attribute__ ( ( packed ) );
	u16 Second_Irq_A_Enable    __attribute__ ( ( packed ) );
	u16 Interrupt_B_Enable	   __attribute__ ( ( packed ) );
	u16 Second_Irq_B_Enable    __attribute__ ( ( packed ) );
	u16 AI_Personal		   __attribute__ ( ( packed ) );
	u16 AO_Personal		   __attribute__ ( ( packed ) );
	u16 RTSI_Trig_A_Output	   __attribute__ ( ( packed ) );
	u16 RTSI_Trig_B_Output	   __attribute__ ( ( packed ) );
	u16 RTSI_Board		   __attribute__ ( ( packed ) );
	u16 Write_Strobe_0	   __attribute__ ( ( packed ) );
	u16 Write_Strobe_1	   __attribute__ ( ( packed ) );
	u16 Write_Strobe_2	   __attribute__ ( ( packed ) );
	u16 Write_Strobe_3	   __attribute__ ( ( packed ) );
	u16 AO_Output_Control	   __attribute__ ( ( packed ) );
	u16 AI_Mode_3              __attribute__ ( ( packed ) );
};


/* Offsets of the DAQ-STC registers (in units of 2 bytes) */

#define STC_Window_Address                     0       /* w    */
#define STC_Window_Data_Write                  1       /* w    */
#define STC_Window_Data_Read                   1       /* r    */
#define STC_Interrupt_A_Ack                    2       /* w    */
#define STC_AI_Status_1                        2       /* r    */
#define STC_Interrupt_B_Ack                    3       /* w    */
#define STC_AO_Status_1                        3       /* r    */
#define STC_AI_Command_2                       4       /* w    */
#define STC_G_Status                           4       /* r    */
#define STC_AO_Command_2                       5       /* w    */
#define STC_AI_Status_2                        5       /* r    */
#define STC_G0_Command                         6       /* w    */
#define STC_AO_Status_2                        6       /* r    */
#define STC_G1_Command                         7       /* w    */
#define STC_DIO_Parallel_Input                 7       /* r    */
#define STC_AI_Command_1                       8       /* w    */
#define STC_G0_HW_Save                         8       /* r, 2 */
#define STC_AO_Command_1                       9       /* w    */
#define STC_DIO_Output                        10       /* w    */
#define STC_G1_HW_Save                        10       /* r, 2 */
#define STC_DIO_Control                       11       /* w    */
#define STC_AI_Mode_1                         12       /* w    */
#define STC_G0_Save                           12       /* r, 2 */
#define STC_AI_Mode_2                         13       /* w    */
#define STC_AI_SI_Load_A                      14       /* w, 2 */
#define STC_G1_Save                           14       /* r, 2 */
#define STC_AI_SI_Load_B                      16       /* w, 2 */
#define STC_AO_UI_Save                        16       /* r, 2 */
#define STC_AI_SC_Load_A                      18       /* w, 2 */
#define STC_AO_BC_Save                        18       /* r, 2 */
#define STC_AI_SC_Load_B                      20       /* w, 2 */
#define STC_AO_UC_Save                        20       /* r, 2 */
#define STC_AI_SI2_Load_A                     23       /* w    */
#define STC_SO_UI2_Save                       23       /* r    */
#define STC_AI_SI2_Load_B                     25       /* w    */
#define STC_AI_SI2_Save                       25       /* r    */
#define STC_G0_Mode                           26       /* w    */
#define STC_AI_DIV_Save                       26       /* r    */
#define STC_G1_Mode                           27       /* w    */
#define STC_Joint_Status_1                    27       /* r    */
#define STC_G0_Load_A                         28       /* w, 2 */
#define STC_DIO_Serial_Input                  28       /* r    */
#define STC_Joint_Status_2                    29       /* r    */
#define STC_G0_Load_B                         30       /* w, 2 */
#define STC_G1_Load_A                         32       /* w, 2 */
#define STC_G1_Load_B                         34       /* w, 2 */
#define STC_G0_Input_Select                   36       /* w    */
#define STC_G1_Input_Select                   37       /* w    */
#define STC_AO_Mode_1                         38       /* w    */
#define STC_AO_Mode_2                         39       /* w    */
#define STC_AO_UI_Load_A                      40       /* w, 2 */
#define STC_AO_UI_Load_B                      42       /* w, 2 */
#define STC_AO_BC_Load_A                      44       /* w, 2 */
#define STC_AO_BC_Load_B                      46       /* w, 2 */
#define STC_AO_UC_Load_A                      48       /* w, 2 */
#define STC_AO_UC_Load_B                      50       /* w, 2 */
#define STC_AO_UI2_Load_A                     53       /* w    */
#define STC_AO_UI2_Load_B                     55       /* w    */
#define STC_Clock_and_FOUT                    56       /* w    */
#define STC_IO_Bidirection_Pin                57       /* w    */
#define STC_RTSI_Trig_Direction               58       /* w    */
#define STC_Interrupt_Control                 59       /* w    */
#define STC_AI_Output_Control                 60       /* w    */
#define STC_Analog_Trigger_Etc                61       /* w    */
#define STC_AI_START_STOP_Select              62       /* w    */
#define STC_AI_Trigger_Select                 63       /* w    */
#define STC_AI_DIV_Load_A                     64       /* w    */
#define STC_AI_SI_Save                        64       /* r, 2 */
#define STC_AO_START_Select                   66       /* w    */
#define STC_AI_SC_Save                        66       /* r, 2 */
#define STC_AO_Trigger_Select                 67       /* w    */
#define STC_G0_Autoincrement                  68       /* w    */
#define STC_G1_Autoincrement                  69       /* w    */
#define STC_AO_Mode_3                         70       /* w    */
#define STC_Generic_Control                   71       /* w    */
#define STC_Joint_Reset                       72       /* w    */
#define STC_Interrupt_A_Enable                73       /* w    */
#define STC_Second_Irq_A_Enable               74       /* w    */
#define STC_Interrupt_B_Enable                75       /* w    */
#define STC_Second_Irq_B_Enable               76       /* w    */
#define STC_AI_Personal                       77       /* w    */
#define STC_AO_Personal                       78       /* w    */
#define STC_RTSI_Trig_A_Output                79       /* w    */
#define STC_RTSI_Trig_B_Output                80       /* w    */
#define STC_RTSI_Board                        81       /* w    */
#define STC_Write_Strobe_0                    82       /* w    */
#define STC_Write_Strobe_1                    83       /* w    */
#define STC_Write_Strobe_2                    84       /* w    */
#define STC_Write_Strobe_3                    85       /* w    */
#define STC_AO_Output_Control                 86       /* w    */
#define STC_AI_Mode_3                         87       /* w    */


#define STC_Gi_Command( i )                   ( ( i ) ?  7 :  6 )
#define STC_Gi_HW_Save( i )                   ( ( i ) ? 10 :  8 )
#define STC_Gi_Save( i )                      ( ( i ) ? 14 : 12 )
#define STC_Gi_Mode( i )                      ( ( i ) ? 27 : 26 )
#define STC_Gi_Load_A( i )                    ( ( i ) ? 32 : 28 )
#define STC_Gi_Load_B( i )                    ( ( i ) ? 34 : 30 )
#define STC_Gi_Input_Select( i )              ( ( i ) ? 37 : 36 )
#define STC_Gi_Autoincrement( i )             ( ( i ) ? 69 : 68 )



/* Relevant bits of the DAQ-STC registers */


/* AI_Command_1 (offset: 8, width: 1, type: write-only) */

#define AI_Analog_Trigger_Reset               (        1 << 14 )   /* Strobe */
#define AI_Disarm                             (        1 << 13 )   /* Strobe */
#define AI_SI2_Arm                            (        1 << 12 )   /* Strobe */
#define AI_SI2_Load                           (        1 << 11 )   /* Strobe */
#define AI_SI_Arm                             (        1 << 10 )   /* Strobe */
#define AI_SI_Load                            (        1 <<  9 )   /* Strobe */
#define AI_DIV_Arm                            (        1 <<  8 )   /* Strobe */
#define AI_DIV_Load                           (        1 <<  7 )   /* Strobe */
#define AI_SC_Arm                             (        1 <<  6 )   /* Strobe */
#define AI_SC_Load                            (        1 <<  5 )   /* Strobe */
#define AI_SCAN_IN_PROG_Pulse                 (        1 <<  4 )
#define AI_EXTMUX_CLK_Pulse                   (        1 <<  3 )   /* Strobe */
#define AI_LOCALMUX_CLK_Pulse                 (        1 <<  2 )   /* Strobe */
#define AI_SC_TC_Pulse                        (        1 <<  1 )
#define AI_CONVERT_Pulse                      (        1 <<  0 )   /* Strobe */


/* AI_Command_2 (offset: 4, width: 1, type: write-only) */

#define AI_End_On_SC_TC                       (        1 << 15 )   /* Strobe */
#define AI_End_On_End_Of_Scan                 (        1 << 14 )   /* Strobe */
#define AI_START1_Disable                     (        1 << 11 )
#define AI_SC_Save_Trace                      (        1 << 10 )
#define AI_SI_Switch_Load_On_SC_TC            (        1 <<  9 )   /* Strobe */
#define AI_SI_Switch_Load_On_STOP             (        1 <<  8 )   /* Strobe */
#define AI_SI_Switch_Load_On_TC               (        1 <<  7 )   /* Strobe */
#define AI_SC_Switch_Load_On_TC               (        1 <<  4 )   /* Strobe */
#define AI_STOP_Pulse                         (        1 <<  3 )   /* Strobe */
#define AI_START_Pulse                        (        1 <<  2 )   /* Strobe */
#define AI_START2_Pulse                       (        1 <<  1 )   /* Strobe */
#define AI_START1_Pulse                       (        1 <<  0 )   /* Strobe */


/* AI_DIV_Load A (offset: 64, width: 1, type: write-only) */

#define AI_DIV_Load_A_Field                   (   0xFFFF <<  0 )


/* AI_DIV_Save (offset: 26, width: 1, type: read-only) */

#define AI_DIV_Save_Value_Field               (   0xFFFF <<  0 )


/* AI_Mode_1 (offset: 12, width: 1, type: write-only) */

#define AI_CONVERT_Source_Select_Field        (     0x1F << 11 )
#define AI_SI_Source_Select_Field             (     0x1F <<  6 )
#define AI_CONVERT_Source_Polarity            (        1 <<  5 )
#define AI_SI_Source_Polarity                 (        1 <<  4 )
#define AI_Start_Stop                         (        1 <<  3 )
#define Reserved_One                          (        1 <<  2 )
#define AI_Continuous                         (        1 <<  1 )
#define AI_Trigger_Once                       (        1 <<  0 )

#define AI_CONVERT_Source_Select_Shift        11
#define AI_SI_Source_Select_Shift             6


/* AI_Mode_2 (offset: 13, width: 1, type: write-only) */

#define AI_SC_Gate_Enable                     (        1 << 15 )
#define AI_Start_Stop_Gate_Enable             (        1 << 14 )
#define AI_Pre_Trigger                        (        1 << 13 )
#define AI_External_MUX_Present               (        1 << 12 )
#define AI_SI2_Initial_Load_Source            (        1 <<  9 )
#define AI_SI2_Reload_Mode                    (        1 <<  8 )
#define AI_SI_Initial_Load_Source             (        1 <<  7 )
#define AI_SI_Reload_Mode_Field               (        7 <<  4 )
#define AI_SI_Write_Switch                    (        1 <<  3 )
#define AI_SC_Initial_Load_Source             (        1 <<  2 )
#define AI_SC_Reload_Mode                     (        1 <<  1 )
#define AI_SC_Write_Switch                    (        1 <<  0 )

#define AI_SI_Reload_Mode_Shift                4


/* AI_Mode_3 (offset: 87, width: 1, type: write-only) */

#define AI_Trigger_Length                     (        1 << 15 )
#define AI_Delay_START                        (        1 << 14 )
#define AI_Software_Gate                      (        1 << 13 )
#define AI_SI_Special_Trigger_Delay           (        1 << 12 )
#define AI_SI2_Source_Select                  (        1 << 11 )
#define AI_Delayed_START2                     (        1 << 10 )
#define AI_Delayed_START1                     (        1 <<  9 )
#define AI_External_Gate_Mode                 (        1 <<  8 )
#define AI_FIFO_Mode_Field                    (        3 <<  6 )
#define AI_External_Gate_Polarity             (        1 <<  5 )
#define AI_External_Gate_Select_Field         (     0x1F <<  0 )

#define AI_FIFO_Mode_Shift                     6


/* AI_Output_Control (offset: 60, width: 1, type: write-only) */

#define AI_START_Output_Select                (        1 << 10 )
#define AI_SCAN_IN_PROG_Output_Select_Field   (        3 <<  8 )
#define AI_EXTMUX_CLK_Output_Select_Field     (        3 <<  6 )
#define AI_LOCALMUX_CLK_Output_Select_Field   (        3 <<  4 )
#define AI_SC_TC_Output_Select_Field          (        3 <<  2 )
#define AI_CONVERT_Output_Select_Field        (        3 <<  0 )

#define AI_SCAN_IN_PROG_Output_Select_Shift    8
#define AI_EXTMUX_CLK_Output_Select_Shift      6
#define AI_LOCALMUX_CLK_Output_Select_Shift    4
#define AI_SC_TC_Output_Select_Shift           2
#define AI_CONVERT_Output_Select_Shift         0


/* AI_Personal (offset: 77, width: 1, type: write-only) */

#define AI_SHIFTIN_Pulse_Width                (        1 << 15 )
#define AI_EOC_Polarity                       (        1 << 14 )
#define AI_SOC_Polarity                       (        1 << 13 )
#define AI_SHIFTIN_Polarity                   (        1 << 12 )
#define AI_CONVERT_Pulse_Timebase             (        1 << 11 )
#define AI_CONVERT_Pulse_Width                (        1 << 10 )
#define AI_CONVERT_Original_Pulse             (        1 <<  9 )
#define AI_FIFO_Flags_Polarity                (        1 <<  8 )
#define AI_Overrun_Mode                       (        1 <<  7 )
#define AI_EXTMUX_CLK_Pulse_Width             (        1 <<  6 )
#define AI_LOCALMUX_CLK_Pulse_Width           (        1 <<  5 )
#define AI_AIFREQ_Polarity                    (        1 <<  4 )


/* AI_SC_Load_A (offset: 18, width: 2, type: write-only) */

#define AI_SC_Load_A_Field                    ( 0xFFFFFF <<  0 )


/* AI_SC_Load_B (offset: 20, width: 2, type: write-only) */

#define AI_SC_Load_B_Field                    ( 0xFFFFFF <<  0 )


/* AI_SC_Save (offset: 66, width: 2, type: read-only) */

#define AI_SC_Save_Value_Field                ( 0xFFFFFF <<  0 )


/* AI_SI_Load_A (offset: 14, width: 2, type: write-only) */

#define AI_SI_Load_A_Field                    ( 0xFFFFFF <<  0 )


/* AI_SI_Load_B (offset: 16, width: 2, type: write-only) */

#define AI_SI_Load_B_Field                    ( 0xFFFFFF <<  0 )


/* AI_SI_Save (offset: 68, width: 2, type: read-only) */

#define AI_SI_Save_Value_Field                ( 0xFFFFFF <<  0 )


/* AI_SI2_Load_A (offset: 23, width: 1, type: write-only) */

#define AI_SI2_Load_A_Field                   (   0xFFFF <<  0 )


/* AI_SI2_Load_B (offset: 25, width: 1, type: write-only) */

#define AI_SI2_Load_A_Field                   (   0xFFFF <<  0 )


/* AI_SI2_Save (offset: 25, width: 1, type: read-only) */

#define AI_SI2_Save_Field                     (   0xFFFF <<  0 )


/* AI_START_STOP_Select (offset: 62, width: 1, type: write-only) */

#define AI_START_Polarity                     (        1 << 15 )
#define AI_STOP_Polarity                      (        1 << 14 )
#define AI_STOP_Sync                          (        1 << 13 )
#define AI_STOP_Edge                          (        1 << 12 )
#define AI_STOP_Select_Field                  (     0x1F <<  7 )
#define AI_START_Sync                         (        1 <<  6 )
#define AI_START_Edge                         (        1 <<  5 )
#define AI_START_Select_Field                 (     0x1F <<  0 )

#define AI_STOP_Select_Shift                   7
#define AI_START_Select_Shift                  0


/* AI_Status_1 (offset: 2, width: 1, type: read-only) */

#define Interrupt_A_St                        (        1 << 15 )
#define AI_FIFO_Full_St                       (        1 << 14 )
#define AI_FIFO_Half_Full_St                  (        1 << 13 )
#define AI_FIFO_Empty_St                      (        1 << 12 )
#define AI_Overrun_St                         (        1 << 11 )
#define AI_Overflow_St                        (        1 << 10 )
#define AI_SC_TC_Error_St                     (        1 <<  9 )
#define AI_START2_St                          (        1 <<  8 )
#define AI_START1_St                          (        1 <<  7 )
#define AI_SC_TC_St                           (        1 <<  6 )
#define AI_START_St                           (        1 <<  5 )
#define AI_STOP_St                            (        1 <<  4 )
#define G0_TC_St                              (        1 <<  3 )
#define G0_Gate_Interrupt_St                  (        1 <<  2 )
#define AI_FIFO_Request_St                    (        1 <<  1 )
#define Pass_Thru_0_Interrupt_St              (        1 <<  0 )


/* AI_Status_2 (offset: 5, width: 1, type: read-only) */

#define Reserved_2000_St                      (        1 << 15 )
#define AI_DIV_Armed_St                       (        1 << 14 )
#define AI_DIV_Q_St                           (        1 << 13 )
#define AI_SI2_Next_Load_Source_St            (        1 << 12 )
#define AI_SI2_Armed_St                       (        1 << 11 )
#define AI_SI_Q_St                            (        1 << 10 )
#define AI_SI_Counting_St_Field               (        3 <<  8 )
#define AI_SI_Next_Load_Source_St             (        1 <<  6 )
#define AI_SI_Armed_St                        (        1 <<  5 )
#define AI_SC_Q_St_Field                      (        3 <<  3 )
#define AI_SC_Save_St                         (        1 <<  2 )
#define AI_SC_Next_Load_Source_St             (        1 <<  1 )
#define AI_SC_Armed_St                        (        1 <<  0 )

#define AI_SI_Counting_St_Shift               10
#define AI_SC_Q_St_Shift                       3


/* AI_Trigger_Select (offset: 63, width: 1, type: write-only) */

#define AI_START1_Polarity                    (        1 << 15 )
#define AI_START2_Polarity                    (        1 << 14 )
#define AI_START2_Sync                        (        1 << 13 )
#define AI_START2_Edge                        (        1 << 12 )
#define AI_START2_Select_Field                (     0x1F <<  7 )
#define AI_START1_Sync                        (        1 <<  6 )
#define AI_START1_Edge                        (        1 <<  5 )
#define AI_START1_Select_Field                (     0x1F <<  0 )

#define AI_START2_Select_Shift                 7
#define AI_START1_Select_Shift                 0


/* Analog_Trigger_Etc (offset: 61, width: 1, type: write-only) */

#define GPFO_1_Output_Enable                  (        1 << 15 )
#define GPFO_0_Output_Enable                  (        1 << 14 )
#define GPFO_0_Output_Select_Field            (        7 << 11 )
#define GPFO_1_Output_Select                  (        1 <<  7 )
#define Misc_Counter_TCs_Output_Enable        (        1 <<  6 )
#define Software_Test                         (        1 <<  5 )   /* Strobe */
#define Analog_Trigger_Drive                  (        1 <<  4 )
#define Analog_Trigger_Enable                 (        1 <<  3 )


#define Analog_Trigger_Mode_Field             (        7 <<  0 )
#define GPFO_0_Output_Select_Shift            11

#define GPFO_i_Output_Enable( i )             ( 1 << ( i ? 15 : 14 ) )


/* AO_BC_Load_A (offset: 44, witdh: 2, type: write-only) */

#define AO_BC_Load_A_Field                    ( 0xFFFFFF <<  0 )


/* AO_BC_Load_B (offset: 46, witdh: 2, type: write-only) */

#define AO_BC_Load_B_Field                    ( 0xFFFFFF <<  0 )


/* AO_BC_Save (offset: 18, witdh: 2, type: read-only) */

#define AO_BC_Save_Value_Field                ( 0xFFFFFF <<  0 )


/* AO_Command_1 (offset: 9, width: 1, type: write-only) */

#define AO_Analog_Trigger_Reset               (        1 << 15 )   /* Strobe */
#define AO_START_Pulse                        (        1 << 14 )   /* Strobe */
#define AO_Disarm                             (        1 << 13 )   /* Strobe */
#define AO_UI2_Arm_Disarm                     (        1 << 12 )
#define AO_UI2_Load                           (        1 << 11 )   /* Strobe */
#define AO_UI_Arm                             (        1 << 10 )   /* Strobe */
#define AO_UI_Load                            (        1 <<  9 )   /* Strobe */
#define AO_UC_Arm                             (        1 <<  8 )   /* Strobe */
#define AO_UC_Load                            (        1 <<  7 )   /* Strobe */
#define AO_BC_Arm                             (        1 <<  6 )   /* Strobe */
#define AO_BC_Load                            (        1 <<  5 )   /* Strobe */
#define AO_DAC1_Update_Mode                   (        1 <<  4 )
#define AO_LDAC1_Source_Select                (        1 <<  3 )
#define AO_DAC0_Update_Mode                   (        1 <<  2 )
#define AO_LDAC0_Source_Select                (        1 <<  1 )
#define AO_UPDATE_Pulse                       (        1 <<  0 )   /* Strobe */


/* AO_Command_2 (offset: 5, width: 1, type: write-only) */

#define AO_End_On_BC_TC                       (        1 << 15 )   /* Strobe */
#define AO_End_On_UC_TC                       (        1 << 14 )   /* Strobe */
#define AO_Start_Stop_Gate_Enable             (        1 << 13 )
#define AO_UC_Save_Trace                      (        1 << 12 )
#define AO_BC_Gate_Enable                     (        1 << 11 )
#define AO_BC_Save_Trace                      (        1 << 10 )
#define AO_UI_Switch_Load_On_BC_TC            (        1 <<  9 )   /* Strobe */
#define AO_UI_Switch_Load_On_Stop             (        1 <<  8 )   /* Strobe */
#define AO_UI_Switch_Load_On_TC               (        1 <<  7 )   /* Strobe */
#define AO_UC_Switch_Load_On_BC_TC            (        1 <<  6 )   /* Strobe */
#define AO_UC_Switch_Load_On_TC               (        1 <<  5 )   /* Strobe */
#define AO_BC_Switch_Load_On_TC               (        1 <<  4 )   /* Strobe */
#define AO_Mute_B                             (        1 <<  3 )
#define AO_Mute_A                             (        1 <<  2 )
#define AO_UPDATE2_Pulse                      (        1 <<  1 )   /* Strobe */
#define AO_START1_Pulse                       (        1 <<  0 )   /* Strobe */


/* AO_Mode_1 (offset: 38, width: 1, type: write-only) */

#define AO_UPDATE_Source_Select_Field         (     0x1F << 11 )
#define AO_UI_Source_Select_Field             (     0x1F <<  6 )
#define AO_Multiple_Channels                  (        1 <<  5 )
#define AO_UPDATE_Source_Polarity             (        1 <<  4 )
#define AO_UI_Source_Polarity                 (        1 <<  3 )
#define AO_UC_Switch_Load_Every_TC            (        1 <<  2 )
#define AO_Continuous                         (        1 <<  1 )
#define AO_Trigger_Once                       (        1 <<  0 )

#define AO_UPDATE_Source_Select_Shift         11
#define AO_UI_Source_Select_Shift              6


/* AO_Mode_2 (offset: 39, width: 1, type: write-only) */

#define AO_FIFO_Mode_Field                    (        3 << 14 )
#define AO_FIFO_Retransmit_Enable             (        1 << 13 )
#define AO_START1_Disable                     (        1 << 12 )
#define AO_UC_Initial_Load_Source             (        1 << 11 )
#define AO_UC_Write_Switch                    (        1 << 10 )
#define AO_UI2_Initial_Load_Source            (        1 <<  9 )
#define AO_UI2_Reload_Mode                    (        1 <<  8 )
#define AO_UI_Initial_Load_Source             (        1 <<  7 )
#define AO_UI_Reload_Mode_Field               (        7 <<  4 )
#define AO_UI_Write_Switch                    (        1 <<  3 )
#define AO_BC_Initial_Load_Source             (        1 <<  2 )
#define AO_BC_Reload_Mode                     (        1 <<  1 )
#define AO_BC_Write_Switch                    (        1 <<  0 )

#define AO_FIFO_Mode_Shift                    14
#define AO_UI_Reload_Mode_Shift                4


/* AO_Mode_3 (offset: 70, width: 1, type: write-only) */

#define AO_UI2_Switch_Load_Next_TC            (        1 << 13 )   /* Strobe */
#define AO_UC_Switch_Load_Every_BC_TC         (        1 << 12 )
#define AO_Trigger_Length                     (        1 << 11 )
#define AO_Stop_On_Overrun_Error              (        1 <<  5 )
#define AO_Stop_On_BC_TC_Trigger_Error        (        1 <<  4 )
#define AO_Stop_On_BC_TC_Error                (        1 <<  3 )
#define AO_Not_An_UPDATE                      (        1 <<  2 )
#define AO_Software_Gate                      (        1 <<  1 )


/* AO_Output_Control (offset: 86, width: 1, type: write-only) */

#define AO_External_Gate_Enable               (        1 << 15 )
#define AO_External_Gate_Select_Field         (     0x1F << 10 )
#define AO_Number_Of_Channels_Field           (      0xF <<  6 )
#define AO_UPDATE2_Output_Select_Field        (        3 <<  4 )
#define AO_External_Gate_Polarity             (        1 <<  3 )
#define AO_UPDATE2_Output_Toggle              (        1 <<  2 )
#define AO_UPDATE_Output_Select_Field         (        3 <<  0 )

#define AO_External_Gate_Select_Shift         10
#define AO_Number_Of_Channels_Shift            6
#define AO_UPDATE2_Output_Select_Shift         4


/* AO_Personal (offset: 78, width: 1, type: write-only) */

#define AO_Number_Of_DAC_Packages             (        1 << 14 )
#define AO_Fast_CPU                           (        1 << 13 )
#define AO_TMRDACWR_Pulse_Width               (        1 << 12 )
#define AO_FIFO_Flags_Polarity                (        1 << 11 )
#define AO_FIFO_Enable                        (        1 << 10 )
#define AO_AOFREQ_Polarity                    (        1 <<  9 )
#define AO_DMA_PIO_Control                    (        1 <<  8 )
#define AO_UPDATE_Original_Pulse              (        1 <<  7 )
#define AO_UPDATE_Pulse_Timebase              (        1 <<  6 )
#define AO_UPDATE_Pulse_Width                 (        1 <<  5 )
#define AO_BC_Source_Select                   (        1 <<  4 )
#define AO_Interval_Buffer_Mode               (        1 <<  3 )
#define AO_UPDATE2_Original_Pulse             (        1 <<  2 )
#define AO_UPDATE2_Pulse_Timebase             (        1 <<  1 )
#define AO_UPDATE2_Pulse_Width                (        1 <<  0 )


/* AO_START_Select (offset: 66, width: 1, type: write-only) */

#define AO_UI2_Software_Gate                  (        1 << 15 )
#define AO_UI2_External_Gate_Polarity         (        1 << 14 )
#define AO_START_Polarity                     (        1 << 13 )
#define AO_AOFREQ_Enable                      (        1 << 12 )
#define AO_UI2_External_Gate_Select_Field     (     0x1F <<  7 )
#define AO_START_Sync                         (        1 <<  6 )
#define AO_START_Edge                         (        1 <<  5 )
#define AO_START_Select_Field                 (     0x1F <<  0 )

#define AO_UI2_External_Gate_Select_Shift     7


/* AO_Status_1 (offset: 3, width: 1, type: read-only) */

#define Interrupt_B_St                        (        1 << 15 )
#define AO_FIFO_Full_St                       (        1 << 14 )
#define AO_FIFO_Half_Full_St                  (        1 << 13 )
#define AO_FIFO_Empty_St                      (        1 << 12 )
#define AO_BC_TC_Error_St                     (        1 << 11 )
#define AO_START_St                           (        1 << 10 )
#define AO_Overrun_St                         (        1 <<  9 )
#define AO_START1_St                          (        1 <<  8 )
#define AO_BC_TC_St                           (        1 <<  7 )
#define AO_UC_TC_St                           (        1 <<  6 )
#define AO_UPDATE_St                          (        1 <<  5 )
#define AO_UI2_TC_St                          (        1 <<  4 )
#define G1_TC_St                              (        1 <<  3 )
#define G1_Gate_Interrupt_St                  (        1 <<  2 )
#define AO_FIFO_Request_St                    (        1 <<  1 )
#define Pass_Thru_1_Interrupt_St              (        1 <<  0 )


/* AO_Status_2 (offset: 6, width: 1, type: read-only) */

#define AO_UC_Next_Load_Source_St             (        1 << 15 )
#define AO_UC_Armed_St                        (        1 << 14 )
#define AO_UI2_Counting_St                    (        1 << 13 )
#define AO_UI2_Next_Load_Source_St            (        1 << 12 )
#define AO_UI2_Armed_St                       (        1 << 11 )
#define AO_UI2_TC_Error_St                    (        1 << 10 )
#define AO_UI_Q_St                            (        1 <<  9 )
#define AO_UI_Counting_St                     (        1 <<  8 )
#define AO_UC_Save_St                         (        1 <<  7 )
#define AO_UI_Next_Load_Source_St             (        1 <<  6 )
#define AO_UI_Armed_St                        (        1 <<  5 )
#define AO_BC_TC_Trigger_Error_St             (        1 <<  4 )
#define AO_BC_Q_St                            (        1 <<  3 )
#define AO_BC_Save_St                         (        1 <<  2 )
#define AO_BC_Next_Load_Source_St             (        1 <<  1 )
#define AO_BC_Armed_St                        (        1 <<  0 )


/* AO_Trigger_Select (offset: 67, width: 1, type: write-only) */

#define AO_UI2_External_Gate_Enable           (        1 << 15 )
#define AO_Delayed_START1                     (        1 << 14 )
#define AO_START1_Polarity                    (        1 << 13 )
#define AO_UI2_Source_Polarity                (        1 << 12 )
#define AO_UI2_Source_Select_Field            (     0x1F <<  7 )
#define AO_START1_Sync                        (        1 <<  6 )
#define AO_START1_Edge                        (        1 <<  5 )
#define AO_START1_Select_Field                (     0x1F <<  0 )

#define AO_UI2_Source_Select_Shift             7


/* AO_UC_Load_A (offset: 48, width: 2, type: write-only) */

#define AO_UC_Load_A_Field                    ( 0xFFFFFF <<  0 )


/* AO_UC_Load_B (offset: 50, width: 2, type: write-only) */

#define AO_UC_Load_B_Field                    ( 0xFFFFFF <<  0 )


/* AO_UC_Save (offset: 20, width: 2, type: read-only) */

#define AO_UC_Save_Value_Field                ( 0xFFFFFF <<  0 )


/* AO_UI2_Load_A (offset: 53, width: 1, type: write-only) */

#define AO_UI2_Load_A_Field                   (   0xFFFF <<  0 )


/* AO_UI2_Load_B (offset: 55, width: 1, type: write-only) */

#define AO_UI2_Load_B_Field                   (   0xFFFF <<  0 )


/* AO_UI2_Save (offset: 23, width: 1, type: read-only) */

#define AO_UI2_Save_Value_Field               (   0xFFFF <<  0 )


/* AO_UI_Load_A (offset: 40, width: 2, type: write-only) */

#define AO_UI_Load_A_Field                    ( 0xFFFFFF <<  0 )


/* AO_UI_Load_B (offset: 42, width: 2, type: write-only) */

#define AO_UI_Load_B_Field                    ( 0xFFFFFF <<  0 )


/* AO_UI_Save (offset: 16, width: 2, type: read-only) */

#define AO_UI_Save_Value_Field                ( 0xFFFFFF <<  0 )


/* Clock_and_FOUT (offset: 56, width: 1, type: write-only) */

#define FOUT_Enable                           (        1 << 15 )
#define FOUT_Timebase_Select                  (        1 << 14 )
#define DIO_Serial_Out_Divide_By_2            (        1 << 13 )
#define Slow_Internal_Time_Divide_By_2        (        1 << 12 )
#define Slow_Internal_Timebase                (        1 << 11 )
#define G_Source_Divide_By_2                  (        1 << 10 )
#define Clock_To_Board_Divide_By_2            (        1 <<  9 )
#define Clock_To_Board                        (        1 <<  8 )
#define AI_Output_Divide_By_2                 (        1 <<  7 )
#define AI_Source_Divide_By_2                 (        1 <<  6 )
#define AO_Output_Divide_By_2                 (        1 <<  5 )
#define AO_Source_Divide_By_2                 (        1 <<  4 )
#define FOUT_Divider_Field                    (      0xF <<  0 )


/* DIO_Control (offset: 11, width: 1, type: write-only) */

#define DIO_Software_Serial_Control           (        1 << 11 )
#define DIO_HW_Serial_Timebase                (        1 << 10 )
#define DIO_HW_Serial_Enable                  (        1 <<  9 )
#define DIO_HW_Serial_Start                   (        1 <<  8 )   /* Strobe */
#define DIO_Pins_Dir_Field                    (     0xFF <<  0 )


/* DIO_Output (offset: 10, width: 1, type: write-only) */

#define DIO_Serial_Data_Out_Field             (     0xFF <<  8 )
#define DIO_Parallel_Data_Out_Field           (     0xFF <<  0 )

#define DIO_Serial_Data_Out_Shift              8


/* DIO_Parallel_Input (offset: 7, width: 1, type: read-only) */

#define DIO_Parallel_Data_In_St               (     0xFF <<  0 )


/* DIO_Serial_Input (offset: 28, width: 1, type: read-only) */

#define DIO_Serial_Data_In_St                 (     0xFF <<  0 )


/* G[01]_Autoincrement (offset: 68/69, width: 1, type: write-only) */

#define Gi_Autoincrement_Field                (     0xFF <<  0 )


/* G[01]_Command (offset: 6/7, width: 1, type: write-only) */

#define Gi_Disarm_Copy                        (        1 << 15 )   /* Strobe */
#define Gi_Save_Trace_Copy                    (        1 << 14 )
#define Gi_Arm_Copy                           (        1 << 13 )   /* Strobe */
#define Gi_Bank_Switch_Enable                 (        1 << 12 )
#define Gi_Bank_Switch_Mode                   (        1 << 11 )
#define Gi_Bank_Switch_Start                  (        1 << 10 )   /* Strobe */
#define Gi_Little_Big_Endian                  (        1 <<  9 )
#define Gi_Synchronized_Gate                  (        1 <<  8 )
#define Gi_Write_Switch                       (        1 <<  7 )
#define Gi_Up_Down_Field                      (        3 <<  5 )
#define Gi_Disarm                             (        1 <<  4 )   /* Strobe */
#define Gi_Analog_Trigger_Reset               (        1 <<  3 )   /* Strobe */
#define Gi_Load                               (        1 <<  2 )   /* Strobe */
#define Gi_Save_Trace                         (        1 <<  1 )
#define Gi_Arm                                (        1 <<  0 )   /* Strobe */

#define Gi_Up_Down_Shift                      5


/* G[01]_HW_Save (offset: 8/9, width: 2, type: read-only) */

#define Gi_HW_Save_Value_Field                ( 0xFFFFFF <<  0 )


/* G[01]_Input_Select (offset: 36/37, width: 1, type: write-only) */

#define Gi_Source_Polarity                    (        1 << 15 )
#define Gi_Output_Polarity                    (        1 << 14 )
#define Gi_OR_Gate                            (        1 << 13 )
#define Gi_Gate_Select_Load_Source            (        1 << 12 )
#define Gi_Gate_Select_Field                  (     0x1F <<  7 )
#define Gi_Source_Select_Field                (     0x1F <<  2 )
#define Gi_Write_Acknowledges_Irq             (        1 <<  1 )
#define Gi_Read_Acknowledges_Irq              (        1 <<  0 )

#define Gi_Gate_Select_Shift                   7
#define Gi_Source_Select_Shift                 2


/* G[01]_Load_A (offset: 28/32, width 2, type: write-only) */

#define Gi_Load_A_Field                       ( 0xFFFFFF <<  0 )


/* G[01]_Load_B (offset: 30/34, width 2, type: write-only) */

#define Gi_Load_B_Field                       ( 0xFFFFFF <<  0 )


/* G[01]_Mode (offset: 26/27, width: 1, type: write-only) */

#define Gi_Reload_Source_Switching            (        1 << 15 )
#define Gi_Loading_On_Gate                    (        1 << 14 )
#define Gi_Gate_Polarity                      (        1 << 13 )
#define Gi_Loading_On_TC                      (        1 << 12 )
#define Gi_Counting_Once_Field                (        3 << 10 )
#define Gi_Output_Mode_Field                  (        3 <<  8 )
#define Gi_Load_Source_Select                 (        1 <<  7 )
#define Gi_Stop_Mode_Field                    (        3 <<  5 )
#define Gi_Trigger_Mode_For_Edge_Gate_Field   (        3 <<  3 )
#define Gi_Gate_On_Both_Edges                 (        1 <<  2 )
#define Gi_Gating_Mode_Field                  (        3 <<  0 )

#define Gi_Counting_Once_Shift                10
#define Gi_Output_Mode_Shift                   8
#define Gi_Stop_Mode_Shift                     5
#define Gi_Trigger_Mode_For_Edge_Gate_Shift    3


/* G[01]_Save (offset: 12/14, width: 2, type: read-only) */

#define Gi_Save_Value_Field                   ( 0xFFFFFF <<  0 )


/* G_Status (offset: 4, width: 1, type: read-only) */

#define Gi_Gate_Error_St( i )                 (        1 << ( 14 + ( i ) ) )
#define Gi_TC_Error_St( i )                   (        1 << ( 12 + ( i ) ) )
#define Gi_No_Load_Between_Gates_St( i )      (        1 << ( 10 + ( i ) ) )
#define Gi_Armed_St( i )                      (        1 << (  8 + ( i ) ) )
#define Gi_Stale_Data_St( i )                 (        1 << (  6 + ( i ) ) )
#define Gi_Next_Load_Source_St( i )           (        1 << (  4 + ( i ) ) )
#define Gi_Counting_St( i )                   (        1 << (  2 + ( i ) ) )
#define Gi_Save_St( i )                       (        1 << (  0 + ( i ) ) )


/* Generic_Control (offset: 71, width: 1, type: write-only) */

#define Control_Field                         (     0xFF <<  8 )


/* Interrupt_A_Ack (offset: 2, width: 1, type: write-only) */

#define G0_Gate_Interrupt_Ack                 (        1 << 15 )   /* Strobe */
#define G0_TC_Interrupt_Ack                   (        1 << 14 )   /* Strobe */
#define AI_Error_Interrupt_Ack                (        1 << 13 )   /* Strobe */
#define AI_STOP_Interrupt_Ack                 (        1 << 12 )   /* Strobe */
#define AI_START_Interrupt_Ack                (        1 << 11 )   /* Strobe */
#define AI_START2_Interrupt_Ack               (        1 << 10 )   /* Strobe */
#define AI_START1_Interrupt_Ack               (        1 <<  9 )   /* Strobe */
#define AI_SC_TC_Interrupt_Ack                (        1 <<  8 )   /* Strobe */
#define AI_SC_TC_Error_Confirm                (        1 <<  7 )   /* Strobe */
#define G0_TC_Error_Confirm                   (        1 <<  6 )   /* Strobe */
#define G0_Gate_Error_Confirm                 (        1 <<  5 )   /* Strobe */


/* Interrupt_A_Enable (offset: 73, width: 1, type: write-only) */

#define Pass_Thru_0_Interrupt_Enable          (        1 <<  9 )
#define G0_Gate_Interrupt_Enable              (        1 <<  8 )
#define AI_FIFO_Interrupt_Enable              (        1 <<  7 )
#define G0_TC_Interrupt_Enable                (        1 <<  6 )
#define AI_Error_Interrupt_Enable             (        1 <<  5 )
#define AI_STOP_Interrupt_Enable              (        1 <<  4 )
#define AI_START_Interrupt_Enable             (        1 <<  3 )
#define AI_START2_Interrupt_Enable            (        1 <<  2 )
#define AI_START1_Interrupt_Enable            (        1 <<  1 )
#define AI_SC_TC_Interrupt_Enable             (        1 <<  0 )


/* Interrupt_B_Ack (offset: 3, width: 1, type: write-only) */

#define G1_Gate_Interrupt_Ack                 (        1 << 15 )   /* Strobe */
#define G1_TC_Interrupt_Ack                   (        1 << 14 )   /* Strobe */
#define AO_Error_Interrupt_Ack                (        1 << 13 )   /* Strobe */
#define AO_STOP_Interrupt_Ack                 (        1 << 12 )   /* Strobe */
#define AO_START_Interrupt_Ack                (        1 << 11 )   /* Strobe */
#define AO_UPDATE_Interrupt_Ack               (        1 << 10 )   /* Strobe */
#define AO_START1_Interrupt_Ack               (        1 <<  9 )   /* Strobe */
#define AO_BC_TC_Interrupt_Ack                (        1 <<  8 )   /* Strobe */
#define AO_UC_TC_Interrupt_Ack                (        1 <<  7 )   /* Strobe */
#define AO_UI2_TC_Interrupt_Ack               (        1 <<  6 )   /* Strobe */
#define AO_UI2_TC_Error_Confirm               (        1 <<  5 )   /* Strobe */
#define AO_BC_TC_Error_Confirm                (        1 <<  4 )   /* Strobe */
#define AO_BC_TC_Trigger_Error_Confirm        (        1 <<  3 )   /* Strobe */
#define G1_TC_Error_Confirm                   (        1 <<  2 )   /* Strobe */
#define G1_Gate_Error_Confirm                 (        1 <<  1 )   /* Strobe */


/* Interrupt_B_Enable (offset: 75, width: 1, type: write-only) */

#define Pass_Thru_1_Interrupt_Enable          (        1 << 11 )
#define G1_Gate_Interrupt_Enable              (        1 << 10 )
#define G1_TC_Interrupt_Enable                (        1 <<  9 )
#define AO_FIFO_Interrupt_Enable              (        1 <<  8 )
#define AO_UI2_TC_Interrupt_Enable            (        1 <<  7 )
#define AO_UC_TC_Interrupt_Enable             (        1 <<  6 )
#define AO_Error_Interrupt_Enable             (        1 <<  5 )
#define AO_STOP_Interrupt_Enable              (        1 <<  4 )
#define AO_START_Interrupt_Enable             (        1 <<  3 )
#define AO_UPDATE_Interrupt_Enable            (        1 <<  2 )
#define AO_START1_Interrupt_Enable            (        1 <<  1 )
#define AO_BC_TC_Interrupt_Enable             (        1 <<  0 )


/* Interrupt_Control (offset:59, width: 1, type: write-only) */

#define Interrupt_B_Enable_Bit                (        1 << 15 )
#define Interrupt_B_Output_Select_Field       (        7 << 12 )
#define Interrupt_A_Enable_Bit                (        1 << 11 )
#define Interrupt_A_Output_Select_Field       (        7 <<  8 )
#define Pass_Thru_1_Interrupt_Polarity        (        1 <<  3 )
#define Pass_Thru_0_Interrupt_Polarity        (        1 <<  2 )
#define Interrupt_Output_On_3_Pins            (        1 <<  1 )
#define Interrupt_Output_Polarity             (        1 <<  0 )


#define Interrupt_B_Output_Select_Shift        12
#define Interrupt_A_Output_Select_Shift         8


/* Bidirection_Pin (offset: 57, width: 1, type: write-only) */

#define BD_9_Pin_Dir                          (        1 <<  9 )
#define BD_8_Pin_Dir                          (        1 <<  8 )
#define BD_7_Pin_Dir                          (        1 <<  7 )
#define BD_6_Pin_Dir                          (        1 <<  6 )
#define BD_5_Pin_Dir                          (        1 <<  5 )
#define BD_4_Pin_Dir                          (        1 <<  4 )
#define BD_3_Pin_Dir                          (        1 <<  3 )
#define BD_2_Pin_Dir                          (        1 <<  2 )
#define BD_1_Pin_Dir                          (        1 <<  1 )
#define BD_0_Pin_Dir                          (        1 <<  0 )


/* Joint_Reset (offset: 72, width: 1, type: write-only) */

#define Software_Reset                        (        1 << 11 )
#define AO_UI2_Configuration_End              (        1 << 10 )   /* Strobe */
#define AO_Configuration_End                  (        1 <<  9 )   /* Strobe */
#define AI_Configuration_End                  (        1 <<  8 )   /* Strobe */
#define AO_UI2_Configuration_Start            (        1 <<  6 )   /* Strobe */
#define AO_Configuration_Start                (        1 <<  5 )   /* Strobe */
#define AI_Configuration_Start                (        1 <<  4 )   /* Strobe */
#define G1_Reset                              (        1 <<  3 )   /* Strobe */
#define G0_Reset                              (        1 <<  2 )   /* Strobe */
#define AO_Reset                              (        1 <<  1 )   /* Strobe */
#define AI_Reset                              (        1 <<  0 )   /* Strobe */


#define Gi_Reset( i )                         (        1 << ( 2 + ( i ) ) )


/* Joint_Status_1 (offset: 27, width: 1, type: read-only) */

#define AI_Last_Shiftin_St                    (        1 << 15 )
#define AO_UC_Q_St                            (        1 << 14 )
#define AO_UI2_Gate_St                        (        1 << 13 )
#define DIO_Serial_IO_In_Progress_St          (        1 << 12 )
#define AO_External_Gate_St                   (        1 << 11 )
#define AI_External_Gate_St                   (        1 << 10 )
#define AI_SI2_Q_St                           (        3 <<  8 )
#define AO_Start_Stop_Gate_St                 (        1 <<  7 )
#define AO_BC_Gate_St                         (        1 <<  6 )
#define AI_Start_Stop_Gate_St                 (        1 <<  5 )
#define AI_SC_Gate_St                         (        1 <<  4 )
#define G1_Gate_St                            (        1 <<  3 )
#define G0_Gate_St                            (        1 <<  2 )
#define G1_Save_St                            (        1 <<  1 )
#define G0_Save_St                            (        1 <<  0 )

#define AI_SI2_Q_St_Shift                     8


/* Joint_Status_2 (offset: 29, width: 1, type: read-only) */

#define G1_Permanent_Stale_Data_St            (        1 << 15 )
#define G0_Permanent_Stale_Data_St            (        1 << 14 )
#define G1_HW_Save_St                         (        1 << 13 )
#define G0_HW_Save_St                         (        1 << 12 )
#define Generic_Status_Field                  (      0xF <<  8 )
#define AI_Scan_In_Progress_St                (        1 <<  7 )
#define AI_Config_Memory_Empty_St             (        1 <<  6 )
#define AO_TMRDACWRs_In_Progress_St           (        1 <<  5 )
#define AI_EOC_St                             (        1 <<  4 )
#define AI_SOC_St                             (        1 <<  3 )
#define AO_STOP_St                            (        1 <<  2 )
#define G1_Output_St                          (        1 <<  1 )
#define G0_Output_St                          (        1 <<  0 )

#define Generic_Status_Shift                   8


/* RTSI_Board (offset: 81, width: 1, type: write-only) */

#define RTSI_Board_3_Pin_Dir                  (        1 << 15 )
#define RTSI_Board_2_Pin_Dir                  (        1 << 14 )
#define RTSI_Board_1_Pin_Dir                  (        1 << 13 )
#define RTSI_Board_0_Pin_Dir                  (        1 << 12 )
#define RTSI_Board_3_Output_Select_Field      (        7 <<  9 )
#define RTSI_Board_2_Output_Select_Field      (        7 <<  6 )
#define RTSI_Board_1_Output_Select_Field      (        7 <<  3 )
#define RTSI_Board_0_Output_Select_Field      (        7 <<  0 )

#define RTSI_Board_3_Output_Select_Shift       9
#define RTSI_Board_2_Output_Select_Shift       6
#define RTSI_Board_1_Output_Select_Shift       3


/* RTSI_Trig_A _Output (offset: 79, width: 1, type: write-only) */

#define RTSI_Trig_3_Output_Select_Field       (      0xF << 12 )
#define RTSI_Trig_2_Output_Select_Field       (      0xF <<  8 )
#define RTSI_Trig_1_Output_Select_Field       (      0xF <<  4 )
#define RTSI_Trig_0_Output_Select_Field       (      0xF <<  0 )

#define RTSI_Trig_3_Output_Select_Shift       12
#define RTSI_Trig_2_Output_Select_Shift        8
#define RTSI_Trig_1_Output_Select_Shift        4


/* RTSI_Trig_B _Output (offset: 80, width: 1, type: write-only) */

#define RTSI_Sub_Selection_1                  (        1 << 15 )
#define RTSI_Trig_6_Output_Select_Field       (      0xF <<  8 )
#define RTSI_Trig_5_Output_Select_Field       (      0xF <<  4 )
#define RTSI_Trig_4_Output_Select_Field       (      0xF <<  0 )

#define RTSI_Trig_6_Output_Select_Shift        8
#define RTSI_Trig_5_Output_Select_Shift        4


/* RTSI_Trig_Direction (offset: 58, width: 1, type: write-only) */

#define RTSI_Trig_6_Pin_Dir                   (        1 << 15 )
#define RTSI_Trig_5_Pin_Dir                   (        1 << 14 )
#define RTSI_Trig_4_Pin_Dir                   (        1 << 13 )
#define RTSI_Trig_3_Pin_Dir                   (        1 << 12 )
#define RTSI_Trig_2_Pin_Dir                   (        1 << 11 )
#define RTSI_Trig_1_Pin_Dir                   (        1 << 10 )
#define RTSI_Trig_0_Pin_Dir                   (        1 <<  9 )
#define RTSI_Clock_Mode_Field                 (        3 <<  0 )


/* Second_Irq_A_Enable (offset: 74, width: 1, type: write-only) */

#define Pass_Thru_0_Second_Irq_Enable         (        1 <<  9 )
#define G0_Gate_Second_Irq_Enable             (        1 <<  8 )
#define AI_FIFO_Second_Irq_Enable             (        1 <<  7 )
#define G0_TC_Second_Irq_Enable               (        1 <<  6 )
#define AI_Error_Second_Irq_Enable            (        1 <<  5 )
#define AI_STOP_Second_Irq_Enable             (        1 <<  4 )
#define AI_START_Second_Irq_Enable            (        1 <<  3 )
#define AI_START2_Second_Irq_Enable           (        1 <<  2 )
#define AI_START1_Second_Irq_Enable           (        1 <<  1 )
#define AI_SC_TC_Second_Irq_Enable            (        1 <<  0 )


/* Second_Irq_B_Enable (offset: 76, width: 1, type: write_only) */

#define Pass_Thru_1_Second_Irq_Enable         (        1 << 11 )
#define G1_Gate_Second_Irq_Enable             (        1 << 10 )
#define G1_TC_Second_Irq_Enable               (        1 <<  9 )
#define AO_FIFO_Second_Irq_Enable             (        1 <<  8 )
#define AO_UI2_TC_Second_Irq_Enable           (        1 <<  7 )
#define AO_UC_TC_Second_Irq_Enable            (        1 <<  6 )
#define AO_Error_Second_Irq_Enable            (        1 <<  5 )
#define AO_STOP_Second_Irq_Enable             (        1 <<  4 )
#define AO_START_Second_Irq_Enable            (        1 <<  3 )
#define AO_UPDATE_Second_Irq_Enable           (        1 <<  2 )
#define AO_START1_Second_Irq_Enable           (        1 <<  1 )
#define AO_BC_TC_Second_Irq_Enable            (        1 <<  0 )


/* Window_Address (offset: 0, width: 1, type: write-only) */

#define Window_Address_Field                  (   0xFFFF <<  0 )


/* Window_Data (offset: 1, width: 1, type: read/write) */

#define Window_Data_Field                     (   0xFFFF <<  0 )


/* Write_Strobe_0 (offset: 82, width: 1, type: write-only) */

#define Write_Strobe_0_Bit                    (        1 <<  0 )   /* Strobe */


/* Write_Strobe_1 (offset: 83, width: 1, type: write-only) */

#define Write_Strobe_1_Bit                    (        1 <<  0 )   /* Strobe */


/* Write_Strobe_2 (offset: 84, width: 1, type: write-only) */

#define Write_Strobe_2_Bit                    (        1 <<  0 )   /* Strobe */


/* Write_Strobe_3 (offset: 85, width: 1, type: write-only) */

#define Write_Strobe_3_Bit                    (        1 <<  0 )   /* Strobe */



/* Definitions for interrupts */

/* Interrupt group A */

#define IRQ_AI_FIFO         0
#define IRQ_Pass_Thru_0     1
#define IRQ_G0_TC           2
#define IRQ_G0_Gate         3
#define IRQ_AI_SC_TC        4
#define IRQ_AI_START1       5
#define IRQ_AI_START2       6
#define IRQ_AI_Error        7
#define IRQ_AI_STOP         8
#define IRQ_AI_START        9

/* Interrupt group B */

#define IRQ_AO_FIFO        10 
#define IRQ_Pass_Thru_1    11
#define IRQ_AO_UI2_TC      12
#define IRQ_G1_TC          13
#define IRQ_G1_Gate        14
#define IRQ_AO_BC_TC       15
#define IRQ_AO_UC_TC       16
#define IRQ_AO_START1      17
#define IRQ_AO_UPDATE      18
#define IRQ_AO_Error       19
#define IRQ_AO_STOP        20
#define IRQ_AO_START       21



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
