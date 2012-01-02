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


/****************************************************************/
/* This is the parser for the EXPERIMENT section of an EDL file */
/****************************************************************/


%{

#include "fsc2.h"


void exp_runparser_init( void );


/* locally used functions */

static void exp_runerror( const char * s );


/* locally used variables */

static int Dont_exec = 0;
static int W_pp;            /* which pulse property */

%}


/* the following union and the token definitions MUST be identical to the ones
   in 'exp.h' and 'condition_parser.y' ! */

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


%type <vptr> expr line list1 l1e ind lhs strs
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
                                     fsc2_assert( Dont_exec == 0 );
                                     if ( EDL.do_quit )
                                         YYACCEPT; }
       | '}'                       { fsc2_assert( EDL.Var_Stack == NULL );
                                     fsc2_assert( Dont_exec == 0 );
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
       | plhs '=' expr             { p_set( $1, W_pp, $3 ); }
       | plhs E_PLSA expr          { p_set( $1, W_pp, vars_add(
                                            p_get_by_num( $1, W_pp ), $3 ) ); }
       | plhs E_MINA expr          { p_set( $1, W_pp, vars_sub(
                                            p_get_by_num( $1, W_pp ), $3 ) ); }
       | plhs E_MULA expr          { p_set( $1, W_pp, vars_mult(
                                            p_get_by_num( $1, W_pp ), $3 ) ); }
       | plhs E_DIVA expr          { p_set( $1, W_pp, vars_div(
                                            p_get_by_num( $1, W_pp ), $3 ) ); }
       | plhs E_MODA expr          { p_set( $1, W_pp, vars_mod(
                                            p_get_by_num( $1, W_pp ), $3 ) ); }
       | plhs E_EXPA expr          { p_set( $1, W_pp, vars_pow(
                                            p_get_by_num( $1, W_pp ), $3 ) ); }
;

lhs:     E_VAR_TOKEN               { $$ = $1; }
       | E_VAR_TOKEN '['           { vars_arr_start( $1 ); }
         list1 ']'                 { $$ = vars_arr_lhs( $4 ); }
;

plhs:    E_PPOS                    { W_pp = P_POS; $$ = $1; }
       | E_PLEN                    { W_pp = P_LEN; $$ = $1; }
       | E_PDPOS                   { W_pp = P_DPOS; $$ = $1; }
       | E_PDLEN                   { W_pp = P_DLEN; $$ = $1; }
;

expr:    E_INT_TOKEN               { if ( ! Dont_exec )
                                         $$ = vars_push( INT_VAR, $1 ); }
       | E_FLOAT_TOKEN             { if ( ! Dont_exec )
                                         $$ = vars_push( FLOAT_VAR, $1 ); }
       | E_VAR_TOKEN               { if ( ! Dont_exec )
                                          $$ = vars_push_copy( $1 ); }
       | E_VAR_TOKEN '['           { if ( ! Dont_exec )
                                         vars_arr_start( $1 ); }
         list1 ']'                 { if ( ! Dont_exec )
                                         $$ = vars_arr_rhs( $4 ); }
       | E_FUNC_TOKEN '(' list2
         ')'                       { if ( ! Dont_exec )
                                         $$ = func_call( $1 );
                                     else
                                         vars_pop( $1 ); }
       | E_FUNC_TOKEN              { print( FATAL, "'%s' is a predefined "
                                            "function.\n", $1->name );
                                     THROW( EXCEPTION ); }
       | strs
       | E_VAR_REF                 { if ( Dont_exec )
                                         vars_pop( $1 ); }
       | E_VAR_TOKEN '('           { print( FATAL, "'%s' isn't a function.\n",
                                            $1->name );
                                     THROW( EXCEPTION ); }
       | E_PPOS                    { if ( ! Dont_exec )
                                          $$ = p_get_by_num( $1, P_POS ); }
       | E_PLEN                    { if ( ! Dont_exec )
                                          $$ = p_get_by_num( $1, P_LEN ); }
       | E_PDPOS                   { if ( ! Dont_exec )
                                          $$ = p_get_by_num( $1, P_DPOS ); }
       | E_PDLEN                   { if ( ! Dont_exec )
                                          $$ = p_get_by_num( $1, P_DLEN ); }
       | expr E_AND                { if ( ! Dont_exec )
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
         expr                      { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_AND, $1, $4 );
                                     else if ( ! --Dont_exec )
                                         $$ = vars_push( INT_VAR, 0L );
                                   }
       | expr E_OR                 { if ( ! Dont_exec )
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
         expr                      { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_OR, $1, $4 );
                                     else if ( ! --Dont_exec )
                                         $$ = vars_push( INT_VAR, 1L );
                                   }
       | expr E_XOR expr           { if ( ! Dont_exec )
                                          $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr %prec E_NOT    { if ( ! Dont_exec )
                                          $$ = vars_lnegate( $2 ); }
       | expr E_EQ expr            { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr E_NE expr            { if ( ! Dont_exec )
                                      $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr E_LT expr            { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr E_GT expr            { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr E_LE expr            { if ( ! Dont_exec )
                                          $$ = vars_comp( COMP_LESS_EQUAL,
                                                          $1, $3 ); }
       | expr E_GE expr            { if ( ! Dont_exec )
                                          $$ = vars_comp( COMP_LESS_EQUAL,
                                                          $3, $1 ); }
       | expr '+' expr             { if ( ! Dont_exec )
                                         $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { if ( ! Dont_exec )
                                         $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { if ( ! Dont_exec )
                                         $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { if ( ! Dont_exec )
                                         $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { if ( ! Dont_exec )
                                         $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { if ( ! Dont_exec )
                                         $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec E_NEG      { if ( ! Dont_exec )
                                         $$ = $2; }
       | '-' expr %prec E_NEG      { if ( ! Dont_exec )
                                         $$ = vars_negate( $2 ); }
       | '(' expr ')'              { if ( ! Dont_exec )
                                         $$ = $2; }
       | expr '?'                  { if ( ! Dont_exec )
                                     {
                                         if ( ! check_result( $1 ) )
                                             Dont_exec++;
                                         vars_pop( $1 );
                                     }
                                     else
                                         Dont_exec +=2;
                                   }
         expr ':'                  { if ( ! Dont_exec )
                                         Dont_exec++;
                                     else
                                         Dont_exec--;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = $7;
                                     else if ( ! --Dont_exec )
                                         $$ = $4;
                                   }
;

/* list of indices for access of an array element */

list1:   /* empty */                 { if ( ! Dont_exec )
                                          $$ = vars_push( UNDEF_VAR ); }
       | l1e
;

l1e:     ind                         { if ( ! Dont_exec )
                                          $$ = $1; }
       | l1e ',' ind                 { if ( ! Dont_exec )
                                          $$ = $3; }
;

ind:     expr                        { if ( ! Dont_exec )
                                           $$ = $1; }
       | expr ':'                    { if ( ! Dont_exec )
                                           vars_push( STR_VAR, ":" ); }
         expr                        { if ( ! Dont_exec )
                                           $$ = $4; }
;

/* list of function arguments */

list2:   /* empty */
       | l2e
;

l2e:     expr                       { }
       | l2e ',' expr               { }
;

strs:    E_STR_TOKEN               { if ( ! Dont_exec )
                                         $$ = vars_push( STR_VAR, $1 ); }
       | strs E_STR_TOKEN          { if ( ! Dont_exec )
                                     {
                                          Var_T *v = vars_push( STR_VAR, $2 );
                                          $$ = vars_add( v->prev, v ); }
                                   }
;


%%


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void
exp_runerror( const char * s  UNUSED_ARG )
{
    print( FATAL, "Syntax error in EXPERIMENT section.\n" );
    THROW( EXCEPTION );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
exp_runparser_init( void )
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
