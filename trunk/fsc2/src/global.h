/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#if ! defined FSC2_GLOBAL
#define FSC2_GLOBAL

#include <sys/socket.h>


/* Define the maximum number of instances of fsc2 that can run at the
   same time (this number must be lower than SOMAXCONN, i.e. the
   maximum backlog value in calls of listen(2). */

#define FSC2_MAX_INSTANCES 16

#if FSC2_MAX_INSTANCES > SOMAXCONN
#error ***********************************************
#error *   Maximum number of instances of fsc2 is    *
#error *   larger than SOMAXCONN, see src/global.h   *
#error ***********************************************
#endif


/* Define the name of fsc2's (Unix domain) socket file via which other
   programs can send EDL scripts */

#define FSC2_SOCKET  "/tmp/fsc2.uds"


/* Define the name of fsc2d's (Unix domain) socket file that is used to
   control the different instances of fsc2 */

#define FSC2D_SOCKET  "/tmp/fsc2d.uds"


/* The maximum length send via sockets by fsc2 */

#define MAX_LINE_LENGTH    4096


/* Define number of colors to be used in 2D graphics - must be less than
   FL_MAX_COLORS - (FL_FREE_COL1 + 3), i.e. not more than about 997. On
   the other hand, the sum of all colors shouldn't exceed the number of
   colors available on the system (and should leave some color slots for
   other processes, so don't ask for more than about the half of the
   number of available colors...) */

#define NUM_COLORS 128


/* The colors used in 2D graphics are also needed when exporting the current
   state of the experiment via the webserver. The lookup is done via a hash
   and the size of the hash is defined here - it should be about ten times
   the total numbers of colors (which is ( NUM_COLORS + 26) and it *MUST* be
   a prime number! */

#define COLOR_HASH_SIZE 1543


/* Sone definitions used for the program display browser */

#define TAB_LENGTH        4
#define MAXLINE        4096


/******************************************************************/
/* The following definitions are for internal use in the program, */
/* so don't change them if its not really necessary (and I mean   */
/* really, really necessary !)                                    */
/******************************************************************/


/* Create a bool type - this is stolen from gcc's stdbool.h ;-)
   When compiling with a C++ compiler this must not be defined! */

#ifndef __cplusplus

typedef enum
{
	  false = 0,
	  true  = 1
} bool;

#endif

#define FAIL    false
#define UNSET   false
#if ! defined FALSE
#define FALSE   false
#endif
#define OK      true
#define SET     true
#if ! defined TRUE
#define TRUE    true
#endif


/* Define some useful abbreviations - never ever change these ! */

#define UNDEFINED -1

enum {
	PREPARATION = 0,
	TEST,
	EXPERIMENT
};


/* States to be returned to the HTTP server */

enum {
	STATE_IDLE = 0,
	STATE_RUNNING,
	STATE_WAITING,
	STATE_FINISHED
};


/* Define error severity types */

enum {
	FATAL = 0,
	SEVERE,
	WARN,
	NO_ERROR
};


/* Enumeration for command line flags */

enum {
	GEOMETRY = 0,
	BROWSERFONTSIZE,
	DISPLAYGEOMETRY,
	DISPLAY1DGEOMETRY,
	DISPLAY2DGEOMETRY,
	CUTGEOMETRY,
	TOOLGEOMETRY,
	AXISFONT,
	TOOLBOXFONTSIZE,
	FILESELFONTSIZE,
	HELPFONTSIZE,
	STOPMOUSEBUTTON,
	NOCRASHMAIL,
	RESOLUTION,
	HTTPPORT
};


/* Definition for HIGH/LOW states */

enum {
	LOW = 0,
	HIGH
};


/* Definitions for slopes (e.g. trigger slopes etc.) */

enum {
	NEGATIVE = 0,
	POSITIVE
};


/* Definition for trigger modes etc. */

enum {
	INTERNAL = 0,
	EXTERNAL
};


/* Definition for level types */

enum {
	TTL_LEVEL = 0,
	ECL_LEVEL
};


/* Some definitions used in graphic */

enum {
	X = 0,
	Y,
	Z
};


enum {
	RED = 0,
	GREEN,
	BLUE
};


/* Define the different section types */

enum {
	NO_SECTION = ( int ) OK + 1,
	DEVICES_SECTION,
	ASSIGNMENTS_SECTION,
	VARIABLES_SECTION,
	PHASES_SECTION,
	PREPARATIONS_SECTION,
	EXPERIMENT_SECTION
};


