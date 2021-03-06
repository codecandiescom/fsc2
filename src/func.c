/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
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


#include <dlfcn.h>
#include "fsc2.h"


/* When an identifier is found in the input it is always tested first if it
   is a function by a calling func_get(). If it is a function, a temporary
   variable of type FUNC is pushed on the stack and a pointer to it gets
   returned.  The variables structure contains a pointer to the function, a
   copy of the name of the function and the number of arguments and the
   access flag (the access flag tells in which sections the function may
   be used).

   The following input results in as may variables on the stack as there are
   function arguments. When the argument list ends, func_call() is called.
   This function first checks if there are at least as many variables on the
   stack as the function needs. If there are more arguments than needed, a
   warning is printed and the superfluous variables (i.e. the last ones) are
   removed. There are functions that accept a variable number of arguments,
   some with an upper limit and some without (e.g. the print() function). For
   these functions a negative number of arguments is specified and to
   distingish between them (and to switch off checking the number of arguments
   for the maximum number) for functions without an upper limit of arguments
   the number of arguments is set to 'INT_MIN'.

   As the next step the function is called. The function gets passed a pointer
   to the first argument on the stack. It has to check by itself that the
   variables have the correct type and are reasonable. Each function has to
   push its result onto the stack. This can be either a simple integer or
   float variable (i.e. of type INT_VAR or FLOAT_VAR) or a transient array.
   In the later case the function has to allocate memory for the array, set
   its elements and push a variable of either INT_ARR or FLOAT_ARR
   onto the variable stack with the pointer to the array and its length (as a
   long!) as additional arguments.

   On return func_call() will remove all arguments from the stack as well as
   the variable for the function.  */


/* The following variables are shared with loader.c which adds the functions
   from the loaded modules to the list of built-in functions */

size_t Num_Func = 0;      /* number of built-in and listed functions */
Func_T * Fncts = NULL;    /* array with list of functions structures */


/* Both these variables are shared with 'func_util.c' */

bool No_File_Numbers;
bool Dont_Save;


/* Take care: The number of maximum parameters has to be changed for
              display() and clear_curve() if the maximum number of curves
              (defined as MAX_CURVES in graphics.h) should ever be changed. */

