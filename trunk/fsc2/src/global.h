/*
  $Id$
*/


#if ! defined FSC2_GLOBAL
#define FSC2_GLOBAL


/* make bool synonymous to unsigned char */

typedef  unsigned char  bool;


/* Define the name of fsc2's lock file */

#define LOCKFILE "/tmp/fsc2.lock"


/* Defines the number of serial ports on the system */

#define NUM_SERIAL_PORTS 2


/* Define the maximum number of pulses (thus pulse numbers have to be in
   the range between 0 and MAX_PULSE_NUM - 1) */

#define MAX_PULSE_NUM  400


/* Define the maximum length of phase sequences */

#define MAX_PHASE_SEQ_LEN  12    


/* Define the default time base (in ns) to be used in the program */

#define DEFAULT_TIME_UNIT 1


/* Define number of colors to be used in 2D graphics */

#define NUM_COLORS 256


/* Define some useful abbreviations - never ever change these ! */

#define	FAIL  ( ( bool ) 0 )      
#define UNSET ( ( bool ) 0 )
#define OK    ( ( bool ) 1 )
#define SET   ( ( bool ) 1 )



/* Define error severity types */

enum {
	   FATAL = 0,
	   SEVERE,
	   WARN,
	   NO_ERROR
};


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



/* Define the different functions pulses can have */


enum {
	PULSER_CHANNEL_NO_TYPE = -1,
	PULSER_CHANNEL_MW,
    PULSER_CHANNEL_TWT,
	PULSER_CHANNEL_TWT_GATE,
	PULSER_CHANNEL_DET,
	PULSER_CHANNEL_DET_GATE,
	PULSER_CHANNEL_RF,
	PULSER_CHANNEL_RF_GATE,
	PULSER_CHANNEL_PHASE_X,
	PULSER_CHANNEL_PHASE_Y,
	PULSER_CHANNEL_OTHER_1,
	PULSER_CHANNEL_OTHER_2,
	PULSER_CHANNEL_OTHER_3,
	PULSER_CHANNEL_OTHER_4
};


#define PULSER_CHANNEL_FUNC_MIN PULSER_CHANNEL_MW
#define PULSER_CHANNEL_FUNC_MAX PULSER_CHANNEL_OTHER_4



/* Define access types for functions in certain sections */

enum {
	  ACCESS_EXP,
	  ACCESS_PREP,
	  ACCESS_ALL
};



/* Define the types of phases used in phase cycling */

enum {
	   PHASE_PLUS_X,
	   PHASE_MINUS_X,
	   PHASE_PLUS_Y,
	   PHASE_MINUS_Y
};


/* Define the acquisition types used in phase cycling */

enum {
	   ACQ_PLUS,
	   ACQ_MINUS
};


/* Define the different types of variables */

enum {
	UNDEF_VAR       = 0,
	INT_VAR         = ( 1 << 0 ),
	FLOAT_VAR       = ( 1 << 1 ),
	STR_VAR         = ( 1 << 2 ),
	INT_ARR         = ( 1 << 3 ),
	FLOAT_ARR       = ( 1 << 4 ),
	FUNC            = ( 1 << 5 ),
	ARR_PTR         = ( 1 << 6 ),
	INT_TRANS_ARR   = ( 1 << 7 ),
	FLOAT_TRANS_ARR = ( 1 << 8 ),
	ARR_REF         = ( 1 << 9 )
};


enum {
	NEW_VARIABLE       = ( 1 << 0 ),
	VARIABLE_SIZED     = ( 1 << 1 ),
	NEED_SLICE         = ( 1 << 2 ),
	NEED_INIT          = ( 1 << 3 ),
	NEED_ALLOC         = ( 1 << 4 ),
};


/* Define a structure used in analyzing the input program */

typedef struct
{
	/* global compilation error states (FATAL, SEVERE and WARN) */

	bool error[ 3 ];

	/* indicates which sections have already been handled to detect
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


/* convinience macros for accessing the value of simple variables */


#define fatal_error  THROW( EXCEPTION );
#define INT          val.lval
#define FLOAT        val.dval
#define VALUE( a )   ( ( a )->type == INT_VAR ?            \
                       ( a )->val.lval : ( a )->val.dval )


/* return codes from function get_lib_symbol() (in func.c) */

enum {
	LIB_OK          =  0,
	LIB_ERR_NO_LIB  = -1,
	LIB_ERR_NO_SYM  = -2
};


#endif FSC2_GLOBAL
