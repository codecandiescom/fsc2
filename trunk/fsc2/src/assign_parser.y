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

static Var *CV;

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
%token OI_TOKEN              /* OTHER_1 */
%token OII_TOKEN             /* OTHER_2 */
%token OIII_TOKEN            /* OTHER_3 */
%token OIV_TOKEN             /* OTHER_4 */

%token POD_TOKEN             /* POD */
%token CH_TOKEN              /* CHANNEL */
%token DEL_TOKEN             /* DELAY */
%token INV_TOKEN             /* INVERTED */
%token VH_TOKEN              /* V_HIGH */
%token VL_TOKEN              /* V_LOW */

%token TB_TOKEN              /* TIMEBASE */
%token TM_TOKEN              /* TRIGGERMODE */

%token MODE_TOKEN            /* MODE */
%token INTERN_TOKEN          /* INTERNAL */
%token EXTERN_TOKEN          /* EXTERNAL */

%token SLOPE_TOKEN           /* SLOPE */
%token NEG_TOKEN             /* NEGATIVE */
%token POS_TOKEN             /* POSITIVE */

%token THRESH_TOKEN          /* THRESHOLD */

%token REPT_TOKEN            /* REPEAT TIME */
%token REPF_TOKEN            /* REPEAT FREQUENCY */

%token <vptr> VAR_TOKEN      /* variable */
%token <vptr> VAR_REF		 
%token <vptr> FUNC_TOKEN     /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token EQ LT LE GT GE

%token NU_TOKEN UU_TOKEN MU_TOKEN KU_TOKEN MEG_TOKEN
%token NT_TOKEN UT_TOKEN MT_TOKEN T_TOKEN

%type <dval> 
%type <vptr> expr list1 unit thresh slope sl_val rept repf

%left EQ LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%right '^'
%left NEG


%%


input:   /* empty */
       | input line ';'            { Channel_Type = PULSER_CHANNEL_NO_TYPE;
                                     assert( Var_Stack == NULL ); }
       | input SECTION_LABEL       { assert( Var_Stack == NULL );
	                                 YYACCEPT; }
       | input error ';'           { THROW ( SYNTAX_ERROR_EXCEPTION ); }
       | input ';'                 { Channel_Type = PULSER_CHANNEL_NO_TYPE;
                                     assert( Var_Stack == NULL ); }
;


/* A (non-empty) line has to start with one of the channel keywords or either
   TIMEBASE or TRIGGERMODE */

