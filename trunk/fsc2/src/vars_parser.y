/*
  $Id$
*/

%{

#include "fsc2.h"


extern int varslex( void );


/* locally used functions */

int varsparse( void );
void varserror( const char *s );

extern char *varstext;


static Var *CV;

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
%token EQ LT LE GT GE
%token AND OR XOR NOT


%token NT_TOKEN UT_TOKEN MT_TOKEN T_TOKEN
%token NU_TOKEN UU_TOKEN MU_TOKEN KU_TOKEN MEG_TOKEN
%type <vptr> expr arrass list1 list2 list3 unit

%left AND OR XOR
%left NOT
%left EQ LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%left NEG
%right '^'


%%

input:   /* empty */
       | input ';'
       | input line ';'           { assert( Var_Stack == NULL ); }
       | input line SECTION_LABEL { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL      { YYACCEPT; }
;								  
								  
line:    linet					  
       | line ',' linet			  
       | line linet               { THROW( MISSING_SEMICOLON_EXCEPTION ); }

;								  
								  
linet:   VAR_TOKEN                { } /* no assignment to be done */
       | VAR_TOKEN '=' expr       { vars_assign( $3, $1 ); }
       | VAR_TOKEN '['            { vars_arr_start( $1 ); }
         list1 ']'                { vars_arr_lhs( $4 ); }
         arhs					  
       | FUNC_TOKEN '(' list4 ')' { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN '['           { eprint( FATAL, "%s:%ld: `%s' is a "
								  			 "function and not an array.\n",
								  			 Fname, Lc, $1->name );
	                                THROW( EXCEPTION ); }
       | VAR_TOKEN '('            { eprint( FATAL, "%s:%ld: `%s' is an "
								  			 "array and not a function.\n",
								  			 Fname, Lc, $1->name );
	                                THROW( EXCEPTION ); }
;

expr:    INT_TOKEN unit           { $$ = apply_unit( vars_push( INT_VAR, $1 ),
													 $2 ); }
       | FLOAT_TOKEN unit         { $$ = apply_unit(
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | VAR_TOKEN unit           { $$ = apply_unit( $1, $2 ); }
       | VAR_TOKEN '['            { vars_arr_start( $1 ); }
         list3 ']'                { CV = vars_arr_rhs( $4 ); }
         unit                     { $$ = apply_unit( CV, $7 ); }
       | FUNC_TOKEN '(' list4 ')' { CV = func_call( $1 ); }
         unit                     { $$ = apply_unit( CV, $6 ); }
       | VAR_REF                  { $$ = $1; }
       | VAR_TOKEN '('            { eprint( FATAL, "%s:%ld: `%s' isn't a "
											"function.\n", Fname, Lc,
											$1->name );
	                                 THROW( EXCEPTION ); }
       | FUNC_TOKEN '['           { eprint( FATAL, "%s:%ld: `%s' is a "
											"predefined function.\n",
											Fname, Lc, $1->name );
	                                THROW( EXCEPTION ); }
       | expr AND expr       	  { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr OR expr        	  { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr XOR expr       	  { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr            	  { $$ = vars_lnegate( $2 ); }
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
       | '(' expr ')' unit        { $$ = apply_unit( $2, $4 ); }
;


unit:    /* empty */               { $$ = NULL; }
       | NT_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | UT_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | MT_TOKEN                  { $$ = vars_push( INT_VAR, 10 ); }
       | T_TOKEN                   { $$ = vars_push( INT_VAR, 10000 ); }
       | NU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | UU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | MU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | KU_TOKEN                  { $$ = vars_push( INT_VAR, 1000 ); }
       | MEG_TOKEN                 { $$ = vars_push( INT_VAR, 1000000 ); }
;


arhs:    /* empty */               { vars_arr_init( vars_push( UNDEF_VAR ) ); }
       | '=' arrass                { vars_arr_init( $2 ); }
       | '=' expr                  { vars_assign( $2, $2->prev ); }
;

/* list of sizes of newly declared array */

list1:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
       | expr                      { $$ = $1; }
       | '*'                       { ( $$ = vars_push( INT_VAR, 0 ) )->flags
										 |= VARIABLE_SIZED; }
       | list1 ',' expr            { $$ = $1; }
       | list1 ',' '*'             { ( $$ = vars_push( INT_VAR, 0 ) )->flags
										 |= VARIABLE_SIZED; }
;

/* list of data for initialization of a newly declared array */

arrass:   '{' '}'                  { $$ = vars_push( UNDEF_VAR ); }
        | '{' list2 '}'            { $$ = $2; }
;

list2:   expr                      { $$ = $1; }
       | list2 ',' expr            { $$ = $3; }
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


void varserror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *varstext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in VARIABLES "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, varstext );
	THROW( EXCEPTION );
}