static Func_T Def_fncts[ ] =                  /* List of built-in functions */
{
    { "int",                 f_int,              1, ACCESS_ALL,  NULL, false },
    { "float",               f_float,            1, ACCESS_ALL,  NULL, false },
    { "round",               f_round,            1, ACCESS_ALL,  NULL, false },
    { "floor",               f_floor,            1, ACCESS_ALL,  NULL, false },
    { "ceil",                f_ceil,             1, ACCESS_ALL,  NULL, false },
    { "abs",                 f_abs,              1, ACCESS_ALL,  NULL, false },
    { "max_of",              f_lmax,       INT_MIN, ACCESS_ALL,  NULL, false },
    { "min_of",              f_lmin,       INT_MIN, ACCESS_ALL,  NULL, false },
    { "sin",                 f_sin,              1, ACCESS_ALL,  NULL, false },
    { "cos",                 f_cos,              1, ACCESS_ALL,  NULL, false },
    { "tan",                 f_tan,              1, ACCESS_ALL,  NULL, false },
    { "asin",                f_asin,             1, ACCESS_ALL,  NULL, false },
    { "acos",                f_acos,             1, ACCESS_ALL,  NULL, false },
    { "atan",                f_atan,             1, ACCESS_ALL,  NULL, false },
    { "sinh",                f_sinh,             1, ACCESS_ALL,  NULL, false },
    { "cosh",                f_cosh,             1, ACCESS_ALL,  NULL, false },
    { "tanh",                f_tanh,             1, ACCESS_ALL,  NULL, false },
    { "asinh",               f_asinh,            1, ACCESS_ALL,  NULL, false },
    { "acosh",               f_acosh,            1, ACCESS_ALL,  NULL, false },
    { "atanh",               f_atanh,            1, ACCESS_ALL,  NULL, false },
    { "exp",                 f_exp,              1, ACCESS_ALL,  NULL, false },
    { "ln",                  f_ln,               1, ACCESS_ALL,  NULL, false },
    { "log",                 f_log,              1, ACCESS_ALL,  NULL, false },
    { "sqrt",                f_sqrt,             1, ACCESS_ALL,  NULL, false },
    { "random",              f_random,          -1, ACCESS_ALL,  NULL, false },
    { "random2",             f_random2,         -1, ACCESS_ALL,  NULL, false },
    { "shuffle",             f_shuffle,          1, ACCESS_ALL,  NULL, false },
    { "grandom",             f_grand,           -1, ACCESS_ALL,  NULL, false },
    { "set_seed",            f_setseed,         -1, ACCESS_ALL,  NULL, false },
    { "add_to_average",      f_add2avg,          3, ACCESS_ALL,  NULL, false },
    { "time",                f_time,            -1, ACCESS_ALL,  NULL, false },
    { "date",                f_date,            -1, ACCESS_ALL,  NULL, false },
    { "delta_time",          f_dtime,            0, ACCESS_EXP,  NULL, false },
    { "find_peak",           f_find_peak,        1, ACCESS_ALL,  NULL, false },
    { "index_of_max",        f_index_of_max,     1, ACCESS_ALL,  NULL, false },
    { "index_of_min",        f_index_of_min,     1, ACCESS_ALL,  NULL, false },
    { "print",               f_print,      INT_MIN, ACCESS_ALL,  NULL, false },
    { "sprint",              f_sprint,     INT_MIN, ACCESS_ALL,  NULL, false },
    { "wait",                f_wait,             1, ACCESS_EXP,  NULL, false },
    { "init_1d",             f_init_1d,         -6, ACCESS_PREP, NULL, false },
    { "init_2d",             f_init_2d,        -10, ACCESS_PREP, NULL, false },
    { "change_scale",        f_cscale,          -4, ACCESS_EXP,  NULL, false },
    { "change_scale_1d",     f_cscale_1d,       -2, ACCESS_EXP,  NULL, false },
    { "change_scale_2d",     f_cscale_2d,       -4, ACCESS_EXP,  NULL, false },
    { "vert_rescale",        f_vrescale,         0, ACCESS_EXP,  NULL, false },
    { "vert_rescale_1d",     f_vrescale_1d,      0, ACCESS_EXP,  NULL, false },
    { "vert_rescale_2d",     f_vrescale_2d,      0, ACCESS_EXP,  NULL, false },
    { "change_label",        f_clabel,          -3, ACCESS_EXP,  NULL, false },
    { "change_label_1d",     f_clabel_1d,       -2, ACCESS_EXP,  NULL, false },
    { "change_label_2d",     f_clabel_2d,       -3, ACCESS_EXP,  NULL, false },
    { "rescale",             f_rescale,         -2, ACCESS_EXP,  NULL, false },
    { "rescale_1d",          f_rescale_1d,       1, ACCESS_EXP,  NULL, false },
    { "rescale_2d",          f_rescale_2d,      -2, ACCESS_EXP,  NULL, false },
    { "display",             f_display,        -16, ACCESS_EXP,  NULL, false },
    { "display_1d",          f_display_1d,     -12, ACCESS_EXP,  NULL, false },
    { "display_2d",          f_display_2d,     -16, ACCESS_EXP,  NULL, false },
    { "clear_curve",         f_clearcv,         -4, ACCESS_EXP,  NULL, false },
    { "clear_curve_1d",      f_clearcv_1d,      -4, ACCESS_EXP,  NULL, false },
    { "clear_curve_2d",      f_clearcv_2d,      -4, ACCESS_EXP,  NULL, false },
    { "display_mode",        f_dmode,           -2, ACCESS_ALL,  NULL, false },
    { "dim",                 f_dim,              1, ACCESS_ALL,  NULL, false },
    { "size",                f_size,             1, ACCESS_ALL,  NULL, false },
    { "mean",                f_mean,            -3, ACCESS_ALL,  NULL, false },
    { "rms",                 f_rms,             -3, ACCESS_ALL,  NULL, false },
    { "slice",               f_slice,           -3, ACCESS_ALL,  NULL, false },
    { "mean_part_array",     f_mean_part_array,  2, ACCESS_ALL,  NULL, false },
    { "condense_array",      f_condense,         2, ACCESS_ALL,  NULL, false },
    { "square",              f_square,           1, ACCESS_ALL,  NULL, false },
    { "G_to_T",              f_G2T,              1, ACCESS_ALL,  NULL, false },
    { "T_to_G",              f_T2G,              1, ACCESS_ALL,  NULL, false },
    { "C_to_K",              f_C2K,              1, ACCESS_ALL,  NULL, false },
    { "K_to_C",              f_K2C,              1, ACCESS_ALL,  NULL, false },
    { "D_to_R",              f_D2R,              1, ACCESS_ALL,  NULL, false },
    { "R_to_D",              f_R2D,              1, ACCESS_ALL,  NULL, false },
    { "WL_to_WN",            f_WL2WN,            1, ACCESS_ALL,  NULL, false },
    { "WN_to_WL",            f_WN2WL,            1, ACCESS_ALL,  NULL, false },
    { "F_to_WN",             f_F2WN,             1, ACCESS_ALL,  NULL, false },
    { "WN_to_F" ,            f_WN2F,             1, ACCESS_ALL,  NULL, false },
    { "int_slice",           f_islice,           1, ACCESS_ALL,  NULL, false },
    { "float_slice",         f_fslice,           1, ACCESS_ALL,  NULL, false },
    { "lin_space",           f_lspace,           3, ACCESS_ALL,  NULL, false },
    { "reverse",             f_reverse,          1, ACCESS_ALL,  NULL, false },
    { "sort",                f_sort,            -2, ACCESS_ALL,  NULL, false },
    { "spike_remove",        f_spike_rem,       -3, ACCESS_EXP,  NULL, false },
    { "get_file",            f_getf,            -5, ACCESS_EXP,  NULL, false },
    { "get_gzip_file",       f_getgzf,          -5, ACCESS_EXP,  NULL, false },
    { "open_file",           f_openf,           -6, ACCESS_EXP,  NULL, false },
    { "open_gzip_file",      f_opengzf,         -6, ACCESS_EXP,  NULL, false },
    { "clone_file",          f_clonef,           3, ACCESS_EXP,  NULL, false },
    { "clone_gzip_file",     f_clonegzf,         3, ACCESS_EXP,  NULL, false },
    { "reset_file",          f_resetf,          -1, ACCESS_EXP,  NULL, false },
    { "delete_file",         f_delf,            -1, ACCESS_EXP,  NULL, false },
    { "save",                f_save,       INT_MIN, ACCESS_EXP,  NULL, false },
    { "fsave",               f_fsave,      INT_MIN, ACCESS_EXP,  NULL, false },
    { "ffsave",              f_ffsave,     INT_MIN, ACCESS_EXP,  NULL, false },
    { "save_program",        f_save_p,          -2, ACCESS_EXP,  NULL, false },
    { "save_output",         f_save_o,          -2, ACCESS_EXP,  NULL, false },
    { "save_comment",        f_save_c,          -4, ACCESS_EXP,  NULL, false },
    { "is_file",             f_is_file,          1, ACCESS_EXP,  NULL, false },
    { "file_name",           f_file_name,       -1, ACCESS_EXP,  NULL, false },
    { "path_name",           f_path_name,       -1, ACCESS_EXP,  NULL, false },
    { "layout",              f_layout,           1, ACCESS_EXP,  NULL, false },
    { "button_create",       f_bcreate,         -4, ACCESS_EXP,  NULL, false },
    { "button_delete",       f_bdelete,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "button_state",        f_bstate,          -2, ACCESS_EXP,  NULL, false },
    { "button_changed",      f_bchanged,         1, ACCESS_EXP,  NULL, false },
    { "slider_create",       f_screate,         -6, ACCESS_EXP,  NULL, false },
    { "slider_delete",       f_sdelete,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "slider_value",        f_svalue,          -2, ACCESS_EXP,  NULL, false },
    { "slider_changed",      f_schanged,         1, ACCESS_EXP,  NULL, false },
    { "input_create",        f_ocreate,         -5, ACCESS_EXP,  NULL, false },
    { "input_delete",        f_odelete,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "input_value",         f_ovalue,          -2, ACCESS_EXP,  NULL, false },
    { "input_changed",       f_ochanged,         1, ACCESS_EXP,  NULL, false },
    { "output_create",       f_ocreate,         -5, ACCESS_EXP,  NULL, false },
    { "output_delete",       f_odelete,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "output_value",        f_ovalue,          -2, ACCESS_EXP,  NULL, false },
    { "menu_create",         f_mcreate,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "menu_add",            f_madd,       INT_MIN, ACCESS_EXP,  NULL, false },
    { "menu_text",           f_mtext,           -3, ACCESS_EXP,  NULL, false },
    { "menu_delete",         f_mdelete,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "menu_choice",         f_mchoice,         -2, ACCESS_EXP,  NULL, false },
    { "menu_changed",        f_mchanged,         1, ACCESS_EXP,  NULL, false },
    { "toolbox_changed",     f_tb_changed, INT_MIN, ACCESS_EXP,  NULL, false },
    { "toolbox_wait",        f_tb_wait,    INT_MIN, ACCESS_EXP,  NULL, false },
    { "object_delete",       f_objdel,     INT_MIN, ACCESS_EXP,  NULL, false },
    { "object_change_label", f_obj_clabel,       2, ACCESS_EXP,  NULL, false },
    { "object_enable",       f_obj_xable,        2, ACCESS_EXP,  NULL, false },
    { "hide_toolbox",        f_freeze,           1, ACCESS_EXP,  NULL, false },
    { "show_message",        f_showm,            1, ACCESS_EXP,  NULL, false },
    { "draw_marker",         f_setmark,         -4, ACCESS_EXP,  NULL, false },
    { "draw_marker_1d",      f_setmark_1d,      -2, ACCESS_EXP,  NULL, false },
    { "draw_marker_2d",      f_setmark_2d,      -4, ACCESS_EXP,  NULL, false },
    { "clear_marker",        f_clearmark,       -4, ACCESS_EXP,  NULL, false },
    { "clear_marker_1d",     f_clearmark_1d,    -4, ACCESS_EXP,  NULL, false },
    { "clear_marker_2d",     f_clearmark_2d,    -4, ACCESS_EXP,  NULL, false },
    { "mouse_position",      f_get_pos,         -1, ACCESS_EXP,  NULL, false },
    { "curve_button",        f_curve_button,    -2, ACCESS_EXP,  NULL, false },
    { "curve_button_1d",     f_curve_button_1d, -2, ACCESS_EXP,  NULL, false },
    { "curve_button_2d",     f_curve_button_2d, -2, ACCESS_EXP,  NULL, false },
    { "zoom",                f_zoom,            -7, ACCESS_EXP,  NULL, false },
    { "zoom_1d",             f_zoom_1d,         -4, ACCESS_EXP,  NULL, false },
    { "zoom_2d",             f_zoom_2d,         -7, ACCESS_EXP,  NULL, false },
    { "fs_button",           f_fs_button,       -2, ACCESS_EXP,  NULL, false },
    { "fs_button_1d",        f_fs_button_1d,    -2, ACCESS_EXP,  NULL, false },
    { "fs_button_2d",        f_fs_button_2d,    -2, ACCESS_EXP,  NULL, false },
    { "end",                 f_stopsim,          0, ACCESS_EXP,  NULL, false },
    { "abort",               f_abort,            0, ACCESS_EXP,  NULL, false },
};


