/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


/*****************************************************************/
/* This is the parser for the ASSIGNMENTS section of an EDL file */
/*****************************************************************/


%{

#include "fsc2.h"
#include "exp.h"


void assignparser_init( void );

extern int assignlex( void );    /* defined in assign_lexer.l */
extern char *assigntext;         /* defined in assign_lexer.l */


/* locally used functions */

static void assignerror( const char * s );
static void ass_func( int function );


/* locally used global variables */

static int Channel_type;
static int Cur_PHS = -1;
static int Cur_PHST = -1;
static bool is_p_phs_setup = UNSET;
static bool Func_is_set = UNSET;
static int Dont_exec = 0;

%}


%union {
    long   lval;
    double dval;
    char   *sptr;
    Var_T  *vptr;
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
%token LSR_TOKEN             /* LASER */
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
%token INTERN_TOKEN          /* INTERNAL */
%token EXTERN_TOKEN          /* EXTERNAL */

%token TTL_TOKEN             /* TTL */
%token ECL_TOKEN             /* ECL */

%token SLOPE_TOKEN           /* SLOPE */
%token NEG_TOKEN             /* NEGATIVE */
%token POS_TOKEN             /* POSITIVE */

%token THRESH_TOKEN          /* THRESHOLD */

%token IMP_TOKEN             /* IMPEDANCE */
%token <lval> HL_TOKEN       /* LOW or HIGH */

%token REPT_TOKEN            /* REPEAT TIME */
%token REPF_TOKEN            /* REPEAT FREQUENCY */

/* The next four tokens are deprecated and will be removed sometime later */

%token MPL_TOKEN             /* MAXIMUM_PATTERN_LENGTH */
%token KAP_TOKEN             /* KEEP_ALL_PULSES */

%token <lval> PSD_TOKEN      /* PHASE SWITCH DELAY */
%token GP_TOKEN              /* GRACE_PERIOD */

%token <lval> PHS_TOK
%token POD1_TOK POD2_TOK ON_TOK OFF_TOK


%token <vptr> VAR_TOKEN      /* variable */
%token <vptr> VAR_REF
%token <vptr> FUNC_TOKEN     /* function */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN
%token <sptr> STR_TOKEN
%token <lval> PXY_TOK        /* X, Y, -X, -Y -- for phases */
%token AND OR XOR NOT
%token EQ NE LT LE GT GE

%type <lval> phsv
%type <vptr> expr list1 l1e ind sl_val strs


%left '?' ':'
%left EQ NE LT LE GT GE
%left AND OR XOR
%left '+' '-'
%left '*' '/'
%left '%'
%right NEG NOT
%right '^'


%%


input:   /* empty */
       | input line ';'            { Channel_type = PULSER_CHANNEL_NO_TYPE;
                                     Cur_PHS = -1;
                                     Cur_PHST = -1;
                                     is_p_phs_setup = UNSET;
                                     Func_is_set = UNSET;
                                     fsc2_assert( Dont_exec == 0 );
                                     fsc2_assert( EDL.Var_Stack == NULL ); }
       | input SECTION_LABEL       { Channel_type = PULSER_CHANNEL_NO_TYPE;
                                     Cur_PHS = -1;
                                     Cur_PHST = -1;
                                     is_p_phs_setup = UNSET;
                                     fsc2_assert( EDL.Var_Stack == NULL );
                                     fsc2_assert( Dont_exec == 0 );
                                     Func_is_set = UNSET;
                                     YYACCEPT; }
       | input ';'                 { Channel_type = PULSER_CHANNEL_NO_TYPE;
                                     Cur_PHS = -1;
                                     Cur_PHST = -1;
                                     is_p_phs_setup = UNSET;
                                     Func_is_set = UNSET;
                                     fsc2_assert( Dont_exec == 0 );
                                     fsc2_assert( EDL.Var_Stack == NULL ); }
;


/* A (non-empty) line has to start with one of the channel keywords, TIMEBASE,
   TRIGGERMODE, the phase-setup keyword, GRACE_PERIOD, MAXIMUM_PATTERN_LEN,
   PHASE_SWITCH_DELAY or KEEP_ALL_PULSES */

line:    func                      { Func_is_set = SET; }
         pcd af
       | tb atb
       | tm atm
       | mpl ampl
       | phs aphs                  { if ( ! is_p_phs_setup )
                                     {
                                         print( FATAL, "Syntax error near "
                                                "'%s'.\n", assigntext );
                                         THROW( EXCEPTION );
                                     }
                                     p_phs_end( Cur_PHS ); }
       | psd apsd
       | gp agp
       | KAP_TOKEN                 { keep_all_pulses( ); }
;


/* all the next entries are just for catching missing semicolon errors */

af:      /* empty */
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;


atb:     /* empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;


atm:     /* empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;

ampl:    /*empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;


aphs:    /* empty */
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PSD_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;


apsd:    /* empty */
       | func                      { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TB_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | TM_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | PHS_TOK                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | GP_TOKEN                  { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;


agp:     /* empty */
       | gp func                   { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp TB_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp TM_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | MPL_TOKEN                 { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp PHS_TOK                { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp PSD_TOKEN              { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | gp GP_TOKEN               { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | SECTION_LABEL             { THROW( MISSING_SEMICOLON_EXCEPTION ); }
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
       | LSR_TOKEN                 { ass_func( PULSER_CHANNEL_LASER ); }
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
   keywords is arbitrary. The last entry is for pulsers with phase switches
   only. */


pcd:    /* empty */
      | pcd pod
      | pcd chd
      | pcd del
      | pcd inv
      | pcd vh
      | pcd vl
      | pcd func sep2
;

pod:    POD_TOKEN sep1 pm
;

pm:     INT_TOKEN sep2             { p_assign_pod( Channel_type,
                                                  vars_push( INT_VAR, $1 ) ); }
      | pm INT_TOKEN sep2          { p_assign_pod( Channel_type,
                                                  vars_push( INT_VAR, $2 ) ); }
;

chd:    CH_TOKEN sep1 ch
;

ch:     INT_TOKEN sep2             { p_assign_channel( Channel_type,
                                                  vars_push( INT_VAR, $1 ) ); }
      | ch INT_TOKEN sep2          { p_assign_channel( Channel_type,
                                                  vars_push( INT_VAR, $2 ) ); }
;

del:    DEL_TOKEN sep1 expr sep2   { p_set_delay( Channel_type, $3 ); }
;

inv:    INV_TOKEN sep2             { p_inv( Channel_type ); }
;

vh:     VH_TOKEN sep1 expr sep2    { p_set_v_high( Channel_type, $3 ); }
;

vl:     VL_TOKEN sep1 expr sep2    { p_set_v_low( Channel_type, $3 ); }
;

expr:    INT_TOKEN                { if ( ! Dont_exec )
                                         $$ = vars_push( INT_VAR, $1 ); }
       | FLOAT_TOKEN              { if ( ! Dont_exec )
                                         $$ = vars_push( FLOAT_VAR, $1 ); }
       | VAR_TOKEN                 { if ( ! Dont_exec )
                                         $$ = vars_push_copy( $1 ); }
       | VAR_TOKEN '('             { print( FATAL, "'%s' isn't a function.\n",
                                            $1->name );
                                     THROW( EXCEPTION ); }
       | VAR_TOKEN '['             { if ( ! Dont_exec )
                                         vars_arr_start( $1 ); }
         list1 ']'                 { if ( ! Dont_exec )
                                         $$ = vars_arr_rhs( $4 ); }
       | FUNC_TOKEN '(' list2 ')'  { if ( ! Dont_exec )
                                         $$ = func_call( $1 ); }
       | FUNC_TOKEN                { print( FATAL, "'%s' is a predefined "
                                            "function.\n", $1->name );
                                     THROW( EXCEPTION ); }
       | VAR_REF
       | expr AND                  { if ( ! Dont_exec )
                                     {
                                         if ( ! check_result( $1 ) )
                                         {
                                             Dont_exec++;
                                             vars_pop( $1 );
                                         }
                                     }
                                     else
                                         Dont_exec++;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_AND, $1, $4 );
                                     else if ( ! --Dont_exec )
                                         $$ = vars_push( INT_VAR, 0L );
                                   }
       | expr OR                   { if ( ! Dont_exec )
                                     {
                                         if ( check_result( $1 ) )
                                         {
                                             Dont_exec++;
                                             vars_pop( $1 );
                                         }
                                     }
                                     else
                                         Dont_exec++;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_OR, $1, $4 );
                                     else if ( ! --Dont_exec )
                                         $$ = vars_push( INT_VAR, 1L );
                                   }
       | expr XOR expr             { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_XOR, $1, $3 ); }
       | NOT expr                  { if ( ! Dont_exec )
                                         $$ = vars_lnegate( $2 ); }
       | expr EQ expr              { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | expr NE expr              { if ( ! Dont_exec )
                                      $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | expr LT expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | expr GT expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | expr LE expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS_EQUAL,
                                                         $1, $3 ); }
       | expr GE expr              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS_EQUAL,
                                                         $3, $1 ); }
       | strs EQ strs              { if ( ! Dont_exec )
                                        $$ = vars_comp( COMP_EQUAL, $1, $3 ); }
       | strs NE strs              { if ( ! Dont_exec )
                                      $$ = vars_comp( COMP_UNEQUAL, $1, $3 ); }
       | strs LT strs              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $1, $3 ); }
       | strs GT strs              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS, $3, $1 ); }
       | strs LE strs              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS_EQUAL,
                                                         $1, $3 ); }
       | strs GE strs              { if ( ! Dont_exec )
                                         $$ = vars_comp( COMP_LESS_EQUAL,
                                                         $3, $1 ); }
       | '+' expr %prec NEG        { if ( ! Dont_exec )
                                         $$ = $2; }
       | '-' expr %prec NEG        { if ( ! Dont_exec )
                                         $$ = vars_negate( $2 ); }
       | expr '+' expr             { if ( ! Dont_exec )
                                         $$ = vars_add( $1, $3 ); }
       | strs '+' strs             { if ( ! Dont_exec )
                                         $$ = vars_add( $1, $3 ); }
       | expr '-' expr             { if ( ! Dont_exec )
                                         $$ = vars_sub( $1, $3 ); }
       | expr '*' expr             { if ( ! Dont_exec )
                                         $$ = vars_mult( $1, $3 ); }
       | expr '/' expr             { if ( ! Dont_exec )
                                         $$ = vars_div( $1, $3 ); }
       | expr '%' expr             { if ( ! Dont_exec )
                                         $$ = vars_mod( $1, $3 ); }
       | expr '^' expr             { if ( ! Dont_exec )
                                         $$ = vars_pow( $1, $3 ); }
       | '(' expr ')'              { if ( ! Dont_exec )
                                         $$ = $2; }
       | expr '?'                  { if ( ! Dont_exec )
                                     {
                                         if ( ! check_result( $1 ) )
                                             Dont_exec++;
                                         vars_pop( $1 );
                                     }
                                     else
                                         Dont_exec +=2;
                                   }
         expr ':'                  { if ( ! Dont_exec )
                                         Dont_exec++;
                                     else
                                         Dont_exec--;
                                   }
         expr                      { if ( ! Dont_exec )
                                         $$ = $7;
                                     else if ( ! --Dont_exec )
                                         $$ = $4;
                                   }
