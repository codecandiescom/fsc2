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


#if ! defined FSC2_HEADER
#define FSC2_HEADER


#define _GNU_SOURCE 1


/* Inclusion of system header files */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
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

#include "fsc2_rsc_hr.h"          /* must be the first to be included ! */
#include "global.h"               /* must be the second to be included ! */
#include "fsc2_assert.h"
#include "dump.h"
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
#include "func_save.h"
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
void usage( int result_status );


/* Global variables */

#if defined ( FSC2_MAIN )

uid_t EUID;                  /* user and group ID the program was called */
gid_t EGID;                  /* with (should both translate to fsc2) */

bool is_i386 = UNSET;

int cmdline_flags;

/* used in compiling and executing the user supplied program */

long Lc = 0;                 /* line number in currently parsed file */
char *Fname = NULL;          /* name of currently parsed file */
const char *Cur_Func = NULL; /* name of currently executed function */
Compilation compilation;     /* structure with infos about compilation state */
Prg_Token *prg_token = NULL; /* array with predigested program */
long prg_length = -1;        /* number of array elements in predigested
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
int Max_Devices_of_a_Kind;

Pulser_Struct pulser_struct;

Phase_Sequence *PSeq = NULL;
Acquisition_Sequence ASeq[ 2 ] = { { UNSET, NULL, 0 }, { UNSET, NULL, 0 } };

long Cur_Pulse = -1;

bool TEST_RUN = UNSET;       /* flag, set while EXPERIMENT section is tested */
bool need_GPIB = UNSET;      /* flag, set if GPIB bus is needed */

bool just_testing = UNSET;

G_FUNCS G_Funcs;
FD_fsc2 *main_form;
FL_FORM *fsc2_main_form;
FD_run *run_form;
FL_FORM *run_main_form;
FD_input_form *input_form;
FL_FORM *input_main_form;
FD_cut *cut_form;
FL_FORM *cut_main_form;

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

extern bool is_i386;

extern int cmdline_flags;

extern long Lc;
extern char *Fname;
extern const char *Cur_Func;
extern Compilation compilation;
extern Prg_Token *prg_token;
extern long prg_length;
extern Prg_Token *cur_prg_token;
extern long On_Stop_Pos;

extern Device *Device_List;
extern Device_Name *Device_Name_List;
extern int Max_Devices_of_a_Kind;

extern Var *var_list;
extern Var *Var_Stack;

extern Pulser_Struct pulser_struct;

extern Phase_Sequence *PSeq;
extern Acquisition_Sequence ASeq[ ];

extern long Cur_Pulse;

extern bool TEST_RUN;
extern bool need_GPIB;

extern bool just_testing;

extern G_FUNCS G_Funcs;
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
