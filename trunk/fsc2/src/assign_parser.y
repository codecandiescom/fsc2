/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

/*****************************************************************/
/* This is the parser for the ASSIGNMENTS section of an EDL file */
/*****************************************************************/


%{

#include "fsc2.h"

extern int assignlex( void );
extern char *assigntext;

/* locally used functions */

static void assignerror( const char *s );
static void ass_func( int function );
static void set_protocol( long prot );

/* locally used global variables */

static int Channel_Type;
static int Cur_PHS = -1;
static int Cur_PHST = -1;
static long Cur_PROT = PHASE_UNKNOWN_PROT;
static bool Func_is_set = UNSET;

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
%token MPL_TOKEN             /* MAXIMUM_PATTERN_LENGTH */
%token KAP_TOKEN             /* KEEP_ALL_PULSES */

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
%token NT_TOKEN UT_TOKEN MT_TOKEN T_TOKEN KT_TOKEN MGT_TOKEN

%type <lval> phsv
%type <vptr> expr list1 unit sl_val



%left EQ NE LT LE GT GE
%left AND OR XOR
%left '+' '-'
%left '*' '/'
%left '%'
%right NEG NOT
%right '^'


%%


input:   /* empty */
       | input line ';'            { Channel_Type = PULSER_CHANNEL_NO_TYPE;
	                                 Cur_PHS = -1;
	                                 Cur_PHST = -1;
									 Func_is_set = UNSET;
                                     fsc2_assert( Var_Stack == NULL ); }
       | input SECTION_LABEL       { fsc2_assert( Var_Stack == NULL );
	                                 Cur_PROT = PHASE_UNKNOWN_PROT;
									 Func_is_set = UNSET;
									 YYACCEPT; }
       | input error ';'           { Func_is_set = UNSET;
	                                 Cur_PROT = PHASE_UNKNOWN_PROT;
		                             THROW ( SYNTAX_ERROR_EXCEPTION ) }
       | input ';'                 { Channel_Type = PULSER_CHANNEL_NO_TYPE;
	                                 Cur_PHS = -1;
	                                 Cur_PHST = -1;
									 Func_is_set = UNSET;
                                     fsc2_assert( Var_Stack == NULL ); }
;


/* A (non-empty) line has to start with one of the channel keywords or either
   TIMEBASE or TRIGGERMODE */

line:    func                      { Func_is_set = SET; }
         pcd af
       | tb atb
       | tm atm
       | mpl ampl
       | phs aphs                  { p_phs_end( Cur_PHS ); }
       | psd apsd
       | gp agp
	   | KAP_TOKEN                 { keep_all_pulses( ); }
;								   


/* all the next entries are just for catching missing semicolon errors */

af:      /* empty */ 
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;


atb:     /* empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;


atm:     /* empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;

ampl:    /*empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;


aphs:    /* empty */
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;
								   

apsd:    /* empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ) }
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;


agp:     /* empty */
       | gp	func                   { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | gp	TB_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | gp	TM_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | gp	PHS_TOK                { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | gp	PSD_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ) }
       | gp	GP_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ) }
	   | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ) }
;


/* all the pulse function tokens */

func:    MW_TOKEN                  { ass_func( PULSER_CHANNEL_MW ); }
       | TWT_TOKEN                 { ass_func( PULSER_CHANNEL_TWT ); }
       | TWT_GATE_TOKEN            { ass_func( PULSER_CHANNEL_TWT_GATE ); }
       | DET_TOKEN                 { ass_func( PULSER_CHANNEL_DET ); }
	   | DET_GATE_TOKEN            { ass_func( PULSER_CHANNEL_DET_GATE ); }
       | DEF_TOKEN                 { ass_func( PULSER_CHANNEL_DEFENSE ); }
       | RF_TOKEN                  { ass_func( PULSER_CHANNEL_RF ); }
	   | RF_GATE_TOKEN             { ass_func( PULSER_CHANNEL_RF_GATE ); }
	   | PSH_TOKEN                 { ass_func( PULSER_CHANNEL_PULSE_SHAPE ); }
       | PH1_TOKEN                 { ass_func( PULSER_CHANNEL_PHASE_1 ); }
       | PH2_TOKEN                 { ass_func( PULSER_CHANNEL_PHASE_2 ); }
       | OI_TOKEN                  { ass_func( PULSER_CHANNEL_OTHER_1 ); }
       | OII_TOKEN                 { ass_func( PULSER_CHANNEL_OTHER_2 ); }
       | OIII_TOKEN                { ass_func( PULSER_CHANNEL_OTHER_3 ); }
       | OIV_TOKEN                 { ass_func( PULSER_CHANNEL_OTHER_4 ); }
