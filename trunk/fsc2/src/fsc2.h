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


#if ! defined FSC2_MODULE_HEADER

#if ! defined FSC2_HEADER
#define FSC2_HEADER


#define _GNU_SOURCE 1


/* Inclusion of system header files */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <float.h>
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
#include "variables.h"
#include "util.h"
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
#include "func_intact_b.h"
#include "func_intact_s.h"
#include "func_intact_o.h"
#include "func_intact_m.h"
#include "conn.h"
#if defined WITH_HTTP_SERVER
#include "dump_graphic.h"
#include "http.h"
#endif
#include "module_util.h"


#if defined MAPATROL
#include <mpatrol.h>
#endif


/* Some global functions */

void clean_up( void );
bool scan_main( const char *name, FILE *fp );
int  devices_parser( FILE *in );
int  assignments_parser( FILE *in );
int  variables_parser( FILE *in );
int  phases_parser( FILE *in );
int  preparations_parser( FILE *in );
int  experiment_parser( FILE *in );
void main_sig_handler( int signo );
void notify_conn( int signo );
void usage( int result_status );
int idle_handler( XEvent *a, void *b );


/* Most global variables used in the program belong to one of the following
   structures. The only exceptions are variables that must be visible within
   the modules and to make it easier for module writers we keep them as simple
   variables (they are also declared as extern in fsc2_module.h, the header
   file the modules have to include) */

typedef struct {

	uid_t EUID;                  /* User and group ID the program was */
	gid_t EGID;				     /* started with */

	pid_t child_pid;             /* pid of child process doing the
                                    measurement */
	pid_t conn_pid;              /* pid of child process for handling
									communication with scripts */
	pid_t http_pid;              /* pid of child process that is a http server
									to allow viewing fsc2's state */
	int http_port;               /* port the http server is running on */

	bool is_i386;                /* Set if running on an i386 processor */

	int cmdline_flags;           /* Stores command line options */

	long num_test_runs;          /* Number of test runs with '-X' flag */
	long check_return;           /* Result of check run, 1 is ok */

	int I_am;                    /* Indicates if we're running the parent
									or the child process (it's either set to
									PARENT or to CHILD) */
	int state;
	int mode;                    /* The mode fsc2 is currently running in,
									either PREPARATION, TEST or EXPERIMENT */
	bool in_hook;                /* Set while module hook functions are run */

	bool just_testing;           /* Set if only syntax check is to be done */

	bool exit_hooks_are_run;     /* Set if modules exit hooks have all already
									been run */
} INTERNALS;


typedef struct {

	long Lc;                     /* Line number in currently parsed EDL file */
	char *Fname;                 /* Name of currently parsed EDL file */

	CALL_STACK *Call_Stack;      /* Stack for storing some kind of frame
									information during nested EDL function
									calls */
	Compilation compilation;     /* Structure with infos about compilation
									states and errors (see also global.h) */
	Prg_Token *prg_token;        /* Array of predigested program tokens */

	long prg_length;             /* Number of array elements in predigested
									program (negative value indicates that
									there is no EXPERIMENT section, not even
									an EXPERIMENT label) */
	Prg_Token *cur_prg_token;    /* Pointer to the currently handled element in
									the array of predigested program tokens */
	long On_Stop_Pos;            /* Index of the ON_STOP command in the array
									of predigested program tokens (negative
									value indicates that there is no ON_STOP
									label) */
	Var *Var_List;               /* List of all user declared EDL variables */
	Var *Var_Stack;              /* Stack of variables used in the evaluation
									of expressions and function calls */
	volatile bool do_quit;       /* Becomes set when a running EDL program has
									to be stopped (because STOP button has
									been pressed) */
	bool react_to_do_quit;       /* Is set when program should not react to
									the STOP button anymore (after the ON_STOP
									label has been processed) */
	FILE_LIST *File_List;        /* List of all files the user opened */
	int File_List_Len;           /* Length of this file list */

    Device *Device_List;
    Device_Name *Device_Name_List;

    long Num_Pulsers;

	long Cur_Pulse;              /* Number of the current pulse during the
									setup of a pulse in the PREPARATIONS
									section */
	double experiment_time;
} EDL_Stuff;