line:    func pcd                  { }
       | func pcd SECTION_LABEL    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd func error       { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd TB_TOKEN         { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd TM_TOKEN         { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb                        { }
       | tb func                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb TM_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm                        { }
       | tm func                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm TB_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;								   
								   
								   
func:    MW_TOKEN                  { Channel_Type = PULSER_CHANNEL_MW; }
	   | TWT_TOKEN                 { Channel_Type = PULSER_CHANNEL_TWT; }
       | TWT_GATE_TOKEN            { Channel_Type = PULSER_CHANNEL_TWT_GATE; }
       | DET_TOKEN                 { Channel_Type = PULSER_CHANNEL_DET; }
	   | DET_GATE_TOKEN            { Channel_Type = PULSER_CHANNEL_DET_GATE; }
       | RF_TOKEN                  { Channel_Type = PULSER_CHANNEL_RF; }
	   | RF_GATE_TOKEN             { Channel_Type = PULSER_CHANNEL_RF_GATE; }
       | PHX_TOKEN                 { Channel_Type = PULSER_CHANNEL_PHASE_X; }
       | PHY_TOKEN                 { Channel_Type = PULSER_CHANNEL_PHASE_Y; }
       | OI_TOKEN                  { Channel_Type = PULSER_CHANNEL_OTHER_1; }
       | OII_TOKEN                 { Channel_Type = PULSER_CHANNEL_OTHER_2; }
       | OIII_TOKEN                { Channel_Type = PULSER_CHANNEL_OTHER_3; }
       | OIV_TOKEN                 { Channel_Type = PULSER_CHANNEL_OTHER_4; }
;


/* Pod and channel assignments consists of the POD and CHANNEL keyword,
   followed by the pod or channel number(s), and, optionally, a DELAY keyword
   followed by the delay time and the INVERTED keyword. The sequence of the
   keywords is arbitrary. */


pcd:    /* empty */                { p_assign_channel( Channel_Type, 
													   vars_push( INT_VAR,
															   UNDEFINED ) ); }
      | pcd podd
      | pcd chd
      | pcd deld
      | pcd invd
	  | pcd vh
      | pcd vl
;


podd:   pod
      | pod ','
;

pod:    POD_TOKEN INT_TOKEN        { p_assign_pod( Channel_Type,
												  vars_push( INT_VAR, $2 ) ); }
      | POD_TOKEN '=' INT_TOKEN    { p_assign_pod( Channel_Type,
												  vars_push( INT_VAR, $3 ) ); }
;								  
								  
chd:    CH_TOKEN ch1			  
      | CH_TOKEN '=' ch1		  
								  
ch1:    INT_TOKEN                  { p_assign_channel( Channel_Type,
												  vars_push( INT_VAR, $1 ) ); }
      | INT_TOKEN ','              { p_assign_channel( Channel_Type,
												  vars_push( INT_VAR, $1 ) ); }
      | ch1 INT_TOKEN              { p_assign_channel( Channel_Type,
												  vars_push( INT_VAR, $2 ) ); }
      | ch1 INT_TOKEN ','          { p_assign_channel( Channel_Type,
												  vars_push( INT_VAR, $2 ) ); }
;								  
								  
deld:   del						  
      | del ','					  
;								  
								  
del:    DEL_TOKEN expr             { p_set_delay( Channel_Type, $2 ); }
      | DEL_TOKEN '=' expr         { p_set_delay( Channel_Type, $3 ); }
;								  
								  
invd:   INV_TOKEN                  { p_inv( Channel_Type ); }
      | INV_TOKEN ','              { p_inv( Channel_Type ); }
;

vh:     VH_TOKEN '=' expr          { p_set_v_high( Channel_Type, $3 ); }
      | VH_TOKEN expr              { p_set_v_high( Channel_Type, $2 ); }
      | VH_TOKEN '=' expr ','      { p_set_v_high( Channel_Type, $3 ); }
      | VH_TOKEN expr ','          { p_set_v_high( Channel_Type, $2 ); }
;

vl:     VL_TOKEN '=' expr          { p_set_v_low( Channel_Type, $3 ); }
      | VL_TOKEN expr              { p_set_v_low( Channel_Type, $2 ); }
      | VL_TOKEN '=' expr ','      { p_set_v_low( Channel_Type, $3 ); }
      | VL_TOKEN expr ','          { p_set_v_low( Channel_Type, $2 ); }
;

expr:    INT_TOKEN unit            { $$ = apply_unit( vars_push( INT_VAR, $1 ),
													  $2 ); }
       | FLOAT_TOKEN unit          { $$ = apply_unit( 
		                                    vars_push( FLOAT_VAR, $1 ), $2 ); }
       | VAR_TOKEN unit            { $$ = apply_unit( $1, $2 ); }
       | VAR_TOKEN '['             { vars_arr_start( $1 ); }
         list1 ']'                 { CV = vars_arr_rhs( $4 ); }
         unit                      { $$ = apply_unit( CV, $7 ); }
       | FUNC_TOKEN '(' list2 ')'  { CV = func_call( $1 ); }
         unit                      { $$ = apply_unit( CV, $6 ); }
       | VAR_REF                   { $$ = $1; }
       | VAR_TOKEN '('             { eprint( FATAL, "%s:%ld: `%s' isn't a "
											 "function.\n", Fname, Lc,
											 $1->name );
	                                  THROW( EXCEPTION ); }
       | FUNC_TOKEN '['            { eprint( FATAL, "%s:%ld: `%s' is a "
											 "predefined function.\n",
											 Fname, Lc, $1->name );
	                                 THROW( EXCEPTION ); }
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
       | '(' expr ')' unit         { $$ = apply_unit( $2, $4 ); }
;

unit:    /* empty */               { $$ = NULL; }
       | NT_TOKEN  				   { $$ = vars_push( FLOAT_VAR, 1.0e-5 ); }
       | UT_TOKEN  				   { $$ = vars_push( FLOAT_VAR, 1.0e-2 ); }
       | MT_TOKEN  				   { $$ = vars_push( INT_VAR, 10 ); }
       | T_TOKEN   				   { $$ = vars_push( INT_VAR, 10000 ); }
       | NU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | UU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | MU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | KU_TOKEN                  { $$ = vars_push( INT_VAR, 1000 ); }
       | MEG_TOKEN                 { $$ = vars_push( INT_VAR, 1000000 ); }
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


/* handling of time base lines */

tb:       TB_TOKEN expr            { p_set_timebase( $2 ); }
;

/* handling of trigger mode lines */

tm:       TM_TOKEN tmp
;

tmp:      /* empty */
        | tmp exter                { p_set_trigger_mode( vars_push( INT_VAR,
																EXTERNAL ) ); }
		| tmp exter ','            { p_set_trigger_mode( vars_push( INT_VAR,
																EXTERNAL ) ); }
        | tmp slope                { p_set_trigger_slope( $2 ); }
        | tmp slope ','            { p_set_trigger_slope( $2 ); }
        | tmp thresh               { p_set_trigger_level( $2 ); }
        | tmp thresh ','           { p_set_trigger_level( $2 ); }
        | tmp inter                { p_set_trigger_mode( vars_push( INT_VAR,
																INTERNAL ) ); }
        | tmp inter ','            { p_set_trigger_mode( vars_push( INT_VAR,
																INTERNAL ) ); }
        | tmp rept                 { p_set_rep_time( $2 ); }
        | tmp rept ','             { p_set_rep_time( $2 ); }
        | tmp repf                 { p_set_rep_freq( $2 ); }
        | tmp repf ','             { p_set_rep_freq( $2 ); }
;

exter:    MODE_TOKEN '=' EXTERN_TOKEN
        | MODE_TOKEN EXTERN_TOKEN
        | EXTERN_TOKEN
;

slope:    SLOPE_TOKEN '=' sl_val   { $$ = $3; }
	    | SLOPE_TOKEN sl_val       { $$ = $2; }
;

sl_val:   NEG_TOKEN                { $$ = vars_push( INT_VAR, NEGATIVE ); }
        | POS_TOKEN                { $$ = vars_push( INT_VAR, POSITIVE ); }
        | '-'                      { $$ = vars_push( INT_VAR, NEGATIVE ); }
		| '+'                      { $$ = vars_push( INT_VAR, POSITIVE ); }
;

thresh:   THRESH_TOKEN '=' expr    { $$ = $3; }
        | THRESH_TOKEN expr        { $$ = $2; }
;

inter:    MODE_TOKEN '=' INTERN_TOKEN
        | MODE_TOKEN INTERN_TOKEN
        | INTERN_TOKEN
;

rept:     REPT_TOKEN '=' expr      { $$ = $3; }
        | REPT_TOKEN expr          { $$ = $2; }
;

repf:     REPF_TOKEN '=' expr      { $$ = $3; }
        | REPF_TOKEN expr          { $$ = $2; }
;


%%


int assignerror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *assigntext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in ASSIGNMENTS "
				"section.\n", Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, assigntext );
	THROW( EXCEPTION );
}