;


/* Pod and channel assignments consists of the POD and CHANNEL keyword,
   followed by the pod or channel number(s), and, optionally, a DELAY keyword
   followed by the delay time and the INVERTED keyword. The sequence of the
   keywords is arbitrary. The last entry is for the Frankfurt version of the
   pulser driver only. */


pcd:    /* empty */
      | pcd pod
      | pcd chd
      | pcd del
      | pcd inv
	  | pcd vh
      | pcd vl
	  | pcd                        { set_protocol( PHASE_FFM_PROT ); }
        func sep2
;

pod:    POD_TOKEN sep1 pm
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
         list1 ']'                 { $$ = vars_arr_rhs( $4 ); }
         unit                      { $$ = apply_unit( $<vptr>6, $7 ); }
       | FUNC_TOKEN '(' list2 ')'  { $$ = func_call( $1 ); }
         unit                      { $$ = apply_unit( $<vptr>5, $6 ); }
       | VAR_REF
       | VAR_TOKEN '('             { eprint( FATAL, SET, "`%s' isn't a "
											 "function.\n", $1->name );
	                                 THROW( EXCEPTION ) }
       | FUNC_TOKEN '['            { eprint( FATAL, SET, "`%s' is a predefined"
                                             " function.\n", $1->name );
	                                 THROW( EXCEPTION ) }
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
       | MT_TOKEN  				   { $$ = vars_push( FLOAT_VAR, 10.0 ); }
       | T_TOKEN   				   { $$ = vars_push( FLOAT_VAR, 1.0e4 ); }
       | KT_TOKEN  				   { $$ = vars_push( FLOAT_VAR, 1.0e7 ); }
       | MGT_TOKEN 				   { $$ = vars_push( FLOAT_VAR, 1.0e10 ); }
       | NU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-9 ); }
       | UU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-6 ); }
       | MU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e-3 ); }
       | KU_TOKEN                  { $$ = vars_push( FLOAT_VAR, 1.0e3 ); }
       | MEG_TOKEN                 { $$ = vars_push( FLOAT_VAR, 1.0e6 ); }
;


/* list of indices for access of an array element */


list1:   /* empty */               { $$ = vars_push( UNDEF_VAR ); }
	   | expr
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

exter:    EXTERN_TOKEN
;

inter:    INTERN_TOKEN
;

sl_val:   NEG_TOKEN                { $$ = vars_push( INT_VAR, NEGATIVE ); }
        | POS_TOKEN                { $$ = vars_push( INT_VAR, POSITIVE ); }
        | '-'                      { $$ = vars_push( INT_VAR, NEGATIVE ); }
		| '+'                      { $$ = vars_push( INT_VAR, POSITIVE ); }
;


/* handling of MAXIMUM_PATTERN_LENGTH commands */

mpl:      MPL_TOKEN expr           { p_set_max_seq_len( $2 ); }
;

/* Handling of PHASE_SETUP commands */

phs:      PHS_TOK                  { Cur_PHS = $1;
                                     Cur_PHST = -1;
                                     Func_is_set = SET; }
          phsl
;

phsl:     /* empty */
		| phsl                     { set_protocol( PHASE_BLN_PROT ); }
		  func sep2
        | phsl PXY_TOK sep1        { Cur_PHST = $2; }
          phsp
;

