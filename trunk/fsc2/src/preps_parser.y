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

/******************************************************************/
/* This is the parser for the PREPARATIONS section of an EDL file */
/******************************************************************/


%{

#include "fsc2.h"


extern int prepslex( void );

extern char *prepstext;

/* locally used functions */

static void prepserror( const char *s );

%}


%union {
	long   lval;
    double dval;
	char   *sptr;
	Var    *vptr;
}


%token SECTION_LABEL            /* new section label */

%token P_TOK                    /* pulse */
%token F_TOK                    /* pulse function */
%token S_TOK                    /* pulse start */
%token L_TOK                    /* pulse length */
%token DS_TOK                   /* pulse position change */
%token DL_TOK                   /* pulse length change */
%token PC_TOK                   /* pulse phase cycle sequence */


%token <vptr> VAR_TOKEN         /* variable */
%token <vptr> VAR_REF
%token <vptr> FUNC_TOKEN        /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ NE LT LE GT GE
%token AND OR XOR NOT
%token PLSA MINA MULA DIVA MODA EXPA

%token NT_TOKEN UT_TOKEN MT_TOKEN T_TOKEN KT_TOKEN MGT_TOKEN
%token NU_TOKEN UU_TOKEN MU_TOKEN KU_TOKEN MEG_TOKEN
%type <vptr> expr unit list1


%left EQ NE LT LE GT GE
%left AND OR XOR
%left '+' '-'
%left '*' '/'
%left '%'
%right NEG NOT
%right '^'


%%


input:   /* empty */
       | input ';'                 { Cur_Pulse = -1; }
       | input line ';'            { Cur_Pulse = -1;
	                                 fsc2_assert( Var_Stack == NULL ); }
       | input error ';'           { THROW( SYNTAX_ERROR_EXCEPTION ); }
       | input line P_TOK          { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { Cur_Pulse = -1;
	                                 fsc2_assert( Var_Stack == NULL );
	                                 YYACCEPT; }
;

line:    P_TOK prop
       | VAR_TOKEN '=' expr        { vars_assign( $3, $1 ); }
       | VAR_TOKEN PLSA expr       { vars_assign( vars_add( $1, $3 ), $1 ); }
       | VAR_TOKEN MINA expr       { vars_assign( vars_sub( $1, $3 ), $1 ); }
       | VAR_TOKEN MULA expr       { vars_assign( vars_mult( $1, $3 ), $1 ); }
       | VAR_TOKEN DIVA expr       { vars_assign( vars_div( $1, $3 ), $1 ); }
       | VAR_TOKEN MODA expr       { vars_assign( vars_mod( $1, $3 ), $1 ); }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list1 ']'                 { vars_arr_lhs( $4 ); }
         ass                       { fsc2_assert( Var_Stack == NULL ); }
       | FUNC_TOKEN '(' list2 ')'  { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN '['            { eprint( FATAL, SET, "`%s' is a predefined"
                                             " function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
;

ass:     '=' expr                  { vars_assign( $2, $2->prev ); }
       | PLSA expr                 { vars_assign( vars_add(
		                                                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | MINA expr                 { vars_assign( vars_sub(
										                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | MULA expr                 { vars_assign( vars_mult(
										                  vars_val( $2->prev ),
														    $2 ), $2->prev ); }
       | DIVA expr                 { vars_assign( vars_div(
										                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | MODA expr                 { vars_assign( vars_div(
										                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
       | EXPA expr                 { vars_assign( vars_pow(
										                  vars_val( $2->prev ),
															$2 ), $2->prev ); }
;                                     

prop:   /* empty */
       | prop F_TOK sep1 expr sep2  { p_set( Cur_Pulse, P_FUNC, $4 ); }
       | prop S_TOK sep1 expr sep2  { p_set( Cur_Pulse, P_POS, $4 ); }
       | prop L_TOK sep1 expr sep2  { p_set( Cur_Pulse, P_LEN,$4 ); }
       | prop DS_TOK sep1 expr sep2 { p_set( Cur_Pulse, P_DPOS, $4 ); }
       | prop DL_TOK sep1 expr sep2 { p_set( Cur_Pulse, P_DLEN, $4 ); }
       | prop PC_TOK sep1 VAR_REF sep2 { p_set( Cur_Pulse, P_PHASE, $4 ); }
;

/* separator between keyword and value */

sep1:    /* empty */
       | '='
       | ':'
;

/* seperator between different keyword-value pairs */

sep2:    /* empty */           
       | ','
;


expr:    INT_TOKEN unit           { $$ = apply_unit( vars_push( INT_VAR, $1 ),
													 $2 ); }
       | FLOAT_TOKEN unit         { $$ = apply_unit( 
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | VAR_TOKEN unit           { $$ = apply_unit( $1, $2 ); }
       | VAR_TOKEN '['            { vars_arr_start( $1 ); }
         list1 ']'                { $$ = vars_arr_rhs( $4 ); }
         unit                     { $$ = apply_unit( $<vptr>6, $7 ); }
       | FUNC_TOKEN '(' list2 ')' { $$ = func_call( $1 ); }
         unit                     { $$ = apply_unit( $<vptr>5, $6 ); }
       | VAR_REF                  { $$ = $1; }
       | VAR_TOKEN '('            { eprint( FATAL, SET, "`%s' isn't a "
											"function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | FUNC_TOKEN '['           { eprint( FATAL, SET, "`%s' is a predefined "
											"function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | expr AND expr       	  { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr OR expr        	  { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr XOR expr       	  { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr            	  { $$ = vars_lnegate( $2 ); }
       | expr EQ expr             { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr             { $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr LT expr             { $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr GT expr             { $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr LE expr             { $$ = vars_comp( COMP_LESS_EQUAL,
													$1, $3 ); }
       | expr GE expr             { $$ = vars_comp( COMP_LESS_EQUAL, 
													$3, $1 ); }
       | expr '+' expr            { $$ = vars_add( $1, $3 ); }
       | expr '-' expr            { $$ = vars_sub( $1, $3 ); }
       | expr '*' expr            { $$ = vars_mult( $1, $3 ); }
       | expr '/' expr            { $$ = vars_div( $1, $3 ); }
       | expr '%' expr            { $$ = vars_mod( $1, $3 ); }
       | expr '^' expr            { $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec NEG       { $$ = $2; }
       | '-' expr %prec NEG       { $$ = vars_negate( $2 ); }
       | '(' expr ')' unit        { $$ = apply_unit( $2, $4 ); }
;

unit:    /* empty */              { $$ = NULL; }
       | NT_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | UT_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | MT_TOKEN                 { $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | T_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | KT_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | MGT_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | NU_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | UU_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | MU_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | KU_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | MEG_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
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
       | STR_TOKEN                { vars_push( STR_VAR, $1 ); }
         strs
;

strs:    /* empty */
       | strs STR_TOKEN           { Var *v = vars_push( STR_VAR, $2 );
	                                vars_add( v->prev, v ); }
;


%%


static void prepserror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *prepstext == '\0' )
		eprint( FATAL, SET, "Unexpected end of file in PREPARATIONS "
				"section.\n" );
	else
		eprint( FATAL, SET, "Syntax error near token `%s'.\n",
				isprint( *prepstext ) ? prepstext : prepstext + 1 );
	THROW( EXCEPTION );
}
