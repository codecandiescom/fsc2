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

/****************************************************************/
/* This is the parser for the EXPERIMENT section of an EDL file */
/****************************************************************/


%{

#include "fsc2.h"

void exp_runparser_init( void );

extern int exp_runlex( void );
extern char *exptext;

static int wpp;        /* which pulse property */

/* locally used functions */

int exp_runparse( void );
static void exp_runerror( const char *s );
static int dont_exec = 0;

%}


/* the following union and the token definitions MUST be identical to the ones
   in 'exp.h' and 'condition_parser.y' ! */

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
%token E_NEG	      270
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

%type <vptr> expr line list1 l1e lhs
%type <lval> plhs


%left '?' ':'
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

eol:     ';'                       { fsc2_assert( EDL.Var_Stack == NULL );
                                     if ( EDL.do_quit )
                                         YYACCEPT; }
       | '}'                       { fsc2_assert( EDL.Var_Stack == NULL );
	                                 YYACCEPT; }
;

line:    lhs '=' expr              { vars_assign( $3, $1 ); }
       | lhs E_PLSA expr           { vars_assign( vars_add( $1, $3 ), $1 ); }
       | lhs E_MINA expr           { vars_assign( vars_sub( $1, $3 ), $1 ); }
       | lhs E_MULA expr           { vars_assign( vars_mult( $1, $3 ), $1 ); }
       | lhs E_DIVA expr           { vars_assign( vars_div( $1, $3 ), $1 ); }
       | lhs E_MODA expr           { vars_assign( vars_mod( $1, $3 ), $1 ); }
       | lhs E_EXPA expr           { vars_assign( vars_pow( $1, $3 ), $1 ); }
       | E_FUNC_TOKEN '(' list2 ')'{ vars_pop( func_call( $1 ) ); }
       | E_FUNC_TOKEN              { print( FATAL, "'%s' is a predefined "
											"function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | plhs '=' expr             { p_set( $1, wpp, $3 ); }
       | plhs E_PLSA expr          { p_set( $1, wpp, vars_add(
		                                    p_get_by_num( $1, wpp ), $3 ) ); }
       | plhs E_MINA expr          { p_set( $1, wpp, vars_sub(
		                                    p_get_by_num( $1, wpp ), $3 ) ); }
       | plhs E_MULA expr          { p_set( $1, wpp, vars_mult(
		                                    p_get_by_num( $1, wpp ), $3 ) ); }
       | plhs E_DIVA expr          { p_set( $1, wpp, vars_div(
		                                    p_get_by_num( $1, wpp ), $3 ) ); }
       | plhs E_MODA expr          { p_set( $1, wpp, vars_mod(
		                                    p_get_by_num( $1, wpp ), $3 ) ); }
       | plhs E_EXPA expr          { p_set( $1, wpp, vars_pow(
		                                    p_get_by_num( $1, wpp ), $3 ) ); }
;

lhs:     E_VAR_TOKEN               { $$ = $1; }
       | E_VAR_TOKEN '['           { vars_arr_start( $1 ); }
         list1 ']'                 { $$ = vars_arr_lhs( $4 ); }
;

plhs:    E_PPOS                    { wpp = P_POS; $$ = $1; }
       | E_PLEN                    { wpp = P_LEN; $$ = $1; }
       | E_PDPOS                   { wpp = P_DPOS; $$ = $1; }
       | E_PDLEN                   { wpp = P_DLEN; $$ = $1; }
;

expr:    E_INT_TOKEN               { if ( ! dont_exec )
	                                     $$ = vars_push( INT_VAR, $1 ); }
       | E_FLOAT_TOKEN             { if ( ! dont_exec )
		                                 $$ = vars_push( FLOAT_VAR, $1 ); }
       | E_VAR_TOKEN               { if ( ! dont_exec )
		                                  $$ = vars_push_copy( $1 ); }
       | E_VAR_TOKEN '['           { if ( ! dont_exec )
                                         vars_arr_start( $1 ); }
         list1 ']'                 { if ( ! dont_exec )
                                         $$ = vars_arr_rhs( $4 ); }
       | E_FUNC_TOKEN '(' list2
         ')'                       { if ( ! dont_exec )
                                         $$ = func_call( $1 );
	                                 else
										 vars_pop( $1 ); }
       | E_FUNC_TOKEN              { print( FATAL, "'%s' is a predefined "
											"function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | E_VAR_REF                 { if ( dont_exec )
										 vars_pop( $1 ); }
       | E_VAR_TOKEN '('           { print( FATAL, "'%s' isn't a function.\n", 
											$1->name );
	                                 THROW( EXCEPTION ); }
       | E_PPOS                    { if ( ! dont_exec )
		                                  $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                    { if ( ! dont_exec )
		                                  $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                   { if ( ! dont_exec )
		                                  $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                   { if ( ! dont_exec )
		                                  $$ = p_get_by_num( $1, P_DLEN ); }
	   | expr E_AND                { if ( ! dont_exec )
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
	     expr                      { if ( ! dont_exec )
                                         $$ = vars_comp( COMP_AND, $1, $4 );
		                             else if ( ! --dont_exec )
										 $$ = vars_push( INT_VAR, 0L );
		                           }
       | expr E_OR                 { if ( ! dont_exec )
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
	     expr                      { if ( ! dont_exec )
                                         $$ = vars_comp( COMP_OR, $1, $4 );
		                             else if ( ! --dont_exec )
										 $$ = vars_push( INT_VAR, 1L );
		                           }
       | expr E_XOR expr           { if ( ! dont_exec )
		                                  $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr %prec E_NOT    { if ( ! dont_exec )
		                                  $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr            { if ( ! dont_exec )
		                                $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr            { if ( ! dont_exec )
		                              $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr E_LT expr            { if ( ! dont_exec )
		                                 $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr            { if ( ! dont_exec )
		                                 $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr            { if ( ! dont_exec )
		                                  $$ = vars_comp( COMP_LESS_EQUAL,
														  $1, $3 ); }
       | expr E_GE expr            { if ( ! dont_exec )
		                                  $$ = vars_comp( COMP_LESS_EQUAL,
														  $3, $1 ); }
       | expr '+' expr             { if ( ! dont_exec )
		                                  $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { if ( ! dont_exec )
		                                  $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { if ( ! dont_exec )
		                                  $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { if ( ! dont_exec )
		                                  $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { if ( ! dont_exec )
		                                  $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { if ( ! dont_exec )
		                                  $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG      { if ( ! dont_exec )
		                                  $$ = $2; }
       | '-' expr %prec E_NEG      { if ( ! dont_exec )
		                                  $$ = vars_negate( $2 ); }
       | '(' expr ')'              { if ( ! dont_exec )
		                                  $$ = $2; }
       | expr '?'                  { if ( ! dont_exec )
	                                 {
		                                 if ( ! check_result( $1 ) )
		                                     dont_exec++;
		                                 vars_pop( $1 );
									 }
	                                 else
										 dont_exec +=2;
	                               }
		 expr ':'                  { if ( ! dont_exec )
										 dont_exec++;
		 							 else
										 dont_exec--;
	                               }
		 expr                      { if ( ! dont_exec )
										 $$ = $7;
		                             else if ( ! --dont_exec )
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

exprs:   expr                      { }
       | strs
;

strs:    E_STR_TOKEN               { if ( ! dont_exec )
		                                  vars_push( STR_VAR, $1 ); }
       | strs E_STR_TOKEN          { if ( ! dont_exec )
	                                 {
		                                  Var *v = vars_push( STR_VAR, $2 );
										  vars_add( v->prev, v ); }
	                               }
;


%%


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void exp_runerror ( const char *s )
{
	UNUSED_ARGUMENT( s );

	print( FATAL, "Syntax error in EXPERIMENT section.\n" );
	THROW( EXCEPTION );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void exp_runparser_init( void )
{
	dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
