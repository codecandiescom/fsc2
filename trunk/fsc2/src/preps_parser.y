/******************************************************************/
/* This is the parser for the PREPARATIONS section of an EDL file */
/******************************************************************/


%{

#include "fsc2.h"


extern int prepslex( void );

extern char *prepstext;


/* locally used functions */

void prepserror( const char *s );

Var *P_Var;


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

%token <vptr> VAR_REF

%token <vptr>  VAR_TOKEN        /* variable */
%token <vptr> FUNC_TOKEN        /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ LT LE GT GE

%token NS_TOKEN US_TOKEN MS_TOKEN S_TOKEN
%type <vptr> expr time unit list3


%left EQ LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left NEG


%%


input:   /* empty */
       | input ';'
       | input line ';'            { assert( Var_Stack == NULL ); }
       | input error ';'           { THROW( SYNTAX_ERROR_EXCEPTION ); }
       | input line line           { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { YYACCEPT; }
;



/* currently only the variables related stuff */

line:    P_TOK prop
       | VAR_TOKEN '=' expr        { vars_assign( $3, $1 ); }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list3 ']'                 { vars_arr_lhs( $4 ); }
         '=' expr                  { vars_assign( $8, $8->prev ); }
       | FUNC_TOKEN '(' list4 ')'  { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN '['            { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( VARIABLES_EXCEPTION ); }
;



prop:   /* empty */
       | prop F_TOK sep1 expr sep2  { pulse_set( Cur_Pulse, P_FUNC, $4 ); }
       | prop S_TOK sep1 time sep2  { pulse_set( Cur_Pulse, P_POS, $4 ); }
       | prop L_TOK sep1 time sep2  { pulse_set( Cur_Pulse, P_LEN,$4 ); }
       | prop DS_TOK sep1 time sep2 { pulse_set( Cur_Pulse, P_DPOS, $4 ); }
       | prop DL_TOK sep1 time sep2 { pulse_set( Cur_Pulse, P_DLEN, $4 ); }
       | prop ML_TOK sep1 time sep2 { pulse_set( Cur_Pulse, P_MAXLEN, $4 ); }
;


time:    expr unit                 { $$ = vars_mult( $1, $2 ); }
;


unit:    /* empty */               { $$ = vars_push( INT_VAR,
													 Default_Time_Base ); }
       | NS_TOKEN                  { $$ = vars_push( INT_VAR, 1L ); }
       | US_TOKEN                  { $$ = vars_push( INT_VAR, 1000L ); }
       | MS_TOKEN                  { $$ = vars_push( INT_VAR, 1000000L ); }
       | S_TOKEN                   { $$ = vars_push( INT_VAR, 1000000000L ); }
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


expr:    INT_TOKEN                 { $$ = vars_push( INT_VAR, $1 ); }
       | FLOAT_TOKEN               { $$ = vars_push( FLOAT_VAR, $1 ); }
       | VAR_TOKEN                 { $$ = vars_push_copy( $1 ); }
       | VAR_REF                   { $$ = $1; }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list3 ']'                 { $$ = vars_arr_rhs( $4 ); }
       | FUNC_TOKEN '(' list4 ')'  { $$ = func_call( $1 ); }
       | expr EQ expr              { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
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
       | '-' expr %prec NEG        { $$ = vars_negate( $2 ); }
       | '(' expr ')'              { $$ = $2 }
;


/* list of indices for access of an array element */


list3:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
	   | expr                      { $$ = $1; }
       | list3 ',' expr            { $$ = $3; }
;

/* list of function arguments */

list4:   /* empty */
       | exprs
	   | list4 ',' exprs
;

exprs:   expr                      { }
       | STR_TOKEN                 { vars_push( STR_VAR, $1 ); }
;


%%


void prepserror ( const char *s )
{
	if ( *prepstext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in PREPARATIONS "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, prepstext );
	THROW( PREPARATIONS_EXCEPTION );
}
