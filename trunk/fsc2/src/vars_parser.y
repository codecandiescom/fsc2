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

extern int varslex( void );


/* locally used functions */

int varsparse( void );
static void varserror( const char *s );

extern char *varstext;


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
%type <vptr> expr arrass list1 list1a list2 list3 list3a unit



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
         list1 ']'                 { vars_arr_lhs( $4 ); }
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

expr:    INT_TOKEN unit            { $$ = apply_unit( vars_push( INT_VAR, $1 ),
													  $2 ); }
       | FLOAT_TOKEN unit          { $$ = apply_unit(
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | VAR_TOKEN                 { if ( $1->flags & NEW_VARIABLE )
	                                 {
										 print( FATAL, "Variable '%s' does not"
												" exist.\n", $1->name );
										 THROW( EXCEPTION );
									 }
	                                 $$ = vars_push_copy( $1 ); }
       | VAR_TOKEN '['             { if ( $1->flags & NEW_VARIABLE )
	                                 {
										 print( FATAL, "Variable '%s' does not"
												" exist.\n", $1->name );
										 THROW( EXCEPTION );
									 }
	                                 vars_arr_start( $1 ); }
         list3 ']'                 { $$ = vars_arr_rhs( $4 ); }
       | FUNC_TOKEN '(' list4 ')'  { $$ = func_call( $1 ); }
       | VAR_REF
       | VAR_TOKEN '('             { print( FATAL, "'%s' isn't a function.\n",
											$1->name );
	                                 THROW( EXCEPTION ); }
       | FUNC_TOKEN '['            { print( FATAL, "'%s()' is a predefined "
											"function.\n", $1->name );
	                                 THROW( EXCEPTION ); }
       | expr AND expr       	   { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr OR expr        	   { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr XOR expr       	   { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr            	   { $$ = vars_lnegate( $2 ); }
       | expr EQ expr              { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr              { $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr LT expr              { $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr GT expr              { $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr LE expr              { $$ = vars_comp( COMP_LESS_EQUAL,
													 $1, $3 ); }
       | expr GE expr              { $$ = vars_comp( COMP_LESS_EQUAL,
													 $3, $1 ); }
       | expr '+' expr             { $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { $$ = vars_pow( $1, $3 ); }
       | '+' expr %prec NEG        { $$ = $2; }
       | '-' expr %prec NEG        { $$ = vars_negate( $2 ); }
       | '(' expr ')'              { $$ = $2; }
;


unit:    /* empty */               { $$ = NULL; }
       | NT_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | UT_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | MT_TOKEN                  { $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | T_TOKEN                   { $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | KT_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | MGT_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | NU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | UU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | MU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | KU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | MEG_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
;


arhs:    /* empty */               { vars_arr_init( vars_push( UNDEF_VAR ) ); }
       | '=' arrass                { vars_arr_init( $2 ); }
       | '=' expr                  { vars_assign( $2, $2->prev ); }
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

arrass:   '{' '}'                  { $$ = vars_push( UNDEF_VAR ); }
        | '{' list2 l2e '}'        { $$ = $2; }
	    | '{' ','                  { print( FATAL, "Superfluous comma in "
											"initializer list.\n" );
	                                 THROW( EXCEPTION ); }
;

l2e:     /* empty */
       | ','                       { print( FATAL, "Superfluous comma in "
											"initializer list.\n" );
	                                 THROW( EXCEPTION ); }
;

list2:   expr                      { $$ = $1; }
       | list2 ',' expr            { $$ = $3; }
;

/* list of indices for access of an array element */

list3:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
	   | list3a l3e                { $$ = $1; }
	   | ','                       { print( FATAL, "Superfluous comma in "
											"array index list.\n" );
	                                 THROW( EXCEPTION ); }
;

l3e:     /* empty */
       | ','                       { print( FATAL, "Superfluous comma in "
											"array index list.\n" );
	                                 THROW( EXCEPTION ); }
;

list3a:   expr                     { $$ = $1; }
	    | list3a ',' expr          { $$ = $3; }
;

/* list of function arguments */

list4:   /* empty */
       | list4a l4e
       | ','                       { print( FATAL, "Superfluous comma in "
											"function argument list.\n" );
	                                 THROW( EXCEPTION ); }
;

l4e:     /* empty */
       | ','                       { print( FATAL, "Superfluous comma in "
											"function argument list.\n" );
	                                 THROW( EXCEPTION ); }
;

list4a:   exprs
        | list4a ',' exprs
;

exprs:   expr                      { }
       | STR_TOKEN                 { vars_push( STR_VAR, $1 ); }
;

%%


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


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
