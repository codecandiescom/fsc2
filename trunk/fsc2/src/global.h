/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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
   FL_MAX_COLORS - FL_FREE_COL1 + 3, i.e. not more than about 1005. On
   the other hand, the sum of all colors shouldn't exceed the number of
   colors available on the system (and should leave some color slots for
   other processes, so don't ask for more than about the half of the
   number of available colors...) */

#define NUM_COLORS 128


/* Sone definitions used for the program display browser */

#define TAB_LENGTH        4
#define MAXLINE        4096


/******************************************************************/
/* The following definitions are for internal use in the program, */
/* so don't change them if its not really necessary (and I mean   */
/* really, really necessary !)                                    */
/******************************************************************/


/* make bool synonymous to unsigned char */

typedef  unsigned char  bool;


/* Define some useful abbreviations - never ever change these ! */

#define	FAIL  ( ( bool ) 0 )      
#define UNSET ( ( bool ) 0 )
#define FALSE ( ( bool ) 0 )
#define OK    ( ( bool ) 1 )
#define SET   ( ( bool ) 1 )
#define TRUE  ( ( bool ) 1 )

#define UNDEFINED -1

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
	RESOLUTION
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


/* Define the different functions pulses may have (it's not dangerous here to
   add further ones as long as the first two and the last remain unchanged) */

enum {
	PULSER_CHANNEL_NO_TYPE = -1,
	PULSER_CHANNEL_MW = 0,
    PULSER_CHANNEL_TWT,
	PULSER_CHANNEL_TWT_GATE,
	PULSER_CHANNEL_DET,
	PULSER_CHANNEL_DET_GATE,
	PULSER_CHANNEL_DEFENSE,
	PULSER_CHANNEL_RF,
	PULSER_CHANNEL_RF_GATE,
	PULSER_CHANNEL_PULSE_SHAPE,
	PULSER_CHANNEL_PHASE_1,
	PULSER_CHANNEL_PHASE_2,
	PULSER_CHANNEL_OTHER_1,
	PULSER_CHANNEL_OTHER_2,
	PULSER_CHANNEL_OTHER_3,
	PULSER_CHANNEL_OTHER_4
};


#if defined ( FSC2_MAIN )
const char *Function_Names[ ] = { "MW", "TWT", "TWT_GATE", "DETECTION",
								  "DETECTION_GATE", "DEFENSE", "RF", "RF_GATE",
								  "PULSE_SHAPE", "PHASE_1", "PHASE_2",
								  "OTHER_1", "OTHER_2", "OTHER_3", "OTHER_4" };
#else
extern const char *Function_Names[ ];
#endif

#define PULSER_CHANNEL_FUNC_MIN PULSER_CHANNEL_MW
#define PULSER_CHANNEL_FUNC_MAX PULSER_CHANNEL_OTHER_4
#define PULSER_CHANNEL_NUM_FUNC \
                      ( PULSER_CHANNEL_FUNC_MAX - PULSER_CHANNEL_FUNC_MIN + 1)


/* Define access types for functions in certain sections */

enum {
	ACCESS_PREP = 1,         // only in preparation phase
	ACCESS_EXP  = 2,         // only in experiment phase
	ACCESS_ALL  = 3          // everywhere
};


/* Defines the types of phases used in phase cycling - don't ever change the
   ordering, just add more to the end ! */

enum {
	PHASE_PLUS_X,
	PHASE_MINUS_X,
	PHASE_PLUS_Y,
	PHASE_MINUS_Y,
	PHASE_CW
};


#define PHASE_TYPES_MIN  PHASE_PLUS_X
#define PHASE_TYPES_MAX  PHASE_CW

#if defined ( FSC2_MAIN )
const char *Phase_Types[ ] = { "+X", "-X", "+Y", "-Y", "CW" };
#else
extern const char *Phase_Types[ ];
#endif


/* Define the acquisition types used in phase cycling */

enum {
	ACQ_PLUS_U,
	ACQ_MINUS_U,
	ACQ_PLUS_A,
	ACQ_MINUS_A,
	ACQ_PLUS_B,
	ACQ_MINUS_B
};


/* Flags for type of phase handling - UNKNOWN -> type of protocol undecided,
                                      FFM -> Frankfurt type protocol,
                                      BLN -> Berlin kind of protocol */

enum {
	PHASE_UNKNOWN_PROT,
	PHASE_FFM_PROT,
	PHASE_BLN_PROT
};


/* Define a structure used in analyzing the input program */

typedef struct
{
	/* Global compilation error states (FATAL, SEVERE and WARN) */

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
	DO_LOAD    =  1,
	DO_TEST    =  2,
	DO_START   =  4,
	DO_SIGNAL  =  8,
	DO_DELETE  = 16,
	NO_MAIL    = 32,
	NO_BALLOON = 64
};

/* Convenience macros for accessing the value of simple variables
   (but be careful with the VALUE macro: the returned value is always
   a double !) */

#define INT          val.lval
#define FLOAT        val.dval
#define VALUE( a )   ( ( a )->type == INT_VAR ?            \
                       ( a )->val.lval : ( a )->val.dval )



#define DO_STOP ( do_quit && react_to_do_quit )


/* Return codes from function get_lib_symbol() (in func.c) */

enum {
	LIB_OK          =  0,        // everything fine
	LIB_ERR_NO_LIB  = -1,        // library not found
	LIB_ERR_NO_SYM  = -2         // symbol not found in library
};


/* The following number is the start number for numbers designating digitizer
   windows. It should be large enough to make it easy in functions to decide
   if a number can be only a window number. The second number is the maximum
   number of windows. Thus any number in the right context in the range
   between WINDOW_START_NUMBER and WINDOW_START_NUMBER + MAX_NUM_OF_WINDOWS
   has a high chance of being a window number - not a very nice hack, but... */

#define WINDOW_START_NUMBER  1024
#define MAX_NUM_OF_WINDOWS   128


#endif /* ! FSC2_GLOBAL */
