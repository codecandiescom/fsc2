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

long One      = 1L;
long Thousand = 1000L;
long Million  = 1000000L;
long Billion  = 1000000000L;


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

%token <lval> FUNC_TOK          /* type of function */


%token <vptr>  VAR_TOKEN        /* variable */
%token <vptr> FUNC_TOKEN        /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token PRINT_TOK
%token EQ LT LE GT GE

%token NS_TOKEN US_TOKEN MS_TOKEN S_TOKEN
%type <lval> func
%type <vptr> expr line time unit


%left EQ LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left NEG


%%


input:   /* empty */
       | input ';'
       | input line ';'
       | input error ';'           { THROW( SYNTAX_ERROR_EXCEPTION ); }
       | input line line           { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { YYACCEPT; }
;



/* currently only the variables related stuff */

line:    P_TOK pprop               {}
       | VAR_TOKEN '=' expr        { vars_assign( $3, $1 );
                                     assert( Var_Stack == NULL );
	                                 assert( Arr_Stack == NULL ); }
       | VAR_TOKEN '['             { vars_push_astack( $1 ); }
         list3 ']' '=' expr        { vars_arr_assign( $1, $7 );
                                     assert( Var_Stack == NULL );
	                                 assert( Arr_Stack == NULL ); }
       | PRINT_TOK '(' STR_TOKEN   { P_Var = vars_push( UNDEF_VAR, $3 ); }
         list5 ')'                 { vars_pop( print_args( P_Var ) ); }
       | PRINT_TOK '['             { eprint( FATAL, "%s:%ld: `print' is a "
											 "predefined function.\n",
											 Fname, Lc );
	                                 THROW( VARIABLES_EXCEPTION ); }
;



pprop:   /* empty */
       | pprop F_TOK sep1 func sep2 
                                   { long f = $4;
									 pulse_set( Cur_Pulse, P_FUNC,
												vars_push( INT_VAR, &f ) ); }
       | pprop S_TOK sep1 time sep2     
                                   { pulse_set( Cur_Pulse, P_POS, $4 ); }
       | pprop L_TOK sep1 time sep2 
                                   { pulse_set( Cur_Pulse, P_LEN,$4 ); }
       | pprop DS_TOK sep1 time sep2
                                   { pulse_set( Cur_Pulse, P_DPOS, $4 ); }
       | pprop DL_TOK sep1 time sep2
                                   { pulse_set( Cur_Pulse, P_DLEN, $4 ); }
       | pprop ML_TOK sep1 time sep2
                                   { pulse_set( Cur_Pulse, P_MAXLEN, $4 ); }
;


func:    FUNC_TOK                  { $$ = $1; }
       | INT_TOKEN                 { $$ = $1; }
;

time:    expr unit                 { $$ = vars_mult( $1, $2 ); }
;


unit:   /* empty */                { $$ = vars_push( INT_VAR,
													 &Default_Time_Base ); }
      | NS_TOKEN                   { $$ = vars_push( INT_VAR, &One ); }
      | US_TOKEN                   { $$ = vars_push( INT_VAR, &Thousand ); }
      | MS_TOKEN                   { $$ = vars_push( INT_VAR, &Million ); }
      | S_TOKEN                    { $$ = vars_push( INT_VAR, &Billion ); }
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


expr:    INT_TOKEN                 { $$ = vars_push( INT_VAR, &$1 ); }
       | FLOAT_TOKEN               { $$ = vars_push( FLOAT_VAR, &$1 ); }
       | VAR_TOKEN                 { $$ = vars_push_simple( $1 ); }
       | VAR_TOKEN '['             { vars_push_astack( $1 ); }
         list3 ']'                 { $$ = vars_pop_astack( ); }
       | FUNC_TOKEN '(' list4 ')'  { $$ = func_call( $1 ); }
       | VAR_TOKEN '('             { eprint( FATAL, "%s:%ld: `%s' isn't a "
											 "function.\n", Fname, Lc,
											 $1->name );
	                                 THROW( UNKNOWN_FUNCTION_EXCEPTION ); }
       | PRINT_TOK '(' STR_TOKEN   { P_Var = vars_push( UNDEF_VAR, $3 ); }
         ',' list5 ')'             { $$ = print_args( P_Var ); }
       | PRINT_TOK '['             { eprint( FATAL, "%s:%ld: `print' is a "
											 "predefined function.\n",
											 Fname, Lc );
	                                 THROW( PREPARATIONS_EXCEPTION ); }
       | FUNC_TOKEN '['            { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( PREPARATIONS_EXCEPTION ); }
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

list3:   /* empty */
	   | expr                      { vars_update_astack( $1 ); }
       | list3 ',' expr            { vars_update_astack( $3 ); }
;

/* list of function arguments */

list4:   /* empty */
	   | expr                      { }
       | list4 ',' expr
;

/* list of print function arguments (following the format string) */

list5:   /* empty */
	   | list5 ',' expr
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
