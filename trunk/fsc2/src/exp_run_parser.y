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

/****************************************************************/
/* This is the parser for the EXPERIMENT section of an EDL file */
/****************************************************************/


%{

#include "fsc2.h"

extern int exp_runlex( void );
extern char *exptext;

/* locally used functions */

int exp_runparse( void );
static void exp_runerror( const char *s );

%}


/* the following union and the token definitions MUST be identical to the ones
   in `exp.h' and `condition_parser.y' ! */

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
%token E_EQ	          264
%token E_NE	          265
%token E_LT	          266
%token E_LE	          267
%token E_GT	          268
%token E_GE	          269
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
%token E_NEG	      281
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
%type <vptr> expr unit line list1



%left E_AND E_OR E_XOR
%left E_EQ E_NE E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right E_NEG
%right E_NOT
%right '^'


%%

input:   /* empty */
       | input eol
       | input line eol
;

eol:     ';'                       { fsc2_assert( Var_Stack == NULL );
                                     if ( do_quit ) 
                                         YYACCEPT; }
       | '}'                       { fsc2_assert( Var_Stack == NULL );
	                                 YYACCEPT; }
;

line:    E_VAR_TOKEN '=' expr      { vars_assign( $3, $1 ); }
       | E_VAR_TOKEN E_PLSA expr   { vars_assign( vars_add( $1, $3 ), $1 ); }
       | E_VAR_TOKEN E_MINA expr   { vars_assign( vars_sub( $1, $3 ), $1 ); }
       | E_VAR_TOKEN E_MULA expr   { vars_assign( vars_mult( $1, $3 ), $1 ); }
       | E_VAR_TOKEN E_DIVA expr   { vars_assign( vars_div( $1, $3 ), $1 ); }
       | E_VAR_TOKEN E_MODA expr   { vars_assign( vars_mod( $1, $3 ), $1 ); }
       | E_VAR_TOKEN E_EXPA expr   { vars_assign( vars_pow( $1, $3 ), $1 ); }

       | E_VAR_TOKEN '['           { vars_arr_start( $1 ); }
         list1 ']'                 { vars_arr_lhs( $4 ) }
         ass                       { fsc2_assert( Var_Stack == NULL ); }
       | E_FUNC_TOKEN '(' list2 ')'{ vars_pop( func_call( $1 ) ); }
       | E_FUNC_TOKEN '['          { eprint( FATAL, SET, "`%s' is a predefined"
											 " function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | E_PPOS '=' expr           { p_set( $1, P_POS, $3 ); }
       | E_PPOS E_PLSA expr        { p_set( $1, P_POS, vars_add(
		                                   p_get_by_num( $1, P_POS ), $3 ) ); }
       | E_PPOS E_MINA expr        { p_set( $1, P_POS, vars_sub(
		                                   p_get_by_num( $1, P_POS ), $3 ) ); }
       | E_PPOS E_MULA expr        { p_set( $1, P_POS, vars_mult(
		                                   p_get_by_num( $1, P_POS ), $3 ) ); }
       | E_PPOS E_DIVA expr        { p_set( $1, P_POS, vars_div(
		                                   p_get_by_num( $1, P_POS ), $3 ) ); }
       | E_PPOS E_MODA expr        { p_set( $1, P_POS, vars_mod(
		                                   p_get_by_num( $1, P_POS ), $3 ) ); }
       | E_PPOS E_EXPA expr        { p_set( $1, P_POS, vars_pow(
		                                   p_get_by_num( $1, P_POS ), $3 ) ); }
       | E_PLEN '=' expr           { p_set( $1, P_LEN, $3 ); }
       | E_PLEN E_PLSA expr        { p_set( $1, P_LEN, vars_add(
		                                   p_get_by_num( $1, P_LEN ), $3 ) ); }
       | E_PLEN E_MINA expr        { p_set( $1, P_LEN, vars_sub(
		                                   p_get_by_num( $1, P_LEN ), $3 ) ); }
       | E_PLEN E_MULA expr        { p_set( $1, P_LEN, vars_mult(
		                                   p_get_by_num( $1, P_LEN ), $3 ) ); }
       | E_PLEN E_DIVA expr        { p_set( $1, P_LEN, vars_div(
		                                   p_get_by_num( $1, P_LEN ), $3 ) ); }
       | E_PLEN E_MODA expr        { p_set( $1, P_LEN, vars_mod(
		                                   p_get_by_num( $1, P_LEN ), $3 ) ); }
       | E_PLEN E_EXPA expr        { p_set( $1, P_LEN, vars_pow(
		                                   p_get_by_num( $1, P_LEN ), $3 ) ); }
       | E_PDPOS '=' expr          { p_set( $1, P_DPOS, $3 ); }
       | E_PDPOS E_PLSA expr       { p_set( $1, P_DPOS, vars_add(
		                                  p_get_by_num( $1, P_DPOS ), $3 ) ); }
       | E_PDPOS E_MINA expr       { p_set( $1, P_DPOS, vars_sub(
		                                  p_get_by_num( $1, P_DPOS ), $3 ) ); }
       | E_PDPOS E_MULA expr       { p_set( $1, P_DPOS, vars_mult(
		                                  p_get_by_num( $1, P_DPOS ), $3 ) ); }
       | E_PDPOS E_DIVA expr       { p_set( $1, P_DPOS, vars_div(
		                                  p_get_by_num( $1, P_DPOS ), $3 ) ); }
       | E_PDPOS E_MODA expr       { p_set( $1, P_DPOS, vars_mod(
		                                  p_get_by_num( $1, P_DPOS ), $3 ) ); }
       | E_PDPOS E_EXPA expr       { p_set( $1, P_DPOS, vars_pow(
		                                  p_get_by_num( $1, P_DPOS ), $3 ) ); }
       | E_PDLEN '=' expr          { p_set( $1, P_DLEN, $3 ); }
       | E_PDLEN E_PLSA expr       { p_set( $1, P_DLEN, vars_add(
		                                  p_get_by_num( $1, P_DLEN ), $3 ) ); }
       | E_PDLEN E_MINA expr       { p_set( $1, P_DLEN, vars_sub(
		                                  p_get_by_num( $1, P_DLEN ), $3 ) ); }
       | E_PDLEN E_MULA expr       { p_set( $1, P_DLEN, vars_mult(
		                                  p_get_by_num( $1, P_DLEN ), $3 ) ); }
       | E_PDLEN E_DIVA expr       { p_set( $1, P_DLEN, vars_div(
		                                  p_get_by_num( $1, P_DLEN ), $3 ) ); }
       | E_PDLEN E_MODA expr       { p_set( $1, P_DLEN, vars_mod(
		                                  p_get_by_num( $1, P_DLEN ), $3 ) ); }
       | E_PDLEN E_EXPA expr       { p_set( $1, P_DLEN, vars_pow(
		                                  p_get_by_num( $1, P_DLEN ), $3 ) ); }
;

ass:     '=' expr                  { vars_assign( $2, $2->prev ); }
       | E_PLSA expr               { vars_assign( vars_add(
		                                                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | E_MINA expr               { vars_assign( vars_sub(
										                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | E_MULA expr               { vars_assign( vars_mult(
										                  vars_val( $2->prev ),
														  $2 ), $2->prev ); }
       | E_DIVA expr               { vars_assign( vars_div(
                                                          vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | E_MODA expr               { vars_assign( vars_div(
                                                          vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | E_EXPA expr               { vars_assign( vars_pow(
                                                          vars_val( $2->prev ),
															$2 ), $2->prev ); }
;

expr:    E_INT_TOKEN unit          { $$ = apply_unit( vars_push( INT_VAR, $1 ),
													  $2 ); }
       | E_FLOAT_TOKEN unit        { $$ = apply_unit(
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | E_VAR_TOKEN unit          { $$ = apply_unit( $1, $2 ); }
       | E_VAR_TOKEN '['           { vars_arr_start( $1 ); }
         list1 ']'                 { $$ = vars_arr_rhs( $4 ); }
         unit                      { $$ = apply_unit( $<vptr>6, $7 ); }
       | E_FUNC_TOKEN '(' list2
         ')'                       { $$ = func_call( $1 ); }
         unit                      { $$ = apply_unit( $<vptr>5, $6 ); }
       | E_VAR_REF
       | E_VAR_TOKEN '('           { eprint( FATAL, SET, "`%s' isn't a "
											 "function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['          { eprint( FATAL, SET, "`%s' is a predefined"
											 " function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | E_PPOS                    { $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                    { $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                   { $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                   { $$ = p_get_by_num( $1, P_DLEN ); }
       | expr E_AND expr           { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr E_OR expr            { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr E_XOR expr           { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr %prec E_NOT    { $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr            { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr            { $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr E_LT expr            { $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr            { $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr            { $$ = vars_comp( COMP_LESS_EQUAL,
													 $1, $3 ); }
       | expr E_GE expr            { $$ = vars_comp( COMP_LESS_EQUAL, 
													 $3, $1 ); }
       | expr '+' expr             { $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG      { $$ = $2; }
       | '-' expr %prec E_NEG      { $$ = vars_negate( $2 ); }
       | '(' expr ')' unit         { $$ = apply_unit( $2, $4 ); }
;

unit:    /* empty */               { $$ = NULL; }
       | E_NT_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | E_UT_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | E_MT_TOKEN                { $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | E_T_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | E_KT_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | E_MGT_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | E_NU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | E_UU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | E_MU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | E_KU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | E_MEG_TOKEN               { $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
;


/* list of indices for access of an array element */

list1:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
	   | expr
       | list1 ',' expr            { $$ = $3; }
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
       | strs E_STR_TOKEN         { Var *v = vars_push( STR_VAR, $2 );
	                                vars_add( v->prev, v ); }
;


%%


static void exp_runerror ( const char *s )
{
	s = s;                    /* avoid compiler warning */


	eprint( FATAL, SET, "Syntax error in EXPERIMENT section.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