/* Define names for often used digitizer and pulser channels - if more get
   added put them before the very last element and don't forget also to
   update the array with the channel names defined in global.c. */

enum {
	CHANNEL_INVALID = -1,
	CHANNEL_CH1,
	CHANNEL_CH2,
	CHANNEL_CH3,
	CHANNEL_CH4,
	CHANNEL_MATH1,
	CHANNEL_MATH2,
	CHANNEL_MATH3,
	CHANNEL_REF1,
	CHANNEL_REF2,
	CHANNEL_REF3,
	CHANNEL_REF4,
	CHANNEL_AUX,
	CHANNEL_AUX1,
	CHANNEL_AUX2,
	CHANNEL_LINE,
	CHANNEL_MEM_C,
	CHANNEL_MEM_D,
	CHANNEL_FUNC_E,
	CHANNEL_FUNC_F,
	CHANNEL_EXT,
	CHANNEL_EXT10,
	CHANNEL_CH0,
	CHANNEL_CH5,
	CHANNEL_CH6,
	CHANNEL_CH7,
	CHANNEL_CH8,
	CHANNEL_CH9,
	CHANNEL_CH10,
	CHANNEL_CH11,
	CHANNEL_CH12,
	CHANNEL_CH13,
	CHANNEL_CH14,
	CHANNEL_CH15,
	CHANNEL_DEFAULT_SOURCE,
	CHANNEL_SOURCE_0,
	CHANNEL_SOURCE_1,
	CHANNEL_SOURCE_2,
	CHANNEL_SOURCE_3,
	CHANNEL_NEXT_GATE,
	CHANNEL_TIMEBASE_1,
	CHANNEL_TIMEBASE_2,
	CHANNEL_CH16,
	CHANNEL_CH17,
	CHANNEL_CH18,
	CHANNEL_CH19,
	CHANNEL_CH20,
	CHANNEL_CH21,
	CHANNEL_CH22,
	CHANNEL_CH23,
	CHANNEL_CH24,
	CHANNEL_CH25,
	CHANNEL_CH26,
	CHANNEL_CH27,
	CHANNEL_CH28,
	CHANNEL_CH29,
	CHANNEL_CH30,
	CHANNEL_CH31,
	CHANNEL_CH32,
	CHANNEL_CH33,
	CHANNEL_CH34,
	CHANNEL_CH35,
	CHANNEL_A0,
	CHANNEL_A1,
	CHANNEL_A2,
	CHANNEL_A3,
	CHANNEL_A4,
	CHANNEL_A5,
	CHANNEL_A6,
	CHANNEL_A7,
	CHANNEL_A8,
	CHANNEL_A9,
	CHANNEL_A10,
	CHANNEL_A11,
	CHANNEL_A12,
	CHANNEL_A13,
	CHANNEL_A14,
	CHANNEL_A15,
	CHANNEL_B0,
	CHANNEL_B1,
	CHANNEL_B2,
	CHANNEL_B3,
	CHANNEL_B4,
	CHANNEL_B5,
	CHANNEL_B6,
	CHANNEL_B7,
	CHANNEL_B8,
	CHANNEL_B9,
	CHANNEL_B10,
	CHANNEL_B11,
	CHANNEL_B12,
	CHANNEL_B13,
	CHANNEL_B14,
	CHANNEL_B15,
	CHANNEL_C0,
	CHANNEL_C1,
	CHANNEL_C2,
	CHANNEL_C3,
	CHANNEL_C4,
	CHANNEL_C5,
	CHANNEL_C6,
	CHANNEL_C7,
	CHANNEL_C8,
	CHANNEL_C9,
	CHANNEL_C10,
	CHANNEL_C11,
	CHANNEL_C12,
	CHANNEL_C13,
	CHANNEL_C14,
	CHANNEL_C15,
	CHANNEL_D0,
	CHANNEL_D1,
	CHANNEL_D2,
	CHANNEL_D3,
	CHANNEL_D4,
	CHANNEL_D5,
	CHANNEL_D6,
	CHANNEL_D7,
	CHANNEL_D8,
	CHANNEL_D9,
	CHANNEL_D10,
	CHANNEL_D11,
	CHANNEL_D12,
	CHANNEL_D13,
	CHANNEL_D14,
	CHANNEL_D15,
	CHANNEL_E0,
	CHANNEL_E1,
	CHANNEL_E2,
	CHANNEL_E3,
	CHANNEL_E4,
	CHANNEL_E5,
	CHANNEL_E6,
	CHANNEL_E7,
	CHANNEL_E8,
	CHANNEL_E9,
	CHANNEL_E10,
	CHANNEL_E11,
	CHANNEL_E12,
	CHANNEL_E13,
	CHANNEL_E14,
	CHANNEL_E15,
	CHANNEL_F0,
	CHANNEL_F1,
	CHANNEL_F2,
	CHANNEL_F3,
	CHANNEL_F4,
	CHANNEL_F5,
	CHANNEL_F6,
	CHANNEL_F7,
	CHANNEL_F8,
	CHANNEL_F9,
	CHANNEL_F10,
	CHANNEL_F11,
	CHANNEL_F12,
	CHANNEL_F13,
	CHANNEL_F14,
	CHANNEL_F15,
	CHANNEL_G0,
	CHANNEL_G1,
	CHANNEL_G2,
	CHANNEL_G3,
	CHANNEL_G4,
	CHANNEL_G5,
	CHANNEL_G6,
	CHANNEL_G7,
	CHANNEL_G8,
	CHANNEL_G9,
	CHANNEL_G10,
	CHANNEL_G11,
	CHANNEL_G12,
	CHANNEL_G13,
	CHANNEL_G14,
	CHANNEL_G15,
	CHANNEL_H0,
	CHANNEL_H1,
	CHANNEL_H2,
	CHANNEL_H3,
	CHANNEL_H4,
	CHANNEL_H5,
	CHANNEL_H6,
	CHANNEL_H7,
	CHANNEL_H8,
	CHANNEL_H9,
	CHANNEL_H10,
	CHANNEL_H11,
	CHANNEL_H12,
	CHANNEL_H13,
	CHANNEL_H14,
	CHANNEL_H15,
	CHANNEL_TRIG_OUT,
	CHANNEL_EXP_A,
	CHANNEL_EXP_B,
	CHANNEL_TRACE_A,
	CHANNEL_TRACE_B,
	CHANNEL_TRACE_C,
	CHANNEL_TRACE_D,
	CHANNEL_M1,
	CHANNEL_M2,
	CHANNEL_M3,
	CHANNEL_M4,
	NUM_CHANNEL_NAMES
};


