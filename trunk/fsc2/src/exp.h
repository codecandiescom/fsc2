/*
  $Id$
*/

#if ! defined EXP_HEADER
#define EXP_HEADER


/* This typedef MUST be identical to the YYSTYPE union defined in
   `exp_run_parser.h' which in turn results from `exp_run_parser.y' !! */

typedef union {
		long   lval;
		double dval;
		char   *sptr;
		Var    *vptr;
} Token_Val;


#define	E_VAR_TOKEN	    258
#define	E_VAR_REF	    259
#define	E_FUNC_TOKEN	260
#define	E_INT_TOKEN	    261
#define	E_FLOAT_TOKEN	262
#define	E_STR_TOKEN	    263
#define	E_EQ	        264
#define	E_NE	        265
#define	E_LT	        266
#define	E_LE	        267
#define	E_GT	        268
#define	E_GE	        269
#define	E_NT_TOKEN	    270
#define	E_UT_TOKEN	    271
#define	E_MT_TOKEN	    272
#define	E_T_TOKEN	    273
#define	E_NU_TOKEN	    274
#define	E_UU_TOKEN	    275
#define	E_MU_TOKEN	    276
#define	E_KU_TOKEN	    277
#define	E_MEG_TOKEN	    278
#define	E_NEG	        279
#define	E_AND           280
#define	E_OR            281
#define	E_XOR           282
#define	E_NOT           283
#define	E_PPOS          284
#define	E_PLEN          285
#define	E_PDPOS         286
#define	E_PDLEN         287
#define	E_PLSA          288
#define	E_MINA          289
#define	E_MULA          290
#define	E_DIVA          291
#define	E_MODA          291

#define IF_TOK         2049
#define ELSE_TOK       2050
#define WHILE_TOK      2052
#define CONT_TOK       2054
#define BREAK_TOK      2055
#define REPEAT_TOK     2056
#define FOR_TOK        2057
#define ON_STOP_TOK    3333


typedef struct 
{
	int    type;              /* type of data */
	long   lval;              /* for long integer data */
	double dval;              /* for floating point data */
} Simp_Var;


typedef struct PT_ {
	int       token;          /* type of current token */
	Token_Val tv;             /* token's value as needed by the parser */
	struct    PT_ *start;     /* used in if, while etc. [1] */
	struct    PT_ *end;       /* used in while, if's etc. [2] */
	union {
		struct {
			long max;         /* maximum counter value for repeat loops */
			long act;         /* actual counter value for repeat loops */
		} repl;
		struct {
			Var *act;         /* loop variable of for loop */
			Simp_Var end;     /* maximum value of loop variable */
			Simp_Var incr;    /* increment for loop variable */
		} forl;
	} count;
	long counter;             /* counts number of loop repetitions */
	char      *Fname;         /* name of file the token comes from */
	long      Lc;             /* number of line the token comes from */
} Prg_Token;



/*
  [1] In WHILE and REPEAT tokens `start' points to the start of the executable
	  block, in NEXT tokens it points to the corresponding while or
	  repeat. In IF tokens it points to the first executable block (the on to
	  be executed if the condition is met).
  [2] In WHILE and REPEAT tokens `end' points to the statement following the
      block. In BREAK tokens it points also the statement following the
      loop. In IF tokens it points to the else (if there is one). For `}'
      tokens of a WHILE or REPEAT token it points to the WHILE or REPEAT. In
      IF-ELSE blocks it points to the statement directly following the if-else
      construct.
*/


#include "fsc2.h"


void store_exp( FILE *in );
void forget_prg( void );
void exp_test_run( void );
int exp_runlex( void );
int conditionlex( void );
bool test_condition( Prg_Token *cur );
void get_max_repeat_count( Prg_Token *cur );
void get_for_cond( Prg_Token *cur );
bool test_for_cond( Prg_Token *cur );
void save_restore_variables( bool flag );


#endif   /* ! EXP_HEADER */
