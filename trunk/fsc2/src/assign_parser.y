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

static int Channel_Type;
static Var *CV;
static int Cur_PHS = -1;
static int Cur_PHST = -1;
static long Cur_PROT = PHASE_UNKNOWN_PROT;

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
%token DEF_TOKEN             /* DEFENSE */
%token RF_TOKEN              /* RADIO_FREQUENCY */
%token RF_GATE_TOKEN         /* RADIO_FREQUENCY_GATE */
%token PSH_TOKEN             /* PULSE_SHAPE */
%token PH1_TOKEN             /* PHASE_1 */
%token PH2_TOKEN             /* PHASE_2 */
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

%token IMP_TOKEN             /* IMPEDANCE */
%token HL_TOKEN              /* LOW or HIGH */

%token REPT_TOKEN            /* REPEAT TIME */
%token REPF_TOKEN            /* REPEAT FREQUENCY */

%token PSD_TOKEN             /* PHASE SWITCH DELAY */
%token GP_TOKEN              /* GRACE_PERIOD */

%token <lval> PHS_TOK PSD_TOKEN HL_TOKEN
%token POD1_TOK POD2_TOK ON_TOK OFF_TOK


%token <vptr> VAR_TOKEN      /* variable */
%token <vptr> VAR_REF		 
%token <vptr> FUNC_TOKEN     /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token <lval> PXY_TOK        /* X, Y, -X, -Y, CW -- for phases */
%token AND OR XOR NOT
%token EQ NE LT LE GT GE

%token NU_TOKEN UU_TOKEN MU_TOKEN KU_TOKEN MEG_TOKEN
%token NT_TOKEN UT_TOKEN MT_TOKEN T_TOKEN

%type <lval> phsv
%type <vptr> expr list1 unit sl_val

%left AND OR XOR
%left NOT
%left EQ NE LT LE GT GE
%left '+' '-'
%left '*' '/'
%left '%'
%left NEG
%right '^'


%%


input:   /* empty */
       | input line ';'            { Channel_Type = PULSER_CHANNEL_NO_TYPE;
	                                 Cur_PHS = -1;
	                                 Cur_PHST = -1;
                                     assert( Var_Stack == NULL ); }
       | input SECTION_LABEL       { assert( Var_Stack == NULL );
	                                 Cur_PROT = PHASE_UNKNOWN_PROT;
									 YYACCEPT; }
       | input error ';'           { THROW ( SYNTAX_ERROR_EXCEPTION ); }
       | input ';'                 { Channel_Type = PULSER_CHANNEL_NO_TYPE;
	                                 Cur_PHS = -1;
	                                 Cur_PHST = -1;
                                     assert( Var_Stack == NULL ); }
;


/* A (non-empty) line has to start with one of the channel keywords or either
   TIMEBASE or TRIGGERMODE */

line:    func pcd                  { }
       | func pcd SECTION_LABEL    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd error            { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd TB_TOKEN         { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd TM_TOKEN         { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd PHS_TOK          { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd PSD_TOKEN        { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | func pcd GP_TOKEN         { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb                        { }
       | tb func                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb TM_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb PHS_TOK                { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb PSD_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tb GP_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm                        { }
       | tm func                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm TB_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm PHS_TOK                { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm PSD_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | tm GP_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phs                       { p_phs_end( Cur_PHS ); }
       | phs TB_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phs TM_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phs PSD_TOKEN             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phs GP_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | psd                       { }
       | psd func                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | psd TB_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | psd TM_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | psd PHS_TOK               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | psd GP_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp                        { }
       | gp	func                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp	TB_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp	TM_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp	PHS_TOK                { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp	PSD_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp	GP_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;								   
								   
								   
func:    MW_TOKEN                  { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_MW;
                                     else
										 p_phase_ref( Cur_PHS,
													  PULSER_CHANNEL_MW ); }
	   | TWT_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_TWT;
                                     else
										 p_phase_ref( Cur_PHS,
													  PULSER_CHANNEL_TWT ); }
       | TWT_GATE_TOKEN            { if ( Cur_PHS == -1 )
	                                     Channel_Type =
											 PULSER_CHANNEL_TWT_GATE;
                                     else
										 p_phase_ref( Cur_PHS,
												   PULSER_CHANNEL_TWT_GATE ); }
       | DET_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_DET;
                                     else
										 p_phase_ref( Cur_PHS,
													  PULSER_CHANNEL_DET ); }
	   | DET_GATE_TOKEN            { if ( Cur_PHS == -1 )
	                                     Channel_Type =
											 PULSER_CHANNEL_DET_GATE;
                                     else
										 p_phase_ref( Cur_PHS,
												   PULSER_CHANNEL_DET_GATE ); }
       | DEF_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_DEFENSE;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_DEFENSE ); }
       | RF_TOKEN                  { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_RF;
                                     else
										 p_phase_ref( Cur_PHS,
													  PULSER_CHANNEL_RF ); }
	   | RF_GATE_TOKEN             { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_RF_GATE;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_RF_GATE ); }
	   | PSH_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = 
											        PULSER_CHANNEL_PULSE_SHAPE;
                                     else
										 p_phase_ref( Cur_PHS,
												PULSER_CHANNEL_PULSE_SHAPE ); }
       | PH1_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_PHASE_1;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_PHASE_1 ); }
       | PH2_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_PHASE_2;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_PHASE_2 ); }
       | OI_TOKEN                  { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_OTHER_1;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_OTHER_1 ); }
       | OII_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_OTHER_2;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_OTHER_2 ); }
       | OIII_TOKEN                { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_OTHER_3;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_OTHER_3 ); }
       | OIV_TOKEN                 { if ( Cur_PHS == -1 )
	                                     Channel_Type = PULSER_CHANNEL_OTHER_4;
                                     else
										 p_phase_ref( Cur_PHS,
													PULSER_CHANNEL_OTHER_4 ); }
