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


#if ! defined FUNC_HEADER
#define FUNC_HEADER

#include "fsc2.h"

typedef struct Lib_Struct Lib_Struct_T;
typedef struct Func Func_T;
typedef struct Call_Stack Call_Stack_T;


struct Lib_Struct {
    void * handle;
    char * lib_name;
    int    ( * init_hook ) ( void );
    bool   is_init_hook;
    int    ( * test_hook ) ( void );
    bool   is_test_hook;
    int    ( * end_of_test_hook ) ( void );
    bool   is_end_of_test_hook;
    int    ( * exp_hook ) ( void );
    bool   is_exp_hook;
    int    ( * end_of_exp_hook ) ( void );
    bool   is_end_of_exp_hook;
    bool   exp_hook_is_run;
    void   ( * exit_hook ) ( void );
    bool   is_exit_hook;
    void   ( * child_exit_hook ) ( void );
    bool   is_child_exit_hook;
    bool   init_hook_is_run;
};


#include "devices.h"


/* Concerning the number of argumuments: If the number of arguments is fixed
   this number has to be used, if the function accepts a variable number of
   arguments but only with an upper limit, the negative of the upper limit
   must be used, while for functions with no upper limit 'LONG_MIN' has to
   be specified. */

struct Func {
    const char * name;                   /* name of the function */
    Var_T *      ( * fnct )( Var_T * );  /* pointer to the function */
    int          nargs;                  /* number of arguments */
    int          access_flag;            /* accessibility flag */
    Device_T   * device;                 /* handle of defining device module */
    bool         to_be_loaded;           /* set if function has to be loaded */
};


struct Call_Stack {
    Func_T       * f;
    Device_T     * device;
    const char   * dev_name;
    int            dev_count;
    long           Cur_Pulser;
    Call_Stack_T * next;
};


bool functions_init( void );

void functions_exit( void );

int func_exists( const char * /* name */ );

Var_T *func_get( const char * /* name */,
                 int        * /* acc  */  );

Var_T *func_get_long( const char * /* name */,
                      int        * /* acc  */,
                      bool         /* flag */  );

Var_T *func_call( Var_T * volatile /* f */ );

void close_all_files( void );

Call_Stack_T *call_push( Func_T     * /* f           */,
                         Device_T   * /* device      */,
                         const char * /* device_name */,
                         int          /* dev_count   */  );

Call_Stack_T *call_pop( void );

/* from func_list_lexer.flex */

size_t func_list_parse( Func_T ** /* fncts    */,
                        size_t    /* num_func */  );


#endif  /* ! FUNC_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
