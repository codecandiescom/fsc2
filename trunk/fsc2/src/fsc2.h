/*
  $Id$
*/

#if ! defined FSC2_HEADER
#define FSC2_HEADER


#define _GNU_SOURCE 1


/* define my email address for bug and crash reports */

//#define MAIL_ADDRESS "Jens.Toerring@physik.fu-berlin.de"
#define MAIL_ADDRESS "jens"

#define FSC2_SOCKET  "/tmp/fsc2.uds"


#if ! defined libdir
#define libdir "./"
#endif

#if ! defined auxdir
#define auxdir "./"
#endif

#if ! defined docdir
#define docdir "./doc"
#endif

#define HI_RES  1
#define LOW_RES 0


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

#include "fsc2_rsc.h"
#include "global.h"               /* must be the very first to be included ! */
#include "fsc2_assert.h"
#include "bugs.h"
#include "xinit.h"
#include "comm.h"
#include "ipc.h"
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
#include "graph_cut.h"
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
void main_sig_handler( int signo );
void notify_conn( int signo );
void usage( void );


#define TAB_LENGTH        4
#define MAXLINE        4096


#define GEOMETRY         0
#define BROWSERFONTSIZE  1
#define BUTTONFONTSIZE   2
#define INPUTFONTSIZE    3
#define LABELFONTSIZE    4
#define DISPLAYGEOMETRY  5
#define CUTGEOMETRY      6
#define TOOLGEOMETRY     7
#define AXISFONT         8
#define CHOICEFONTSIZE   9
#define SLIDERFONTSIZE  10
#define FILESELFONTSIZE 11
#define HELPFONTSIZE    12
#define STOPMOUSEBUTTON 13


/* Global variables */

#if defined ( FSC2_MAIN )

uid_t EUID;                  /* user and group ID the program was called */
gid_t EGID;                  /* with (should both translate to fsc2) */

/* used in compiling and executing the user supplied program */

long Lc = 0;                 /* line number in currently parsed file */
char *Fname = NULL;          /* name of currently parsed file */
const char *Cur_Func = NULL; /* name of currently executed function */
Fsc2_Assert Assert_struct;
Compilation compilation;     /* structure with infos about compilation state */
Prg_Token *prg_token = NULL; /* array with predigested program */
long prg_length = 0;         /* number of array elements in predigested
								program */
Prg_Token *cur_prg_token;    /* index of currently handled element in
								predigested program */
long On_Stop_Pos = -1;       /* index of the ON_STOP command in the array with
								the predigested program */

Var *var_list = NULL;        /* list of all variables used in the program */
Var *Var_Stack = NULL;       /* list for stack of variables used in evaluation
								of expressions and function calls */

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
FD_cut *cut_form;

int I_am = PARENT;
int pd[ 4 ];                    /* pipe descriptors */
int conn_pd[ 2 ];
pid_t child_pid = 0;            /* pid of child */
pid_t conn_pid = -1;            /* pid of communication child */
int semaphore = -1;
volatile bool do_quit = UNSET;
volatile bool conn_child_replied = UNSET;
bool react_to_do_quit = SET;
bool exit_hooks_are_run = UNSET;

int border_offset_x;
int border_offset_y;

int stop_button_mask = 0;

Graphics G;
KEY *Key;
int Key_ID = -1;
KEY *Message_Queue = NULL;
volatile int message_queue_low, message_queue_high;

FILE_LIST *File_List = NULL;
int File_List_Len = 0;

TOOL_BOX *Tool_Box = NULL;


#else   /*  ! FSC2_MAIN */

extern uid_t EUID;
extern gid_t EGID;

extern long Lc;
extern char *Fname;
extern const char *Cur_Func;
extern Fsc2_Assert Assert_struct;
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
extern FD_cut *cut_form;


extern int I_am;
extern int pd[ ];                  /* pipe descriptors */
extern int conn_pd[ ];
extern pid_t child_pid;            /* pid of child */
extern pid_t conn_pid;
extern int semaphore;
extern volatile bool do_quit;
extern bool react_to_do_quit;
extern bool exit_hooks_are_run;

extern int border_offset_x;
extern int border_offset_y;

extern int stop_button_mask;

extern Graphics G;
extern KEY *Key;
extern int Key_ID;
extern KEY *Message_Queue;
extern volatile int message_queue_low, message_queue_high;
extern volatile bool conn_child_replied;

extern FILE_LIST *File_List;
extern int File_List_Len;

extern TOOL_BOX *Tool_Box;


#endif


#endif  /* ! FSC2_HEADER */
