/*
  $Id$
*/

#if ! defined FUNC_HEADER
#define FUNC_HEADER

#include "fsc2.h"


typedef struct
{
	const char *name;                    /* name of the function */
    Var * ( * fnct )( Var * );           /* pointer to the function */
	long nargs;                          /* number of arguments */
    int access_flag;                     /* asscessibility flag */
	bool to_be_loaded;                   /* if set function has to be loaded */
} Func;

typedef struct
{
	const char *name;
    Var * ( * fnct )( Var * );
} Function_List;


bool functions_init_hook( void );
void functions_exit_hook( void );
Var *func_get( char *name, int *access );
Var *func_call( Var *f );
Var *print_args( Var *print_statement );


/* from func_list_lexer.flex */

void func_list_parse( Func **fncts, int num_def_func, int *num_func );

void load_user_functions( Func *fncts, int num_def_func, int num_func );


#endif  /* ! FUNC_HEADER */
