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

%{

#include "fsc2.h"

void varsparser_init( void );

extern int varslex( void );


/* locally used functions */

int varsparse( void );
static void varserror( const char *s );

extern char *varstext;

static ssize_t max_level;
static ssize_t level;
static int dont_exec = 0;

%}


%union {
	long   lval;
    double dval;
	char   *sptr;
	Var    *vptr;
}


%token SECTION_LABEL            /* new section label */
%token <vptr> VAR_TOKEN         /* variable */
%token <vptr> FUNC_TOKEN        /* function */
%token <vptr> VAR_REF
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ NE LT LE GT GE
%token AND OR XOR NOT


%token NT_TOKEN UT_TOKEN MT_TOKEN T_TOKEN KT_TOKEN MGT_TOKEN
%token NU_TOKEN UU_TOKEN MU_TOKEN KU_TOKEN MEG_TOKEN
%type <vptr> arrass alist alist1 aitem expr list1 list1a list3 l3e unit


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
       | input ';'
       | input line ';'            { fsc2_assert( EDL.Var_Stack == NULL ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { YYACCEPT; }
;

line:    linet
       | line ',' linet
       | line linet                { THROW( MISSING_SEMICOLON_EXCEPTION ); }

;

linet:   VAR_TOKEN                 { }        /* no assignment to be done */
       | VAR_TOKEN '=' expr        { vars_assign( $3, $1 ); }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list1 ']'                 { vars_arr_lhs( $4 );
									 max_level = $1->dim; }
         arhs
	   | FUNC_TOKEN '('            { print( FATAL, "Function calls can only "
											"be used in variable "
											"initialisations.\n" );
	                                 THROW( EXCEPTION ); }
       | FUNC_TOKEN '['            { print( FATAL, "'%s()' is a function and "
											"not an array.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | VAR_TOKEN '('             { print( FATAL, "'%s' is an array and not "
											 "a function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
;

expr:    INT_TOKEN unit            { if ( ! dont_exec )
	                                     $$ = apply_unit( vars_push( INT_VAR,
														  $1 ), $2 ); }
       | FLOAT_TOKEN unit          { if ( ! dont_exec )
	                                     $$ = apply_unit(
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | VAR_TOKEN                 { if ( ! dont_exec )
	                                 {
	                                     if ( $1->flags & NEW_VARIABLE )
	                                     {
											 print( FATAL, "Variable '%s' "
													"does not exist.\n",
													$1->name );
											 THROW( EXCEPTION );
										 }
	                                     $$ = vars_push_copy( $1 ); }
	                               }
       | VAR_TOKEN '['             { if ( ! dont_exec )
	                                 {
	                                     if ( $1->flags & NEW_VARIABLE )
										 {
											 print( FATAL, "Variable '%s' "
													"does not exist.\n",
													$1->name );
											 THROW( EXCEPTION );
										 }
										 vars_arr_start( $1 ); }
	                               }
         list3 ']'                 { if ( ! dont_exec )
	                                     $$ = vars_arr_rhs( $4 ); }
       | FUNC_TOKEN '(' list4 ')'  { if ( ! dont_exec )
	                                     $$ = func_call( $1 ); }
       | VAR_REF
       | VAR_TOKEN '('             { dont_exec = 0;
	                                 print( FATAL, "'%s' isn't a function.\n",
											$1->name );
	                                 THROW( EXCEPTION ); }
       | FUNC_TOKEN '['            { dont_exec = 0;
	                                 print( FATAL, "'%s()' is a predefined "
											"function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
	   | expr AND                  { if ( ! dont_exec )
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
										 $$ = vars_push( INT_VAR, 0L );;
		                           }
       | expr OR                   { if ( ! dont_exec )
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
       | expr XOR expr       	   { if ( ! dont_exec )
	                                     $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr            	   { if ( ! dont_exec )
	                                     $$ = vars_lnegate( $2 ); }
       | expr EQ expr              { if ( ! dont_exec )
	                                    $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr              { if ( ! dont_exec )
	                                  $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr LT expr              { if ( ! dont_exec )
	                                     $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr GT expr              { if ( ! dont_exec )
	                                     $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr LE expr              { if ( ! dont_exec )
	                                     $$ = vars_comp( COMP_LESS_EQUAL,
														 $1, $3 ); }
       | expr GE expr              { if ( ! dont_exec )
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
       | '+' expr %prec NEG        { if ( ! dont_exec )
	                                     $$ = $2; }
       | '-' expr %prec NEG        { if ( ! dont_exec )
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


unit:    /* empty */               { if ( ! dont_exec )
	                                     $$ = NULL; }
       | NT_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | UT_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | MT_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | T_TOKEN                   { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | KT_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | MGT_TOKEN                 { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | NU_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | UU_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | MU_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | KU_TOKEN                  { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | MEG_TOKEN                 { if ( ! dont_exec )
	                                     $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
;


/* list of sizes of newly declared array */

list1:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
       | list1a list1b
;

list1a:   expr                     { $$ = $1; }
        | '*'                      { ( $$ = vars_push( INT_VAR, 0 ) )->flags
										                       |= IS_DYNAMIC; }
;

list1b:   /* empty */
        | list1b ',' list1a
;

/* list of data for initialization of a newly declared array */

arhs:    /* empty */               { vars_arr_init( NULL ); }
      | '='                        { level = max_level; }
	     arrass                    { vars_arr_init( $3 ); }
       | '=' expr                  { vars_assign( $2, $2->prev ); }
;
	     
arrass:  '{'                       { level--;
									 vars_push( REF_PTR, NULL ); }
         alist '}'                 { $$ = vars_init_list( $3,
														  level++ ); }
;

alist:   /* empty */               { ( $$ = vars_push( UNDEF_VAR ) )->dim =
										 							   level; }
        | alist1                   { $$ = $1; }
;

alist1:  aitem                     { $$ = $1; }
       | alist1 ',' aitem          { $$ = $3; }
;

aitem:   expr                      { $$ = $1; }
       | arrass                    { $$ = $1; }
;

/* list of indices for access of an array element */

list3:   /* empty */               { if ( ! dont_exec )
	                                     $$ = vars_push( UNDEF_VAR ) };
       | l3e                       { if ( ! dont_exec )
	                                     $$ = $1; }
;

l3e:     expr                      { if ( ! dont_exec )
	                                     $$ = $1; }
       | l3e ',' expr              { if ( ! dont_exec )
	                                     $$ = $3; }
;

/* list of function arguments */

list4:   /* empty */
       | l4e
;

l4e:      exprs
        | l4e ',' exprs
;

exprs:   expr                      { }
       | STR_TOKEN                 { }
;

%%


/*----------------------------------------------------*/
/*----------------------------------------------------*/

static void varserror ( const char *s )
{
	UNUSED_ARGUMENT( s );

	if ( *varstext == '\0' )
		print( FATAL, "Unexpected end of file in VARIABLES section.\n");
	else
	{
		if ( isprint( *varstext ) )
			print( FATAL, "Syntax error near token '%s'.\n", varstext );
		else
			print( FATAL, "Syntax error near token '\"%s\"'.\n",
				   varstext + 1 );
	}
	THROW( EXCEPTION );
}


/*----------------------------------------------------*/
/*----------------------------------------------------*/

void varsparser_init( void )
{
	dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
