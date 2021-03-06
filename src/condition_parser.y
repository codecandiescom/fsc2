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


/*******************************************************************/
/* This is the parser for conditions in if's, while's and repeat's */
/*******************************************************************/


%{

#include "fsc2.h"


void conditionparser_init( void );

/* locally used functions */

static void conditionerror( const char * s );


/* A problem are expressions of the form 'expr & expr' or 'expr | expr'.
   Here the second part (i.e. after the '&' or '|') must not be evaluated if
   the first part already determines the final result completely because it
   is false (for the AND case) or true (for OR). In theses cases the second
   expression must not be evaluated where it is especially important not to
   run functions that might have side-effects. Thus when such a case is
   detected 'Dont_exec' is set to 1 which will prevent the evaluation.
   At the end of the expression 'Dont_exec' is decremented back to 0.

   A further problem are cases like 'expr1 | ( expr2 & expr3 ) | expr4'. If,
   e.g. expr2 is false expr3 must not evaluated but expr4 must (at least if
   expr 1 was false). So skipping all the rest after expr2 is not possible
   and thus 'Dont_exec' must work as a counter, incremented on expr2 and again
   decremented at the end of expr3. Thus it's again 0 when expr4 is dealt with
   which in turn is evaluated.

   'Dont_exec' must be set to zero before the parser is started to make sure
   it's 0 even if, due to syntax errors etc., the last run of the parser was
   interrupted with 'Dont_exec' being still non-zero. */

static int Dont_exec = 0;

%}


/* The following union and the token definitions MUST be identical to
   the ones in 'exp.h' and 'exp_run_parser.y' ! */

%union {
    long   lval;
    double dval;
    char   *sptr;
    Var_T  *vptr;
}


%token <vptr> E_VAR_TOKEN     258       /* variable name */
%token <vptr> E_VAR_REF       259
%token <vptr> E_FUNC_TOKEN    260       /* function name */
%token <lval> E_INT_TOKEN     261
%token <dval> E_FLOAT_TOKEN   262
%token <sptr> E_STR_TOKEN     263
%token E_EQ                   264
%token E_NE                   265
%token E_LT                   266
%token E_LE                   267
%token E_GT                   268
%token E_GE                   269
%token E_NEG                  270
%token E_AND                  271
%token E_OR                   272
%token E_XOR                  273
%token E_NOT                  274
%token <lval> E_PPOS          275
%token <lval> E_PLEN          276
%token <lval> E_PDPOS         277
%token <lval> E_PDLEN         278
%token E_PLSA                 279
%token E_MINA                 280
%token E_MULA                 281
%token E_DIVA                 282
%token E_MODA                 283
%token E_EXPA                 284

%token IF_TOK                2049
%token ELSE_TOK              2050
%token UNLESS_TOK            2051
%token WHILE_TOK             2052
%token UNTIL_TOK             2053
%token NEXT_TOK              2054
%token BREAK_TOK             2055
%token REPEAT_TOK            2056
%token FOR_TOK               2057
%token FOREVER_TOK           2058
%token ON_STOP_TOK           3333


%type <vptr> expr list1 l1e ind strs

%left '?' ':'
%left E_AND E_OR E_XOR
%left E_EQ E_NE E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right E_NEG E_NOT
%right '^'


%%


input:   expr                     { fsc2_assert( Dont_exec == 0 );
                                    YYACCEPT; }
;

