/*
  $Id$
*/

#if ! defined FUNC_HEADER
#define FUNC_HEADER

#include "fsc2.h"


typedef struct
{
	const char *name;                 /* name of the function */
    Var * ( * fnct )( Var * );        /* pointer to the function */
	long nargs;                       /* number of arguments */
    int access_flag;                  /* asscessibility flag */
	bool to_be_loaded;                /* if set function has to be loaded */
} Func;


typedef struct {
	void *handle;
	int ( * init_hook ) ( void );
	bool is_init_hook;
	int ( * test_hook ) ( void );
	bool is_test_hook;
	int ( * exp_hook ) ( void );
	bool is_exp_hook;
	void ( * exit_hook ) ( void );
	bool is_exit_hook;
} Lib_Struct;


bool functions_init( void );
void functions_exit( void );
void load_all_drivers( void );
void run_test_hooks( void );
void run_exp_hooks( void );
int get_lib_symbol( const char *from, const char *symbol, void **symbol_ptr );
Var *func_get( char *name, int *access );
Var *func_call( Var *f );

/* from func_list_lexer.flex */

void func_list_parse( Func **fncts, int num_def_func, int *num_func );

#endif  /* ! FUNC_HEADER */
