/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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

/* locally used functions */

int conditionparse( void );
static bool check_result( Var *v );
static void conditionerror( const char *s );


/* A problem are expressions of the form 'expr & expr' or 'expr | expr'.
   Here the second part (i.e. after the '&' or '|') must not be evaluated if
   the first part already determines the final result completely because it
   is false (for the AND case) or true (for OR). In theses cases the second
   expression must not be evaluated where it is especially important not to
   run functions that might have side-effects. Thus when such a case is
   detected 'condition_gobble' is set to 1 which will prevent the evaluation.
   At the end of the expression 'condition_gobble' is decremented back to 0.

   A further problem are cases like 'expr1 | ( expr2 & expr3 ) | expr4'. If,
   e.g. expr2 is false expr3 must not evaluated but expr4 must (at least if
   expr 1 was false). So, skipping all the rest after expr2 is not possible
   and thus 'condition_gobble' must work as a counter, incremented on expr2
   and again decremented at the end of expr3. Thus it's again 0 when expr4
   is dealt with which in turn is evaluated.

   'condition_gobble' must be set to zero before the parser is started to make
   sure it's 0 even if, due to syntax errors etc., the last run of the parser
   was interrupted with 'condition_gobble' being still non-zero. */

int condition_gobble;


%}


/* The following union and the token definitions MUST be identical to
   the ones in `exp.h' and `exp_run_parser.y' ! */

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
%token E_NT_TOKEN	  270
%token E_UT_TOKEN	  271
%token E_MT_TOKEN	  272
%token E_T_TOKEN	  273
%token E_KT_TOKEN	  274
%token E_MGT_TOKEN	  275
%token E_NU_TOKEN	  276
%token E_UU_TOKEN	  277
%token E_MU_TOKEN	  278
%token E_KU_TOKEN	  279
%token E_MEG_TOKEN	  280
%token E_NEG		  281
%token E_AND          282
%token E_OR           283
%token E_XOR          284
%token E_NOT          285
%token E_PPOS         286
%token E_PLEN         287
%token E_PDPOS        288
%token E_PDLEN        289
%token E_PLSA         290
%token E_MINA         291
%token E_MULA         292
%token E_DIVA         293
%token E_MODA         294
%token E_EXPA         295

%token <vptr> E_VAR_TOKEN         /* variable name */
%token <vptr> E_VAR_REF
%token <vptr> E_FUNC_TOKEN        /* function name */
%token <lval> E_INT_TOKEN
%token <dval> E_FLOAT_TOKEN
%token <sptr> E_STR_TOKEN
%token E_EQ E_NE E_LT E_LE E_GT E_GE
%token <lval> E_PPOS E_PLEN E_PDPOS E_PDLEN

%token E_NT_TOKEN E_UT_TOKEN E_MT_TOKEN E_T_TOKEN E_KT_TOKEN E_MGT_TOKEN
%token E_NU_TOKEN E_UU_TOKEN E_MU_TOKEN E_KU_TOKEN E_MEG_TOKEN
%type <vptr> expr unit list


%left E_AND E_OR E_XOR
%left E_EQ E_NE E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right E_NEG E_NOT
%right '^'


%%


input:   expr                     { YYACCEPT; }
;

