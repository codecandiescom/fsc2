/*
  $Id$
*/

#if ! defined FUNC_UTIL_HEADER
#define FUNC_UTIL_HEADER


#include "fsc2.h"


typedef struct {
	FILE *fp;
	char *name;
} FILE_LIST;


Var *f_print(   Var *v );
Var *f_wait(    Var *v );
Var *f_init_1d( Var *v );
Var *f_init_2d( Var *v );
Var *f_display( Var *v );
Var *f_clearcv( Var *v );
Var *f_getf(    Var *v );
Var *f_save(    Var *v );
Var *f_fsave(   Var *v );
Var *f_save_p(  Var *v );
Var *f_save_o(  Var *v );
Var *f_save_c(  Var *v );


#endif  /* ! FUNC_UTIL_HEADER */
