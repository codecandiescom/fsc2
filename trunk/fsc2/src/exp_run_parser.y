/*
  $Id$
*/

/****************************************************************/
/* This is the parser for the EXPERIMENT section of an EDL file */
/****************************************************************/


%{

#include "fsc2.h"

extern int exp_runlex( void );

/* locally used functions */

int exp_runparse( void );
int exp_runerror( const char *s );

static Var *CV;

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
%token E_LT	          265
%token E_LE	          266
%token E_GT	          267
%token E_GE	          268
%token E_NS_TOKEN	  269
%token E_US_TOKEN	  270
%token E_MS_TOKEN	  271
%token E_S_TOKEN	  272
%token E_NT_TOKEN	  273
%token E_UT_TOKEN	  274
%token E_MT_TOKEN	  275
%token E_T_TOKEN	  276
%token E_NU_TOKEN	  277
%token E_UU_TOKEN	  278
%token E_MU_TOKEN	  279
%token E_KU_TOKEN	  280
%token E_MEG_TOKEN	  281
%token E_NEG	      282
%token E_AND          283
%token E_OR           284
%token E_XOR          285
%token E_NOT          286


%token <vptr> E_VAR_TOKEN         /* variable name */
%token <vptr> E_VAR_REF
%token <vptr> E_FUNC_TOKEN        /* function name */
%token <lval> E_INT_TOKEN
%token <dval> E_FLOAT_TOKEN
%token <sptr> E_STR_TOKEN
%token E_EQ E_LT E_LE E_GT E_GE

%token E_NS_TOKEN E_US_TOKEN E_MS_TOKEN E_S_TOKEN
%token E_NT_TOKEN E_UT_TOKEN E_MT_TOKEN E_T_TOKEN
%token E_NU_TOKEN E_UU_TOKEN E_MU_TOKEN E_KU_TOKEN E_MEG_TOKEN
%type <vptr> expr unit line list1

%left E_AND E_OR E_XOR
%left E_NOT
%left E_EQ E_LT E_LE E_GT E_GE
%left '+' '-'
%left '*' '/'
%left '%'
%left E_NEG
%right '^'


%%


input:   /* empty */
       | input eol
       | input line eol
;

eol:     ';'                       { assert( Var_Stack == NULL );
                                     if ( do_quit ) 
                                         YYACCEPT; }
       | '}'                       { assert( Var_Stack == NULL );
	                                 YYACCEPT; }

/* currently only the variables related stuff */

line:    E_VAR_TOKEN '=' expr      { vars_assign( $3, $1 ); }
       | E_VAR_TOKEN '['           { vars_arr_start( $1 ); }
         list1 ']'                 { vars_arr_lhs( $4 ) }
         '=' expr                  { vars_assign( $8, $8->prev );
                                     assert( Var_Stack == NULL ); }
       | E_FUNC_TOKEN '(' list2 ')'{ vars_pop( func_call( $1 ) ); }
       | E_FUNC_TOKEN '['          { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( EXCEPTION ); }
;

expr:    E_INT_TOKEN unit         { if ( $2 == NULL )
	                                  $$ = vars_push( INT_VAR, $1 );
                                    else
									   $$ = vars_mult(
                                              vars_push( INT_VAR, $1 ), $2 ); }
       | E_FLOAT_TOKEN unit       { if ( $2 == NULL )
	                                  $$ = vars_push( FLOAT_VAR, $1 );
	                                else
                                      $$ = vars_mult(
                                            vars_push( FLOAT_VAR, $1 ), $2 ); }
       | E_VAR_TOKEN unit         { $$ = apply_unit( $1, $2 ); }
       | E_VAR_TOKEN '['          { vars_arr_start( $1 ); }
         list1 ']'                { CV = vars_arr_rhs( $4 ); }
         unit                     { $$ = apply_unit( CV, $7 ); }
       | E_FUNC_TOKEN '(' list2
         ')'                      { CV = func_call( $1 ); }
         unit                     { $$ = apply_unit( CV, $6 ); }
       | E_VAR_REF                { $$ = $1; }
       | E_VAR_TOKEN '('          { eprint( FATAL, "%s:%ld: `%s' isn't a "
											"function.\n", Fname, Lc,
											$1->name );
	                                 THROW( EXCEPTION ); }
       | E_FUNC_TOKEN '['         { eprint( FATAL, "%s:%ld: `%s' is a "
											"predefined function.\n",
											Fname, Lc, $1->name );
	                                THROW( EXCEPTION ); }
       | expr E_AND expr          { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr E_OR expr           { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr E_XOR expr          { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | E_NOT expr               { $$ = vars_lnegate( $2 ); }
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
       | '(' expr ')' unit        { if ( $4 == NULL )
	                                  $$ = $2;
                                    else
                                      $$ = vars_mult( $2, $4 ); }
;

unit:    /* empty */               { $$ = NULL; }
       | E_NS_TOKEN                { $$ = vars_push( INT_VAR, 1L ); }
       | E_US_TOKEN                { $$ = vars_push( INT_VAR, 1000L ); }
       | E_MS_TOKEN                { $$ = vars_push( INT_VAR, 1000000L ); }
       | E_S_TOKEN                 { $$ = vars_push( INT_VAR, 1000000000L ); }
       | E_NT_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | E_UT_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | E_MT_TOKEN                { $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | E_T_TOKEN                 { $$ = vars_push( INT_VAR, 1.0e4 ); }
       | E_NU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | E_UU_TOKEN                { $$ = vars_push( INT_VAR, 1.0e-6 ); }
       | E_MU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | E_KU_TOKEN                { $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | E_MEG_TOKEN               { $$ = vars_push( INT_VAR, 1.0e6 ); }
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


int exp_runerror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	eprint( FATAL, "%s:%ld: Syntax error in EXPERIMENT section.\n",
			Fname, Lc );
	THROW( EXCEPTION );
}