;


/* Pod and channel assignments consists of the POD and CHANNEL keyword,
   followed by the pod or channel number(s), and, optionally, a DELAY keyword
   followed by the delay time and the INVERTED keyword. The sequence of the
   keywords is arbitrary. The last entry is for the Franfurt version of
   the pulser driver only. */


pcd:    /* empty */
      | pcd pod
      | pcd chd
      | pcd del
      | pcd inv
	  | pcd vh
      | pcd vl
	  | pcd func2 sep2             { Cur_PROT = PHASE_FFM_PROT; }
;


/* These are needed for the Frankfurt version of the driver only */

func2:  MW_TOKEN                   { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_MW ); }
      | TWT_TOKEN                  { p_phase_ref_f( Channel_Type,
													PULSER_CHANNEL_TWT ); }
      | TWT_GATE_TOKEN             { p_phase_ref_f( Channel_Type,
												   PULSER_CHANNEL_TWT_GATE ); }
      | DET_TOKEN                  { p_phase_ref_f( Channel_Type,
													PULSER_CHANNEL_DET ); }
      | DEF_TOKEN                  { p_phase_ref_f( Channel_Type,
													PULSER_CHANNEL_DEFENSE ); }
      | RF_TOKEN                   { p_phase_ref_f( Channel_Type,
													PULSER_CHANNEL_RF ); }
      | RF_GATE_TOKEN              { p_phase_ref_f( Channel_Type,
													PULSER_CHANNEL_RF_GATE ); }
      | PSH_TOKEN                  { p_phase_ref_f( Channel_Type,
												PULSER_CHANNEL_PULSE_SHAPE ); }
      | PH1_TOKEN                  { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_PHASE_1 ); }
      | PH2_TOKEN                  { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_PHASE_2 ); }
      | OI_TOKEN                   { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_OTHER_1 ); }
      | OII_TOKEN                  { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_OTHER_2 ); }
      | OIII_TOKEN                 { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_OTHER_3 ); }
      | OIV_TOKEN                  { p_phase_ref_f( Channel_Type,
												    PULSER_CHANNEL_OTHER_4 ); }
;

pod:    POD_TOKEN sep1
        pm
;

pm:     INT_TOKEN sep2             { p_assign_pod( Channel_Type,
												  vars_push( INT_VAR, $1 ) ); }
      | pm INT_TOKEN sep2          { p_assign_pod( Channel_Type,
												  vars_push( INT_VAR, $2 ) ); }
;
								  
chd:    CH_TOKEN sep1 ch1
;
								  
ch1:    INT_TOKEN sep2             { p_assign_channel( Channel_Type,
												  vars_push( INT_VAR, $1 ) ); }
      | ch1 INT_TOKEN sep2         { p_assign_channel( Channel_Type,
												  vars_push( INT_VAR, $2 ) ); }
;								  
								  
del:    DEL_TOKEN sep1 expr sep2   { p_set_delay( Channel_Type, $3 ); }
;								  
								  