;

/* list of indices for access of an array element */


list1:   /* empty */              { if ( ! Dont_exec )
                                         $$ = vars_push( UNDEF_VAR ); }
       | l1e                      { if ( ! Dont_exec )
                                         $$ = $1; }
;

l1e:     ind                      { if ( ! Dont_exec )
                                         $$ = $1; }
       | l1e ',' ind              { if ( ! Dont_exec )
                                         $$ = $3; }
;

ind:     expr                     { if ( ! Dont_exec )
                                        $$ = $1; }
       | expr ':'                 { if ( ! Dont_exec )
                                        vars_push( STR_VAR, ":" ); }
         expr                     { if ( ! Dont_exec )
                                        $$ = $4; }
;


/* list of function arguments */

list2:   /* empty */
       | l2e
;

l2e:     exprs
       | l2e ',' exprs
;

exprs:   expr                     { }
       | strs                     { }
;

strs:    STR_TOKEN                { if ( ! Dont_exec )
                                        $$ = vars_push( STR_VAR, $1 ); }
       | strs STR_TOKEN           { if ( ! Dont_exec )
                                    {
                                        Var_T *v = vars_push( STR_VAR, $2 );
                                        $$ = vars_add( v->prev, v );
                                     }
                                   }
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

tb:      TB_TOKEN expr sep2 tb1    { p_set_timebase( $2 ); }
       | TB_TOKEN tb2 expr         { p_set_timebase( $3 ); }
