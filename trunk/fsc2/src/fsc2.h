/*
  $Id$
*/

#if ! defined FSC2_HEADER
#define FSC2_HEADER


#define _GNU_SOURCE


/* inclusion system header files */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>


/* inclusion of programs own header files */

#include "global.h"               /* must be the very first to be included ! */
#include "exceptions.h"
#include "T.h"
#include "util.h"
#include "variables.h"
#include "func.h"                 /* load before "devices.h" ! */
#include "devices.h"
#include "assign.h"
#include "phases.h"
#include "pulse.h"
#include "ppcheck.h"
#include "prim_exp.h"


/* The diverse lexers */

void clean_up( void );
bool split( char *file );
int devices_parser( FILE *in );
int assignments_parser( FILE *in );
int variables_parser( FILE *in );
int phases_parser( FILE *in );
int preparations_parser( FILE *in );
int primary_experiment_parser( FILE *in );


/* Global variables */

#if defined ( FSC2_MAIN )


/* used in compiling the user supplied program */

long Lc = 0;
char *Fname = NULL;
Compilation compilation;
Prg_Token *prg_token = NULL;
long prg_length = 0;
Prg_Token *cur_prg_token;

Var *var_list = NULL;
Var *var_list_copy;
Var *Var_Stack = NULL;

Device *Device_List = NULL;
Device_Name *Device_Name_List = NULL;


ASSIGNMENTS assignment[ PULSER_CHANNEL_PHASE_Y + 1 ];

Phase_Sequence PSeq[ MAX_PHASE_SEQ_LEN ];
Acquisition_Sequence ASeq[ 2 ];

Pulse *Plist = NULL;
Pulse *Cur_Pulse = NULL;

int TEST_RUN = 0;            /* flag, set while EXPERIMENT section is tested */
int need_GPIB = 0;           /* flag, set if GPIB bus is needed */


#else   /*  ! FSC2_MAIN */


extern long Lc;
extern char *Fname;
extern Compilation compilation;
extern Prg_Token *prg_token;
extern long prg_length;
extern Prg_Token *cur_prg_token;

extern Device *Device_List;
extern Device_Name *Device_Name_List;

extern Var *var_list;
extern Var *var_list_copy;
extern Var *Var_Stack;

extern ASSIGNMENTS assignment[ ];

extern Phase_Sequence PSeq[ ];
extern Acquisition_Sequence ASeq[ ];

extern Pulse *Plist;
extern Pulse *Cur_Pulse;

extern int TEST_RUN;
extern int need_GPIB;

#endif


#endif  /* ! FSC2_HEADER */