/* Locally used functions and variables */

static int func_cmp1( const void * a,
                      const void * b );
static int func_cmp2( const void * a,
                      const void * b );


/*--------------------------------------------------------------------*
 * Function parses the function data base in 'Functions' and makes up
 * a complete list of all built-in and user-supplied functions.
 *--------------------------------------------------------------------*/

bool
functions_init( void )
{
    No_File_Numbers = false;
    Dont_Save = false;

    /* 1. Get new memory for the functions structures and copy the built-in
          functions into it.
       2. Parse the function name data base 'Functions' where all additional
          (module) functions have to be listed and add them to the list.
       3. Sort the functions by name so that they can be found using bsearch()
    */

    Num_Func = NUM_ELEMS( Def_fncts );     /* number of built-in functions */

    TRY
    {
        Fncts = T_malloc( sizeof Def_fncts );
        memcpy( Fncts, Def_fncts, sizeof Def_fncts );
        qsort( Fncts, Num_Func, sizeof *Fncts, func_cmp1 );
        Num_Func = func_list_parse( &Fncts, Num_Func );
        qsort( Fncts, Num_Func, sizeof *Fncts, func_cmp1 );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        functions_exit( );
        return false;
    }

    return true;
}


/*------------------------------------------------------------*
 * Function for qsort()ing functions according to their names
 *------------------------------------------------------------*/