typedef struct {

	int pd[ 4 ];                 /* pipe descriptors for measurement child */
	int conn_pd[ 2 ];            /* pipe for communication child */
	int http_pd[ 4 ];            /* pipes for HTTP server */

	int data_semaphore;          /* Semaphore for DATA messages */
	int request_semaphore;       /* Semaphore for REQUEST mesages */

	MESSAGE_QUEUE *MQ;           /* The message queue for data send from the
									child to the parent process */
	int MQ_ID;                   /* shared memory segment ID of the message
									queue */
} COMMUNICATION;


typedef struct {

	G_FUNCS G_Funcs;

	FD_fsc2 *main_form;
	FD_run *run_form;
	FD_input_form *input_form;
	FD_cut *cut_form;
	FD_print_comment *print_comment;

	int border_offset_x;         /* Width and height of window left and */
	int border_offset_y;         /* top border */

	int stop_button_mask;        /* Which mouse buttons to use for STOP
									button */
} GUI_Stuff;


/* Global variables */

#if defined ( FSC2_MAIN )

INTERNALS Internals;
EDL_Stuff EDL;
COMMUNICATION Comm;
GUI_Stuff GUI;
Graphics G;

char *prog_name;                 /* Name the program was started with */
bool need_GPIB = UNSET;          /* Flag, set if GPIB bus is needed */

Pulser_Struct *pulser_struct = NULL;
Phase_Sequence *PSeq = NULL;
Acquisition_Sequence ASeq[ 2 ] = { { UNSET, NULL, 0 }, { UNSET, NULL, 0 } };
long Cur_Pulser = -1;

const char *Digitizer_Channel_Names[ NUM_DIGITIZER_CHANNEL_NAMES ] =
			{ "CH1",
              "CH2",
              "CH3",
              "CH4",
              "MATH1",
              "MATH2",
              "MATH3",
			  "REF1",
              "REF2",
              "REF3",
              "REF4",
              "AUX",
              "AUX1",
              "AUX2",
              "LINE",
			  "MEM_C",
              "MEM_D",
              "FUNC_E",
              "FUNC_F",
              "EXT",
              "EXT10" };

const char *Function_Names[ PULSER_CHANNEL_NUM_FUNC ] =
			{ "MW",
              "TWT",
              "TWT_GATE",
              "DETECTION",
			  "DETECTION_GATE",
              "DEFENSE",
              "RF",
              "RF_GATE",
			  "PULSE_SHAPE",
              "PHASE_1",
              "PHASE_2",
			  "OTHER_1",
              "OTHER_2",
              "OTHER_3",
              "OTHER_4" };

const char *Phase_Types[ PHASE_TYPES_MAX ] = { "+X", "-X", "+Y", "-Y", "CW" };


#else   /*  ! FSC2_MAIN */

extern INTERNALS Internals;
extern EDL_Stuff EDL;
extern COMMUNICATION Comm;
extern GUI_Stuff GUI;
extern Graphics G;


extern bool need_GPIB;
extern char *prog_name;

extern Pulser_Struct *pulser_struct;
extern Phase_Sequence *PSeq;
extern Acquisition_Sequence ASeq[ 2 ];
extern long Cur_Pulser;

extern const char *Digitizer_Channel_Names[ NUM_DIGITIZER_CHANNEL_NAMES ];
extern const char *Function_Names[ PULSER_CHANNEL_NUM_FUNC ];
extern const char *Phase_Types[ PHASE_TYPES_MAX ];


#endif


#endif  /* ! FSC2_HEADER */

#endif  /* ! FSC2_MODULE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