;

tb1:     /* empty */
       | tb2
;

tb2:     TTL_TOKEN sep2            { p_set_timebase_level( TTL_LEVEL ); }
       | ECL_TOKEN sep2            { p_set_timebase_level( ECL_LEVEL ); }
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
        | tmp TTL_TOKEN sep2       { p_set_trigger_level( vars_push(
                                         FLOAT_VAR, ( double ) TTL_LEVEL ) ); }
        | tmp ECL_TOKEN sep2       { p_set_trigger_level( vars_push(
                                         FLOAT_VAR, ( double ) ECL_LEVEL ) ); }
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

phs:      PHS_TOK                  { p_phs_check( );
                                     Cur_PHS = $1;
                                     Cur_PHST = -1;
                                     Func_is_set = SET; }
          phsl
;

phsl:     /* empty */
        | phsl
          func sep2
        | phsl PXY_TOK sep1        { Cur_PHST = $2; }
          phsp
;

phsp:     /* empty */
        | phsp phsv sep2           { p_phs_setup( Cur_PHS, Cur_PHST,
                                                  -1, $2, UNSET );
                                     is_p_phs_setup = SET; }
        | phsp POD1_TOK sep1
          phsv sep2                { p_phs_setup( Cur_PHS, Cur_PHST,
                                                  0, $4, SET );
                                     is_p_phs_setup = SET; }
        | phsp POD2_TOK sep1
          phsv sep2                { p_phs_setup( Cur_PHS, Cur_PHST,
                                                  1, $4, SET );
                                     is_p_phs_setup = SET; }
        | POD_TOKEN sep1 INT_TOKEN
          sep2                     { p_phs_setup( Cur_PHS, Cur_PHST,
                                                  0, $3, SET );
                                     is_p_phs_setup = SET; }
        | CH_TOKEN sep1 INT_TOKEN
          sep2                     { p_phs_setup( Cur_PHS, Cur_PHST,
                                                  0, $3, UNSET );
                                     is_p_phs_setup = SET; }