static
int func_cmp1( const void * a,
               const void * b )
{
    return strcmp( ( ( Func_T * ) a )->name, ( ( Func_T * ) b )->name );
}


/*-----------------------------------------------------------------*
 * Function gets rid of all loaded functions (i.e. functions from
 * modules). It also closes all files opened due to user requests.
 *-----------------------------------------------------------------*/

void
functions_exit( void )
{
    if ( ! Fncts )
        return;

    /* Get rid of the names of loaded functions (but not the built-in ones) */

    for ( size_t i = 0; i < Num_Func; i++ )
        if ( Fncts[ i ].to_be_loaded )
            T_free( ( char * ) Fncts[ i ].name );

    Fncts = T_free( Fncts );

    /* Clean up the call stack */

#ifndef NDEBUG
    if ( EDL.Call_Stack != NULL )
        eprint( SEVERE, false, "Internal error detected at %s:%d, call stack "
                "not empty!\n", __FILE__, __LINE__ );
#endif

    while ( call_pop( ) )
        /* empty */ ;

    No_File_Numbers = false;
    Dont_Save = false;
    close_all_files( );
}


/*-------------------------------------------------------------------*
 * This function is thought for modules that just need to check if a
 * function exists.
 *-------------------------------------------------------------------*/

int
func_exists( const char * name )
{
    char * fn = T_strdup( name );

    /* If the function name ends in "#1" strip it off, the function for
       the first device of a generic type is stored in the list of functions
       without it.*/

    char *hp;
    if ( ( hp = strrchr( fn, '#' ) ) != NULL && ! strcmp( hp, "#1" ) )
        *hp = '\0';

    void * res = bsearch( fn, Fncts, Num_Func, sizeof *Fncts, func_cmp2 );

    T_free( fn );

    return res != NULL;
}


