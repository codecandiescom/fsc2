/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#if ! defined FSC2_GLOBAL
#define FSC2_GLOBAL


/* Define the name of fsc2's lock file */

#define FSC2_LOCKFILE "/tmp/fsc2.lock"


/* Define the name of fsc2's (Unix domain) socket file */

#define FSC2_SOCKET  "/tmp/fsc2.uds"


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
   When compiling with a C++ compiler this must not be defined ! */

#ifndef __cplusplus

typedef enum
{
	  false = 0,
	  true  = 1
} bool;

#endif

#define FAIL    false
#define UNSET   false
#define FALSE   false
#define OK      true
#define SET     true
#define TRUE    true


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
	BUTTONFONTSIZE,
	INPUTFONTSIZE,
	LABELFONTSIZE,
	DISPLAYGEOMETRY,
	CUTGEOMETRY,
	TOOLGEOMETRY,
	AXISFONT,
	CHOICEFONTSIZE,
	SLIDERFONTSIZE,
	FILESELFONTSIZE,
	HELPFONTSIZE,
	STOPMOUSEBUTTON,
	NOCRASHMAIL,
	RESOLUTION,
	HTTPPORT
};

/* Definition for HIGH/LOW states */

enum {
	LOW,
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


/* Define names for often used digitizer channels - if more get added put
   them before the very last element and don't forget also to update the
   array with the channel names. */

enum {
	DIGITIZER_CHANNEL_INVALID = -1,
	DIGITIZER_CHANNEL_CH1,
	DIGITIZER_CHANNEL_CH2,
	DIGITIZER_CHANNEL_CH3,
	DIGITIZER_CHANNEL_CH4,
	DIGITIZER_CHANNEL_MATH1,
	DIGITIZER_CHANNEL_MATH2,
	DIGITIZER_CHANNEL_MATH3,
	DIGITIZER_CHANNEL_REF1,
	DIGITIZER_CHANNEL_REF2,
	DIGITIZER_CHANNEL_REF3,
	DIGITIZER_CHANNEL_REF4,
	DIGITIZER_CHANNEL_AUX,
	DIGITIZER_CHANNEL_AUX1,
	DIGITIZER_CHANNEL_AUX2,
	DIGITIZER_CHANNEL_LINE,
	DIGITIZER_CHANNEL_MEM_C,
	DIGITIZER_CHANNEL_MEM_D,
	DIGITIZER_CHANNEL_FUNC_E,
	DIGITIZER_CHANNEL_FUNC_F,
	DIGITIZER_CHANNEL_EXT,
	DIGITIZER_CHANNEL_EXT10,
	NUM_DIGITIZER_CHANNEL_NAMES
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
	PHASE_PLUS_X = 0,
	PHASE_MINUS_X,
	PHASE_PLUS_Y,
	PHASE_MINUS_Y,
	PHASE_CW,
	PHASE_TYPES_MAX
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

typedef struct
{
	/* Number of errors detected during compilation (FATAL, SEVERE and WARN) */

	int error[ 3 ];

	/* Indicates which sections have already been handled to detect
	   multiple instances of the same section */

	bool sections[ EXPERIMENT_SECTION + 1 ];

} Compilation;


/* Access directions of pipe */

enum {
	READ = 0,
	WRITE
};


enum {
	PARENT = 0,
	CHILD  = 1
};

enum {
	DO_LOAD    =   1,
	DO_TEST    =   2,
	DO_START   =   4,
	DO_SIGNAL  =   8,
	DO_DELETE  =  16,
	NO_MAIL    =  32,
	NO_BALLOON =  64,
	DO_CHECK   = 128                  /* used for check runs on new versions */
};

/* Convenience macros for accessing the value of simple variables
   (but be careful with the VALUE macro: the returned value is always
   a double !) */

#define INT          val.lval
#define FLOAT        val.dval
#define STRING       val.sptr
#define VALUE( a )   ( ( a )->type == INT_VAR ?            \
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
   identifiers. Valid file numbers start from FILE_NUMBER_OFFSET. */

#define FILE_NUMBER_OFFSET    215927
#define FILE_NUMBER_NOT_OPEN  ( FILE_NUMBER_OFFSET - 1 )


/* This constants are used in converting integer and double numbers to short
   integers as expected by the X library routines as point coordinates */

#define SHRT_MAX_HALF ( SHRT_MAX >> 1 )
#define SHRT_MIN_HALF ( SHRT_MIN >> 1 )


#endif /* ! FSC2_GLOBAL */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
