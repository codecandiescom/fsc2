/*
  $Id$
*/

#if ! defined PRIM_EXP_HEADER
#define PRIM_EXP_HEADER


/* this typedef MUST be identical to the YYSTYPE union defined in
   `prim_exp_parser.h' which in turn results from `prim_exp_parser.y' !!!! */

typedef union {
		long   lval;
		double dval;
		char   *sptr;
		Var    *vptr;
} Token_Val;


typedef struct PT_ {
	int token;                /* type of current token */
	Token_Val tv;             /* token's value as needed by the parser */
	struct PT_ *start;        /* used in if, while etc. [1] */
	struct PT_ *end;          /* used in while, if's etc. [2] */
	struct PT_ *altern;       /* used in if's */
	char *Fname;              /* name of file the token comes from */
	long Lc;                  /* number of line the token comes from */
} Prg_Token;



/*
  [1] In while and repeat loops `start' points to the start of the executable
	  block, in continue's it points to the corresponding while or repeat.  In
	  if's it also points to the executable block (the on to be executed if
	  the condition is met).
  [2] in while and repeat loops `end' points to the statement following the
      block. In break's it points also the statement following the loop. In
	  if's it points to the statement following the if-else construct.
  [3] In if's with an else `altern' points to the executable block following
      the else.
*/


#define	E_VAR_TOKEN	    258
#define	E_VAR_REF	    259
#define	E_FUNC_TOKEN	260
#define	E_INT_TOKEN	    261
#define	E_FLOAT_TOKEN	262
#define	E_STR_TOKEN	    263
#define	E_EQ	        264
#define	E_LT	        265
#define	E_LE	        266
#define	E_GT	        267
#define	E_GE	        268
#define	E_NS_TOKEN	    269
#define	E_US_TOKEN	    270
#define	E_MS_TOKEN	    271
#define	E_S_TOKEN	    272
#define	E_NEG	        273

#define IF_TOK         2049
#define ELSE_TOK       2050
#define WHILE_TOK      2052
#define CONT_TOK       2054
#define BREAK_TOK      2055
#define REPEAT_TOK     2056


#include "fsc2.h"


void store_exp( FILE *in );
void forget_prg( void );
void prim_loop_setup( void );
void setup_while_or_repeat( int type, long *pos );
void setup_if_else( int type, long *pos, Prg_Token *cur_wr );
int conditionlex( void );

#endif   /* ! PRIM_EXP_HEADER */
