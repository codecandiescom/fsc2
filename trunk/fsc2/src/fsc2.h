/*
  $Id$
*/

#if ! defined FSC2_HEADER
#define FSC2_HEADER


#define _GNU_SOURCE 1


/* define my email address for bug reports */

#define MAIL_ADDRESS "Jens.Toerring@physik.fu-berlin.de"

#define FSC2_SOCKET  "/tmp/fsc2.uds"

/* AWK might be defined via compiler flags - otherwise define it here */

#if defined AWK
#define AWK_PROG AWK
#else
#define AWK_PROG "awk"
#endif


#if ! defined libdir
#define libdir "./"
#endif


#if ! defined auxdir
#define auxdir "./"
#endif

#define HI_RES  1
#define LOW_RES 0

#if ! defined SIZE
#define SIZE HI_RES
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
#include <pwd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <forms.h>


/* inclusion of programs own header files */

#if ( SIZE == HI_RES )
#include "fsc2_rsc_hr.h"
#else
#include "fsc2_rsc_lr.h"
#endif

#include "global.h"               /* must be the very first to be included ! */
#include "comm.h"
#include "exceptions.h"
#include "T.h"
#include "util.h"
#include "variables.h"
#include "vars_util.h"
#include "func.h"                 /* load before "devices.h" */
#include "devices.h"              /* load before "loader.h"  */
#include "func_basic.h"
#include "func_util.h"
#include "loader.h"
#include "phases.h"
#include "pulser.h"
#include "exp.h"
#include "run.h"
#include "chld_func.h"
#include "graphics.h"
#include "accept.h"
#include "graph_handler_1d.h"
#include "graph_handler_2d.h"
#include "print.h"
#include "func_intact.h"
#include "conn.h"


/* Some global functions */

void clean_up( void );
bool scan_main( char *file );
int devices_parser( FILE *in );
int assignments_parser( FILE *in );
int variables_parser( FILE *in );
int phases_parser( FILE *in );
int preparations_parser( FILE *in );
int experiment_parser( FILE *in );
void sigusr1_handler( int signo );
void notify_conn( int signo );


#define TAB_LENGTH        4
#define MAXLINE        4096


/* Global variables */

#if defined ( FSC2_MAIN )

int EUID;


/* used in compiling the user supplied program */

long Lc = 0;
char *Fname = NULL;
Compilation compilation;
Prg_Token *prg_token = NULL;
long prg_length = 0;
Prg_Token *cur_prg_token;
long On_Stop_Pos = -1;

Var *var_list = NULL;
Var *Var_Stack = NULL;

Device *Device_List = NULL;
Device_Name *Device_Name_List = NULL;

Pulser_Struct pulser_struct;

Phase_Sequence *PSeq = NULL;
Acquisition_Sequence ASeq[ 2 ] = { { UNSET, NULL, 0 }, { UNSET, NULL, 0 } };

long Cur_Pulse = -1;

bool TEST_RUN = UNSET;       /* flag, set while EXPERIMENT section is tested */
bool need_GPIB = UNSET;      /* flag, set if GPIB bus is needed */
bool need_Serial_Port[ NUM_SERIAL_PORTS ];

bool just_testing = UNSET;

FD_fsc2 *main_form;
FD_run *run_form;
FD_input_form *input_form;

int I_am = PARENT;
int pd[ 4 ];                    /* pipe descriptors */
int conn_pd[ 2 ];
pid_t child_pid = 0;            /* pid of child */
pid_t conn_pid = -1;            /* pid of communication child */
volatile bool do_send = UNSET;  /* globals used with the signal handlers */
volatile bool do_quit = UNSET;
volatile bool conn_child_replied = UNSET;
bool react_to_do_quit = SET;
bool exit_hooks_are_run = UNSET;

Graphics G;
KEY *Key;
KEY *Message_Queue;
volatile int message_queue_low, message_queue_high;

FILE_LIST *File_List = NULL;
int File_List_Len = 0;

TOOL_BOX *Tool_Box = NULL;


#else   /*  ! FSC2_MAIN */

extern int EUID;

extern long Lc;
extern char *Fname;
extern Compilation compilation;
extern Prg_Token *prg_token;
extern long prg_length;
extern Prg_Token *cur_prg_token;
extern long On_Stop_Pos;

extern Device *Device_List;
extern Device_Name *Device_Name_List;

extern Var *var_list;
extern Var *Var_Stack;

extern Pulser_Struct pulser_struct;

extern Phase_Sequence *PSeq;
extern Acquisition_Sequence ASeq[ ];

extern long Cur_Pulse;

extern bool TEST_RUN;
extern bool need_GPIB;
extern bool need_Serial_Port[ NUM_SERIAL_PORTS ];

extern bool just_testing;
extern FD_fsc2 *main_form;
extern FD_run *run_form;
extern FD_input_form *input_form;


extern int I_am;
extern int pd[ ];                  /* pipe descriptors */
extern int conn_pd[ ];
extern pid_t child_pid;            /* pid of child */
extern pid_t conn_pid;
extern volatile bool do_send;      /* globals used with the signal handlers */
extern volatile bool do_quit;
extern bool react_to_do_quit;
extern bool exit_hooks_are_run;

extern Graphics G;
extern KEY *Key;
extern KEY *Message_Queue;
extern volatile int message_queue_low, message_queue_high;
extern volatile bool conn_child_replied;

extern FILE_LIST *File_List;
extern int File_List_Len;

extern TOOL_BOX *Tool_Box;


#endif


#endif  /* ! FSC2_HEADER */
