/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

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
#include <strings.h>
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
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <forms.h>
#include <X11/Xft/Xft.h>

#if defined WITH_RULBUS
#include <rulbus.h>
#endif

#if defined WITH_MEDRIVER
#include <medriver.h>
#endif

/* inclusion of programs own header files */

#include "fsc2_config.h"
#include "fsc2_rsc_hr.h"          /* must be the first to be included ! */
#include "global.h"               /* must be the second to be included ! */
#include "fsc2_assert.h"
#include "fsc2d.h"
#include "inline.h"
#include "dump.h"
#include "bugs.h"
#include "xinit.h"
#include "comm.h"
#include "help.h"
#include "edit.h"
#include "mail.h"
#include "ipc.h"
#include "locks.h"
#include "exceptions.h"
#include "T.h"
#include "variables.h"
#include "vars_rhs.h"
#include "vars_lhs.h"
#include "vars_ass.h"
#include "vars_add.h"
#include "vars_sub.h"
#include "vars_mult.h"
#include "vars_div.h"
#include "vars_mod.h"
#include "vars_pow.h"
#include "vars_util.h"
#include "util.h"
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
#include "graphics_edl.h"
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
#include "lan.h"
#if defined WITH_HTTP_SERVER
#include "dump_graphic.h"
#include "http.h"
#endif
#include "module_util.h"


#if defined MAPATROL
#include <mpatrol.h>
#endif


/* Some global functions */

bool get_edl_file( const char * fname );

void clean_up( void );

bool scan_main( const char * /* name */,
                FILE *       /* fp   */  );

int  devices_parser( FILE * /* in */ );

int  assignments_parser( FILE * /* in */ );

int  variables_parser( FILE * /* in */ );

int  phases_parser( FILE * /* in */ );

int  preparations_parser( FILE * /* in */ );

int  experiment_parser( FILE * /* in */ );

void usage( int /* result_status */ );

int idle_handler( void );


/* Most global variables used in the program belong to one of the following
   structures. The only exceptions are variables that must be visible within
   the modules and to make it easier for module writers we keep them as simple
   variables (they are also declared as extern in fsc2_module.h, the header
   file the modules have to include) */

typedef struct Internals Internals_T;
typedef struct EDL_Files EDL_Files_T;
typedef struct EDL_Info EDL_Info_T;
typedef struct Communication Communication_T;
typedef struct GUI_Info GUI_Info_T;
typedef struct Crash_Info Crash_Info_T;


struct Internals {
    uid_t EUID;                  /* user and group ID the program got */
    gid_t EGID;                  /* started with */

    pid_t fsc2_clean_pid;
    volatile sig_atomic_t fsc2_clean_died;
    volatile sig_atomic_t fsc2_clean_status;
    volatile sig_atomic_t fsc2_clean_status_ok;

    pid_t child_pid;             /* pid of child process doing the
                                    measurement */
    volatile pid_t http_pid;     /* pid of child process that is a http server
                                    to allow viewing fsc2's state */
    int http_port;               /* port the http server is running on */

    int cmdline_flags;           /* stores command line options */

    long num_test_runs;          /* number of test runs with '-X' flag */
    long check_return;           /* result of check run, 1 is ok */

    int I_am;                    /* indicates if we're running the parent
                                    or the child process (it's either set to
                                    PARENT or to CHILD) */
    int state;
    int mode;                    /* the mode fsc2 is currently running in,
                                    either PREPARATION, TEST or EXPERIMENT */
    bool in_hook;                /* set while module hook functions are run */

    bool exit_hooks_are_run;     /* Set if modules exit hooks have all already
                                    been run */

    void *rsc_handle;            /* handle of the graphics resources library */

    /* The following are flags that get set in signal handlers and when set
       indicate that certain actions have to be taken the idle callbacks */

    volatile sig_atomic_t child_is_quitting; /* set when the child is done */

    volatile sig_atomic_t tb_wait; /* set if the child is waiting for the
                                      change of the state of an object in the
                                      toolbox */

    volatile sig_atomic_t http_server_died; /* set when the http server dies */

    char *title;                 /* string with title of the main window */
    char *def_directory;         /* default directory for file open dialog */
};


struct EDL_Files {
    char *name;                  /* name of EDL file */
    time_t mod_date;             /* last modification date when read in */
};


struct EDL_Info {
    EDL_Files_T *files;          /* List of EDL files being used */ 
    size_t file_count;           /* Number of EDL files in list */

    long Lc;                     /* line number in currently parsed EDL file */
    char *Fname;                 /* name of currently parsed EDL file */

