/*
  $Id$
*/

#if ! defined PRIM_EXP_HEADER
#define PRIM_EXP_HEADER

#include "fsc2.h"


#define IF_TOK     2049
#define ELSE_TOK   2050
#define ENDIF_TOK  2051
#define WHILE_TOK  2052
#define DONE_TOK   2053
#define CONT_TOK   2054
#define BREAK_TOK  2055


bool store_exp( FILE *in );
void forget_prg( void );
void prim_check( void );


#endif   /* ! PRIM_EXP_HEADER */