/*----------------------------------------------------------------------*
 * Function tries to find a function in the list of built-in and loaded
 * functions. If it finds it it creates a new variable on the variables
 * stack with a pointer to the function and returns a pointer to the
 * variable. If the function can't be found it returns a NULL pointer.
 * -> 1. Name of the function
 *    2. Pointer for returning the access flag, i.e. a flag indicating
 *       in which part of the EDL program the function can be used.
 * <- Pointer to variable on variable stack that can be used to execute
 *    the function or NULL if function does not exist. If the function
 *    exists but has not been loaded an exception is thrown.
 *----------------------------------------------------------------------*/

Var_T *
func_get( const char * name,
          int        * acc )
{
    /* We have to help the writers of modules a bit: If they want a
       pointer to a function from within the same module they don't know
       if there are other modules with the same generic type and thus
       have no chance to figure out if they need to append a '#' plus the
       number of the device to the function name to get the correct
       function within the same module.

       We can here figure out if this is needed and which number to append
       by checking the following:

       1. There's still a function call that has not yet returned in which
          case the call stack isn't empty.
       2. The function which is still running comes from a module - in this
          case the device member of the last call stack element isn't NULL.
       3. The function that's still running came from a device that's not
          the first of a set of devices with the same generic type, indicated
          by the dev_count member of the last call stack element being
          larger than 1.
       4. The functions name we got as the argument hasn't already a '#'
          appended to it.

       If now a function of the name passed to this function is found in the
       module from which the call came we append the '#' plus the device
       count number to the name of the function and use this adorned name
       as the name of the function the caller is really looking for. */

    if (    EDL.Call_Stack != NULL
         && EDL.Call_Stack->device != NULL
         && EDL.Call_Stack->dev_count > 1
         && strrchr( name, '#' ) == NULL )
    {
        dlerror( );
        dlsym( EDL.Call_Stack->device->driver.handle, name );
        if ( dlerror( ) == NULL )
        {
            char * sec_name = get_string( "%s#%d", name,
                                          EDL.Call_Stack->dev_count );
            Var_T * func_ptr;

            TRY
            {
                func_ptr = func_get_long( sec_name, acc, true );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( sec_name );
                RETHROW;
            }

            T_free( sec_name );
            return func_ptr;
        }
    }

    return func_get_long( name, acc, true );
}


