/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
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
#define	E_KT_TOKEN	    274
#define	E_MGT_TOKEN	    275
#define	E_NU_TOKEN	    276
#define	E_UU_TOKEN	    277
#define	E_MU_TOKEN	    278
#define	E_KU_TOKEN	    279
#define	E_MEG_TOKEN	    280
#define	E_NEG	        281
#define	E_AND           282
#define	E_OR            283
#define	E_XOR           284
#define	E_NOT           285
#define	E_PPOS          286
#define	E_PLEN          287
#define	E_PDPOS         288
#define	E_PDLEN         289
#define	E_PLSA          290
#define	E_MINA          291
#define	E_MULA          292
#define	E_DIVA          293
#define	E_MODA          294
#define E_EXPA          295

/* The following don't get defined via `exp_run_parser.y' but are for local
   use only (actually, also `run.c' needs them also and gets them from here) */

#define IF_TOK         2049
#define ELSE_TOK       2050
#define UNLESS_TOK     2051
#define WHILE_TOK      2052
#define UNTIL_TOK      2053
#define NEXT_TOK       2054
#define BREAK_TOK      2055
#define REPEAT_TOK     2056
#define FOR_TOK        2057
#define FOREVER_TOK    2058
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
	struct    PT_ *start;     /* used in IF, WHILE etc. [1] */
	struct    PT_ *end;       /* used in WHILE, IF etc. [2] */
	union {
		struct {
			long max;         /* maximum counter value for REPEAT loops */
			long act;         /* actual counter value for REPEAT loops */
		} repl;
		struct {
			Var *act;         /* loop variable of FOR loop */
			Simp_Var end;     /* maximum value of loop variable */
			Simp_Var incr;    /* increment for loop variable */
		} forl;
	} count;
	long      counter;        /* counts number of loop repetitions [3] */
	char      *Fname;         /* name of file the token comes from */
	long      Lc;             /* number of line the token comes from */
} Prg_Token;


typedef struct CB_ {
	char *Fname;
	long Lc;
	struct CB_ *next;
} CB_Stack;


/*
  The following remarks aren't completely in sync with the current state of
  the program... (8.3.2001)

  [1] In WHILE, UNTIL and REPEAT tokens `start' points to the start of the
      executable block, in NEXT tokens it points to the corresponding WHILE or
	  REPEAT. In IF tokens it points to the first executable block (the on to
	  be executed if the condition is met).
  [2] In WHILE, UNTIL and REPEAT tokens `end' points to the statement
      following the block. In BREAK tokens it points also the statement
	  following the loop. In IF tokens it points to the else (if there is
	  one). For `}' tokens of a WHILE, UNTIL or REPEAT token it points to
	  the WHILE, UNTIL or REPEAT. In IF-ELSE blocks it points to the statement
	  directly following the IF-ELSE construct.
  [3] Actually, it does not only count the number of repetitions but also
      indicates by a value of 0 that the loop condition has not to be simply
	  checked but reevaluated, e.g. if counter for a FOR loop is zero the
	  loop variable has to be initialized. Thus it has always to be reset to
	  zero at the end of a loop (either when the end condition is met or a
	  BREAK statement is executed).
*/


#include "fsc2.h"


void store_exp( FILE *in );
void forget_prg( void );
int exp_testlex( void );
void exp_test_run( void );
int exp_runlex( void );
int conditionlex( void );
bool test_condition( Prg_Token *cur );
void get_max_repeat_count( Prg_Token *cur );
void get_for_cond( Prg_Token *cur );
bool test_for_cond( Prg_Token *cur );


#endif   /* ! EXP_HEADER */
