#if ! defined FSC2_HEADER
#define FSC2_HEADER


/* inclusion system header files */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>


/* defines for several things already needed in the programs header files */


/* make bool synonymous to unsigned char */

typedef  unsigned char  bool;

#define MAX_PHASE_SEQ_LEN  12    /* maximum length of phase sequence */


/* inclusion of programs own header files */


#include "exceptions.h"
#include "T.h"
#include "util.h"
#include "assign.h"
#include "variables.h"
#include "func.h"
#include "phases.h"
#include "ppcheck.h"


#define	FAIL  ( bool ) 0         /* never ever change these ! */
#define UNSET ( bool ) 0
#define OK    ( bool ) 1
#define SET   ( bool ) 1



enum {                           /* error severity types */
	   FATAL = 0,
	   SEVERE,
	   WARN,
	   NO_ERROR
};


enum {                           /* all the different section types */
	  NO_SECTION = ( int ) OK + 1,
	  ASSIGNMENTS_SECTION,
	  DEFAULTS_SECTION,
	  VARIABLES_SECTION,
	  PHASES_SECTION,
	  PREPARATIONS_SECTION,
	  EXPERIMENT_SECTION
};

enum {
	  ACCESS_RESTRICTED,
	  ACCESS_ALL_SECTIONS
};


enum {                           /* phase types for phase cycling */
	   PHASE_PLUS_X,
	   PHASE_MINUS_X,
	   PHASE_PLUS_Y,
	   PHASE_MINUS_Y
};

enum {                           /* acquisition types for phase cycling */
	   ACQ_PLUS,
	   ACQ_MINUS
};


typedef struct
{
	/* global compilation error states (FATAL, SEVERE and WARN) */

	bool error[ 3 ];

	/* indicates which sections have already been handled to detect
	   multiple instances of the same section */

	bool sections[ EXPERIMENT_SECTION + 1 ];
} Compilation;


/* The diverse lexers */

bool split( char *file );
int assignments_parser( FILE *in );
int defaults_parser( FILE *in );
int variables_parser( FILE *in );
int phases_parser( FILE *in );
int preparations_parser( FILE *in );
int primary_experiment_parser( FILE *in );


/* Global variables */

#if defined ( FSC2_MAIN )

long Lc = 0;
char *Fname = NULL;
Compilation compilation;

Var *var_list = NULL;
Var *Var_Stack = NULL;
AStack *Arr_Stack = NULL;
Var *Cur_Arr = NULL;
ASSIGNMENTS assignment[ PULSER_CHANNEL_PHASE_Y + 1 ];
Phase_Sequence PSeq[ MAX_PHASE_SEQ_LEN ];
Acquisition_Sequence ASeq[ 2 ];

#else

extern long Lc;
extern char *Fname;
extern Compilation compilation;

extern Var *var_list;
extern Var *Var_Stack;
extern AStack *Arr_Stack;
extern Var *Cur_Arr;

extern ASSIGNMENTS assignment[ ];

extern Phase_Sequence PSeq[ ];
extern Acquisition_Sequence ASeq[ ];


#endif


#endif  /* ! FSC2_HEADER */