/*----------------------------------------------------------------*
 * This function is the one really used for finding a function
 * but with an additional argument that indicates if on failure
 * for existing but not loaded functions an error message is
 * printed and an exception is thrown or simply NULL is returned.
 *----------------------------------------------------------------*/

Var_T *
func_get_long( const char * name,
               int        * acc,
               bool         flag )
{
    char * fn = T_strdup( name );

    /* If the function name ends in "#1" strip it off, the function for
       the first device of a generic type is stored in the list of functions
       without it.*/

    char * hp;
    if ( ( hp = strrchr( fn, '#' ) ) != NULL && ! strcmp( hp, "#1" ) )
        *hp = '\0';

    /* Try to find the function by its name and if found create a variable on
       the variable stack with a pointer to the function and the number of
       arguments. Also copy the functions name and access flag. */

    Func_T * f = bsearch( fn, Fncts, Num_Func, sizeof *Fncts, func_cmp2 );

    T_free( fn );

    if ( f == NULL )             /* function not found */
        return NULL;

    if ( f->fnct == NULL )       /* function found but not loaded */
    {
        if ( flag )
        {
            print( FATAL, "%s(): Function has not been loaded.\n", f->name );
            THROW( EXCEPTION );
        }
        else                     /* some callers do their own error handling */
        {
            return NULL;
        }
    }

    Var_T * ret = vars_push( FUNC, f );
    ret->name = T_strdup( name );
    ret->dim = f->nargs;
    if ( acc != NULL )
        *acc = f->access_flag;

    return ret;
}


/*------------------------------------------------------------*
 * Function for bsearch to search for a function by its name.
 *------------------------------------------------------------*/

static int
func_cmp2( const void * a,
           const void * b )
{
    return strcmp( ( const char * ) a, ( ( const Func_T * ) b )->name );
}


/*----------------------------------------------------------------*
 * This function is called to execute an EDL function. It must be
 * able to also handle situations where, for example, an EDL
 * function calls another EDL function, which throws an exception
 * that then is caught by the first function etc.
 *----------------------------------------------------------------*/