/* Define the generic type name for pulsers */

#define PULSER_GENERIC_TYPE "pulser"


/* Define the different functions pulses may have - if more get added put them
   before the very last entry and don't forget to also update the array of
   function names (plus the lexers dealing with channel names). */

enum {
	PULSER_CHANNEL_NO_TYPE = -1,
	PULSER_CHANNEL_MW,
	PULSER_CHANNEL_PULSE_SHAPE,
	PULSER_CHANNEL_PHASE_1,
	PULSER_CHANNEL_PHASE_2,
    PULSER_CHANNEL_TWT,
	PULSER_CHANNEL_TWT_GATE,
	PULSER_CHANNEL_DEFENSE,
	PULSER_CHANNEL_DET,
	PULSER_CHANNEL_DET_GATE,
	PULSER_CHANNEL_RF,
	PULSER_CHANNEL_RF_GATE,
	PULSER_CHANNEL_OTHER_1,
	PULSER_CHANNEL_OTHER_2,
	PULSER_CHANNEL_OTHER_3,
	PULSER_CHANNEL_OTHER_4,
	PULSER_CHANNEL_NUM_FUNC
};


/* Define access types for functions in certain sections */

enum {
	ACCESS_PREP = 1,         /* only in preparation phase */
	ACCESS_EXP  = 2,         /*	only in experiment phase */
	ACCESS_ALL  = 3          /*	everywhere */
};


/* Defines the types of phases used in phase cycling - don't ever change the
   ordering, if necessary add more just before the last entry (and don't
   forget to also extend the array of of phase type names). */

enum {
	PHASE_PLUS_X = 0,           /* this must always be 0 ! */
	PHASE_MINUS_X,
	PHASE_PLUS_Y,
	PHASE_MINUS_Y,
	NUM_PHASE_TYPES
};


/* Define the acquisition types used in phase cycling */

enum {
	ACQ_PLUS_U = 0,
	ACQ_MINUS_U,
	ACQ_PLUS_A,
	ACQ_MINUS_A,
	ACQ_PLUS_B,
	ACQ_MINUS_B
};


/* Define a structure used in analyzing the input program */

typedef struct Compilation Compilation_T;

struct Compilation {
	/* Number of errors detected during compilation (FATAL, SEVERE and WARN) */

	int error[ 3 ];