    Call_Stack_T *Call_Stack;    /* stack for storing some kind of frame
                                    information during nested EDL function
                                    calls */
    Compilation_T compilation;   /* structure with infos about compilation
                                    states and errors (see also global.h) */
    Prg_Token_T *prg_token;      /* array of predigested program tokens */

    bool needs_test_run;         /* set if full test run is required */

    long prg_length;             /* number of array elements in predigested
                                    program (negative value indicates that
                                    there is no EXPERIMENT section, not even
                                    an EXPERIMENT label) */
    Prg_Token_T *cur_prg_token;  /* pointer to the currently handled element in
                                    the array of predigested program tokens */
    long On_Stop_Pos;            /* Index of the ON_STOP command in the array
                                    of predigested program tokens (negative
                                    value indicates that there is no ON_STOP
                                    label) */
    Var_T *Var_List;             /* list of all user declared EDL variables */
    Var_T *Var_Stack;            /* Stack of variables used in the evaluation
                                    of expressions and function calls */
    volatile sig_atomic_t do_quit;  /* becomes set when a running EDL program
                                    has to be stopped (because STOP button
                                    has been pressed) */
    volatile sig_atomic_t react_to_do_quit;  /* set when program should not
                                    react to the STOP button anymore (after the
                                    ON_STOP label has been processed) */
    File_List_T *File_List;      /* list of all files the user opened */
    int File_List_Len;           /* length of this file list */

    Device_T *Device_List;
    Device_Name_T *Device_Name_List;

    long Num_Pulsers;

    long Cur_Pulse;              /* number of the current pulse during the
                                    setup of a pulse in the PREPARATIONS
                                    section */
    double experiment_time;
};


struct Communication {
    int pd[ 4 ];                 /* pipe descriptors for measurement child */
    int http_pd[ 4 ];            /* pipes for HTTP server */

    int mq_semaphore;            /* semaphore for message queue */

    Message_Queue_T *MQ;         /* the message queue for data send from the
                                    child to the parent process */
    int MQ_ID;                   /* shared memory segment ID of the message
                                    queue */
};


struct GUI_Info {
    bool is_init;
    Display *d;

    G_Funcs_T G_Funcs;

    FD_fsc2 *main_form;
    FD_run_1d *run_form_1d;
    FD_run_2d *run_form_2d;
    FD_input_form *input_form;
    FD_cut *cut_form;
    FD_print_comment *print_comment;

    int stop_button_mask;        /* which mouse buttons to use for STOP
                                    button */

    int win_x;
    int win_y;
    bool win_has_pos;
    unsigned int win_width;
    unsigned int win_height;
    bool win_has_size;

    int display_1d_x;
    int display_1d_y;
    bool display_1d_has_pos;
    unsigned int display_1d_width;
    unsigned int display_1d_height;
    bool display_1d_has_size;

    int display_2d_x;
    int display_2d_y;
    bool display_2d_has_pos;
    unsigned int display_2d_width;
    unsigned int display_2d_height;
    bool display_2d_has_size;

    int cut_win_x;
    int cut_win_y;
    bool cut_win_has_pos;
    unsigned int cut_win_width;
    unsigned int cut_win_height;
    bool cut_win_has_size;

    int toolbox_x;
    int toolbox_y;
    bool toolbox_has_pos;

    int toolboxFontSize;
};


struct Crash_Info {
    bool already_crashed;         /* set when crash has happened */
    volatile sig_atomic_t signo;  /* signal indicating the type of crash */
    void *address;                /* address the crash happened */
    void *trace[ MAX_TRACE_LEN ]; /* addresses of backtrace */
    size_t trace_length;          /* length of backtrace */
};


/* Global variables */

extern Internals_T Fsc2_Internals;
extern EDL_Info_T EDL;
extern Communication_T Comm;
extern GUI_Info_T GUI;
extern Crash_Info_T Crash;
extern Graphics_T G;
extern Graphics_1d_T G_1d;
extern Graphics_2d_T G_2d;
extern Cut_Graphics_T G_cut;

extern bool Need_GPIB;
extern bool Need_RULBUS;
extern bool Need_LAN;
extern bool Need_MEDRIVER;
extern bool Need_USB;

extern PA_Seq_T PA_Seq;

extern const char *Channel_Names[ NUM_CHANNEL_NAMES ];
extern const char *Phase_Types[ NUM_PHASE_TYPES ];

extern Pulser_Struct_T *Pulser_Struct;
extern const char *Function_Names[ PULSER_CHANNEL_NUM_FUNC ];
extern long Cur_Pulser;


#endif  /* ! FSC2_HEADER */

#endif  /* ! FSC2_MODULE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
