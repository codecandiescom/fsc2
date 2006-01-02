/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


/******************************************************************/
/* This is the parser for the PREPARATIONS section of an EDL file */
/******************************************************************/


%{

#include "fsc2.h"


void prepsparser_init( void );

extern int prepslex( void );   /* defined in preps_lexer.l */
extern char *prepstext;        /* defined in preps_lexer.l */


/* locally used functions */

static void prepserror( const char * s );


/* locally used variables */

static int Dont_exec = 0;

%}


%union {
	long   lval;
    double dval;
	char   *sptr;
	Var_T  *vptr;
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

%type <vptr> expr list1 l1e ind lhs strs


%left '?' ':'
%left EQ NE LT LE GT GE
%left AND OR XOR
%left '+' '-'
%left '*' '/'
%left '%'
%right NEG NOT
%right '^'


%%


input:   /* empty */
       | input ';'                  { EDL.Cur_Pulse = -1; }
       | input line ';'             { EDL.Cur_Pulse = -1;
									  fsc2_assert( Dont_exec == 0 );
	                                  fsc2_assert( EDL.Var_Stack == NULL ); }
       | input line P_TOK           { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input line SECTION_LABEL   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL        { EDL.Cur_Pulse = -1;
	                                  fsc2_assert( EDL.Var_Stack == NULL );
									  fsc2_assert( Dont_exec == 0 );
	                                  YYACCEPT; }
;

line:    P_TOK prop
       | lhs '=' expr               { vars_assign( $3, $1 ); }
       | lhs PLSA expr              { vars_assign( vars_add( $1, $3 ), $1 ); }
       | lhs MINA expr              { vars_assign( vars_sub( $1, $3 ), $1 ); }
       | lhs MULA expr              { vars_assign( vars_mult( $1, $3 ), $1 ); }
       | lhs DIVA expr              { vars_assign( vars_div( $1, $3 ), $1 ); }
       | lhs MODA expr              { vars_assign( vars_mod( $1, $3 ), $1 ); }
       | lhs EXPA expr              { vars_assign( vars_pow( $1, $3 ), $1 ); }
       | FUNC_TOKEN '(' list2 ')'   { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN                 { print( FATAL, "'%s' is a predefined "
											 "function.\n", $1->name );
	                                  THROW( EXCEPTION ); }
;

lhs:     VAR_TOKEN                  { $$ = $1; }
       | VAR_TOKEN '['              { vars_arr_start( $1 ); }
         list1 ']'                  { $$ = vars_arr_lhs( $4 ); }
;

prop:   /* empty */
       | prop F_TOK sep1 expr sep2  { p_set( EDL.Cur_Pulse, P_FUNC, $4 ); }
       | prop S_TOK sep1 expr sep2  { p_set( EDL.Cur_Pulse, P_POS, $4 ); }
       | prop L_TOK sep1 expr sep2  { p_set( EDL.Cur_Pulse, P_LEN,$4 ); }
       | prop DS_TOK sep1 expr sep2 { p_set( EDL.Cur_Pulse, P_DPOS, $4 ); }
       | prop DL_TOK sep1 expr sep2 { p_set( EDL.Cur_Pulse, P_DLEN, $4 ); }
       | prop PC_TOK sep1 VAR_REF
         sep2                       { p_set( EDL.Cur_Pulse, P_PHASE, $4 ); }
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

expr:    INT_TOKEN                { if ( ! Dont_exec )
		                                $$ = vars_push( INT_VAR, $1 ); }
       | FLOAT_TOKEN              { if ( ! Dont_exec )
		                                $$ = vars_push( FLOAT_VAR, $1 ); }
       | VAR_TOKEN                { if ( ! Dont_exec )
		                                $$ = vars_push_copy( $1 ); }
       | VAR_TOKEN '['            { if ( ! Dont_exec )
		                                vars_arr_start( $1 ); }
         list1 ']'                { if ( ! Dont_exec )
		                                $$ = vars_arr_rhs( $4 ); }
       | FUNC_TOKEN '(' list2 ')' { if ( ! Dont_exec )
		                                $$ = func_call( $1 ); }
       | FUNC_TOKEN               { print( FATAL, "'%s' is a predefined "
										   "function.\n", $1->name );
	                                THROW( EXCEPTION ); }
       | strs
       | VAR_REF
       | VAR_TOKEN '('            { print( FATAL, "'%s' isn't a function.\n",
										   $1->name );
	                                THROW( EXCEPTION ); }
	   | expr AND                 { if ( ! Dont_exec )
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
										$$ = vars_push( INT_VAR, 0L );
		                          }
       | expr OR                  { if ( ! Dont_exec )
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
									    $$ = vars_push( INT_VAR, 1L );
		                          }
       | expr XOR expr       	  { if ( ! Dont_exec )
		                                $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr            	  { if ( ! Dont_exec )
		                                $$ = vars_lnegate( $2 ); }
       | expr EQ expr             { if ( ! Dont_exec )
		                                $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr             { if ( ! Dont_exec )
		                              $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr LT expr             { if ( ! Dont_exec )
		                                $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr GT expr             { if ( ! Dont_exec )
		                                $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr LE expr             { if ( ! Dont_exec )
		                                $$ = vars_comp( COMP_LESS_EQUAL,
														$1, $3 ); }
       | expr GE expr             { if ( ! Dont_exec )
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
       | '+' expr %prec NEG       { if ( ! Dont_exec )
		                                $$ = $2; }
       | '-' expr %prec NEG       { if ( ! Dont_exec )
		                                $$ = vars_negate( $2 ); }
       | '(' expr ')'             { if ( ! Dont_exec )
		                                $$ = $2; }
       | expr '?'                 { if ( ! Dont_exec )
	                                {
										if ( ! check_result( $1 ) )
											Dont_exec++;
										vars_pop( $1 );
									}
	                                else
										Dont_exec +=2;
	                              }
         expr ':'                 { if ( ! Dont_exec )
										Dont_exec++;
		 							else
										Dont_exec--;
	                              }
		 expr                     { if ( ! Dont_exec )
										$$ = $7;
		                            else if ( ! --Dont_exec )
										$$ = $4;
                                  }
;

/* list of indices for access of array element */

list1:   /* empty */              { if ( ! Dont_exec )
		                                $$ = vars_push( UNDEF_VAR ); }
       | l1e                      { if ( ! Dont_exec )
		                                $$ = $1; }
;

l1e:     ind                      { if ( ! Dont_exec )
		                                $$ = $1; }
       | l1e ',' ind              { if ( ! Dont_exec )
		                                $$ = $3; }
;

ind:     expr                     { if ( ! Dont_exec )
		                                $$ = $1; }
	   | expr ':'                 { if ( ! Dont_exec )
										vars_push( STR_VAR, ":" ); }
         expr                     { if ( ! Dont_exec )
		                                $$ = $4; }
;

/* list of function arguments */

list2:   /* empty */
       | l2e
;

l2e:     expr                     { }
       | l2e ',' expr             { }
;

strs:    STR_TOKEN                { if ( ! Dont_exec )
		                                $$ = vars_push( STR_VAR, $1 ); }
       | strs STR_TOKEN           { if ( ! Dont_exec )
	                                {
		                                Var_T *v = vars_push( STR_VAR, $2 );
	                                    $$ = vars_add( v->prev, v );
									}
	                              }
;


%%


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void prepserror( const char * s  UNUSED_ARG )
{
	if ( *prepstext == '\0' )
		print( FATAL, "Unexpected end of file in PREPARATIONS section.\n" );
	else
		print( FATAL, "Syntax error near '%s'.\n", prepstext );
	THROW( EXCEPTION );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void prepsparser_init( void )
{
	Dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
