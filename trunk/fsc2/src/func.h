#if ! defined FUNC_HEADER
#define FUNC_HEADER

#include "fsc2.h"


Var *func_get( char *name, int *access );
Var *func_call( Var *f );
Var *print_call( char *fmt );
Var *print_args( Var *print_statement );


#endif  /* ! FUNC_HEADER */
