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

int prim_exp_runparse( void );
int prim_exp_runerror( const char *s );

static Var *CV;

%}


/* the following union and the token definitions MUST be identical to the ones
   in `prim_exp.h' and `condition_parser.y' ! */

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
%type <vptr> expr unit line list1


%left E_EQ E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left E_NEG


%%


input:   /* empty */
       | input eol
       | input line eol
;

eol:     ';'                       { assert( Var_Stack == NULL ); }
       | '}'                       { assert( Var_Stack == NULL );
	                                 YYACCEPT; }

/* currently only the variables related stuff */

line:    E_VAR_TOKEN '=' expr      { vars_assign( $3, $1 ); }
       | E_VAR_TOKEN '['           { $$ = vars_arr_start( $1 ); }
         list1 ']'                 { $$ = vars_arr_lhs( $$ ) }
         '=' expr                  { vars_assign( $8, $$ );
                                     assert( Var_Stack == NULL ); }
       | E_FUNC_TOKEN '(' list2 ')'{ vars_pop( func_call( $1 ) ); }
       | E_FUNC_TOKEN '['          { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( VARIABLES_EXCEPTION ); }
;

expr:    E_INT_TOKEN unit         { $$ = vars_mult( vars_push( INT_VAR, $1 ), 
													$2 ); }
       | E_FLOAT_TOKEN unit       { $$ = vars_mult( vars_push( FLOAT_VAR, $1 ),
													$2 ); }
       | E_VAR_TOKEN unit         { $$ = vars_mult( $1, $2 ); }
       | E_VAR_TOKEN '['          { vars_arr_start( $1 ); }
         list1 ']'                { CV = vars_arr_rhs( $4 ); }
         unit                     { if ( CV->type & ( INT_VAR | FLOAT_VAR ) )
			                            $$ = vars_mult( CV, $7 );
		                            else
									{
										vars_pop( $7 );
									    $$ = CV;
									} }
       | E_FUNC_TOKEN '(' list2 ')'{ CV = func_call( $1 ); }
         unit                     { if ( CV->type & ( INT_VAR | FLOAT_VAR ) )
			                            $$ = vars_mult( CV, $6 );
		                            else
									{
										vars_pop( $6 );
									    $$ = CV;
									} }
       | E_VAR_REF                { $$ = $1; }
       | E_VAR_TOKEN '('          { eprint( FATAL, "%s:%ld: `%s' isn't a "
											"function.\n", Fname, Lc,
											$1->name );
	                                 THROW( UNKNOWN_FUNCTION_EXCEPTION ); }
       | E_FUNC_TOKEN '['         { eprint( FATAL, "%s:%ld: `%s' is a "
											"predefined function.\n",
											Fname, Lc, $1->name );
	                                THROW( VARIABLES_EXCEPTION ); }
       | expr E_EQ expr           { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
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
       | '-' expr %prec E_NEG     { $$ = vars_negate( $2 ); }
       | '(' expr ')' unit        { $$ = vars_mult( $2, $4 ); }
;

unit:    /* empty */              { $$ = vars_push( INT_VAR, 1L ); }
       | E_NS_TOKEN               { $$ = vars_push( INT_VAR, 1L ); }
       | E_US_TOKEN               { $$ = vars_push( INT_VAR, 1000L ); }
       | E_MS_TOKEN               { $$ = vars_push( INT_VAR, 1000000L ); }
       | E_S_TOKEN                { $$ = vars_push( INT_VAR, 1000000000L ); }
;


/* list of indices for access of an array element */

list1:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
	   | expr                      { $$ = $1; }
       | list1 ',' expr            { $$ = $3; }
;

/* list of function arguments */

list2:   /* empty */
       | exprs
	   | list2 ',' exprs
;

exprs:   expr                      { }
       | E_STR_TOKEN               { vars_push( STR_VAR, $1 ); }
;


%%


int prim_exp_runerror ( const char *s )
{
	eprint( FATAL, "%s:%ld: Syntax error in EXPERIMENT section.\n",
				Fname, Lc );
	THROW( EXPERIMENT_EXCEPTION );
}
