%{

#include "fsc2.h"


extern int variableslex( void );


/* locally used functions */

int variablesparse( void );
void variableserror( const char *s );

extern char *variablestext;


Var *P_Var;

%}


%union {
	long   lval;
    double dval;
	char   *sptr;
	Var    *vptr;
}


%token SECTION_LABEL            /* new section label */
%token <vptr>  VAR_TOKEN        /* variable */
%token <vptr> FUNC_TOKEN        /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ LT LE GT GE


%type <vptr> expr line

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
/*       | input line */
       | input line line           { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input line SECTION_LABEL  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | input SECTION_LABEL       { YYACCEPT; }
;

line:    VAR_TOKEN                 /* no assignment to be done */
       | VAR_TOKEN '=' expr        { vars_new_assign( $3, $1 );
                                     assert( Var_Stack == NULL );
	                                 assert( Arr_Stack == NULL ); }
       | VAR_TOKEN '['             { vars_arr_start( Cur_Arr = $1 ); }
         list1 ']' arrass          { assert( Var_Stack == NULL );
	                                 assert( Arr_Stack == NULL ); }
       | FUNC_TOKEN '(' list4 ')'  { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN '['            { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( VARIABLES_EXCEPTION ); }
;

expr:    INT_TOKEN                 { $$ = vars_push( INT_VAR, $1 ); }
       | FLOAT_TOKEN               { $$ = vars_push( FLOAT_VAR, $1 ); }
       | VAR_TOKEN                 { $$ = vars_push_copy( $1 ); }
       | VAR_TOKEN '['             { vars_push_astack( $1 ); }
         list3 ']'                 { $$ = vars_pop_astack( ); }
       | FUNC_TOKEN '(' list4 ')'  { $$ = func_call( $1 ); }
       | VAR_TOKEN '('             { eprint( FATAL, "%s:%ld: `%s' isn't a "
											 "function.\n", Fname, Lc,
											 $1->name );
	                                 THROW( UNKNOWN_FUNCTION_EXCEPTION ); }
       | FUNC_TOKEN '['            { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( VARIABLES_EXCEPTION ); }
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

/* list of sizes of newly declared array */

list1:   expr                      { vars_arr_extend( Cur_Arr, $1 ); }
       | '*'                       { vars_arr_extend( Cur_Arr, NULL ); }
       | list1 ',' expr            { vars_arr_extend( Cur_Arr, $3 ); }
       | list1 ',' '*'             { vars_arr_extend( Cur_Arr, NULL ); }
;

/* list of data for assignment to newly declared array */

arrass:  /* empty */
       | '=' '{' list2 '}'
;

list2:   expr                      { vars_arr_init( Cur_Arr, $1 ); }
       | list2 ',' expr            { vars_arr_init( Cur_Arr, $3 ); }
;

/* list of indices for access of an array element */

list3:   /* empty */
	   | expr                      { vars_update_astack( $1 ); }
       | list3 ',' expr            { vars_update_astack( $3 ); }
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


void variableserror ( const char *s )
{
	if ( *variablestext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in VARIABLES "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, variablestext );
	THROW( VARIABLES_EXCEPTION );
}
