/*
  $Id$
*/

#if ! defined FSC2_HEADER
#define FSC2_HEADER


#define _GNU_SOURCE


/* AWK might be defined via compiler flags - otherwise define it here */

#if defined AWK
#define AWK_PROG AWK
#else
#define AWK_PROG "awk"
#endif


/* inclusion system header files */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <forms.h>


/* inclusion of programs own header files */

#include "fsc2_rsc.h"
#include "global.h"               /* must be the very first to be included ! */
#include "comm.h"
#include "exceptions.h"
#include "T.h"
#include "util.h"
#include "variables.h"
#include "func.h"                 /* load before "devices.h" */
#include "devices.h"              /* load before "loader.h"  */
#include "loader.h"
#include "assign.h"
#include "phases.h"
#include "pulse.h"
#include "ppcheck.h"
#include "prim_exp.h"
#include "run.h"
#include "chld_func.h"
#include "graphics.h"


/* The diverse lexers */

void clean_up( void );
bool scan_main( char *file );
int devices_parser( FILE *in );
int assignments_parser( FILE *in );
int variables_parser( FILE *in );
int phases_parser( FILE *in );
int preparations_parser( FILE *in );
int primary_experiment_parser( FILE *in );
void sigchld_handler( int sig_type, void *data );


#define TAB_LENGTH          4
#define BROWSER_MAXLINE  1024
#define EDITOR_FAILED    123



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
Var *Var_Stack = NULL;

Device *Device_List = NULL;
Device_Name *Device_Name_List = NULL;


ASSIGNMENTS assignment[ PULSER_CHANNEL_PHASE_Y + 1 ];

Phase_Sequence PSeq[ MAX_PHASE_SEQ_LEN ];
Acquisition_Sequence ASeq[ 2 ];

Pulse *Plist = NULL;
Pulse *Cur_Pulse = NULL;

bool TEST_RUN = UNSET;       /* flag, set while EXPERIMENT section is tested */
bool need_GPIB = UNSET;      /* flag, set if GPIB bus is needed */
bool need_Serial_Port[ NUM_SERIAL_PORTS ];


bool just_testing;
FD_fsc2 *main_form;
FD_run *run_form;
FD_device *device_form;


int I_am = PARENT;
int pd[ 4 ];                    /* pipe descriptors */
int child_pid = 0;              /* pid of child */
volatile bool do_send = UNSET;  /* globals used with the signal handlers */
volatile bool do_quit = UNSET;
bool exit_hooks_are_run = UNSET;


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
extern Var *Var_Stack;

extern ASSIGNMENTS assignment[ ];

extern Phase_Sequence PSeq[ ];
extern Acquisition_Sequence ASeq[ ];

extern Pulse *Plist;
extern Pulse *Cur_Pulse;

extern bool TEST_RUN;
extern bool need_GPIB;
extern bool need_Serial_Port[ NUM_SERIAL_PORTS ];

extern bool just_testing;
extern FD_fsc2 *main_form;
extern FD_run *run_form;
extern FD_device *device_form;


extern int I_am;
extern int pd[ ];                  /* pipe descriptors */
extern int child_pid;              /* pid of child */
extern volatile bool do_send;      /* globals used with the signal handlers */
extern volatile bool do_quit;
extern bool exit_hooks_are_run;

#endif


#endif  /* ! FSC2_HEADER */
