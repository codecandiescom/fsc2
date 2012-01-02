/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


#if ! defined EXP_HEADER
#define EXP_HEADER


/* This typedef MUST be identical to the YYSTYPE union defined in
   `exp_run_parser.h' which in turn results from `exp_run_parser.y' !! */

typedef union Token_Val Token_Val_T;

union Token_Val {
        long   lval;
        double dval;
        char   *sptr;
        Var_T  *vptr;
};


typedef struct Simp_Var Simp_Var_T;
typedef struct Prg_Token Prg_Token_T;


struct Simp_Var {
    int    type;              /* type of data */
    long   lval;              /* for long integer data */
    double dval;              /* for floating point data */
};


struct Prg_Token {
    int       token;          /* type of current token */
    Token_Val_T tv;           /* tokens value as needed by the parser */
    Prg_Token_T *start;       /* used in IF, WHILE etc. [1] */
    Prg_Token_T *end;         /* used in WHILE, IF etc. [2] */
    union {
        struct {
            long max;         /* maximum counter value for REPEAT loops */
            long act;         /* actual counter value for REPEAT loops */
        } repl;
        struct {
            Var_T *act;       /* loop variable of FOR loop */
            Simp_Var_T end;   /* maximum value of loop variable */
            Simp_Var_T incr;  /* increment for loop variable */
        } forl;
    } count;
    long      counter;        /* counts number of loop repetitions [3] */
    char      *Fname;         /* name of file the token was found in */
    long      Lc;             /* number of line the token was found in */
};


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
      checked but reevaluated, e.g. if the counter for a FOR loop is zero the
      loop variable has to be initialized. Thus it has always to be reset to
      zero at the end of a loop (either when the end condition is met or a
      BREAK statement is executed).
*/


#include "fsc2.h"


void store_exp( FILE * /* in */ );

void forget_prg( void );

int exp_testlex( void );

void exp_test_run( void );

int exp_runlex( void );

int conditionlex( void );

bool test_condition( Prg_Token_T * /* cur */ );

void get_max_repeat_count( Prg_Token_T * /* cur */ );

void get_for_cond( Prg_Token_T * /* cur */ );

bool test_for_cond( Prg_Token_T * /* cur */ );

bool check_result( Var_T * /* v */ );


#endif   /* ! EXP_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
