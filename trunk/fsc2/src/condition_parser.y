/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


/*******************************************************************/
/* This is the parser for conditions in if's, while's and repeat's */
/*******************************************************************/


%{

#include "fsc2.h"

int conditionparse( void );
void conditionparser_init( void );

/* locally used functions */

static void conditionerror( const char *s );


/* A problem are expressions of the form 'expr & expr' or 'expr | expr'.
   Here the second part (i.e. after the '&' or '|') must not be evaluated if
   the first part already determines the final result completely because it
   is false (for the AND case) or true (for OR). In theses cases the second
   expression must not be evaluated where it is especially important not to
   run functions that might have side-effects. Thus when such a case is
   detected 'dont_exec' is set to 1 which will prevent the evaluation.
   At the end of the expression 'dont_exec' is decremented back to 0.

   A further problem are cases like 'expr1 | ( expr2 & expr3 ) | expr4'. If,
   e.g. expr2 is false expr3 must not evaluated but expr4 must (at least if
   expr 1 was false). So, skipping all the rest after expr2 is not possible
   and thus 'dont_exec' must work as a counter, incremented on expr2 and again
   decremented at the end of expr3. Thus it's again 0 when expr4 is dealt with
   which in turn is evaluated.

   'dont_exec' must be set to zero before the parser is started to make sure
   it's 0 even if, due to syntax errors etc., the last run of the parser was
   interrupted with 'dont_exec' being still non-zero. */

static int dont_exec = 0;

%}


/* The following union and the token definitions MUST be identical to
   the ones in 'exp.h' and 'exp_run_parser.y' ! */

%union {
	long   lval;
    double dval;
	char   *sptr;
	Var    *vptr;
}


%token E_VAR_TOKEN	  258
%token E_VAR_REF	  259
%token E_FUNC_TOKEN	  260
%token E_INT_TOKEN	  261
%token E_FLOAT_TOKEN  262
%token E_STR_TOKEN	  263
%token E_EQ			  264
%token E_NE			  265
%token E_LT			  266
%token E_LE			  267
%token E_GT			  268
%token E_GE			  269
%token E_NEG		  270
%token E_AND          271
%token E_OR           272
%token E_XOR          273
%token E_NOT          274
%token E_PPOS         275
%token E_PLEN         276
%token E_PDPOS        277
%token E_PDLEN        278
%token E_PLSA         279
%token E_MINA         280
%token E_MULA         281
%token E_DIVA         282
%token E_MODA         283
%token E_EXPA         284

%token <vptr> E_VAR_TOKEN         /* variable name */
%token <vptr> E_VAR_REF
%token <vptr> E_FUNC_TOKEN        /* function name */
%token <lval> E_INT_TOKEN
%token <dval> E_FLOAT_TOKEN
%token <sptr> E_STR_TOKEN
%token E_EQ E_NE E_LT E_LE E_GT E_GE
%token <lval> E_PPOS E_PLEN E_PDPOS E_PDLEN

%type <vptr> expr list1 l1e

%left '?' ':'
%left E_AND E_OR E_XOR
%left E_EQ E_NE E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right E_NEG E_NOT
%right '^'


%%


input:   expr                     { fsc2_assert( dont_exec == 0 );
                                    YYACCEPT; }
;