;

phsv:     INT_TOKEN                { $$ = $1; }
        | ON_TOK                   { $$ =  1; }
        | OFF_TOK                  { $$ =  0; }
;

/* Handling of PHASE_SWITCH_DELAY commands (pulsers with phase switch only) */

psd:      PSD_TOKEN expr           { p_set_psd( $1, $2 ); }
;

/* Handling of GRACE_PERIOD commands (pulsers with phase switch only) */

gp:       GP_TOKEN expr            { p_set_gp( $2 ); }
;

%%


/*---------------------------------*
 * Called in case of syntax errors
 *---------------------------------*/

static void
assignerror( const char * s  UNUSED_ARG )
{
    if ( *assigntext == '\0' )
        print( FATAL, "Unexpected end of file in ASSIGNMENTS section.\n" );
    else
        print( FATAL, "Syntax error near '%s'.\n", assigntext );
    THROW( EXCEPTION );
}


/*----------------------------------------------------*
 * Called when a pulse function token has been found.
 *----------------------------------------------------*/

static void
ass_func( int function )
{
    /* If we're at the start of a function line (i.e. when 'Func_is_set' is
       unset) test if the function is usable (if there are no phase switches
       we can't use the PHASE functions). Then store the current function in a
       global variable for later use. */

    if ( ! Func_is_set )
    {
        p_exists_function( function );
        Channel_type = function;
        return;
    }

    /* Otherwise there are two valid alternatives. We're either in within
       a function line - in this case we're dealing with a pulser that
       has phase switches and this is the declaration of the function that
       needs phase switching */

    if ( Channel_type != PULSER_CHANNEL_NO_TYPE )
    {
        if ( Pulser_Struct[ Cur_Pulser ].needs_phase_pulses )
            p_phase_ref( Channel_type, function );
        else
        {
            print( FATAL, "Syntax error near '%s' when using pulser %s.\n",
                   assigntext, Pulser_Struct[ Cur_Pulser ].name );
            THROW( EXCEPTION );
        }
        return;
    }

    /* The other alternative is that we're dealing with a pulser with no
       phase switches. In this case Cur_PHS must be set to the number of
       the PHASE_SETUP (i.e. 0 or 1 for the first or second PHASE_SETUP) */

    if (    ! Pulser_Struct[ Cur_Pulser ].needs_phase_pulses
         && Cur_PHS != -1 )
        p_phase_ref( Cur_PHS, function );
    else
    {
        print( FATAL, "Syntax error near '%s' when using pulser %s.\n",
               assigntext, Pulser_Struct[ Cur_Pulser ].name );
        THROW( EXCEPTION );
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
assignparser_init( void )
{
    Dont_exec = 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