phsp:     /* empty */
		| phsp phsv  sep2          { p_phs_setup( Cur_PHS, Cur_PHST, -1, $2,
												  Cur_PROT ); }
        | phsp POD1_TOK sep1
		  phsv sep2                { set_protocol( PHASE_FFM_PROT );
			                         p_phs_setup( Cur_PHS, Cur_PHST, 0, $4,
												  Cur_PROT ); }
        | phsp POD2_TOK sep1
		  phsv sep2                { set_protocol( PHASE_FFM_PROT );
			                         p_phs_setup( Cur_PHS, Cur_PHST, 1, $4,
												  Cur_PROT ); }
		| POD_TOKEN sep1 INT_TOKEN
          sep2                     { set_protocol( PHASE_BLN_PROT );
                                     p_phs_setup( Cur_PHS, Cur_PHST, 0, $3,
												  Cur_PROT ); }
;

phsv:     INT_TOKEN
        | ON_TOK                   { set_protocol( PHASE_FFM_PROT );
		                             $$ = 1;}
        | OFF_TOK                  { set_protocol( PHASE_FFM_PROT );
		                             $$ = 0; }
;

/* Handling of PHASE_SWITCH_DELAY commands (Frankfurt version only) */

psd:      PSD_TOKEN expr           { p_set_psd( $1, $2 );
                                     set_protocol( PHASE_FFM_PROT ); }
;

/* Handling of GRACE_PERIOD commands (Frankfurt version only) */

gp:       GP_TOKEN expr            { set_protocol( PHASE_FFM_PROT );
                                     p_set_gp( $2 ); }

%%


static void assignerror ( const char *s )
{
	s = s;                                 /* avoid compiler warning */

	if ( *assigntext == '\0' )
		eprint( FATAL, SET, "Unexpected end of file in ASSIGNMENTS "
				"section.\n" );
	else
		eprint( FATAL, SET, "Syntax error near `%s'.\n", assigntext );
	THROW( EXCEPTION )
}


/*--------------------------------------------------------------------*/
/* Called when a pulse function token has been found. If no function  */
/* has been set yet (flag `Func_is_set' is still unset) just the      */
/* global variable `Channel_Type' has to be set - thus we know that   */
/* we're at a line with a function definition. `Func_is_set' is also  */
/* set at the start of a (Berlin version) PHASE_SETUP line because a  */
/* following function token is meant to be the function the phase     */
/* setup is meant up.                                                 */
/* If `Func_is_set' is set the function token is the function the     */
/* phase function (Frankfurt version) or the PHASE_SETUP line (Berlin */
/* is for.                                                            */
/* `Func_is_set' has to be unset at the start of each line!           */
/*--------------------------------------------------------------------*/

static void ass_func( int function )
{
	if ( ! Func_is_set )
	{
		Channel_Type = function;
		return;
	}

	switch ( Cur_PROT )
	{
		case PHASE_FFM_PROT :             /* Frankfurt driver syntax */
			p_phase_ref( Cur_PROT, Channel_Type, function );
			break;

		case PHASE_BLN_PROT :             /* Berlin driver syntax */
			p_phase_ref( Cur_PROT, Cur_PHS, function );
			break;

		default :                         /* this better never happens... */
			eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION )
	}
}


/*-----------------------------------------------------------------*/
/* Called when it becomes clear which phase protocol (Frankfurt or */
/* Berlin) is to be used. If a protocol is already recognized it   */
/* can't be changed later. It mist be made sure that the global    */
/* variable `Cur_Prot' is unset and reset to `PHASE_UNKNOWN_PROT'  */
/* at the start and after the end of the ASSIGNMENTS section.      */
/*-----------------------------------------------------------------*/

static void set_protocol( long prot )
{
	if ( Cur_PROT != PHASE_UNKNOWN_PROT && Cur_PROT != prot )
	{
		eprint( FATAL, SET, "Mixing of Berlin and Frankfurt version "
				"phase syntax.\n" );
		THROW( EXCEPTION )
	}

	Cur_PROT = prot;
}