expr:    E_INT_TOKEN              { if ( ! dont_exec )
                                        $$ = vars_push( INT_VAR, $1 ); }
       | E_FLOAT_TOKEN            { if ( ! dont_exec )
		                                $$ = vars_push( FLOAT_VAR, $1 ); }
       | E_VAR_TOKEN              { if ( ! dont_exec )
		                                $$ = vars_push_copy( $1 ); }
       | E_VAR_TOKEN '['          { if ( ! dont_exec )
		                                vars_arr_start( $1 ); }
         list1                    { if ( $4 == NULL && ! dont_exec )
                                        $$ = vars_push( UNDEF_VAR ); }
		 ']'                      { if ( ! dont_exec )
                                        $$ = vars_arr_rhs( $4 ); }
       | E_FUNC_TOKEN '('
	     list2 ')'                { if ( ! dont_exec )
			                            $$ = func_call( $1 );
	                                else
										vars_pop( $1 ); }
       | E_FUNC_TOKEN             { print( FATAL, "'%s' is a predefined "
										   "function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | E_VAR_REF                { if ( dont_exec )
		                                 vars_pop( $1 ); }
       | E_VAR_TOKEN '('          { print( FATAL, "'%s' isn't a function.\n",
										   $1->name );
	                                THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['         { print( FATAL, "'%s' is a predefined "
                                            "function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | E_PPOS                   { if ( ! dont_exec )
                                        $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                   { if ( ! dont_exec )
                                        $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                  { if ( ! dont_exec )
                                        $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                  { if ( ! dont_exec )
                                        $$ = p_get_by_num( $1, P_DLEN ); }
	   | expr E_AND               { if ( ! dont_exec )
	                                {
                                        if ( ! check_result( $1 ) )
										{
											dont_exec++;
											vars_pop( $1 );
										}
									}
									else
										dont_exec++;
	                              }
	     expr                     { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_AND, $1, $4 );
		                            else if ( ! --dont_exec )
										$$ = vars_push( INT_VAR, 0 );
		                          }
       | expr E_OR                { if ( ! dont_exec )
	                                {
                                        if ( check_result( $1 ) )
										{
											dont_exec++;
											vars_pop( $1 );
										}
									}
									else
										dont_exec++;
	                              }
	     expr                     { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_OR, $1, $4 );
		                            else if ( ! --dont_exec )
										$$ = vars_push( INT_VAR, 1 );
		                          }
       | expr E_XOR expr          { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr               { if ( ! dont_exec )
                                        $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr           { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr           { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_UNEQUAL,
														$1, $3 ); }
       | expr E_LT expr           { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr           { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr           { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_LESS_EQUAL,
														$1, $3 ); }
       | expr E_GE expr           { if ( ! dont_exec )
                                        $$ = vars_comp( COMP_LESS_EQUAL,
														$3, $1 ); }
       | expr '+' expr            { if ( ! dont_exec )
                                        $$ = vars_add( $1, $3 ); }
       | expr '-' expr            { if ( ! dont_exec )
                                        $$ = vars_sub( $1, $3 ); }
       | expr '*' expr            { if ( ! dont_exec )
                                        $$ = vars_mult( $1, $3 ); }
       | expr '/' expr            { if ( ! dont_exec )
                                        $$ = vars_div( $1, $3 ); }
       | expr '%' expr            { if ( ! dont_exec )
                                        $$ = vars_mod( $1, $3 ); }
       | expr '^' expr            { if ( ! dont_exec )
                                        $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG     { if ( ! dont_exec )
                                        $$ = $2; }
       | '-' expr %prec E_NEG     { if ( ! dont_exec )
                                        $$ = vars_negate( $2 ); }
       | '(' expr ')'             { if ( ! dont_exec )
                                        $$ = $2; }
       | expr '?'                  { if ( ! dont_exec )
	                                 {
		                                 if ( ! check_result( $1 ) )
		                                     dont_exec++;
		                                 vars_pop( $1 );
									 }
	                                 else
										 dont_exec += 2;
	                               }
		 expr ':'                  { if ( ! dont_exec )
										 dont_exec++;
		 							 else
										 dont_exec--;
	                               }
		 expr                      { if ( ! dont_exec )
										 $$ = $7;
		                             else if ( ! -- dont_exec )
									     $$ = $4;
                                   }
;

/* list of indices for access of an array element */

list1:   /* empty */                 { if ( ! dont_exec )
	                                       $$ = vars_push( UNDEF_VAR ); }
	   | l1e
;

l1e:     expr                        { if ( ! dont_exec )
	                                       $$ = $1; }
       | l1e ',' expr                { if ( ! dont_exec )
	                                       $$ = $3; }
;

/* list of function arguments */

list2:   /* empty */
       | l2e
;

l2e:     exprs
       | l2e ',' exprs
;

exprs:   expr                       { }
       | strs
;

strs:    E_STR_TOKEN                { if ( ! dont_exec )
	                                       vars_push( STR_VAR, $1 ); }
       | strs E_STR_TOKEN           { if ( ! dont_exec )
	                                  {
	                                       Var *v = vars_push( STR_VAR, $2 );
	                                       vars_add( v->prev, v );
									  }
	                                }
;

%%


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

static void conditionerror( const char *s )
{
	UNUSED_ARGUMENT( s );

	print( FATAL, "Syntax error in loop or IF/UNLESS condition.\n" );
	THROW( EXCEPTION );
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

void conditionparser_init( void )
{
	dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