Var_T *
func_call( Var_T * volatile f )
{
#ifndef NDEBUG
    /* Check (and double-check) that it's really a function variable - one
       can never be sure someone really got it right... */

    if ( f->type != FUNC )
    {
        eprint( FATAL, false, "Internal error detected at %s:%d.\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }

/* A weaker check than the one we're currently using....

    if ( f->val.fnct < Fncts || f->val.fnct >= Fncts + Num_Func )
    {
        eprint( FATAL, false, "Internal error detected at %s:%d.\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }
*/

    size_t i;
    for ( i = 0; i < Num_Func; i++ )
        if ( Fncts[ i ].fnct == f->val.fnct->fnct )
            break;

    if ( i >= Num_Func )
    {
        eprint( FATAL, false, "Internal error detected at %s:%d.\n",
                __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }
#endif

    /* If the number of function arguments isn't INT_MIN (indicating a
       variable number of arguments without an upper limit) count the number
       of variables on the stack */

    Var_T *ap;
    if ( f->dim > INT_MIN )
    {
        long abs_len = f->dim > 0 ? f->dim : - f->dim;

        /* Count number of arguments on the stack */

        long ac;
        for ( ac = 0, ap = f->next; ap != NULL; ++ac, ap = ap->next )
            /* empty */ ;

        /* If there are too many arguments utter a warning and remove the
           superfluous ones (distinuguish between functions with a fixed
           and a variable number of arguments - in the first case f->dim
           is positive, in the second negative) */

        if ( ac > abs_len )
        {
            print( WARN, "%s(): Too many arguments, discarding superfluous "
                   "arguments.\n", f->name );

            for ( ac = 0, ap = f->next; ac < abs_len; ++ac, ap = ap->next )
                /* empty */ ;
            while ( ( ap = vars_pop( ap ) ) != NULL )
                /* empty */ ;
        }

        /* For functions with a fixed number of arguments (a positive number
           of arguments is specified in the nargs entry of the functions
           structure, which has been copied to the dim entry of the variables
           structure) less arguments than needed by the function is a fatal
           error. */

        if ( f->dim >= 0 && ac < f->dim )
        {
            print( FATAL, "%s(): Function expects %d argument%s but only %d "
                   "%s found.\n", f->name, f->dim, f->dim == 1 ? "" : "s",
                   ac, ac == 1 ? "was" : "were" );
            THROW( EXCEPTION );
        }
    }

    /* Now call the function after storing some information about the
       function on the call stack */

    if ( call_push( f->val.fnct, NULL,
                    f->val.fnct->device ? f->val.fnct->device->name : NULL,
                    f->val.fnct->device ? f->val.fnct->device->count : 0 )
         == NULL )
        THROW( OUT_OF_MEMORY_EXCEPTION );

    Var_T *ret = NULL;
    TRY
    {
        ret = f->val.fnct->fnct( f->next );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
#ifndef NDEBUG
        if ( ! vars_exist( f ) )
        {
            if (    EDL.Call_Stack->f != NULL
                 && ! EDL.Call_Stack->f->to_be_loaded )
                eprint( FATAL, false, "Internal error detected at %s:%d.\n",
                        __FILE__, __LINE__ );
            else
                print( FATAL, "Function %s() from module %s.fsc2_so messed up "
                       "the variable stack at %s:%d.\n",
                       EDL.Call_Stack->f->name,
                       EDL.Call_Stack->f->device->name, __FILE__, __LINE__ );
            call_pop( );
            RETHROW;
        }
#endif
        call_pop( );

        for ( ap = f; ap != NULL; ap = vars_pop( ap ) )
            /* empty */ ;

        RETHROW;
    }
#ifndef NDEBUG

    /* Before starting to delete the now defunct variables do another sanity
       check, i.e. test that the variables stack didn't get corrupted. */

    if ( ! vars_exist( f ) )
    {
        if ( EDL.Call_Stack->f != NULL && ! EDL.Call_Stack->f->to_be_loaded )
            eprint( FATAL, false, "Internal error detected at %s:%d.\n",
                    __FILE__, __LINE__ );
        else
            print( FATAL, "Function %s() from module %s.fsc2_so messed up the "
                   "variables stack at %s:%d.\n", EDL.Call_Stack->f->name,
                   EDL.Call_Stack->f->device->name, __FILE__, __LINE__ );
        THROW( EXCEPTION );
    }
#endif

    call_pop( );

    /* Finally do the clean up, i.e. remove the variable with the function
       and all parameters that survived - just keep the return value. */

    for ( ap = f; ap != ret; ap = vars_pop( ap ) )
        /* empty */ ;

    return ret;
}


/*-----------------------------------------------------------------------*
 * The following functions are for maintaining a stack of EDL (and other
 * module) functions calls, which is actually implemented as a linked
 * list. The stack pointer, i.e. the pointer to the head of the linked
 * list, is the 'Call_Stack' member of the global 'EDL' structure. The
 * topmost element of the stack (i.e. the head of the linked list)
 * always refers to the currently running (EDL) function.
 * For the main program the functions only get called when an EDL
 * function is invoked. In contrast, for modules they also are called
 * for hook-function calls and, for pulser modules, also for calls of
 * the functions declared in the pulser structure (i.e. functions, that
 * the pulser module must export to allow the setting of the pulsers
 * properties and the creation of pulses). The information stored in the
 * call stack elements is a pointer to the EDL function structure (if
 * applicable, i.e. for EDL function calls), a pointer to the module
 * structure (for calls from modules) and the name of the device
 * controlled by the module (i.e. not the module name, but the name as
 * it should appear in error messages for the device), and, finally, the
 * number of the device (this can be larger than 1 if there's more than
 * one devices of the same generic type).
 * These informations are used in two situations. First when printing
 * messages the print() function is supposed to prepend an EDL file name
 * and line number, device name (if applicable) and function name to the
 * message. It gets the device and function name from the current call
 * stack entry. Second, when within a module an EDL function from the
 * same module is called (via func_get() and func_call()) the writer of
 * the module has no information if there are other modules with the
 * same generic type loaded, supplying a function by the same name. The
 * information in the current call stack entry is used to determine this
 * and thus to automatically return the appropriate fucntion handle to
 * the module, i.e. the function from the same module the func_get()
 * call came from.
 *-----------------------------------------------------------------------*/

Call_Stack_T *
call_push( Func_T     * f,
           Device_T   * device,
           const char * device_name,
           int          dev_count )
{
    const char * t;

    Call_Stack_T * cs = T_malloc( sizeof *cs );
    cs->next = EDL.Call_Stack;
    cs->f = f;

    if ( f != NULL )
    {
        cs->device = f->device;
    }
    else
    {
        fsc2_assert( device != NULL );
        cs->device = device;
    }
    cs->dev_name = device_name;
    cs->dev_count = dev_count;

    /* If this is a function for a pulser figure out the number of the pulser
       (there might be more than one pulser) */

    if (    f != NULL
         && f->device != NULL
         && f->device->generic_type != NULL
         && ! strcasecmp( f->device->generic_type, PULSER_GENERIC_TYPE ) )
    {
        if ( ( t = strrchr( f->name, '#' ) ) != NULL )
            Cur_Pulser = cs->Cur_Pulser = T_atol( t + 1 ) - 1;
    }
    else
    {
        cs->Cur_Pulser = Cur_Pulser;
    }

    /* If this is a call of function within one of the modules during the
       test run add an extremely rough estimate for the mean time spend in
       the function for the call to the global variable that is used to keep
       a time estimate for the modules. */

    if ( Fsc2_Internals.mode == TEST && device_name != NULL )
        EDL.experiment_time += MODULE_CALL_ESTIMATE;

    return EDL.Call_Stack = cs;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

Call_Stack_T *
call_pop( void )
{
    if ( EDL.Call_Stack == NULL )
        return NULL;

    Call_Stack_T * cs = EDL.Call_Stack;
    EDL.Call_Stack = cs->next;

    Cur_Pulser = cs->Cur_Pulser;

    T_free( cs );

    return EDL.Call_Stack;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
