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



/* inclusion of programs own header files */

#include "global.h"                    /* must be the very first one ! */
#include "exceptions.h"
#include "T.h"
#include "util.h"
#include "assign.h"
#include "variables.h"
#include "func.h"
#include "phases.h"
#include "pulse.h"
#include "ppcheck.h"




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

Pulse *Plist = NULL;
Pulse *Cur_Pulse = NULL;

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

extern Pulse *Plist;
extern Pulse *Cur_Pulse;

#endif


#endif  /* ! FSC2_HEADER */