inv:    INV_TOKEN sep2             { p_inv( Channel_Type ); }
;

vh:     VH_TOKEN sep1 expr sep2    { p_set_v_high( Channel_Type, $3 ); }
;

vl:     VL_TOKEN sep1 expr sep2    { p_set_v_low( Channel_Type, $3 ); }
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
       | expr AND expr       	   { $$ = vars_comp( COMP_AND, $1, $3 ); }
       | expr OR expr        	   { $$ = vars_comp( COMP_OR, $1, $3 ); }
       | expr XOR expr       	   { $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr            	   { $$ = vars_lnegate( $2 ); }
       | expr EQ expr              { $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr              { $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
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
       | '+' expr %prec NEG        { $$ = $2; }
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

/* separator between keyword and value */

sep1:    /* empty */
       | '='
       | ':'
;

/* seperator between different keyword-value pairs */

sep2:    /* empty */           
       | ','
;

/* handling of TIME_BASE commands */

tb:      TB_TOKEN expr             { p_set_timebase( $2 ); }
;

/* handling of TRIGGER_MODE lines */

tm:       TM_TOKEN tmp
;

tmp:      /* empty */
        | tmp exter sep2           { p_set_trigger_mode( vars_push( INT_VAR,
																EXTERNAL ) ); }
        | tmp inter sep2           { p_set_trigger_mode( vars_push( INT_VAR,
																INTERNAL ) ); }
        | tmp SLOPE_TOKEN sep1
		  sl_val sep2              { p_set_trigger_slope( $4 ); }
        | tmp THRESH_TOKEN sep1
		  expr sep2                { p_set_trigger_level( $4 ); }
        | tmp REPT_TOKEN sep1
		  expr sep2                { p_set_rep_time( $4 ); }
        | tmp REPF_TOKEN sep1
		  expr sep2                { p_set_rep_freq( $4 ); }
        | tmp IMP_TOKEN sep1
		  HL_TOKEN sep2            { p_set_trigger_impedance(
			                                      vars_push( INT_VAR, $4 ) ); }
;

exter:    MODE_TOKEN sep1 EXTERN_TOKEN
        | EXTERN_TOKEN
;

inter:    MODE_TOKEN sep1 INTERN_TOKEN
        | INTERN_TOKEN
;

sl_val:   NEG_TOKEN                { $$ = vars_push( INT_VAR, NEGATIVE ); }
        | POS_TOKEN                { $$ = vars_push( INT_VAR, POSITIVE ); }
        | '-'                      { $$ = vars_push( INT_VAR, NEGATIVE ); }
		| '+'                      { $$ = vars_push( INT_VAR, POSITIVE ); }
;


/* Handling of PHASE_SETUP commands */

phs:      PHS_TOK                  { Cur_PHS = $1;
                                     Cur_PHST = -1; }
          phsl
;

phsl:     /* empty */
		| phsl func sep2
        | phsl PXY_TOK sep1        { Cur_PHST = $2; }
          phsp
;

phsp:    /* empty */
		| phsp phsv  sep2          { p_phs_setup( Cur_PHS, Cur_PHST, -1, $2,
												  Cur_PROT ); }
        | phsp POD1_TOK sep1
		  phsv sep2                { p_phs_setup( Cur_PHS, Cur_PHST, 0, $4,
												  PHASE_FFM_PROT ); }
        | phsp POD2_TOK sep1
		  phsv sep2                { p_phs_setup( Cur_PHS, Cur_PHST, 1, $4,
												  PHASE_FFM_PROT ); }
		| POD_TOKEN sep1 INT_TOKEN
          sep2                     { p_phs_setup( Cur_PHS, Cur_PHST, 0, $3,
												  PHASE_BLN_PROT ); }
;

phsv:     INT_TOKEN                { $$ = $1;
                                     Cur_PROT = PHASE_UNKNOWN_PROT; } 
        | ON_TOK                   { $$ = 1;
		                             Cur_PROT = PHASE_FFM_PROT; }
        | OFF_TOK                  { $$ = 0;
		                             Cur_PROT = PHASE_FFM_PROT; }
;

/* Handling of PHASE_SWITCH_DELAY commands */

psd:      PSD_TOKEN expr           { p_set_psd( $1, $2 ); }
;

/* Handling of GRACE_PERIOD commands */

gp:       GP_TOKEN expr            { p_set_gp( $2 ); }

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
