/*
  $Id$
*/

/******************************************************************/
/* This is the parser for the PREPARATIONS section of an EDL file */
/******************************************************************/


%{

#include "fsc2.h"


extern int prepslex( void );

extern char *prepstext;


/* locally used functions */

void prepserror( const char *s );

static Var *CV;

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
%token PH_TOK                   /* phase sequence */
%token ML_TOK                   /* maximum pulse length */
%token RP_TOK                   /* replacement pulses */


%token <vptr> VAR_TOKEN         /* variable */
%token <vptr> VAR_REF
%token <vptr> FUNC_TOKEN        /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token <vptr> RPP_TOK
%token EQ LT LE GT GE

%token NS_TOKEN US_TOKEN MS_TOKEN S_TOKEN
%type <vptr> expr unit list1


%left EQ LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left NEG


%%


input:   /* empty */
       | input ';'                 { Cur_Pulse = NULL; }
       | input line ';'            { Cur_Pulse = NULL;
	                                 assert( Var_Stack == NULL ); }
       | input error ';'           { THROW( SYNTAX_ERROR_EXCEPTION ); }
       | input line line           { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { Cur_Pulse = NULL;
	                                 assert( Var_Stack == NULL );
	                                 YYACCEPT; }
;

line:    P_TOK prop
       | VAR_TOKEN '=' expr        { vars_assign( $3, $1 ); }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list1 ']'                 { vars_arr_lhs( $4 ); }
         '=' expr                  { vars_assign( $8, $8->prev );
                                     assert( Var_Stack == NULL ); }
       | FUNC_TOKEN '(' list2 ')'  { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN '['            { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( EXCEPTION ); }
;

prop:   /* empty */
       | prop F_TOK sep1 expr sep2  { pulse_set( Cur_Pulse, P_FUNC, $4 ); }
       | prop S_TOK sep1 expr sep2  { pulse_set( Cur_Pulse, P_POS, $4 ); }
       | prop L_TOK sep1 expr sep2  { pulse_set( Cur_Pulse, P_LEN,$4 ); }
       | prop DS_TOK sep1 expr sep2 { pulse_set( Cur_Pulse, P_DPOS, $4 ); }
       | prop DL_TOK sep1 expr sep2 { pulse_set( Cur_Pulse, P_DLEN, $4 ); }
       | prop ML_TOK sep1 expr sep2 { pulse_set( Cur_Pulse, P_MAXLEN, $4 ); }
       | prop RP_TOK sep1 rps sep2
;

/* replacement pulse settings */

rps:     RPP_TOK                    { pulse_set( Cur_Pulse, P_REPL, $1 ); }
       | rps sep2 RPP_TOK           { pulse_set( Cur_Pulse, P_REPL, $3 ); }
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


expr:    INT_TOKEN unit           { $$ = vars_mult( vars_push( INT_VAR, $1 ), 
													$2 ); }
       | FLOAT_TOKEN unit         { $$ = vars_mult( vars_push( FLOAT_VAR, $1 ),
													$2 ); }
       | VAR_TOKEN unit           { $$ = vars_mult( $1, $2 ); }
       | VAR_TOKEN '['            { vars_arr_start( $1 ); }
         list1 ']'                { CV = vars_arr_rhs( $4 ); }
         unit                     { if ( CV->type & ( INT_VAR | FLOAT_VAR ) )
			                            $$ = vars_mult( CV, $7 );
		                            else
									{
										vars_pop( $7 );
									    $$ = CV;
									} }
       | FUNC_TOKEN '(' list2 ')' { CV = func_call( $1 ); }
         unit                     { if ( CV->type & ( INT_VAR | FLOAT_VAR ) )
			                            $$ = vars_mult( CV, $6 );
		                            else
									{
										vars_pop( $6 );
									    $$ = CV;
									} }
       | VAR_REF
       | VAR_TOKEN '('            { eprint( FATAL, "%s:%ld: `%s' isn't a "
											"function.\n", Fname, Lc,
											$1->name );
	                                 THROW( EXCEPTION ); }
       | FUNC_TOKEN '['           { eprint( FATAL, "%s:%ld: `%s' is a "
											"predefined function.\n",
											Fname, Lc, $1->name );
	                                THROW( EXCEPTION ); }
       | expr EQ expr             { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
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
       | '-' expr %prec NEG       { $$ = vars_negate( $2 ); }
       | '(' expr ')' unit        { $$ = vars_mult( $2, $4 ); }
;

unit:    /* empty */               { $$ = vars_push( INT_VAR, 1L ); }
       | NS_TOKEN                  { $$ = vars_push( INT_VAR, 1L ); }
       | US_TOKEN                  { $$ = vars_push( INT_VAR, 1000L ); }
       | MS_TOKEN                  { $$ = vars_push( INT_VAR, 1000000L ); }
       | S_TOKEN                   { $$ = vars_push( INT_VAR, 1000000000L ); }
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
       | STR_TOKEN                 { vars_push( STR_VAR, $1 ); }
;


%%


void prepserror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *prepstext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in PREPARATIONS "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, prepstext );
	THROW( EXCEPTION );
}
