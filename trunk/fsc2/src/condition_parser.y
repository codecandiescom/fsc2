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

/*******************************************************************/
/* This is the parser for conditions in if's, while's and repeat's */
/*******************************************************************/


%{

#include "fsc2.h"

/* locally used functions */

int conditionparse( void );
void conditionerror( const char *s );

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
%type <vptr> expr unit list1


%left E_AND E_OR E_XOR
%left E_EQ E_NE E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right E_NEG E_NOT
%right '^'


%%


input:   expr                      { YYACCEPT; }
;

expr:    E_INT_TOKEN unit         { $$ = apply_unit( vars_push( INT_VAR, $1 ),
													 $2 ); }
       | E_FLOAT_TOKEN unit       { $$ = apply_unit(
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | E_VAR_TOKEN unit         { $$ = apply_unit( $1, $2 ); }
       | E_VAR_TOKEN '['          { vars_arr_start( $1 ); }
         list1 ']'                { $$ = vars_arr_rhs( $4 ); }
         unit                     { $$ = apply_unit( $<vptr>6, $7); }
       | E_FUNC_TOKEN '(' list2
         ')'                      { $$ = func_call( $1 ); }
         unit                     { $$ = apply_unit( $<vptr>5, $6 ); }
       | E_VAR_REF                { $$ = $1; }
       | E_VAR_TOKEN '('          { eprint( FATAL, SET, "`%s' isn't a "
											"function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['         { eprint( FATAL, SET, "`%s' is a predefined "
                                            "function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | E_PPOS                   { $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                   { $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                  { $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                  { $$ = p_get_by_num( $1, P_DLEN ); }
       | expr E_AND expr          { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr E_OR expr           { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr E_XOR expr          { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr               { $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr           { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr           { $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr E_LT expr           { $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr           { $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr           { $$ = vars_comp( COMP_LESS_EQUAL,
													$1, $3 ); }
       | expr E_GE expr           { $$ = vars_comp( COMP_LESS_EQUAL, 
													$3, $1 ); }
       | expr '+' expr            { $$ = vars_add( $1, $3 ); }
       | expr '-' expr            { $$ = vars_sub( $1, $3 ); }
       | expr '*' expr            { $$ = vars_mult( $1, $3 ); }
       | expr '/' expr            { $$ = vars_div( $1, $3 ); }
       | expr '%' expr            { $$ = vars_mod( $1, $3 ); }
       | expr '^' expr            { $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG     { $$ = $2; }
       | '-' expr %prec E_NEG     { $$ = vars_negate( $2 ); }
       | '(' expr ')' unit        { $$ = apply_unit( $2, $4 ); }
;

unit:    /* empty */              { $$ = NULL; }
       | E_NT_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | E_UT_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | E_MT_TOKEN               { $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | E_T_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | E_KT_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | E_MGT_TOKEN              { $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | E_NU_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | E_UU_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | E_MU_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | E_KU_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | E_MEG_TOKEN              { $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
;


/* list of indices for access of an array element */

list1:   /* empty */              { $$ = vars_push( UNDEF_VAR ); }
	   | expr                     { $$ = $1; }
       | list1 ',' expr           { $$ = $3; }
;

/* list of function arguments */

list2:   /* empty */
       | exprs
	   | list2 ',' exprs
;

exprs:   expr                     { }
       | E_STR_TOKEN              { vars_push( STR_VAR, $1 ); }
         strs
;

strs:    /* empty */
       | strs E_STR_TOKEN         { Var *v;
		                            v = vars_push( STR_VAR, $2 );
	                                vars_add( v->prev, v ); }
;


%%


void conditionerror( const char *s )
{
	s = s;                    /* avoid compiler warning */

	eprint( FATAL, SET, "Syntax error in loop or IF/UNLESS condition.\n" );
	THROW( EXCEPTION );
}
