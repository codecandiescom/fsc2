/*
  $Id$
*/

/*****************************************************************/
/* This is the parser for the ASSIGNMENTS section of an EDL file */
/*****************************************************************/


%{

#include "fsc2.h"

extern int assignlex( void );
extern char *assigntext;

/* locally used functions */

int assignerror( const char *s );

/* locally used global variables */

int Channel_Type;

/* externally (in assign_lexer.flex) defined global variables */ 

%}


%union {
	long   lval;
	double dval;
	char   *sptr;
	Var    *vptr;
}


%token SECTION_LABEL         /* new section label */
%token MW_TOKEN              /* MICRO_WAVE */
%token TWT_TOKEN             /* TRAVELING_WAVE_TUBE */
%token TWT_GATE_TOKEN        /* TRAVELING_WAVE_TUBE_GATE */
%token DET_TOKEN             /* DETECTION */
%token DET_GATE_TOKEN        /* DETECTION_GATE */
%token RF_TOKEN              /* RADIO_FREQUENCY */
%token RF_GATE_TOKEN         /* RADIO_FREQUENCY_GATE */
%token PHX_TOKEN             /* PHASE_X */
%token PHY_TOKEN             /* PHASE_Y */
%token POD_TOKEN             /* POD */
%token CH_TOKEN              /* CHANNEL */
%token DEL_TOKEN             /* DELAY */
%token INV_TOKEN             /* INVERTED */
%token NS_TOKEN              /* nsec */
%token US_TOKEN              /* usec */
%token MS_TOKEN              /* msec */
%token S_TOKEN               /* sec */

%token <vptr> VAR_TOKEN            /* variable */
%token <vptr> VAR_REF
%token <vptr> FUNC_TOKEN           /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ LT LE GT GE

%type <dval> time unit
%type <vptr> expr list1

%left EQ LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left NEG


%%


input:   /* empty */
       | input line ';'               { Channel_Type =
											PULSER_CHANNEL_NO_TYPE;
                                        assert( Var_Stack == NULL ); }
       | input SECTION_LABEL          { assert( Var_Stack == NULL );
	                                    YYACCEPT; }
       | input error ';'              { THROW ( SYNTAX_ERROR_EXCEPTION ); }
       | input ';'
;


/* A (non-empty) line has to start with one of the channel keywords */