	/* Indicates which sections have already been handled to detect
	   multiple instances of the same section */

	bool sections[ EXPERIMENT_SECTION + 1 ];
};


/* Access directions of pipe */

enum {
	READ = 0,
	WRITE
};


enum {
	PARENT = 0,
	CHILD  = 1
};


/* Flags set according to the command line arguments */

enum {
	DO_LOAD       = ( 1 <<  0 ),
	DO_TEST       = ( 1 <<  1 ),
	DO_START      = ( 1 <<  2 ),
	DO_SIGNAL     = ( 1 <<  3 ),
	DO_DELETE     = ( 1 <<  4 ),
	NO_MAIL       = ( 1 <<  5 ),
	NO_BALLOON    = ( 1 <<  6 ),
	NON_EXCLUSIVE = ( 1 <<  7 ),
	BATCH_MODE    = ( 1 <<  8 ),
	DO_CHECK      = ( 1 <<  9 ),                /* used for check runs only */
	TEST_ONLY     = ( 1 << 10 ),
	NO_GUI_RUN    = ( 1 << 11 ),
	ICONIFIED_RUN = ( 1 << 12 )
};


/* States the program can be in when the main child process raised the
   QUITTING signal */

enum {
	QUITTING_UNSET,
	QUITTING_RAISED,
	QUITTING_ACCEPTED
};


/* Convenience macros for accessing the value of simple variables */

#define INT          val.lval
#define FLOAT        val.dval
#define STRING       val.sptr
#define VALUE( a )   ( ( a )->type == INT_VAR ?             \
                       ( a )->val.lval : ( a )->val.dval )


/* Return codes from function get_lib_symbol() (in func.c) */

enum {
	LIB_OK          =  0,        /*	everything fine */
	LIB_ERR_NO_LIB  = -1,        /*	library not found */
	LIB_ERR_NO_SYM  = -2         /*	symbol not found in library */
};


/* The following number is the start number for numbers designating digitizer
   windows. It should be large enough to give functions at least a fighting
   chance to decide if a number is possibly a window number. (Actual window
   numbers start from WINDOW_START_NUMBER + 1.) */

#define WINDOW_START_NUMBER  172438


/* The following number is the start number for numbers designating file
   identifiers. Valid file numbers start from FILE_NUMBER_OFFSET + 2, the
   first two ones are for stdout and stderr. */

#define FILE_NUMBER_OFFSET    215927
#define FILE_NUMBER_NOT_OPEN  ( FILE_NUMBER_OFFSET - 1 )
#define FILE_NUMBER_STDOUT    FILE_NUMBER_OFFSET
#define FILE_NUMBER_STDERR    ( FILE_NUMBER_OFFSET + 1 )


/* This constants are used in converting integer and double numbers to short
   integers as expected by the X library routines as point coordinates */

#define SHRT_MAX_HALF ( SHRT_MAX / 2 )
#define SHRT_MIN_HALF ( SHRT_MIN / 2 )


/* In case of a crash we want to send out a mail including the program counter
   where the crash happened. Within the signal handler the program counter
   is stored at the following offset from the content of the EBP register
   (of course, this only will work with a i386 type processor and only with
   gcc of the versions where I tested it...) */

#define CRASH_ADDRESS_OFFSET 0x11


/* This constant is the start number for the IDs of objects in the toolbox */

#define ID_OFFSET             328102


/* The following defines the states the function for waiting for an user
   event from the toolbox can be in. */

enum {
	TB_WAIT_NOT_RUNNING,
	TB_WAIT_RUNNING_WITH_NO_TIMER,
	TB_WAIT_TIMER_EXPIRED,
	TB_WAIT_TIMER_RUNNING
};


/* Having "UNUSED_ARG" after an argument of a function that is never needed
   will keep gcc from complaining about it and make it easier to see that
   the argument isn't used on purpose. "UNUSED_ARGUMENT" has the same function
   (but needs to be put into the body of the function) and is mostly used in
   legacy code or some places where it would be much harder to use the other
   method. */

#if defined __GNUC__
#define UNUSED_ARG __attribute__ ((unused))
#else
#define UNUSED_ARG
#endif

#define UNUSED_ARGUMENT( a )   ( ( void ) ( a ) )


/* Macro for calculation the number of elements of an array (but take care,
   it must be a real array, it won't work with just a pointer to an array!) */

#define NUM_ELEMS( arr ) ( sizeof( arr ) / sizeof *( arr ) )


#endif /* ! FSC2_GLOBAL */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
