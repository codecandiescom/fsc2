/*
  $Id$
*/

/****************************************************************/
/* This is the parser for the EXPERIMENT section of an EDL file */
/****************************************************************/


%{

#include "fsc2.h"

extern int prim_exp_runlex( void );

/* locally used functions */

int prim_exp_runerror( const char *s );


%}


/* the following union and the token definitions MUST be identical to the ones
   in `condition_parser.y', `prim_exp.h' ! */

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
%token E_LT	          265
%token E_LE	          266
%token E_GT	          267
%token E_GE	          268
%token E_NS_TOKEN	  269
%token E_US_TOKEN	  270
%token E_MS_TOKEN	  271
%token E_S_TOKEN	  272
%token E_NEG	      273


%token <vptr> E_VAR_TOKEN         /* variable name */
%token <vptr> E_VAR_REF
%token <vptr> E_FUNC_TOKEN        /* function name */
%token <lval> E_INT_TOKEN
%token <dval> E_FLOAT_TOKEN
%token <sptr> E_STR_TOKEN
%token E_EQ E_LT E_LE E_GT E_GE

%token E_NS_TOKEN E_US_TOKEN E_MS_TOKEN E_S_TOKEN
%type <vptr> expr line


%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left E_NEG


%%


input:   /* empty */
       | input ';'
       | input line ';'            { assert( Var_Stack == NULL ); }
       | input error ';'           { THROW( SYNTAX_ERROR_EXCEPTION ); }
       | input line line           { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;

/* currently only the variables related stuff */

line:    E_VAR_TOKEN '=' expr      { vars_assign( $3, $1 ); }
       | E_VAR_TOKEN '['           { $$ = vars_arr_start( $1 ); }
         list1 ']'                 { $$ = vars_arr_lhs( $$ ) }
         '=' expr                  { vars_assign( $8, $$ );
                                     assert( Var_Stack == NULL ); }
;

expr:    E_INT_TOKEN               { $$ = vars_push( INT_VAR, &$1 ); }
       | E_FLOAT_TOKEN             { $$ = vars_push( FLOAT_VAR, &$1 ); }
       | E_VAR_TOKEN               { $$ = vars_push_copy( $1 ); }
       | E_VAR_REF                 { $$ = $1; }
       | E_VAR_TOKEN '['           { $$ = vars_arr_start( $1 ); }
         list1 ']'                 { $$ = vars_arr_lhs( $$ ); }
       | E_FUNC_TOKEN '(' list2 ')'{ $$ = func_call( $1 ); }
       | expr '+' expr             { $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { $$ = vars_pow( $1, $3 ); }
       | '-' expr %prec E_NEG      { $$ = vars_negate( $2 ); }
       | '(' expr ')'              { $$ = $2 }
;

list1:   expr                      { }
       | list1 ',' expr            { }
;

list2:   expr                      { }
       | list2 ',' expr            { }
;


%%


int prim_exp_runerror ( const char *s )
{
	if ( *prim_exptext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in EXPERIMENT "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, prim_exptext );
	THROW( EXPERIMENT_EXCEPTION );
}
