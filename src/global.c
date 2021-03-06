/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"

/* This file only exists to hold the definitions of several global variables
   in a single, easy to find place (but not all are defined here, others
   are defined in the files were they seem to fit in better). */

const char *Prog_Name;           /* Name the program was started with */

Internals_T Fsc2_Internals;
EDL_Info_T EDL;
Communication_T Comm;
GUI_Info_T GUI;
Crash_Info_T Crash;


bool Need_GPIB     = false;        /* Flag, set if GPIB bus is needed */
bool Need_RULBUS   = false;        /* Flag, set if RULBUS is needed */
bool Need_LAN      = false;        /* Flag, set if LAN is needed */
bool Need_MEDRIVER = false;        /* Flag, set if Meilhaus driver is needed */
bool Need_USB      = false;        /* Flag, set if libusb driver is needed */


/* If you change the following always remember to also adjust the
   enum for channel numbers in global.h accordingly! */

const char *Channel_Names[ NUM_CHANNEL_NAMES ] =
{
    "CH1",
    "CH2",
    "CH3",
    "CH4",
    "MATH1",
    "MATH2",
    "MATH3",
    "REF1",
    "REF2",
    "REF3",
    "REF4",
    "AUX",
    "AUX1",
    "AUX2",
    "LINE",
    "MEM_C",
    "MEM_D",
    "FUNC_E",
    "FUNC_F",
    "EXT",
    "EXT10",
    "CH0",
    "CH5",
    "CH6",
    "CH7",
    "CH8",
    "CH9",
    "CH10",
    "CH11",
    "CH12",
    "CH13",
    "CH14",
    "CH15",
    "DEFAULT_SOURCE",
    "SOURCE_0",
    "SOURCE_1",
    "SOURCE_2",
    "SOURCE_3",
    "NEXT_GATE",
    "TIMEBASE_1",
    "TIMEBASE_2",
    "CH16",
    "CH17",
    "CH18",
    "CH19",
    "CH20",
    "CH21",
    "CH22",
    "CH23",
    "CH24",
    "CH25",
    "CH26",
    "CH27",
    "CH28",
    "CH29",
    "CH30",
    "CH31",
    "CH32",
    "CH33",
    "CH34",
    "CH34",
    "A0",
    "A1",
    "A2",
    "A3",
    "A4",
    "A5",
    "A6",
    "A7",
    "A8",
    "A9",
    "A10",
    "A11",
    "A12",
    "A13",
    "A14",
    "A15",
    "B0",
    "B1",
    "B2",
    "B3",
    "B4",
    "B5",
    "B6",
    "B7",
    "B8",
    "B9",
    "B10",
    "B11",
    "B12",
    "B13",
    "B14",
    "B15",
    "C0",
    "C1",
    "C2",
    "C3",
    "C4",
    "C5",
    "C6",
    "C7",
    "C8",
    "C9",
    "C10",
    "C11",
    "C12",
    "C13",
    "C14",
    "C15",
    "D0",
    "D1",
    "D2",
    "D3",
    "D4",
    "D5",
    "D6",
    "D7",
    "D8",
    "D9",
    "D10",
    "D11",
    "D12",
    "D13",
    "D14",
    "D15",
    "E0",
    "E1",
    "E2",
    "E3",
    "E4",
    "E5",
    "E6",
    "E7",
    "E8",
    "E9",
    "E10",
    "E11",
    "E12",
    "E13",
    "E14",
    "E15",
    "F0",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "F13",
    "F14",
    "F15",
    "G0",
    "G1",
    "G2",
    "G3",
    "G4",
    "G5",
    "G6",
    "G7",
    "G8",
    "G9",
    "G10",
    "G11",
    "G12",
    "G13",
    "G14",
    "G15",
    "H0",
    "H1",
    "H2",
    "H3",
    "H4",
    "H5",
    "H6",
    "H7",
    "H8",
    "H9",
    "H10",
    "H11",
    "H12",
    "H13",
    "H14",
    "H15",
    "TRIG_OUT",
    "EXP_A",
    "EXP_B",
    "TRACE_A",
    "TRACE_B",
    "TRACE_C",
    "TRACE_D",
    "M1",
    "M2",
    "M3",
    "M4",
    "MATH",
    "Z1",
    "Z2",
    "Z3",
    "Z4",
    "MATH4"
};


Pulser_Struct_T *Pulser_Struct = NULL;
long Cur_Pulser = -1;

/* If you change the following always remember to also adjust the
   enum for pulser function numbers in global.h accordingly! */

const char *Function_Names[ PULSER_CHANNEL_NUM_FUNC ] =
{
    "MW",
    "PULSE_SHAPE",
    "PHASE_1",
    "PHASE_2",
    "TWT",
    "TWT_GATE",
    "DEFENSE",
    "DETECTION",
    "DETECTION_GATE",
    "RF",
    "RF_GATE",
    "LASER",
    "OTHER_1",
    "OTHER_2",
    "OTHER_3",
    "OTHER_4",
};


/* If you change the following always remember to also adjust the
   enum for phase type numbers in global.h accordingly! */

const char *Phase_Types[ NUM_PHASE_TYPES ] =
{
    "+X",
    "-X",
    "+Y",
    "-Y",
};


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