expr:    E_INT_TOKEN              { if ( ! Dont_exec )
                                        $$ = vars_push( INT_VAR, $1 ); }
       | E_FLOAT_TOKEN            { if ( ! Dont_exec )
                                        $$ = vars_push( FLOAT_VAR, $1 ); }
       | E_VAR_TOKEN              { if ( ! Dont_exec )
                                        $$ = vars_push_copy( $1 ); }
       | E_VAR_TOKEN '['          { if ( ! Dont_exec )
                                        vars_arr_start( $1 ); }
         list1 ']'                { if ( ! Dont_exec )
                                        $$ = vars_arr_rhs( $4 ); }
       | E_FUNC_TOKEN '('
         list2 ')'                { if ( ! Dont_exec )
                                        $$ = func_call( $1 );
                                    else
                                        vars_pop( $1 ); }
       | E_FUNC_TOKEN             { print( FATAL, "'%s' is a predefined "
                                           "function.\n", $1->name );
                                    THROW( EXCEPTION ); }
       | strs
       | E_VAR_REF                { if ( Dont_exec )
                                         vars_pop( $1 ); }
       | E_VAR_TOKEN '('          { print( FATAL, "'%s' isn't a function.\n",
                                           $1->name );
                                    THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['         { print( FATAL, "'%s' is a predefined "
                                            "function.\n", $1->name );
                                    THROW( EXCEPTION ); }
       | E_PPOS                   { if ( ! Dont_exec )
                                        $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                   { if ( ! Dont_exec )
                                        $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                  { if ( ! Dont_exec )
                                        $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                  { if ( ! Dont_exec )
                                        $$ = p_get_by_num( $1, P_DLEN ); }
       | expr E_AND               { if ( ! Dont_exec )
                                    {
                                        if ( ! check_result( $1 ) )
                                        {
                                            Dont_exec++;
                                            vars_pop( $1 );
                                        }
                                    }
                                    else
                                        Dont_exec++;
                                  }
         expr                     { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_AND, $1, $4 );
                                    else if ( ! --Dont_exec )
                                        $$ = vars_push( INT_VAR, 0 );
                                  }
       | expr E_OR                { if ( ! Dont_exec )
                                    {
                                        if ( check_result( $1 ) )
                                        {
                                            Dont_exec++;
                                            vars_pop( $1 );
                                        }
                                    }
                                    else
                                        Dont_exec++;
                                  }
         expr                     { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_OR, $1, $4 );
                                    else if ( ! --Dont_exec )
                                        $$ = vars_push( INT_VAR, 1 );
                                  }
       | expr E_XOR expr          { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr               { if ( ! Dont_exec )
                                        $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr           { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr           { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_UNEQUAL,
                                                        $1, $3 ); }
       | expr E_LT expr           { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr           { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr           { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_LESS_EQUAL,
                                                        $1, $3 ); }
       | expr E_GE expr           { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_LESS_EQUAL,
                                                        $3, $1 ); }
       | expr '+' expr            { if ( ! Dont_exec )
                                        $$ = vars_add( $1, $3 ); }
       | expr '-' expr            { if ( ! Dont_exec )
                                        $$ = vars_sub( $1, $3 ); }
       | expr '*' expr            { if ( ! Dont_exec )
                                        $$ = vars_mult( $1, $3 ); }
       | expr '/' expr            { if ( ! Dont_exec )
                                        $$ = vars_div( $1, $3 ); }
       | expr '%' expr            { if ( ! Dont_exec )
                                        $$ = vars_mod( $1, $3 ); }
       | expr '^' expr            { if ( ! Dont_exec )
                                        $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG     { if ( ! Dont_exec )
                                        $$ = $2; }
       | '-' expr %prec E_NEG     { if ( ! Dont_exec )
                                        $$ = vars_negate( $2 ); }
       | '(' expr ')'             { if ( ! Dont_exec )
                                        $$ = $2; }
       | expr '?'                  { if ( ! Dont_exec )
                                     {
                                         if ( ! check_result( $1 ) )
                                             Dont_exec++;
                                         vars_pop( $1 );
                                     }
                                     else
                                         Dont_exec += 2;
                                   }
         expr ':'                  { if ( ! Dont_exec )
                                         Dont_exec++;
                                     else
                                         Dont_exec--;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = $7;
                                     else if ( ! -- Dont_exec )
                                         $$ = $4;
                                   }
;

/* list of indices for access of an array element */

list1:   /* empty */                 { if ( ! Dont_exec )
                                           $$ = vars_push( UNDEF_VAR ); }
       | l1e
;

l1e:     ind                        { if ( ! Dont_exec )
                                           $$ = $1; }
       | l1e ',' ind                { if ( ! Dont_exec )
                                           $$ = $3; }
;

ind:     expr                       { if ( ! Dont_exec )
                                          $$ = $1; }
       | expr ':'                   { if ( ! Dont_exec )
                                          vars_push( STR_VAR, ":" ); }
         expr                       { if ( ! Dont_exec )
                                          $$ = $4; }
;

/* list of function arguments */

list2:   /* empty */
       | l2e
;

l2e:     expr                       { }
       | l2e ',' expr               { }
;

strs:    E_STR_TOKEN                { if ( ! Dont_exec )
                                          $$ = vars_push( STR_VAR, $1 ); }
       | strs E_STR_TOKEN           { if ( ! Dont_exec )
                                      {
                                          Var_T *v = vars_push( STR_VAR, $2 );
                                          $$ = vars_add( v->prev, v );
                                      }
                                    }
;

%%


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static void
conditionerror( const char * s  UNUSED_ARG )
{
    print( FATAL, "Syntax error in loop or IF/UNLESS condition.\n" );
    THROW( EXCEPTION );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

void
conditionparser_init( void )
{
    Dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