line:    keywd pcd                    { }
       | keywd pcd SECTION_LABEL      { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | keywd pcd keywd error        { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | VAR_TOKEN '=' expr           { vars_assign( $3, $1 ); }
       | VAR_TOKEN '['                { vars_arr_start( $1 ); }
         list1 ']'                    { vars_arr_lhs( $4 ); }
         '=' expr ';'                 { vars_assign( $8, $8->prev );
                                        assert( Var_Stack == NULL ); }
       | FUNC_TOKEN '(' list2 ')' ';' { vars_pop( func_call( $1 ) ); }
       | FUNC_TOKEN '['               { eprint( FATAL, "%s:%ld: `%s' is a "
												"function and not an array.\n",
												Fname, Lc, $1->name );
	                                    THROW( VARIABLES_EXCEPTION ); }
;


keywd:   MW_TOKEN                     { Channel_Type = PULSER_CHANNEL_MW; }
	   | TWT_TOKEN                    { Channel_Type = PULSER_CHANNEL_TWT; }
       | TWT_GATE_TOKEN               { Channel_Type = 
											PULSER_CHANNEL_TWT_GATE; }
       | DET_TOKEN                    { Channel_Type = PULSER_CHANNEL_DET; }
	   | DET_GATE_TOKEN               { Channel_Type =
											PULSER_CHANNEL_DET_GATE; }
       | RF_TOKEN                     { Channel_Type = PULSER_CHANNEL_RF; }
	   | RF_GATE_TOKEN                { Channel_Type =
											PULSER_CHANNEL_RF_GATE; }
       | PHX_TOKEN                    { Channel_Type = 
											PULSER_CHANNEL_PHASE_X; }
       | PHY_TOKEN                    { Channel_Type =
											PULSER_CHANNEL_PHASE_Y; }
;


/* Pod and channel assignments consists of the POD and CHANNEL keyword,
   followed by the pod or channel number(s), and, optionally, a DELAY keyword
   followed by the delay time and the INVERTED keyword. The sequence of the
   keywords is arbitrary. */


pcd:    /* empty */
      | pcd podd
      | pcd chd
      | pcd deld
      | pcd invd
;


podd:   pod
      | pod ','
;

pod:    POD_TOKEN INT_TOKEN          { assign_pod( Channel_Type, $2 ); }
      | POD_TOKEN '=' INT_TOKEN      { assign_pod( Channel_Type, $3 ); }
;

chd:    CH_TOKEN ch1
      | CH_TOKEN '=' ch1

ch1:    INT_TOKEN                    { assign_channel( Channel_Type, $1 ); }
      | INT_TOKEN ','                { assign_channel( Channel_Type, $1 ); }
      | ch1 INT_TOKEN                { assign_channel( Channel_Type, $2 ); }
      | ch1 INT_TOKEN ','            { assign_channel( Channel_Type, $2 ); }
;

deld:   del
      | del ','
;

del:    DEL_TOKEN time               { set_pod_delay( Channel_Type, $2 ); }
      | DEL_TOKEN '=' time           { set_pod_delay( Channel_Type, $3 ); }
;

time:   INT_TOKEN unit               { $$ = ( double ) $1 * $2; }
      | FLOAT_TOKEN unit             { $$ = $1 * $2; }
;									 
									 
unit:   /* empty */                  { $$ = 1.0; }
      | NS_TOKEN                     { $$ = 1.0; }
      | US_TOKEN                     { $$ = 1.0e3; }
      | MS_TOKEN                     { $$ = 1.0e6; }
      | S_TOKEN                      { $$ = 1.0e9; }
;

invd:   INV_TOKEN                    { assign_inv_channel( Channel_Type ); }
      | INV_TOKEN ','                { assign_inv_channel( Channel_Type ); }
;

expr:   INT_TOKEN                    { $$ = vars_push( INT_VAR, $1 ); }
      | FLOAT_TOKEN                  { $$ = vars_push( FLOAT_VAR, $1 ); }
      | VAR_TOKEN                    { $$ = vars_push_copy( $1 ); }
      | VAR_TOKEN '['                { vars_arr_start( $1 ); }
        list1 ']'                    { $$ = vars_arr_rhs( $4 ); }
      | FUNC_TOKEN '(' list2 ')'     { $$ = func_call( $1 ); }
      | VAR_REF                      { $$ = $1; }
      | expr EQ expr                 { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
      | expr LT expr                 { $$ = vars_comp( COMP_LESS, $1, $3 ); }
      | expr GT expr                 { $$ = vars_comp( COMP_LESS, $3, $1 ); }
      | expr LE expr                 { $$ = vars_comp( COMP_LESS_EQUAL,
	 							     				   $1, $3 ); }
      | expr GE expr                 { $$ = vars_comp( COMP_LESS_EQUAL,
	 							     				   $3, $1 ); }
      | expr '+' expr                { $$ = vars_add( $1, $3 ); }
      | expr '-' expr                { $$ = vars_sub( $1, $3 ); }
      | expr '*' expr                { $$ = vars_mult( $1, $3 ); }
      | expr '/' expr                { $$ = vars_div( $1, $3 ); }
      | expr '%' expr                { $$ = vars_mod( $1, $3 ); }
      | expr '^' expr                { $$ = vars_pow( $1, $3 ); }
      | '-' expr %prec NEG           { $$ = vars_negate( $2 ); }
      | '(' expr ')'                 { $$ = $2 }
;


/* list of indices for access of an array element */


list1:   /* empty */                 { $$ = vars_push( UNDEF_VAR ); }
	   | expr                        { $$ = $1; }
       | list1 ',' expr              { $$ = $3; }
;

/* list of function arguments */

list2:   /* empty */
       | exprs
	   | list2 ',' exprs
;

exprs:   expr                        { }
       | STR_TOKEN                   { vars_push( STR_VAR, $1 ); }
;



%%


int assignerror ( const char *s )
{
	if ( *assigntext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in ASSIGNMENTS "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, assigntext );
	THROW( VARIABLES_EXCEPTION );
}