expr:    E_INT_TOKEN unit         { if ( ! condition_gobble )
                                        $$ = apply_unit( vars_push( INT_VAR,
																  $1 ), $2 ); }
       | E_FLOAT_TOKEN unit       { if ( ! condition_gobble )
		                                $$ = apply_unit(
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | E_VAR_TOKEN unit         { if ( ! condition_gobble )
		                                $$ = apply_unit( $1, $2 );
	                                else
										vars_pop( $1 ); }
       | E_VAR_TOKEN '['          { if ( ! condition_gobble )
		                                vars_arr_start( $1 );
	                                else
										vars_pop( $1 ); }
         list                     { if ( $4 == NULL && ! condition_gobble )
                                        $$ = vars_push( UNDEF_VAR ); }
		 ']'                      { if ( ! condition_gobble )
                                        $$ = vars_arr_rhs( $4 ); }
         unit                     { if ( $<vptr>7 != NULL &&
										 ! condition_gobble )
                                        $$ = apply_unit( $<vptr>7, $8 ); }
       | E_FUNC_TOKEN '(' list
         ')'                      { if ( ! condition_gobble )
			                            $$ = func_call( $1 );
	                                else
										vars_pop( $1 ); }
         unit                     { if ( ! condition_gobble )
                                        $$ = apply_unit( $<vptr>5, $6 ); }
       | E_VAR_REF                { if ( condition_gobble )
		                                 vars_pop( $1 ); }
       | E_STR_TOKEN              { if ( ! condition_gobble )
		                                 $$ = vars_push( STR_VAR, $<sptr>1 ); }
       | E_VAR_TOKEN '('          { eprint( FATAL, SET, "`%s' isn't a "
											"function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['         { eprint( FATAL, SET, "`%s' is a predefined "
                                            "function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | E_PPOS                   { if ( ! condition_gobble )
                                        $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                   { if ( ! condition_gobble )
                                        $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                  { if ( ! condition_gobble )
                                        $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                  { if ( ! condition_gobble )
                                        $$ = p_get_by_num( $1, P_DLEN ); }
	   | expr E_AND               { if ( ! condition_gobble )
	                                {
                                        if ( ! check_result( $1 ) )
											condition_gobble = 1;
									}
									else
										condition_gobble++;
	                              }
	     expr                     { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_AND, $1, $4 );
		                            else
									{
									    if ( condition_gobble == 1 )
										     $$ = $1;
										condition_gobble--;
									}
		                          }
       | expr E_OR                { if ( ! condition_gobble )
	                                {
                                        if ( check_result( $1 ) )
											condition_gobble = 1;
									}
									else
										condition_gobble++;
	                              }
	     expr                     { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_OR, $1, $4 );
		                            else
									{
									    if ( condition_gobble == 1 )
										     $$ = $1;
										condition_gobble--;
									}
		                          }
       | expr E_XOR expr          { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr               { if ( ! condition_gobble )
                                        $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr           { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr           { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_UNEQUAL,
														$1, $3 ); }
       | expr E_LT expr           { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr           { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr           { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_LESS_EQUAL,
														$1, $3 ); }
       | expr E_GE expr           { if ( ! condition_gobble )
                                        $$ = vars_comp( COMP_LESS_EQUAL,
														$3, $1 ); }
       | expr '+' expr            { if ( ! condition_gobble )
                                        $$ = vars_add( $1, $3 ); }
       | expr '-' expr            { if ( ! condition_gobble )
                                        $$ = vars_sub( $1, $3 ); }
       | expr '*' expr            { if ( ! condition_gobble )
                                        $$ = vars_mult( $1, $3 ); }
       | expr '/' expr            { if ( ! condition_gobble )
                                        $$ = vars_div( $1, $3 ); }
       | expr '%' expr            { if ( ! condition_gobble )
                                        $$ = vars_mod( $1, $3 ); }
       | expr '^' expr            { if ( ! condition_gobble )
                                        $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG     { if ( ! condition_gobble )
                                        $$ = $2; }
       | '-' expr %prec E_NEG     { if ( ! condition_gobble )
                                        $$ = vars_negate( $2 ); }
       | '(' expr ')' unit        { if ( ! condition_gobble )
                                        $$ = apply_unit( $2, $4 ); }
;

unit:    /* empty */              { $$ = NULL; }
       | E_NT_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | E_UT_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | E_MT_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | E_T_TOKEN                { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | E_KT_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | E_MGT_TOKEN              { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | E_NU_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | E_UU_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | E_MU_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | E_KU_TOKEN               { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | E_MEG_TOKEN              { if ( ! condition_gobble )
                                        $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
;


/* List of indices of an array element or list of function arguments */

list :   /* empty */              { $$ = NULL; }
       | expr
       | list ',' expr            { if ( ! condition_gobble )
                                        $$ = $3; }
;


%%


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

static bool check_result( Var *v )
{
    if ( ! ( v->type & ( INT_VAR | FLOAT_VAR ) ) )
		return FALSE;

	/* Test the result - everything nonzero returns OK */

	if ( v->type == INT_VAR )
		return v->val.lval ? OK : FAIL;
	else
		return v->val.dval != 0.0 ? OK : FAIL;
}


/*---------------------------------------------------------*/
/*---------------------------------------------------------*/

static void conditionerror( const char *s )
{
	s = s;                    /* avoid compiler warning */

	eprint( FATAL, SET, "Syntax error in loop or IF/UNLESS condition.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
